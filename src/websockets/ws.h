#ifndef WS_H
#define WS_H

#include <stdint.h>

#include <sys/types.h>


/* ============================================================================ 
 * Data structures/types
 */

typedef ssize_t (*ws_read_bytes_fp)(int fd, char *ptr, size_t maxlen);

enum WebsocketFrameType {
        WS_FT_ERROR = -1,
        WS_FT_TEXT,
        WS_FT_CLOSE,
        WS_FT_PING,
        WS_FT_PONG
};


/* ============================================================================ 
 * Public API
 */

/* 
 * Websocket handshake
 * -------------------
 */
int ws_is_handshake(const char* req_str);
const char *ws_complete_handshake(const char *req_str);

/* 
 * Writing websocket frames
 * ------------------------
 */
size_t ws_make_text_frame(const char *message, const uint8_t mask[4],
                                                         uint8_t **frame_p);
size_t ws_make_close_frame(uint8_t **frame_p);
size_t ws_make_ping_frame(uint8_t **frame_p);
size_t ws_make_pong_frame(uint8_t **frame_p);


/* 
 * Reading websocket frames
 * ------------------------
 */
enum WebsocketFrameType ws_read_next_message(int fd,
                                    ws_read_bytes_fp read_bytes, char **message);



#endif
