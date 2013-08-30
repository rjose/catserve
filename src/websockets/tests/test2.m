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


int main()
{
        const char *response_str = NULL;

        /*
         * Complete handshake
         */
        START_SET("Complete handshake");
        response_str = ws_complete_handshake(valid_ws_request_string);
        pass(1 == check_response(response_str,
                   "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), "Check handshake response");

        if (response_str != NULL)
                free(response_str);
        END_SET("Complete handshake");
        
        return 0;
}
