#pragma once
#include "prolog.h"

#ifdef USE_LUA
#include <lua.h>

// Helpers para integração no motor
void init_lua_vm(prolog *pl);
cell *lua_to_prolog(lua_State *L, int index, query *q);
void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx);
bool call_lua_function(query *q, cell *c, pl_ctx c_ctx);
#else
// Fallback para quando Lua não está habilitado
inline static bool call_lua_function(query *q, cell *c, pl_ctx c_ctx) { return false; }
#endif

// Predicados Lua registrados no Trealla
bool bif_lua_eval_1(query *q);
bool bif_lua_call_3(query *q);
bool bif_lua_set_2(query *q);
bool bif_lua_get_2(query *q);
bool bif_lua_yield_2(query *q);

// Transparent Hybrid Built-ins
bool bif_lua_state_visit_1(query *q);
bool bif_lua_state_clear_0(query *q);
bool bif_lua_fib_2(query *q);
bool bif_lua_powerset_2(query *q);
bool bif_lua_union_3(query *q);
bool bif_lua_intersection_3(query *q);
bool bif_lua_gcd_3(query *q);
bool bif_lua_is_prime_1(query *q);

// Tabela de built-ins para o prolog.c
extern builtins g_lua_bifs[];
