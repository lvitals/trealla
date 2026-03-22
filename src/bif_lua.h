#pragma once
#include "prolog.h"

// Predicados Lua registrados no Trealla
bool bif_lua_eval_1(query *q);
bool bif_lua_call_3(query *q);

// Tabela de built-ins para o prolog.c
extern builtins g_lua_bifs[];
