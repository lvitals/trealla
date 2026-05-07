#ifndef BIF_LUA_H
#define BIF_LUA_H

#include "internal.h"

#ifdef USE_LUA
#include <lua.h>

// Helpers for engine integration
void init_lua_vm(prolog *pl);
lua_State *get_lua_vm(query *q);
void lua_vm_lock();
void lua_vm_unlock();
cell *lua_to_prolog(lua_State *L, int index, query *q);
void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx);
bool call_lua_function(query *q, cell *c, pl_ctx c_ctx);

// Lua predicates registered in Trealla
bool bif_lua_eval_1(query *q);
bool bif_lua_call_3(query *q);
bool bif_lua_set_2(query *q);
bool bif_lua_get_2(query *q);
bool bif_lua_yield_2(query *q);

// Transparent Hybrid Built-ins
bool bif_lua_state_visit_1(query *q);
bool bif_lua_state_clear_0(query *q);
bool bif_lua_fib_2(query *q);
bool bif_lua_gcd_3(query *q);
bool bif_lua_is_prime_1(query *q);
bool bif_lua_union_3(query *q);
bool bif_lua_intersection_3(query *q);
bool bif_lua_powerset_2(query *q);

extern builtins g_lua_bifs[];

#else
// Fallback when Lua is not enabled
static inline void init_lua_vm(prolog *pl) { (void)pl; }
#endif

#endif
