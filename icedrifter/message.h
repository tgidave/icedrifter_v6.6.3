/*                                                                              
 *  message.h                                                  
 *                                                                               
 *  Header file for implementing the message system in the icedrifter.                     
 */ 
 
#ifndef _MESSAGE_H
#define _MESSAGE_H

#define MSG_RX 3
#define MSG_TX 2
#define MSG_BAUD  9600

// The DROP_MSG_RX_TX define tells the code to turn the TX and RX 
// line off to keep any voltage leakage to a minimum.
#define DROP_MSG_RX_TX

#define MAX_XFER_TRIES  5

#define MAX_TRIES       500

bool processMessage(uint8_t *messageBuffPtr);

#endif // _MESSAGE_H
