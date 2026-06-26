/*                                                                              
 *  icedrifter.ino                                                  
 *                                                                               
 *  Code to implement the Icedrifter buoy.                     
 *                                                                               
 *  Introduction                                              
 *  
 *  This code implements functionality that gathers GPS location data,
 *  temperature and air pressure data, and optionally, a remote
 *  temperature probe and light and temperature chain data.  This data
 *  is then sent back to the user using the Iridium system on a
 *  periodic basis.
 *                                                                               
 *  Author                                                       
 *                                                                               
 *  Uncle Dave                                                  
 *                                                                               
 *  License                                                     
 *                                                                               
 *  Unknown (Talk to Cy)                                                        
 *                                                                               
 *  HISTORY                                                             
 *                                                                               
 *  v1.0   - First release
 *  v1.0.1 - Fixed a problem with the date and time of the sample.
 *  v1.1.0  - Added ability to transmit messages from other processors.
 */

#include <LowPower.h>
#include <time.h>
#include <TinyGPS++.h> // NMEA parsing: http://arduiniana.org
#include <bosejis_PString.h> // String buffer formatting: http://arduiniana.org

#include "icedrifter.h"
#include "gps.h"
#include "ms5837_02ba.h"
#include "rockblock.h"

#if defined(PROCESS_DS18B20) && defined(PROCESS_OPEN_STRING)
    #error "PROCESS_DS18B20 and PROCESS_OPEN_STRING can not be on at the same time."
    #error "Both devices use the same I/O port and I/O connector."
#endif

#if defined(PROCESS_CHAIN_DATA) && defined(PROCESS_OPEN_STRING)
    #error "PROCESS_CHAIN_DATA and PROCESS_OPEN_STRING can not be on at the same time."
    #error "Both sensors need a lot of memory and have not been tested together."
    #error "Call if you need this ability and I will try to get it going."
#endif

#ifdef PROCESS_DS18B20
  #include "ds18b20.h"
#endif // PROCESS_DS18B20

#ifdef PROCESS_CHAIN_DATA
  #include "chain.h"
#endif // PROCESS_CHAIN_DATA

#ifdef PROCESS_MESSAGE
  #include "message.h"
#endif // PROCESS_MESSAGE

#ifdef PROCESS_OPEN_STRING
  #include "openstring.h"
#endif // PROCESS_OPEN_STRING


#define CONSOLE_BAUD 115200

// This table is used to determine when to report data through the
// Iridium system.  It is set up as UTC times from midnight to 23:00
// hours.  If an hour is set to false, the system will not report.
// If an hour is set to true, a report will be attempted on the next
// half hour.  I.E. if Noon UTC is set to true, the system will try
// to report at 12:30 UTC.  If all hour entries are set to true the
// system will try to report every hour on the half hour.  If all hour
// entries are set to false, the system will never try to report.
//
// This table is set for standard time and does not account for local
// daylight savings time.

const bool timeToReport[24] = {
  false,  // Midnight UTC
  true,   // 01:00 UTC - 6PM MST Thistle Grove time.
  false,  // 02:00 UTC
  false,  // 03:00 UTC
  false,  // 04:00 UTC
  false,  // 05:00 UTC
  false,  // 06:00 UTC
  true,   // 07:00 UTC - Midnight MST
  false,  // 08:00 UTC
  false,  // 09:00 UTC
  false,  // 10:00 UTC
  false,  // 11:00 UTC
  false,  // Noon  UTC
  true,   // 13:00 UTC - 6AM MST Thistle Grove time.
  false,  // 14:00 UTC
  false,  // 15:00 UTC
  false,  // 16:00 UTC
  false,  // 17:00 UTC
  false,  // 18:00 UTC
  true,   // 19:00 UTC - Noon MST
  false,  // 20:00 UTC
  false,  // 21:00 UTC  
  false,  // 22:00 UTC
  false,  // 23:00 UTC
};

bool firstTime;   // Set true in the setup function and set false after the first
                  // time through the loop function.  Used to indicate when to
                  // capture the last boot date and time.

bool gotFullFix;  // Indicates that a Full fix was received.

int noFixFoundCount;  // Number of times the GPS device could not get a fix.

int fixFound;     // indicates weather the last call to the GPS system returned a fix.

int totalDataLength;  // total length of the output data.

icedrifterData idData;  // Structure for accumulating and sending sensor data,

time_t lbTime;  // Time and date of the last boot.

#ifdef PROCESS_MESSAGE
uint8_t messageBuff[MESSAGE_BUFF_SIZE]; // Buffer used to receive and transmit any available message.
bool gotMessage;                        // Indicator that a message was received and needs to be transmitted.
#endif  // PROCESS_MESSAGE

// print hex charactors mainly for debugging perposes.

const char hexchars[] = "0123456789ABCDEF";

void printHexChar(uint8_t x) {
  Serial.print(hexchars[(x >> 4)]);
  Serial.print(hexchars[(x & 0x0f)]);
}

