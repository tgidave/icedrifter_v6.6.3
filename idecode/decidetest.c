
//*****************************************************************************
// openstringdecode.c
//
// This program reads icedrifter the Open String data.  The data is sent as an
// array of float values representing the termperatures collexted when the 
// temperatures are read from the open string device.
//
//*****************************************************************************

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

//#include "../icedrifter_v6.5/icedrifter.h"
//#include "../icedrifter_v6.5/rockblock.h"

uint32_t tempData [40] = {
  0x41AC0000,
  0x0000AC41,
  0x0080de41,
  0x0000de41,
  0x0000de41,
  0x0080dc41,
  0x0080dd41,
  0x0000dc41,
  0x0000dd41,
  0x0080dd41,
  0x0080dd41,
  0x0000de41,
  0x0000de41,
  0x0000de41,
  0x0080dd41,
  0x0080de41,
  0x0000e041,
  0x0080de41,
  0x0000de41,
  0x0080de41,
  0x0080df41,
  0x0000df41,
  0x0000df41,
  0x0000df41,
  0x0080de41,
  0x0000df41,
  0x0000df41,
  0x0080dd41,
  0x0000df41,
  0x0000df41,
  0x0000df41,
  0x0000df41,
  0x0080df41,
  0x0000df41,
  0x0080df41,
  0x0000df41,
  0x0000e041,
  0x0080df41,
  0x0080e041,
  0x0000e141,
};

union test {
  float ftest;
  uint32_t xtest;
};

#define BUFF_SIZE 2048  // size of the buffer used to decode character data.
#define FILE_NAME_SIZE  1024  // size of buffers used for file names.
#define GPS_TIME_SIZE 16  // size of the buffer used to decode gps time and date.

// number of seconds in the 30 years betweem 01/01/1970 and 01/01/2000.
// Used during the conversion of arduino's time_t and linux's time_t.
#define SECONDS_IN_30_YEARS (time_t)946684800                                     

//bool mailResultsSwitch; // switch to indicate an email should be sent.

//#define maxEmailAddresses 5  // The maximum number of email addresses that can be specified.

//char emailAddress[maxEmailAddresses][256]; // email addsress to send the email to.

//int getDataByChunk(char**, int);
//void saveData(char* fileName);
//int getDataByFile(char**);
//int getDataByChar(char**, int);
//void decodeData(char* fileName);
//char convertCharToHex(char);
//void convertBigEndianToLittleEndian(char* sPtr, int size);
//float convertTempToC(short temp);
//void printHelp(void);

//*****************************************************************************
//
// main entry point to program.  The options are evaluated and the proper
// routines called.
//
//*****************************************************************************

int main(int argc, char** argv) {

  int i;
  union test t1;

//  int argIx;
//  int argCompare;
//  struct stat fileStat;
//  float *ptr = &ftest;

  t1.ftest = 21.5;

  fprintf(stdout, "%lu\n", sizeof(float));

  fprintf(stdout, "%f\n", t1.ftest);

//  fprintf(stdout, "%x\n", t1.xtest);

//  for (i = 0; i < 4; i++) {
    fprintf(stdout, "%X", t1.xtest);
//    fprintf(stdout, "%02X", t1.xtest[1]);
//    fprintf(stdout, "%02X", t1.xtest[2]);
//    fprintf(stdout, "%02X", t1.xtest[3]);
//    i++;
//  }

  fprintf(stdout, "\n");

  for (i = 0; i < 40; i++) {


    t1.xtest = tempData[i];
    fprintf(stdout, "%d: %f C\n", i + 1, t1.ftest);
  }

  return(0);
}


