#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "../ws.h"
#include "test_util.h"
#import "Testing.h"


/*
 * Test data
 */
static const char valid_ws_request_string[] = 
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Origin: http://example.com\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

static const char invalid_ws_request_string[] = 
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: garbage\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Origin: http://example.com\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

static const char non_ws_request_string[] = 
        "GET / HTTP/1.1\r\n"
        "\r\n";



int main()
{
        const char *response_str = NULL;

        /*
         * Is handshake
         */
        START_SET("Is websocket handshake");

        pass(1 == ws_is_handshake(valid_ws_request_string),
                                                    "Check start of handshake");
        pass(0 == ws_is_handshake(invalid_ws_request_string),
                                                    "Check start of handshake");
        pass(0 == ws_is_handshake(non_ws_request_string),
                                                    "Check start of handshake");
        END_SET("Is websocket handshake");

        return 0;
}
