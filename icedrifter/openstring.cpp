// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Cryosphere Innovation
// (a wholly owned subsidiary of JASCO Applied Sciences).
//
// Contact: Dr. Cameron Planck <cjp@cryosphereinnovation.com>
//
// =============================================================================
// OpenString reference firmware — DS28EA00 temperature strings
// =============================================================================
//
// What this sketch does
// ---------------------
// This code reads a chain of DS28EA00 1-Wire temperature sensors from
// the OpenString PCB and prints each reading over serial in physical
// order. It is designed as a reference implementation and starting
// point for using the OpenString PCB. The structure and flow of this
// code can be easily adapted into custom firmware routines to
// implement OpenString in any project.
//
// Where it runs
// -------------
// For simplicity, this code is written as an Arduino sketch. It is
// verified on an Adafruit Feather M0, but can be ported to virtually
// any Arduino-compatible board with a free GPIO.
//
// Hardware requirements
// ---------------------
// To run an OpenString, the host needs one 3.3 V GPIO for the 1-Wire
// data line, a 3.3 V supply, and a shared ground. The 1-Wire bus also
// requires a pull-up resistor (typically 4.7 kohm) between the data
// line and 3.3 V. The 1-Wire protocol supports parasitic power, but
// results with OpenString are mixed because the 20+ chips on the bus
// all draw current simultaneously during Convert T — a dedicated
// supply is recommended.
//
// Porting outside Arduino
// -----------------------
// This firmware can be ported to other platforms and languages by
// adapting the core logical flow: CHAIN ON, CONDITIONAL READ ROM, and
// CHAIN DONE for discovery, then Convert T, Match ROM, and Read
// Scratchpad for the reads. These are just serial byte sequences on
// the 1-Wire bus, so any 1-Wire master can run the same sequence.
//
// Dependencies
// ------------
// One Arduino library is required: OneWire. PlatformIO is recommended
// for development, dependency management, and version control. The sketch can also be run
// directly from the Arduino IDE; in that case install the library via
// Sketch > Include Library > Manage Libraries.
//
// =============================================================================

#include <Arduino.h>
#include <OneWire.h>

// Local includes
#include "icedrifter.h"
#include "openstring.h"

/*
// =============================================================================
// Pin map and tunable constants
// =============================================================================

// 1-Wire data line. Needs an external 4.7 kohm pull-up to 3V3.
#define DATA_PIN            2

// Maximum sensors expected on one string. Costs 8 bytes of RAM each.
#define MAX_SENSORS         40

// 12-bit Convert T worst case (DS28EA00 datasheet, Conversion Time row).
#define CONVERT_T_MS        750

// Time between completed read cycles.
#define INTERVAL_MS         10000

// =============================================================================
// DS28EA00 / 1-Wire commands
//
// References below are to the DS28EA00 datasheet (rev 2, 6/19) sections
// "1-Wire ROM Function Commands" and "Control Function Commands".
// =============================================================================

#define CMD_MATCH_ROM        0x55  // address one chip by its 64-bit ROM
#define CMD_SKIP_ROM         0xCC  // address every chip on the bus
#define CMD_COND_READ_ROM    0x0F  // chain-only: ROM of the selected chip
#define CMD_CONVERT_T        0x44  // start a temperature conversion
#define CMD_READ_SCRATCHPAD  0xBE  // read the 9-byte scratchpad
#define CMD_CHAIN            0x99  // followed by a 2-byte chain sub-command

// Chain sub-command pairs: a control byte followed by its bitwise inverse.
#define CHAIN_ON_A           0x5A
#define CHAIN_ON_B           0xA5
#define CHAIN_DONE_A         0x96
#define CHAIN_DONE_B         0x69

#define FAMILY_DS28EA00      0x42  // first byte of every DS28EA00 ROM
*/

// =============================================================================
// State
// =============================================================================

OneWire owbus(ONE_WIRE_BUS);  // Instantiate OneWire.

uint8_t osSensorCount;  // number of valid entries in romAddresss[]

uint8_t romAddress[OS_MAX_SENSORS][8];  // ROM addresses in physical cable order (head first)

float openStringData[OS_MAX_SENSORS];

const char temperature[OS_MAX_SENSORS][10]; // Table of temperatures decending the string

//const char nextBuff[MESSAGE_BUFF_SIZE]; // Used to hold the next page of the open_string data

// =============================================================================
// Helpers
// =============================================================================

// Print one 64-bit ROM as space-separated hex bytes (head first).
void printRomAddress(const uint8_t *rom) {

  for (uint8_t i = 0; i < 8; i++) {

    if (rom[i] < 0x10) {
      DEBUG_SERIAL.print('0');
    }

    DEBUG_SERIAL.print(rom[i], HEX);

    if (i != 7) {
      DEBUG_SERIAL.print(' ');
    }
  }
}

