#include <Arduino.h>
#include <IridiumSBD.h>
#include <SoftwareSerial.h>

#include "icedrifter.h"
#include "rockblock.h"
#include "openstring.h"

SoftwareSerial isbdss(ROCKBLOCK_RX_PIN, ROCKBLOCK_TX_PIN);

IridiumSBD isbd(isbdss, ROCKBLOCK_SLEEP_PIN);

iceDrifterChunk idcChunk;

char tempBuff[32];

char rbhexchars[] = "0123456789ABCDEF";

void rbprintHexChar(uint8_t x) {
  Serial.print(rbhexchars[(x >> 4)]);
  Serial.print(rbhexchars[(x & 0x0f)]);
}

#ifdef SERIAL_DEBUG_ROCKBLOCK
void ISBDConsoleCallback(IridiumSBD *device, char c) {
  DEBUG_SERIAL.write(c);
}

void ISBDDiagsCallback(IridiumSBD *device, char c) {
  DEBUG_SERIAL.write(c);
}
#endif

void rbTransmitIcedrifterData(icedrifterData *idPtr, int idLen) {

  int rc;
  int recCount;
  int dataLen;
  int chunkLen;
  int sensorCount;
  int len;
  int i;
  uint8_t *tempPtr;
  uint8_t *dataPtr;
  uint8_t *chunkPtr;
  uint8_t *wkPtr;
  uint8_t *msgBuffPtr;
  float *fPtr;
  float osTempFloat;
  struct tm *timeInfo;
  char *buffPtr;
  char buff[128];
  char oBuff[MESSAGE_BUFF_SIZE];
  char osTemp[10];
  
if (idLen == 0) {
    oBuff[0] = 0;

//#define SERIAL_DEBUG_ROCKBLOCK_GMT

#ifdef SERIAL_DEBUG_ROCKBLOCK_GMT
    DEBUG_SERIAL.print("\nTime = ");
    DEBUG_SERIAL.print((int)(&idData.idGPSTime));
    DEBUG_SERIAL.print("\n");
#endif // SERIAL_DEBUG_ROCKBLOCK_GMT

    strcat(oBuff, "\nGMT=");
    timeInfo = gmtime(&idPtr->idGPSTime);
    buffPtr = asctime(timeInfo);
    strcat(oBuff, buffPtr);


#ifdef SERIAL_DEBUG_ROCKBLOCK_GMT
    DEBUG_SERIAL.print("\nGMT debug=");
    DEBUG_SERIAL.print((int)buffPtr);
    DEBUG_SERIAL.print("\n");
#endif // SERIAL_DEBUG_ROCKBLOCK

    strcat(oBuff, "\nLBT=");
    timeInfo = gmtime(&idPtr->idLastBootTime);
    buffPtr = asctime(timeInfo);
    strcat(oBuff, buffPtr);
    strcat(oBuff, "\nLat=");
    buffPtr = dtostrf(idPtr->idLatitude, 4, 6, buff);
    strcat(oBuff, buffPtr);
    strcat(oBuff, "\nLon=");
    buffPtr = dtostrf(idPtr->idLongitude, 4, 6, buff);
    strcat(oBuff, buffPtr);
//    strcat(oBuff, "\nTmp=");
//    buffPtr = dtostrf(idPtr->idTemperature, 4, 2, buff);
//    strcat(oBuff, buffPtr);
    strcat(oBuff, "\nBP=");
    buffPtr = dtostrf(idPtr->idPressure, 6, 2, buff);
    strcat(oBuff, buffPtr);
    strcat(oBuff, " hPa\n");

#ifdef PROCESS_DS18B20
    strcat(oBuff, "Ts=");
    buffPtr = dtostrf(idPtr->idRemoteTemp, 4, 2, buff);
    strcat(oBuff, buffPtr);
    strcat(oBuff, " C = ");
    buffPtr = dtostrf(((idPtr->idRemoteTemp * 1.8) + 32), 4, 2, buff);
    strcat(oBuff, buffPtr);
    strcat(oBuff, " F\n");
#endif // PROCESS_DS18B20

    strcat(oBuff, "\nIcedrifter H/V ");
    strcat(oBuff, HARDWARE_VERSION);
    strcat(oBuff, " S/V ");
    strcat(oBuff, SOFTWARE_VERSION);

#ifdef SERIAL_DEBUG_ROCKBLOCK
    strcat(oBuff, "\n*** Debug is ON ***\n");
    DEBUG_SERIAL.print((char *)&oBuff);
    delay(1000);  
#endif // SERIAL_DEBUG_ROCKBLOCK
  }
  
//#ifdef PROCESS_OPEN_STRING 
//  sensorCount = openStringGetSensorCount();
//  
//  // Read each chip in cable order and print its temperature.
//  for (i = 0; i < sensorCount; i++) {
//    openStringGetTemp(osTemp, i);
//
//    sprintf(tempBuff, "%d: %s C\n", i + 1, osTemp);
//
//    // When adding the  temperatures to the message we need to leave two bytes at the end for the \r and null...  
//    if(strlen(oBuff) + strlen(tempBuff) + 2 < MESSAGE_BUFF_SIZE) {
//      strcat(oBuff, tempBuff);
//
//  #ifdef SERIAL_DEBUG_OPEN_STRING
//     DEBUG_SERIAL.print(tempBuff);
//      DEBUG_SERIAL.print("\n");
//  #endif // SERIAL_DEBUG_OPEN_STRING
//
//    } else {
//      openStringFinishSendingTemps(i);
//    }
//  }
//
//  #ifdef SERIAL_DEBUG_OPEN_STRING
//    DEBUG_SERIAL.print(oBuff);
//    DEBUG_SERIAL.print(""oBuff);
//  #endif // SERIAL_DEBUG_OPEN_STRING
//
//#endif // PROCESS_OPEN_STRING

#ifdef NEVER_TRANSMIT
  #ifdef SERIAL_DEBUG_ROCKBLOCK
    DEBUG_SERIAL.print(F("Transmission disabled by NEVER_TRANSMIT switch.\n"));
  #endif
#else // NEVER_TRANSMIT

  // Setup the RockBLOCK
  isbd.setPowerProfile(IridiumSBD::USB_POWER_PROFILE);

#ifdef SERIAL_DEBUG_ROCKBLOCK
  DEBUG_SERIAL.flush();
  DEBUG_SERIAL.println(F("Powering up RockBLOCK\n"));
  delay(1000);
#endif // SERIAL_DEBUG_ROCKBLOCK

  digitalWrite(ROCKBLOCK_POWER_PIN, HIGH);
  delay(1000);

  isbdss.begin(ROCKBLOCK_BAUD);

  // Step 3: Start talking to the RockBLOCK and power it up
#ifdef SERIAL_DEBUG_ROCKBLOCK
//  DEBUG_SERIAL.flush();
  DEBUG_SERIAL.println(F("RockBLOCK begin\n"));
//  DEBUG_SERIAL.flush();
  delay(1000);
#endif // SERIAL_DEBUG_ROCKBLOCK
  isbdss.listen();

  if ((rc = isbd.begin()) == ISBD_SUCCESS) {
#ifdef SERIAL_DEBUG_ROCKBLOCK
//    DEBUG_SERIAL.flush();
    DEBUG_SERIAL.print(F("Transmitting address="));
    DEBUG_SERIAL.print((long)idPtr, HEX);
    DEBUG_SERIAL.print(F(" length="));
    DEBUG_SERIAL.print(idLen);
    DEBUG_SERIAL.print(F("\n"));
//    DEBUG_SERIAL.flush();
    delay(1000);
#endif // SERIAL_DEBUG_ROCKBLOCK

    recCount = 0;
    dataPtr = (uint8_t *)idPtr;
    chunkPtr = (uint8_t *)&idcChunk.idcBuffer;
    dataLen = idLen;

    if (dataLen == 0) {
      dataLen = strlen(oBuff) + 1;
#ifdef SERIAL_DEBUG_ROCKBLOCK
//        DEBUG_SERIAL.flush();
//        DEBUG_SERIAL.print(F("Chunk address="));
//        DEBUG_SERIAL.print((long)chunkPtr, HEX);
//        DEBUG_SERIAL.print(F(" Chunk length="));
//        DEBUG_SERIAL.print(chunkLen);
//        DEBUG_SERIAL.print(F("\n"));
//        wkPtr = (uint8_t *)&idcChunk;
//        DEBUG_SERIAL.print(*wkPtr);
//        DEBUG_SERIAL.print(F("\n"));
//
//      DEBUG_SERIAL.flush();

      DEBUG_SERIAL.print((long)oBuff, HEX);
      DEBUG_SERIAL.print(" data length=");
      DEBUG_SERIAL.print(dataLen);
      DEBUG_SERIAL.print("\n");
      DEBUG_SERIAL.println(oBuff);
//      delay(1000);
#endif // SERIAL_DEBUG_ROCKBLOCK

      rc = isbd.sendSBDBinary((const char *)oBuff, dataLen);

#ifdef PROCESS_OPEN_STRING

      if (rc == 0) {
        oBuff[0] = 0;
        sensorCount = openStringGetSensorCount();

        for (i = 0; i < sensorCount; i++) {
          osTempFloat = openStringGetTempByIndex(i);

          if (osTempFloat < 0) {
            if (osTempFloat > -10) {
              dtostrf(osTempFloat, 5, 2, osTemp);
            } else {
              dtostrf(osTempFloat, 6, 2, osTemp);
            }
          } else {
            if (osTempFloat < 10) {
              dtostrf(osTempFloat, 4, 2, osTemp);
            } else {
              dtostrf(osTempFloat, 5, 2, osTemp);
            }
          }

          if ((strlen(oBuff) + strlen(osTemp)) <  (MESSAGE_BUFF_SIZE - 8)) {
            strcat(oBuff, osTemp);
            strcat(oBuff, ",");

//#ifdef SERIAL_DEBUG_ROCKBLOCK
//        DEBUG_SERIAL.println(osTemp, HEX);
//        DEBUG_SERIAL.println(oBuff);
//#endif // SERIAL_DEBUG_ROCKBLOCK

          } else {
            break;
          }

          fPtr++;
        }

        // Change the last comma in the string to a null.
        oBuff[strlen(oBuff) - 1] = 0;

#ifdef SERIAL_DEBUG_ROCKBLOCK
//        tempPtr = (uint8_t *)openStringGetTempArrayAddr();
        dataLen = strlen(oBuff) + 1;
        DEBUG_SERIAL.print("dataLen = ");
        DEBUG_SERIAL.print(dataLen);
        DEBUG_SERIAL.print("\n");
        DEBUG_SERIAL.print("openString data being sent.\n");
        DEBUG_SERIAL.println(oBuff);

//        tempPtr = (uint8_t *)&oBuff;

//        for (i = 0; i < dataLen; i++) {
//          rbprintHexChar(tempPtr);
//          tempPtr++;
//        }

//        DEBUG_SERIAL.print("\n");
#endif // SERIAL_DEBUG_ROCKBLOCK
      
        rc = isbd.sendSBDBinary(oBuff, dataLen);
      }
#endif  // PROCESS_OPEN_STRING

    } else {

      while (dataLen > 0) {
        idcChunk.idcSendTime = idPtr->idGPSTime;
        idcChunk.idcRecordType[0] = 'I';
        idcChunk.idcRecordType[1] = 'D';
        idcChunk.idcRecordNumber = recCount;

        if (dataLen > MAX_CHUNK_DATA_LENGTH) {
          chunkLen = MAX_CHUNK_LENGTH;
          dataLen -= MAX_CHUNK_DATA_LENGTH;
        } else {
          chunkLen = (dataLen + CHUNK_HEADER_SIZE);
          dataLen = 0;
        }

        memmove(chunkPtr, dataPtr, chunkLen);
        dataPtr += MAX_CHUNK_DATA_LENGTH;
        ++recCount;

#ifdef SERIAL_DEBUG_ROCKBLOCK
//        DEBUG_SERIAL.flush();
        DEBUG_SERIAL.print(F("Chunk address="));
        DEBUG_SERIAL.print((long)chunkPtr, HEX);
        DEBUG_SERIAL.print(F(" Chunk length="));
        DEBUG_SERIAL.print(chunkLen);
        DEBUG_SERIAL.print(F("\n"));
        wkPtr = (uint8_t *)&idcChunk;

  #ifdef HUMAN_READABLE_DISPLAY
        DEBUG_SERIAL.print(*wkPtr);
  #else
        for (i = 0; i < chunkLen; i++) {
          rbprintHexChar((uint8_t)*wkPtr);
          ++wkPtr;
        }
  #endif // HUMAN_READABLE_DISPLAY

        DEBUG_SERIAL.print(F("\n"));
//        DEBUG_SERIAL.flush();
#endif // SERIAL_DEBUG_ROCKBLOCK

        rc = isbd.sendSBDBinary((uint8_t *)&idcChunk, chunkLen);
      }
    }

#ifdef PROCESS_MESSAGE
    if ((msgBuffPtr = (uint8_t *)checkForMessage()) != NULL) {
//      dataLen = strlen(msgBuffPtr) + 1;
      rc = isbd.sendSBDBinary((const uint8_t*)msgBuffPtr, (strlen(msgBuffPtr) + 1));
    }
#endif // PROCESS_MESSAGE

#ifdef SERIAL_DEBUG_ROCKBLOCK
    DEBUG_SERIAL.flush();
    if (rc == 0) {
      DEBUG_SERIAL.print(F("Good return code from send!\n"));
//      DEBUG_SERIAL.flush();
    } else {
      DEBUG_SERIAL.print(F("Bad return code from send = "));
      DEBUG_SERIAL.print(rc);
      DEBUG_SERIAL.print(F("\n"));
//      DEBUG_SERIAL.flush();
    }
#endif // SERIAL_DEBUG_ROCKBLOCK

#ifdef SERIAL_DEBUG_ROCKBLOCK
  } else {
    DEBUG_SERIAL.print("Bad return code from begin = ");
    DEBUG_SERIAL.print(rc);
    DEBUG_SERIAL.print("\n");
//    DEBUG_SERIAL.flush();
#endif // SERIAL_DEBUG_ROCKBLOCK
  }

  isbd.sleep();
  isbdss.end();
  digitalWrite(ROCKBLOCK_POWER_PIN, LOW);

#endif // NEVER_TRANSMIT
}


