#ifndef CONSTANTS_H
#define CONSTANTS_H

/* Byte 0 of websocket frame */
#define WS_FRAME_FIN 0x80
#define WS_FRAME_OP_CONT 0x00
#define WS_FRAME_OP_TEXT 0x01
#define WS_FRAME_OP_BIN 0x02
#define WS_FRAME_OP_CLOSE 0x08
#define WS_FRAME_OP_PING 0x09
#define WS_FRAME_OP_PONG 0x0A

/* Byte 1 of websocket frame */
#define WS_FRAME_MASK 0x80

/* Message lengths */
#define SHORT_MESSAGE_LEN 125
#define MED_MESSAGE_LEN 0xFFFF 
#define MED_MESSAGE_KEY 126
#define LONG_MESSAGE_KEY 127
#define NUM_MED_LEN_BYTES 2
#define NUM_LONG_LEN_BYTES 8
#define MASK_LEN 4

#endif