#ifdef PROCESS_MESSAGE
char* checkForMessage(void) {
 
  if (gotMessage == true) {
#ifdef SERIAL_DEBUG
    DEBUG_SERIAL.println("Got msg!");
#endif
    gotMessage = false;
    return((char *)&messageBuff);
  }
#ifdef SERIAL_DEBUG
  DEBUG_SERIAL.println("No msg!");
#endif
  return(NULL);
}
#endif // PROCESS_MESSAGE

// Accumulate and send data. This function captures the sender
// data and sends that data to the user.
void accumulateandsendData(void) {

  int i, j;

  int chainRetryCount;
  int recCount;
  uint8_t* wkPtr;

#ifdef SERIAL_DEBUG
  struct tm* debugtimeInfo;
  char* debugGMTPtr;
  char debugbuff[32];
#endif // SERIAL_DEBUG

  totalDataLength = BASE_RECORD_LENGTH;
  idData.idSwitches = idData.idTempByteCount = idData.idLightByteCount = idData.idcdError = 0;

#ifdef PROCESS_REMOTE_TEMP_SWITCH
  idData.idSwitches |= PROCESS_REMOTE_TEMP_SWITCH;
#endif // PROCESS_REMOTE_TEMP_SWITCH

#ifdef PROCESS_CHAIN_DATA
  idData.idSwitches |= PROCESS_CHAIN_DATA_SWITCH;
  idData.idTempByteCount = idData.idLightByteCount = 0;
#endif // PROCESS_CHAIN_DATA

  idData.idLastBootTime = lbTime;

  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, HIGH);
  delay(1000);

  if ((fixFound = gpsGetFix(&idData)) == false) {
    idData.idGPSTime = 0;
    idData.idLatitude = 0;
    idData.idLongitude = 0;
  }

#ifdef SERIAL_DEBUG_ROCKBLOCK
  debugtimeInfo = gmtime(&idData.idGPSTime);
  debugGMTPtr = asctime(debugtimeInfo);
  strcpy(debugbuff, debugGMTPtr);
  DEBUG_SERIAL.print("\nGMT debug=");
  DEBUG_SERIAL.print(debugbuff);
  DEBUG_SERIAL.print("\n");
#endif // SERIAL_DEBUG_ROCKBLOCK

  getMs5837Data(&idData);

#ifdef PROCESS_DS18B20
  getRemoteTemp(&idData);
#else
  idData.idRemoteTemp = -127;
#endif // PROCESS_REMOTE_TEMP  // Read the temperature string.

#ifdef PROCESS_OPEN_STRING
  #ifdef SERIAL_DEBUG
  DEBUG_SERIAL.println("calling openStringReadTemps.");
  #endif  // SERIAL_DEBUG

  openStringReadTemps();

  #ifdef SERIAL_DEBUG
  DEBUG_SERIAL.println("back from openStringReadTemps.");
  #endif  // SERIAL_DEBUG
#endif // PROCESS_OPEN_STRING

// Turn off the power to the MS5837, DS18B20, and GPS.
  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, LOW);

#ifdef PROCESS_CHAIN_DATA
  processChainData(&idData);
  totalDataLength += (idData.idTempByteCount + idData.idLightByteCount);
#endif  // PROCESS_CHAIN_DATA

  wkPtr = (uint8_t*)&idData;

#ifdef SERIAL_DEBUG
  DEBUG_SERIAL.print(F("Dumping data record\n"));
  DEBUG_SERIAL.print(F("Address = "));
  DEBUG_SERIAL.print((uint32_t)wkPtr, HEX);
  DEBUG_SERIAL.print(F(" size = "));
  DEBUG_SERIAL.print(totalDataLength);
  DEBUG_SERIAL.print(F("\n"));

  for (i = 0; i < totalDataLength; i++) {
    printHexChar((uint8_t)*wkPtr);
    ++wkPtr;
  }

  DEBUG_SERIAL.print(F("\n"));
#endif // SERIAL_DEBUG

// Before the data is sent out, check to see if there is a 
// message waiting to be sent... 
#ifdef PROCESS_MESSAGE
  gotMessage = processMessage((uint8_t *)&messageBuff);
#endif

#ifdef HUMAN_READABLE_DISPLAY
  rbTransmitIcedrifterData(&idData, 0);
#else
  rbTransmitIcedrifterData(&idData, totalDataLength);
#endif // HUMAN_READABLE_DISPLAY

}

// setup - This is an arduino defined routine that is called only once after the processor is booted.

void setup() {

  pinMode(MS5837_DS18B20_GPS_POWER_PIN, OUTPUT);
  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, LOW);

  pinMode(ROCKBLOCK_POWER_PIN, OUTPUT);
  digitalWrite(ROCKBLOCK_POWER_PIN, LOW);

#ifdef PROCESS_CHAIN_DATA
  pinMode(CHAIN_POWER_PIN, OUTPUT);
  digitalWrite(CHAIN_POWER_PIN, LOW);
