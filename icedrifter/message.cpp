#include <Arduino.h>
#include <SoftwareSerial.h>

#include "icedrifter.h"
#include "message.h"

#ifdef PROCESS_MESSAGE

SoftwareSerial SerialMsg(MSG_RX, MSG_TX);

bool getMessage(uint8_t *);

bool processMessage(uint8_t *messageBuffPtr) {

  int xferTries = 0;
  bool rtn;

  SerialMsg.begin(MSG_BAUD);

  while (1) {

    if ((rtn = getMessage(messageBuffPtr)) == true) {
      break;
    }

    ++xferTries;

    if (xferTries >= MAX_XFER_TRIES) {
      break;
    }
  }

//  SerialMsg.flush();
  SerialMsg.end();
#ifdef DROP_MSG_RX_TX
  digitalWrite(MSG_RX, LOW);
  digitalWrite(MSG_TX, LOW);
#endif  // DROP_MSG_RX_TX
  return (rtn);
}

bool getMessage(uint8_t *messageBuffPtr) {

  int i;
  int tries;
  bool xferDone;
  bool xferGotData;
  bool xferOverrun;

  xferDone = false;
  xferGotData = false;
  xferOverrun = false;
  //  xferNoResponse = false;
  tries = 0;

  while (SerialMsg.available()) { // Clear the read buffer.
    SerialMsg.read(); 
  }

  SerialMsg.write('?');

#ifdef SERIAL_DEBUG_MESSAGE
  DEBUG_SERIAL.println("? sent...");
#endif // SERIAL_DEBUG_MESSAGE

  i = 0;

  while (1) {
    if (SerialMsg.available()) {
      messageBuffPtr[i] = SerialMsg.read();
      xferGotData = true;
      tries = 0;

      if (messageBuffPtr[i] == '\r') {
        messageBuffPtr[i] = '\0';
        xferDone = true;

#ifdef SERIAL_DEBUG_MESSAGE
        DEBUG_SERIAL.println("Got xferDone");
        delay(1000);
#endif // SERIAL_DEBUG_MESSAGE

        break;
      }

      i++;

      if (i >= MESSAGE_BUFF_SIZE) {
        xferOverrun = true;
        break;
      }
    } else {  // Data not available

      ++tries;

      if (tries > MAX_TRIES) {
        break;
      }

      delay(10);
    }
  }

  if (xferOverrun == true) {
    xferDone = false;
#ifdef SERIAL_DEBUG_MESSAGE
    DEBUG_SERIAL.println("Overrun on message receive!!!");
#endif // SERIAL_DEBUG_MESSAGE

  } else {

#ifdef SERIAL_DEBUG_MESSAGE

    if (xferGotData == true) {
      if (xferDone == false) {

        DEBUG_SERIAL.print("Timeout receiving message! i = ");
        DEBUG_SERIAL.print(i);
        DEBUG_SERIAL.print(" tries = ");
        DEBUG_SERIAL.println(tries);
      }
    }

#endif // SERIAL_DEBUG_MESSAGE

  }

#ifdef SERIAL_DEBUG_MESSAGE

  if (xferDone == true) {

    DEBUG_SERIAL.println("Message received!!!");
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("Done...");
    DEBUG_SERIAL.println((char *)messageBuffPtr);
    DEBUG_SERIAL.println();
  } else {
    DEBUG_SERIAL.println("No response!!!");
  }

#endif // SERIAL_DEBUG_MESSAGE
/*
    SerialMsg.end();
  #ifdef DROP_MSG_RX_TX
    digitalWrite(MSG_RX, LOW);
    digitalWrite(MSG_TX, LOW);
  #endif  // DROP_MSG_RX_TX
*/

  return (xferDone);
}

#endif  //PROCESS_MESSAGE