// =============================================================================
// Chain-mode ROM discovery
//
// How chain mode works (DS28EA00 datasheet, "Chain Function"):
//
//   - Each chip has two extra pins, PIOB (input) and PIOA (output). On the
//     cable, chip 1's PIOB is tied to GND; chip N's PIOA drives chip N+1's
//     PIOB. This auxiliary conductor is what gives us physical order.
//
//   - CHAIN ON (0x99 + 5A A5) places every chip in "chain ON" state. In
//     that state, only the chip whose PIOB is currently low responds to
//     CONDITIONAL READ ROM (0x0F).
//
//   - After we read that chip's ROM, CHAIN DONE (0x99 + 96 69) makes it
//     pull its PIOA low, which lowers the next chip's PIOB and makes that
//     chip the new responder.
//
//   - When every chip has been read, the next 0x0F goes unanswered and
//     the bus idles high, so we read 0xFF and stop.
// =============================================================================

uint8_t openStringDiscoverChain() {

  uint8_t i;
  uint8_t n = 0;
  uint8_t rom[8];
  uint8_t compByte;

  // Step 1 — arm chain mode on every chip on the bus.
  owbus.reset();
  owbus.write(CMD_SKIP_ROM);
  owbus.write(CMD_CHAIN);
  owbus.write(CHAIN_ON_A);
  owbus.write(CHAIN_ON_B);
  owbus.read();   // 0xAA confirmation byte

//  if (compByte != 0xAA) {
//    DEBUG_SERIAL.print("compByte != 0xAA. ");
//    DEBUG_SERIAL.print(compByte, HEX);
//    DEBUG_SERIAL.print("\n");
//    return 0;
//  }

  // Step 2 — walk the chain, one chip per iteration.
  while (n < OS_MAX_SENSORS) {
    owbus.reset();
    owbus.write(CMD_COND_READ_ROM);

    for (uint8_t i = 0; i < 8; i++) {
      rom[i] = owbus.read();
    }

    if (rom[0] == 0xFF) {

#ifdef SERIAL_DEBUG_OPEN_STRING
      DEBUG_SERIAL.println("rom[0] == 0xFF.");
#endif  // SERIAL_DEBUG_OPEN_STRING

      break;  // end of chain
    }

    if (rom[0] != FAMILY_DS28EA00) {
      DEBUG_SERIAL.print("rom[0] != FAMILY_DS28EA00. ");
      DEBUG_SERIAL.print(rom[0], HEX);
      DEBUG_SERIAL.print("\n");
    }

    if (OneWire::crc8(rom, 7) != rom[7]) {
      DEBUG_SERIAL.println("rom CRC error.");
      break;  // bad CRC
    }

    memcpy(romAddress[n], rom, 8);
    n++;

    // Hand the chain token off to the next chip.
    owbus.write(CMD_CHAIN);
    owbus.write(CHAIN_DONE_A);
    owbus.write(CHAIN_DONE_B);
    owbus.read();
  }

  DEBUG_SERIAL.println(n);
  return n;
}

// =============================================================================
// Single-sensor temperature read
//
// Returns the chip's last-converted temperature in degrees C, or NAN on a
// CRC error. Caller must have issued Convert T and waited tCONV first.
// =============================================================================

float openStringReadTemperature(const uint8_t *rom) {

  float raw;
  uint8_t s[9];
  uint8_t i;
  const char outBuff[10];

//#ifdef SERIAL_DEBUG_OPEN_STRING
//  DEBUG_SERIAL.println("Starting openStringReadTemperature.");
//#endif  // SERIAL_DEBUG_OPEN_STRING

  if (!owbus.reset()) {

#ifdef SERIAL_DEBUG_OPEN_STRING
    DEBUG_SERIAL.println("bus reset failed - returning NAN.");
#endif  // SERIAL_DEBUG_OPEN_STRING

    return NAN;
  }

  owbus.write(CMD_MATCH_ROM);

  for (i = 0; i < 8; i++) {
    owbus.write(rom[i]);
  }

  owbus.write(CMD_READ_SCRATCHPAD);


  for (i = 0; i < 9; i++) {
    s[i] = owbus.read();
  }

  if (OneWire::crc8(s, 8) != s[8]) {

#ifdef SERIAL_DEBUG_OPEN_STRING
    DEBUG_SERIAL.println("Bad CRC - returning NAN.");
#endif  // SERIAL_DEBUG_OPEN_STRING

    return NAN;
  }

  // cast the second byte of s to an unsigned int and shift it over 8 bits
  // or in the first byte of s and cast the result to a float and save it.
  raw = (float)(((int16_t)s[1] << 8) | s[0]);

  // 1/16 C per LSB at 12-bit resolution
  raw *= 0.0625f;

//#ifdef SERIAL_DEBUG_OPEN_STRING
//    dtostrf(raw, 6, 2, outBuff);
//    DEBUG_SERIAL.print("temp = ");
//    DEBUG_SERIAL.println(outBuff);
//#endif  // SERIAL_DEBUG_OPEN_STRING

  return raw;   // return the temperature.
}

