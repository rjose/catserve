#ifndef WEB_H
#define WEB_H

#include "lua.h"

/*=============================================================================
 * API
 */

int web_register_lua_funcs(lua_State *L);
void *web_routine(void *arg);

#endif
