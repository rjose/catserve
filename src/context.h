#ifndef QPLAN_CONTEXT_H
#define QPLAN_CONTEXT_H

#include <netinet/in.h>

#include <pthread.h>

#include "lua.h"


/*=============================================================================
 * Data structure
 */

/*
 * This provides access to the main lua state within the app. Any operation on
 * the lua state needs "main_mutex" to be locked first.
 */
typedef struct Context_ {
        lua_State *main_lua_state;
        uint16_t port;
        pthread_mutex_t *main_mutex;
} Context;


/*=============================================================================
 * API
 */

void lock_main(Context *ctx);
void unlock_main(Context *ctx);

#endif
