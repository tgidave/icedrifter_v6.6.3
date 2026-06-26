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

// =============================================================================
// Pin map and tunable constants
// =============================================================================

#define ONE_WIRE_BUS 21
#define MS5837_DS18B20_GPS_POWER_PIN 14


// 1-Wire data line. Needs an external 4.7 kohm pull-up to 3V3.
#define DATA_PIN            ONE_WIRE_BUS

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

// =============================================================================
// State
// =============================================================================

OneWire bus(DATA_PIN);
uint8_t roms[MAX_SENSORS][8];   // ROMs in physical cable order (head first)
uint8_t sensor_count;           // number of valid entries in roms[]

// =============================================================================
// Helpers
// =============================================================================

// Print one 64-bit ROM as space-separated hex bytes (head first).
void print_rom(const uint8_t* rom) {
  for (uint8_t i = 0; i < 8; i++) {
    if (rom[i] < 0x10) Serial.print('0');
    Serial.print(rom[i], HEX);
    if (i != 7) Serial.print(' ');
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

uint8_t discover_chain() {
  // Step 1 — arm chain mode on every chip on the bus.
  bus.reset();
  bus.write(CMD_SKIP_ROM);
  bus.write(CMD_CHAIN);
  bus.write(CHAIN_ON_A);
  bus.write(CHAIN_ON_B);
  bus.read();   // 0xAA confirmation byte

  // Step 2 — walk the chain, one chip per iteration.
  uint8_t n = 0;
  while (n < MAX_SENSORS) {
    bus.reset();
    bus.write(CMD_COND_READ_ROM);

    uint8_t rom[8];
    for (uint8_t i = 0; i < 8; i++) rom[i] = bus.read();

    if (rom[0] == 0xFF) break;                                // end of chain
    if (rom[0] != FAMILY_DS28EA00) break;                     // wrong device
    if (OneWire::crc8(rom, 7) != rom[7]) break;               // bad CRC

    memcpy(roms[n], rom, 8);
    n++;

    // Hand the chain token off to the next chip.
    bus.write(CMD_CHAIN);
    bus.write(CHAIN_DONE_A);
    bus.write(CHAIN_DONE_B);
    bus.read();
  }
  return n;
}

// =============================================================================
// Single-sensor temperature read
//
// Returns the chip's last-converted temperature in degrees C, or NAN on a
// CRC error. Caller must have issued Convert T and waited tCONV first.
// =============================================================================

float read_temperature(const uint8_t* rom) {
  if (!bus.reset()) return NAN;
  bus.write(CMD_MATCH_ROM);
  for (uint8_t i = 0; i < 8; i++) bus.write(rom[i]);
  bus.write(CMD_READ_SCRATCHPAD);

  uint8_t s[9];
  for (uint8_t i = 0; i < 9; i++) s[i] = bus.read();
  if (OneWire::crc8(s, 8) != s[8]) return NAN;

  int16_t raw = ((int16_t)s[1] << 8) | s[0];
  return raw * 0.0625f;   // 1/16 C per LSB at 12-bit resolution
}

// =============================================================================
// Arduino entry points
// =============================================================================

void setup() {
  // 1. Open the USB serial console.
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 3000) { /* wait briefly for USB */ }

  Serial.println();
  Serial.println("=== OpenString firmware ===");
  Serial.print("Data pin: D"); Serial.println(DATA_PIN);
  Serial.println();

  pinMode(MS5837_DS18B20_GPS_POWER_PIN, OUTPUT);
  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, HIGH);

  delay(1000);

  // 2. Discover all sensors via chain mode (one-shot at boot).
  sensor_count = discover_chain();
  Serial.print("Discovered "); Serial.print(sensor_count);
  Serial.println(" sensor(s) in physical cable order:");
  for (uint8_t i = 0; i < sensor_count; i++) {
    Serial.print("  "); Serial.print(i + 1); Serial.print(": [");
    print_rom(roms[i]);
    Serial.println("]");
  }
  Serial.println();
}

void loop() {
  if (sensor_count == 0) {
    Serial.println("No sensors. Check wiring and reset.");
    delay(5000);
    return;
  }

  // 1. Kick off a temperature conversion on every chip at once.
  bus.reset();
  bus.write(CMD_SKIP_ROM);
  bus.write(CMD_CONVERT_T);
  delay(CONVERT_T_MS);

  // 2. Read each chip in cable order and print its temperature.
  Serial.print("--- t=");
  Serial.print(millis());
  Serial.println(" ms ---");
  for (uint8_t i = 0; i < sensor_count; i++) {
    float c = read_temperature(roms[i]);
    Serial.print("  "); Serial.print(i + 1); Serial.print(": ");
    if (isnan(c)) Serial.println("CRC error");
    else { Serial.print(c, 2); Serial.println(" C"); }
  }
  Serial.println();

  // 3. Wait until the next cycle.
  delay(INTERVAL_MS);
}
