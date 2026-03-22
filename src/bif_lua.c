#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bif_lua.h"
#include "module.h"
#include "prolog.h"
#include "query.h"
#include "internal.h"

// Lua headers with version compatibility
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM < 502
#define lua_rawlen lua_objlen
#endif

// Global Lua VM state
static lua_State *g_lua_vm = NULL;

// Initialize Lua VM if not already done
static void init_lua_vm() {
    if (g_lua_vm) return;
    g_lua_vm = luaL_newstate();
    luaL_openlibs(g_lua_vm);
}

// Convert a Trealla Prolog cell to a Lua value (Recursive)
// Ensures variables are correctly dereferenced using the provided context.
static void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx) {
    c = deref(q, c, c_ctx);
    c_ctx = q->latest_ctx; // Update context to the one found by deref

    if (is_integer(c)) {
        lua_pushinteger(L, (lua_Integer)c->val_int);
    } else if (is_float(c)) {
        lua_pushnumber(L, (lua_Number)c->val_float);
    } else if (is_string(c)) {
        size_t len = c->strb_len;
        const char *s = c->val_strb->cstr + c->strb_off;
        lua_pushlstring(L, s, len);
    } else if (is_iso_list(c)) {
        lua_newtable(L);
        int index = 1;
        cell *curr = c;
        pl_ctx curr_ctx = c_ctx;
        
        while (is_iso_list(curr)) {
            cell *head = deref(q, curr + 1, curr_ctx);
            pl_idx head_ctx = q->latest_ctx;
            
            prolog_to_lua(L, q, head, head_ctx);
            lua_rawseti(L, -2, index++);
            
            curr = deref(q, curr + 1 + 1, curr_ctx);
            curr_ctx = q->latest_ctx;
        }
        
        // Handle tail of improper list or end of list
        if (!is_nil(curr)) {
            prolog_to_lua(L, q, curr, curr_ctx);
            lua_setfield(L, -2, "_tail");
        }
    } else if (is_nil(c)) {
        lua_newtable(L); // Map empty list [] to empty table
    } else if (is_compound(c)) {
        lua_newtable(L);
        lua_pushstring(L, C_STR(q, c));
        lua_rawseti(L, -2, 0); // Store functor name at index 0
        
        int arity = c->arity;
        for (int i = 1; i <= arity; i++) {
            cell *arg = deref(q, c + i, c_ctx);
            prolog_to_lua(L, q, arg, q->latest_ctx);
            lua_rawseti(L, -2, i);
        }
    } else if (is_atom(c)) {
        lua_pushstring(L, C_STR(q, c));
    } else if (is_var(c)) {
        // If still a variable after deref, it's unbound.
        // We push a placeholder table to avoid nil errors in Lua.
        lua_newtable(L);
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "_is_var");
    } else {
        lua_pushnil(L);
    }
}

// lua_eval(+Script)
// Executes an arbitrary Lua script string.
bool bif_lua_eval_1(query *q) {
    GET_FIRST_ARG(c1, any);
    if (!is_atom(c1) && !is_string(c1)) return false;

    init_lua_vm();
    const char *script = is_atom(c1) ? C_STR(q, c1) : (c1->val_strb->cstr + c1->strb_off);
    
    if (luaL_dostring(g_lua_vm, script) != 0) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(g_lua_vm, -1));
        lua_pop(g_lua_vm, 1);
        return false;
    }
    return true;
}

// lua_call(+Function, +Args, -Result)
// Calls a global Lua function with a list of arguments and unifies the result.
bool bif_lua_call_3(query *q) {
    GET_FIRST_ARG(c1, atom); // Function name
    GET_NEXT_ARG(c2, any);   // List of arguments
    GET_NEXT_ARG(c3, any);   // Result variable or term

    init_lua_vm();
    lua_getglobal(g_lua_vm, C_STR(q, c1));
    if (!lua_isfunction(g_lua_vm, -1)) {
        lua_pop(g_lua_vm, 1);
        return false;
    }

    int n_args = 0;
    cell *curr = c2;
    pl_ctx curr_ctx = c2_ctx;
    
    // Convert argument list (c2) to multiple Lua arguments
    if (is_iso_list(curr)) {
        while (is_iso_list(curr)) {
            cell *head = deref(q, curr + 1, curr_ctx);
            pl_idx head_ctx = q->latest_ctx;
            prolog_to_lua(g_lua_vm, q, head, head_ctx);
            n_args++;
            curr = deref(q, curr + 1 + 1, curr_ctx);
            curr_ctx = q->latest_ctx;
        }
    } else if (!is_nil(curr)) {
        // If it's not a list and not nil, pass it as a single argument.
        prolog_to_lua(g_lua_vm, q, curr, curr_ctx);
        n_args++;
    }

    if (lua_pcall(g_lua_vm, n_args, 1, 0) != 0) {
        fprintf(stderr, "Lua Call Error: %s\n", lua_tostring(g_lua_vm, -1));
        lua_pop(g_lua_vm, 1);
        return false;
    }

    // Convert result from Lua back to Trealla (basic types)
    bool success = true;
    if (lua_isinteger(g_lua_vm, -1)) {
        cell tmp;
        make_int(&tmp, (int64_t)lua_tointeger(g_lua_vm, -1));
        success = unify(q, c3, c3_ctx, &tmp, q->latest_ctx);
    } else if (lua_isstring(g_lua_vm, -1)) {
        size_t len;
        const char *s = lua_tolstring(g_lua_vm, -1, &len);
        cell tmp;
        make_atom(&tmp, new_atom(q->pl, s));
        success = unify(q, c3, c3_ctx, &tmp, q->latest_ctx);
    } else if (lua_isnumber(g_lua_vm, -1)) {
        cell tmp;
        make_float(&tmp, (double)lua_tonumber(g_lua_vm, -1));
        success = unify(q, c3, c3_ctx, &tmp, q->latest_ctx);
    } else if (lua_isnil(g_lua_vm, -1)) {
        cell tmp;
        make_atom(&tmp, g_nil_s);
        success = unify(q, c3, c3_ctx, &tmp, q->latest_ctx);
    } else if (lua_istable(g_lua_vm, -1)) {
        // For now, return a generic 'true' if it's a table
        cell tmp;
        make_atom(&tmp, g_true_s);
        success = unify(q, c3, c3_ctx, &tmp, q->latest_ctx);
    }

    lua_pop(g_lua_vm, 1);
    return success;
}

// Built-in table registration
builtins g_lua_bifs[] = {
    { .name = "lua_eval", .arity = 1, .fn = bif_lua_eval_1, .help = "+script", .iso = true },
    { .name = "lua_call", .arity = 3, .fn = bif_lua_call_3, .help = "+func, +args, -res", .iso = true },
    { 0 }
};
