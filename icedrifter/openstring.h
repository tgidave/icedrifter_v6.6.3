
/*                                                                              
 *  open-string.h                                                  
 *                                                                               
 *  Header file for processing the open-string temperature string.                     
 */                                                                              

#ifndef _OPEN_STRING_H
#define _OPEN_STRING_H

// =============================================================================
// Pin map and tunable constants
// =============================================================================

// 1-Wire data line. Needs an external 4.7 kohm pull-up to 3V3.
//#define DATA_PIN  2

// Maximum sensors expected on one string. Costs 8 bytes of RAM each.
#define OS_MAX_SENSORS  40

// 12-bit Convert T worst case (DS28EA00 datasheet, Conversion Time row).
#define CONVERT_T_MS  750

// Time between completed read cycles.
#define INTERVAL_MS 10000

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

// Initialize the open-string system and send back the sensor count.
void openStringInit(void);

// Read all the temperature sensors on the string.
void openStringReadTemps(void);

//uint16_t openStringGetosSensorCount(uint16_t );

// Send temperature data to the calling routine using an index into the array.
void openStringGetTemp(const char *, int ix);

// return the number of sensor temperature readings in the temperature array.
int openStringGetSensorCount(void);

// Build the next page to be sent out by the rockblock.
float* openStringGetTempArrayAddr(void);

float openStringGetTempByIndex(int ix);
#endif  // _OPEN_STRING_H
