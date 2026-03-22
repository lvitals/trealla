#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bif_lua.h"
#include "module.h"
#include "prolog.h"
#include "query.h"
#include "internal.h"

// Inclusão da Lua com compatibilidade de versão
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM < 502
#define lua_rawlen lua_objlen
#endif

// Estado global da VM Lua para este interpretador
static lua_State *g_L = NULL;

static void init_lua() {
    if (g_L) return;
    g_L = luaL_newstate();
    luaL_openlibs(g_L);
}

// Converte uma célula Trealla para um valor Lua
static void cell_to_lua(lua_State *L, query *q, cell *c, pl_idx c_ctx) {
    c = deref(q, c, c_ctx);
    if (is_integer(c)) {
        lua_pushinteger(L, c->val_int);
    } else if (is_float(c)) {
        lua_pushnumber(L, c->val_float);
    } else if (is_string(c)) {
        size_t len = c->strb_len;
        const char *s = c->val_strb->cstr + c->strb_off;
        lua_pushlstring(L, s, len);
    } else if (is_atom(c)) {
        lua_pushstring(L, C_STR(q, c));
    } else {
        lua_pushnil(L);
    }
}

// lua_eval(+Script)
bool bif_lua_eval_1(query *q) {
    GET_FIRST_ARG(c1, any);
    if (!is_atom(c1) && !is_string(c1)) return false;

    init_lua();
    const char *script = is_atom(c1) ? C_STR(q, c1) : (c1->val_strb->cstr + c1->strb_off);
    
    if (luaL_dostring(g_L, script) != 0) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return false;
    }
    return true;
}

// lua_call(+Function, +Args, -Result)
bool bif_lua_call_3(query *q) {
    GET_FIRST_ARG(c1, atom); // Nome da função
    GET_NEXT_ARG(c2, any);  // Lista de argumentos (ou nil)
    GET_NEXT_ARG(c3, any);  // Resultado

    init_lua();
    lua_getglobal(g_L, C_STR(q, c1));
    if (!lua_isfunction(g_L, -1)) {
        lua_pop(g_L, 1);
        return false;
    }

    int n_args = 0;
    cell *l = c2;
    pl_idx l_ctx = q->latest_ctx;
    while (is_list(l)) {
        cell *h = deref(q, l+1, l_ctx);
        cell_to_lua(g_L, q, h, q->latest_ctx);
        n_args++;
        l = deref(q, l+1+1, l_ctx);
    }

    if (lua_pcall(g_L, n_args, 1, 0) != 0) {
        fprintf(stderr, "Lua Call Error: %s\n", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return false;
    }

    if (lua_isinteger(g_L, -1)) {
        cell tmp;
        make_int(&tmp, lua_tointeger(g_L, -1));
        bool ok = unify(q, c3, q->latest_ctx, &tmp, q->latest_ctx);
        lua_pop(g_L, 1);
        return ok;
    } else if (lua_isstring(g_L, -1)) {
        size_t len;
        const char *s = lua_tolstring(g_L, -1, &len);
        cell tmp;
        make_cstring(&tmp, s); 
        bool ok = unify(q, c3, q->latest_ctx, &tmp, q->latest_ctx);
        lua_pop(g_L, 1);
        return ok;
    }

    lua_pop(g_L, 1);
    return true;
}

builtins g_lua_bifs[] = {
    { .name = "lua_eval", .arity = 1, .fn = bif_lua_eval_1, .help = "+script", .iso = true },
    { .name = "lua_call", .arity = 3, .fn = bif_lua_call_3, .help = "+func, +args, -res", .iso = true },
    { 0 }
};
