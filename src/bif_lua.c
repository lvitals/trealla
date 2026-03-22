#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prolog.h"
#include "query.h"
#include "internal.h"
#include "heap.h"
#include "parser.h"
#include "module.h"
#include "bif_lua.h"

// Lua headers
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lua_State *g_lua_vm = NULL;

static void init_lua_vm() {
    if (g_lua_vm) return;
    g_lua_vm = luaL_newstate();
    luaL_openlibs(g_lua_vm);
    
    // Global table for the backing store
    lua_newtable(g_lua_vm);
    lua_setglobal(g_lua_vm, "_PROLOG_STORE");
}

// --- Prolog -> Lua (Recursive) ---
static void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx) {
    c = deref(q, c, c_ctx);
    pl_idx actual_ctx = q->latest_ctx;

    if (is_integer(c)) {
        lua_pushinteger(L, (lua_Integer)c->val_int);
    } else if (is_float(c)) {
        lua_pushnumber(L, (lua_Number)c->val_float);
    } else if (is_string(c)) {
        lua_pushlstring(L, c->val_strb->cstr + c->strb_off, c->strb_len);
    } else if (is_iso_list(c)) {
        lua_newtable(L);
        int index = 1;
        cell *curr = c;
        pl_idx curr_node_ctx = actual_ctx;
        while (is_iso_list(curr)) {
            pl_idx saved_node_ctx = curr_node_ctx;
            prolog_to_lua(L, q, deref(q, curr + 1, saved_node_ctx), q->latest_ctx);
            lua_rawseti(L, -2, index++);
            curr = deref(q, curr + 1 + 1, saved_node_ctx);
            curr_node_ctx = q->latest_ctx;
        }
        if (!is_nil(curr)) {
            prolog_to_lua(L, q, curr, curr_node_ctx);
            lua_setfield(L, -2, "_tail");
        }
    } else if (is_nil(c)) {
        lua_newtable(L);
    } else if (is_compound(c)) {
        lua_newtable(L);
        lua_pushstring(L, C_STR(q, c));
        lua_rawseti(L, -2, 0); 
        int arity = (int)c->arity;
        for (int i = 1; i <= arity; i++) {
            prolog_to_lua(L, q, deref(q, c + i, actual_ctx), q->latest_ctx);
            lua_rawseti(L, -2, i);
        }
    } else if (is_atom(c)) {
        lua_pushstring(L, C_STR(q, c));
    } else if (is_var(c)) {
        lua_newtable(L);
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "_is_var");
    } else {
        lua_pushnil(L);
    }
}

// --- Lua -> Prolog (Safe) ---
static cell *lua_to_prolog(lua_State *L, int index, query *q) {
    int type = lua_type(L, index);
    cell *c = alloc_heap(q, 1);
    if (type == LUA_TNUMBER) {
        if (lua_isinteger(L, index)) make_int(c, lua_tointeger(L, index));
        else make_float(c, lua_tonumber(L, index));
    } else if (type == LUA_TSTRING) {
        make_atom(c, new_atom(q->pl, lua_tostring(L, index)));
    } else if (type == LUA_TBOOLEAN) {
        make_atom(c, lua_toboolean(L, index) ? g_true_s : g_fail_s);
    } else if (type == LUA_TTABLE) {
        make_atom(c, g_true_s);
    } else {
        make_atom(c, g_nil_s);
    }
    return c;
}

bool bif_lua_eval_1(query *q) {
    GET_FIRST_ARG(p1, any);
    init_lua_vm();
    const char *script = is_atom(p1) ? C_STR(q, p1) : (p1->val_strb->cstr + p1->strb_off);
    if (luaL_dostring(g_lua_vm, script) != 0) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(g_lua_vm, -1));
        lua_pop(g_lua_vm, 1);
        return false;
    }
    return true;
}