// =============================================================================
// Arduino entry points
// =============================================================================

void openStringInit(void) {

//  uint32_t t0 = millis();

  // 1. Open the USB serial console.
//  Serial.begin(115200);

  // wait briefly for USB
//  while ((millis() - t0) < 3000) {}

#ifdef SERIAL_DEBUG_OPEN_STRING
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("=== OpenString firmware ===");
  DEBUG_SERIAL.print("ONE_WIRE_BUS: D");
  DEBUG_SERIAL.println(ONE_WIRE_BUS);
  DEBUG_SERIAL.println();
#endif // SERIAL_DEBUG_OPEN_STRING

  // 2. Discover all sensors via chain mode (one-shot at boot).
  osSensorCount = openStringDiscoverChain();

  for (uint8_t i = 0; i < osSensorCount; i++) {

#ifdef SERIAL_DEBUG_OPEN_STRING
  if (i < 9) { // For counts 0 through 9 print a leading 0;
    DEBUG_SERIAL.print(0);
  }

  DEBUG_SERIAL.print(i + 1);
  DEBUG_SERIAL.print(": [");
  printRomAddress(romAddress[i]);
  DEBUG_SERIAL.println("]");
#endif // SERIAL_DEBUG_OPEN_STRING

  }

#ifdef SERIAL_DEBUG_OPEN_STRING
  DEBUG_SERIAL.print("\n");
  DEBUG_SERIAL.print(osSensorCount);
  DEBUG_SERIAL.println(" sensor(s) in physical cable order:");
  DEBUG_SERIAL.print("Discovered ");
  DEBUG_SERIAL.print("  ");
#endif // SERIAL_DEBUG_OPEN_STRING

}

void openStringReadTemps(void) {

  int i;
  const char tempBuff[32];
  const char crcErr = "CRC error";

  if (osSensorCount == 0) {

#ifdef SERIAL_DEBUG_OPEN_STRING
    DEBUG_SERIAL.println("No sensors. Check wiring and reset.");
    delay(5000);
#endif // SERIAL_DEBUG_OPEN_STRING

    return;
  }

  // Kick off a temperature conversion on every chip at once.
  owbus.reset();
  owbus.write(CMD_SKIP_ROM);
  owbus.write(CMD_CONVERT_T);
  delay(CONVERT_T_MS);

//#ifdef SERIAL_DEBUG_OPEN_STRING
//  DEBUG_SERIAL.print("--- t=");
//  DEBUG_SERIAL.print(millis());
//  DEBUG_SERIAL.println(" ms ---");
//#endif // SERIAL_DEBUG_OPEN_STRING

  // Read each chip in cable order and print its temperature.
  for (i = 0; i < osSensorCount; i++) {

    openStringData[i] = openStringReadTemperature(romAddress[i]);

#ifdef SERIAL_DEBUG_OPEN_STRING
    dtostrf(openStringData[i], 6, 2, temperature[i]);
    sprintf(tempBuff, "  %02d: %s C\n",i + 1, temperature[i]);
    DEBUG_SERIAL.print(tempBuff);
#endif // SERIAL_DEBUG_OPEN_STRING

/*    
#ifdef SERIAL_DEBUG_OPEN_STRING
    DEBUG_SERIAL.print("  ");
    DEBUG_SERIAL.print(i + 1);
    DEBUG_SERIAL.print(": ");

    if (isnan(temperature[i])) {
      DEBUG_SERIAL.println("CRC error");

    } else {

      DEBUG_SERIAL.print(temperature[i]);
      DEBUG_SERIAL.println(" C"); }
#endif // SERIAL_DEBUG_OPEN_STRING
*/

  }

#ifdef SERIAL_DEBUG_OPEN_STRING
  DEBUG_SERIAL.print("\n");
#endif // SERIAL_DEBUG_OPEN_STRING
}

int openStringGetSensorCount(void) {
  return (osSensorCount);
}float* openStringGetTempArrayAddr(void) {
  return((float *)&openStringData);
}

float openStringGetTempByIndex(int ix) {
  if ((ix < osSensorCount) && (osSensorCount > 0)) {
    return (openStringData[ix]);
  }

#ifdef SERIAL_DEBUG_OPEN_STRING
    DEBUG_SERIAL.println("openStringGetTempByIndex called with invalid value = ");
    DEBUG_SERIAL.print(ix);
    DEBUG_SERIAL.print("\n");
#endif // SERIAL_DEBUG_OPEN_STRING

  return(-1);
}