#endif // PROCESS_CHAIN_DATA

#ifdef SERIAL_DEBUG
  // Start the serial ports
  DEBUG_SERIAL.begin(CONSOLE_BAUD);

  while (!DEBUG_SERIAL) {
    delay(10);     // will pause Zero, Leonardo, etc until serial console opens
  }

  DEBUG_SERIAL.print(F("Icedrifter\nHardware version "));
  DEBUG_SERIAL.print(HARDWARE_VERSION);
  DEBUG_SERIAL.print(F("\nSoftware version "));
  DEBUG_SERIAL.print(SOFTWARE_VERSION);
  DEBUG_SERIAL.print(F("\n"));
//  DEBUG_SERIAL.flush(); // Make sure the above message is displayed before continuing.
  delay(100);
#endif // SERIAL_DEBUG

  firstTime = true;

#ifdef PROCESS_OPEN_STRING
  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, HIGH);
  delay(1000);
  openStringInit();
  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, LOW);
#endif // PROCESS_OPEN_STRING

#ifdef SERIAL_DEBUG
  DEBUG_SERIAL.print(F("Setup done\n")); // Let the user know we are done with the setup function.
#endif // SERIAL_DEBUG

}

// loop - This is the main processing function for the arduino system.
//
// The first time through this function a full GPS fix is requested.  If no fix is
// received the processor is put to sleep for 60 minutes and then a full GPS fix
// will be requested again.  This continues until a full fix is received.
//
// Upon receiving a full GPS fix. the minutes are calculated to wake up the processor
// at the next half hour and the processor is put to sleep.
//
// Once a full GPS fix is received, only the current time is requested from the GPS.
// That's all that is needed to calculate the minutes to the next wake up time.

void loop() {

  int sleepSecs;  // Number of seconds to sleep before the processor is woken up.
  int sleepMins;  // Number of minutes to sleep before the processor is woken up.

  noFixFoundCount = 0;  // clear the no fix found count.

  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, HIGH);
  delay(1000);

//#define DEBUG_SERIAL_SCOPE

#ifdef DEBUG_SERIAL_SCOPE
  while (1) {
    gotMessage = processMessage((uint8_t *)&messageBuff);
  }
#endif //DEBUG_SERIAL_SCOPE

  // Try to get the GPS fix data.
  fixFound = gpsGetFix(&idData);

  // If a GPS fix was received, set the gotFullFix switch and clear the noFixFound count.
  // Otherwise add one to the noFixFoundCount.

  if (fixFound) {
    gotFullFix = true;
    noFixFoundCount = 0;
    if (firstTime) {
      lbTime = idData.idLastBootTime = idData.idGPSTime;
    }
  } else {
    ++noFixFoundCount;
  }

#ifdef TEST_ALL
  accumulateandsendData();
#elif defined(TRANSMIT_AT_BOOT)
  if (firstTime ||
      ((fixFound && timeToReport[gpsGetHour()] == true) ||
       noFixFoundCount >= 24)) {
    noFixFoundCount = 0;
    accumulateandsendData();
  }
#else // TRANSMIT_AT_BOOT
  if (!firstTime &&
      (fixFound && timeToReport[gpsGetHour()] == true) ||
      noFixFoundCount >= 24) {
    noFixFoundCount = 0;
    accumulateandsendData();
  }
#endif // TEST_ALL

  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, HIGH);
  delay(1000);

  // Accumulating and sending the data can take a while so update the time again.
  fixFound = gpsGetFix(&idData);
  firstTime = false;

  digitalWrite(MS5837_DS18B20_GPS_POWER_PIN, LOW);

  // If a GPS fix was found
  if (fixFound) {
    // Calculate the minutes until the next half hour,
    sleepMins = 90 - gpsGetMinutes();
    // If it less than 15 minutes until the nex half hour,
    if (sleepMins >= 75) {
      sleepMins -= 60;
    }

#ifdef SERIAL_DEBUG
    DEBUG_SERIAL.print(F("Fix found - sleep "));
    DEBUG_SERIAL.print(sleepMins);
    DEBUG_SERIAL.print(F(" minutes\n"));
//    DEBUG_SERIAL.flush();
    DEBUG_SERIAL.end();
#endif // SERIAL_DEBUG
    sleepSecs = sleepMins * 60;
  } else {
#ifdef SERIAL_DEBUG
    DEBUG_SERIAL.print(F("Fix not found - sleep 60 minutes\n"));
//    DEBUG_SERIAL.flush();
    DEBUG_SERIAL.end();
#endif // SERIAL_DEBUG
    sleepSecs = 3600;
  }

  do {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    sleepSecs -= 8;
  } while (sleepSecs > 0);

#ifdef SERIAL_DEBUG
  DEBUG_SERIAL.begin(CONSOLE_BAUD);
  delay(1000);
  DEBUG_SERIAL.print(F("woke up!\n"));
//  DEBUG_SERIAL.flush();
#endif // SERIAL_DEBUG
}