bool bif_lua_call_3(query *q) {
    GET_FIRST_ARG(p1, atom);
    GET_NEXT_ARG(p2, any);
    GET_NEXT_ARG(p3, any);
    init_lua_vm();
    lua_getglobal(g_lua_vm, C_STR(q, p1));
    if (!lua_isfunction(g_lua_vm, -1)) { lua_pop(g_lua_vm, 1); return false; }

    int n_args = 0;
    cell *curr = p2;
    pl_ctx curr_ctx = p2_ctx;
    while (is_iso_list(curr)) {
        pl_idx saved_node_ctx = curr_ctx;
        prolog_to_lua(g_lua_vm, q, deref(q, curr + 1, saved_node_ctx), q->latest_ctx);
        n_args++;
        curr = deref(q, curr + 1 + 1, saved_node_ctx);
        curr_ctx = q->latest_ctx;
    }
    if (lua_pcall(g_lua_vm, n_args, 1, 0) != 0) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(g_lua_vm, -1));
        lua_pop(g_lua_vm, 1); return false;
    }
    
    // Check if we expect a failure (Lua false -> Prolog fail)
    if (lua_isboolean(g_lua_vm, -1) && !lua_toboolean(g_lua_vm, -1)) {
        lua_pop(g_lua_vm, 1);
        return false;
    }

    cell *res = lua_to_prolog(g_lua_vm, -1, q);
    bool ok = unify(q, p3, p3_ctx, res, q->st.cur_ctx);
    lua_pop(g_lua_vm, 1);
    return ok;
}

bool bif_lua_set_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    init_lua_vm();
    lua_getglobal(g_lua_vm, "_PROLOG_STORE");
    prolog_to_lua(g_lua_vm, q, p1, p1_ctx);
    prolog_to_lua(g_lua_vm, q, p2, p2_ctx);
    lua_settable(g_lua_vm, -3);
    lua_pop(g_lua_vm, 1);
    return true;
}

bool bif_lua_get_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    init_lua_vm();
    lua_getglobal(g_lua_vm, "_PROLOG_STORE");
    prolog_to_lua(g_lua_vm, q, p1, p1_ctx);
    lua_gettable(g_lua_vm, -2);
    if (lua_isnil(g_lua_vm, -1)) { lua_pop(g_lua_vm, 2); return false; }
    cell *res = lua_to_prolog(g_lua_vm, -1, q);
    bool ok = unify(q, p2, p2_ctx, res, q->st.cur_ctx);
    lua_pop(g_lua_vm, 2);
    return ok;
}

bool bif_lua_yield_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    init_lua_vm();
    if (!q->retry) {
        if (is_atom(p1)) lua_getglobal(g_lua_vm, C_STR(q, p1));
        else prolog_to_lua(g_lua_vm, q, p1, p1_ctx);
        if (!lua_istable(g_lua_vm, -1)) { lua_pop(g_lua_vm, 1); return false; }
        lua_setglobal(g_lua_vm, "_CURRENT_TABLE");
        lua_pushnil(g_lua_vm);
        lua_setglobal(g_lua_vm, "_CURRENT_KEY");
    }
    lua_getglobal(g_lua_vm, "_CURRENT_TABLE");
    lua_getglobal(g_lua_vm, "_CURRENT_KEY");
    if (lua_next(g_lua_vm, -2)) {
        lua_pushvalue(g_lua_vm, -2);
        lua_setglobal(g_lua_vm, "_CURRENT_KEY");
        cell *res = lua_to_prolog(g_lua_vm, -1, q);
        bool ok = unify(q, p2, p2_ctx, res, q->st.cur_ctx);
        lua_pop(g_lua_vm, 3);
        if (ok) return retry_choice(q);
    }
    return false;
}

builtins g_lua_bifs[] = {
    { .name = "lua_eval", .arity = 1, .fn = bif_lua_eval_1, .help = "+script", .iso = true },
    { .name = "lua_call", .arity = 3, .fn = bif_lua_call_3, .help = "+func, +args, -res", .iso = true },
    { .name = "lua_set", .arity = 2, .fn = bif_lua_set_2, .help = "+key, +val", .iso = true },
    { .name = "lua_get", .arity = 2, .fn = bif_lua_get_2, .help = "+key, -val", .iso = true },
    { .name = "lua_yield", .arity = 2, .fn = bif_lua_yield_2, .help = "+table, -val", .iso = true },
    { 0 }
};
