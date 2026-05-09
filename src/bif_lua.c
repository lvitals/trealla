#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "prolog.h"
#include "query.h"
#include "internal.h"
#include "heap.h"
#include "parser.h"
#include "module.h"
#include "bif_lua.h"

#ifdef USE_LUA
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lock g_lua_lock;

// --- Lua-C Wrappers for Arithmetic ---
static int l_native_fib(lua_State *L) {
    long long n = luaL_checkinteger(L, 1);
    long long a = 1, b = 1;
    for (long long i = 0; i < n; i++) {
        long long tmp = a; a = b; b = tmp + b;
    }
    lua_pushinteger(L, a);
    return 1;
}

static int l_native_gcd(lua_State *L) {
    long long a = llabs(luaL_checkinteger(L, 1));
    long long b = llabs(luaL_checkinteger(L, 2));
    while (b) { a %= b; long long t = a; a = b; b = t; }
    lua_pushinteger(L, a);
    return 1;
}

static int l_native_is_prime(lua_State *L) {
    long long n = luaL_checkinteger(L, 1);
    if (n < 2) { lua_pushboolean(L, 0); return 1; }
    if (n == 2 || n == 3) { lua_pushboolean(L, 1); return 1; }
    if (n % 2 == 0 || n % 3 == 0) { lua_pushboolean(L, 0); return 1; }
    for (long long i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, 1);
    return 1;
}

void init_lua_vm(prolog *pl) {
    (void)pl;
    init_lock(&g_lua_lock);
}

lua_State *get_lua_vm(query *q) {
    unsigned id = q->worker_id;
    if (id >= MAX_THREADS) id = 0;
    acquire_lock(&g_lua_lock);
    if (!q->pl->lua_vms[id]) {
        q->pl->lua_vms[id] = luaL_newstate();
        lua_State *L = q->pl->lua_vms[id];
        luaL_openlibs(L);
        lua_newtable(L); lua_setglobal(L, "_PROLOG_STORE");
        lua_newtable(L); lua_setglobal(L, "_PROLOG_STATE");
        lua_pushcfunction(L, l_native_fib); lua_setglobal(L, "fib");
        lua_pushcfunction(L, l_native_gcd); lua_setglobal(L, "gcd");
        lua_pushcfunction(L, l_native_is_prime); lua_setglobal(L, "is_prime");
    }
    lua_State *L = q->pl->lua_vms[id];
    release_lock(&g_lua_lock);
    return L;
}

void lua_vm_lock() { acquire_lock(&g_lua_lock); }
void lua_vm_unlock() { release_lock(&g_lua_lock); }

// --- Prolog -> Lua ---
void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx) {
    if (!lua_checkstack(L, 100)) return;
    c = deref(q, c, c_ctx);
    pl_idx actual_ctx = q->latest_ctx;

    if (is_var(c)) {
        lua_newtable(L);
        lua_pushboolean(L, 1);
        lua_setfield(L, -2, "_is_var");
    } else if (is_integer(c)) {
        lua_pushinteger(L, (lua_Integer)get_smallint(c));
    } else if (is_float(c)) {
        lua_pushnumber(L, (lua_Number)get_float(c));
    } else if (is_string(c)) {
        lua_pushlstring(L, C_STR(q, c), C_STRLEN(q, c));
    } else if (is_iso_list(c)) {
        lua_newtable(L);
        int index = 1;
        cell *curr = c;
        pl_ctx curr_ctx = actual_ctx;
        while (is_iso_list(curr)) {
            cell *h = curr + 1;
            prolog_to_lua(L, q, h, curr_ctx);
            lua_rawseti(L, -2, index++);
            curr = deref(q, h + h->num_cells, curr_ctx);
            curr_ctx = q->latest_ctx;
        }
        if (!is_nil(curr)) {
            prolog_to_lua(L, q, curr, curr_ctx);
            lua_setfield(L, -2, "_tail");
        }
    } else if (is_nil(c)) {
        lua_newtable(L);
    } else if (is_compound(c)) {
        lua_newtable(L);
        lua_pushstring(L, C_STR(q, c));
        lua_rawseti(L, -2, 0); 
        int arity = (int)c->arity;
        cell *arg = c + 1;
        for (int i = 1; i <= arity; i++) {
            prolog_to_lua(L, q, arg, actual_ctx);
            lua_rawseti(L, -2, i);
            arg += arg->num_cells;
        }
    } else if (is_atom(c)) {
        lua_pushstring(L, C_STR(q, c));
    } else {
        lua_pushnil(L);
    }
}

// --- Lua -> Prolog (Recursive using tmp_heap scratchpad) ---
static unsigned lua_to_tmp_heap(lua_State *L, int index, query *q, bool force_list) {
    if (!lua_checkstack(L, 100)) return 0;
    index = lua_absindex(L, index);
    int type = lua_type(L, index);
    pl_idx start = q->tmphp;
    
    if (type == LUA_TNUMBER) {
        cell *c = alloc_tmp(q, 1);
        if (lua_isinteger(L, index)) make_int(c, lua_tointeger(L, index));
        else make_float(c, lua_tonumber(L, index));
        return 1;
    } else if (type == LUA_TSTRING) {
        cell *c = alloc_tmp(q, 1);
        make_atom(c, new_atom(q->pl, lua_tostring(L, index)));
        return 1;
    } else if (type == LUA_TBOOLEAN) {
        cell *c = alloc_tmp(q, 1);
        make_atom(c, lua_toboolean(L, index) ? g_true_s : g_fail_s);
        return 1;
    } else if (type == LUA_TTABLE) {
        // Check for compound tag
        lua_rawgeti(L, index, 0);
        if (lua_isstring(L, -1)) {
            const char *functor = lua_tostring(L, -1);
            lua_pop(L, 1);
            int arity = (int)lua_rawlen(L, index);
            cell *c = alloc_tmp(q, 1);
            make_atom(c, new_atom(q->pl, functor));
            c->arity = arity;
            for (int i = 1; i <= arity; i++) {
                lua_rawgeti(L, index, i);
                lua_to_tmp_heap(L, -1, q, true);
                lua_pop(L, 1);
            }
            pl_idx end = q->tmphp;
            get_tmp_heap(q, start)->num_cells = end - start;
            return end - start;
        }
        lua_pop(L, 1);

        int n = (int)lua_rawlen(L, index);
        if (n == 0 && !force_list) {
            cell *c = alloc_tmp(q, 1);
            make_atom(c, g_nil_s);
            return 1;
        }
        
        // If not forced list and no elements, might be an empty table -> true
        if (n == 0 && !force_list) {
             cell *c = alloc_tmp(q, 1);
             make_atom(c, g_true_s);
             return 1;
        }

        // Build list
        for (int i = 1; i <= n; i++) {
            alloc_tmp(q, 1); // space for dot
            lua_rawgeti(L, index, i);
            lua_to_tmp_heap(L, -1, q, true);
            lua_pop(L, 1);
        }
        cell *nil = alloc_tmp(q, 1);
        make_atom(nil, g_nil_s);
        
        pl_idx end = q->tmphp;
        pl_idx curr = start;
        for (int i = 1; i <= n; i++) {
            cell *dot = get_tmp_heap(q, curr);
            make_atom(dot, g_dot_s);
            dot->arity = 2;
            dot->num_cells = end - curr;
            curr++; 
            curr += get_tmp_heap(q, curr)->num_cells;
        }
        return end - start;
    } else {
        cell *c = alloc_tmp(q, 1);
        make_atom(c, g_nil_s);
        return 1;
    }
}

cell *lua_to_prolog(lua_State *L, int index, query *q) {
    pl_idx save_tmphp = q->tmphp;
    if (save_tmphp == 0) init_tmp_heap(q);
    lua_to_tmp_heap(L, index, q, false);
    pl_idx num_cells = q->tmphp - save_tmphp;
    cell *res = alloc_heap(q, num_cells);
    if (!res) return NULL;
    dup_cells(res, get_tmp_heap(q, save_tmphp), num_cells);
    q->tmphp = save_tmphp;
    return res;
}

static cell *lua_to_prolog_list(lua_State *L, int index, query *q) {
    pl_idx save_tmphp = q->tmphp;
    if (save_tmphp == 0) init_tmp_heap(q);
    lua_to_tmp_heap(L, index, q, true);
    pl_idx num_cells = q->tmphp - save_tmphp;
    cell *res = alloc_heap(q, num_cells);
    if (!res) return NULL;
    dup_cells(res, get_tmp_heap(q, save_tmphp), num_cells);
    q->tmphp = save_tmphp;
    return res;
}

bool bif_lua_eval_1(query *q) {
    GET_FIRST_ARG(p1, any);
    lua_State *L = get_lua_vm(q);
    const char *script = is_atom(p1) ? C_STR(q, p1) : (p1->val_strb->cstr + p1->strb_off);
    if (luaL_dostring(L, script) != 0) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool bif_lua_call_3(query *q) {
    GET_FIRST_ARG(p1, atom);
    GET_NEXT_ARG(p2, any);
    GET_NEXT_ARG(p3, any);
    lua_State *L = get_lua_vm(q);
    lua_State *coro;
    int n_args = 0;

    if (!q->retry) {
        coro = lua_newthread(L);
        // Save coro in registry to avoid GC
        lua_pushlightuserdata(L, q);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
        lua_pop(L, 1); // pop thread from stack

        lua_getglobal(coro, C_STR(q, p1));
        if (!lua_isfunction(coro, -1)) {
            // Clean up registry
            lua_pushlightuserdata(L, q);
            lua_pushnil(L);
            lua_settable(L, LUA_REGISTRYINDEX);
            return false;
        }

        cell *curr = p2;
        pl_ctx curr_ctx = p2_ctx;
        while (is_iso_list(curr)) {
            cell *h = curr + 1;
            prolog_to_lua(coro, q, h, curr_ctx);
            n_args++;
            curr = deref(q, h + h->num_cells, curr_ctx);
            curr_ctx = q->latest_ctx;
        }
    } else {
        lua_pushlightuserdata(L, q);
        lua_gettable(L, LUA_REGISTRYINDEX);
        coro = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (!coro) return false;
    }

    int nresults = 0;
    int status = lua_resume(coro, NULL, n_args, &nresults);

    if (status == LUA_YIELD) {
        return do_yield(q, 0);
    }

    // Clean up registry
    lua_pushlightuserdata(L, q);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    if (status != LUA_OK) {
        fprintf(stderr, "Lua Error: %s\n", lua_tostring(coro, -1));
        return false;
    }
    
    if (nresults > 0 && lua_isboolean(coro, -1) && !lua_toboolean(coro, -1)) {
        return false;
    }

    cell *res = (nresults > 0) ? lua_to_prolog(coro, -1, q) : NULL;
    bool ok = res ? unify(q, p3, p3_ctx, res, q->st.curr_fp) : true;
    return ok;
}

bool bif_lua_set_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    lua_State *L = get_lua_vm(q);
    int top = lua_gettop(L);
    lua_getglobal(L, "_PROLOG_STORE");
    
    module *m = q->pl->global_bb ? q->pl->user_m : q->st.m;
    if (is_compound(p1) && p1->val_off == g_colon_s && p1->arity == 2) {
        cell *p1_m = p1 + 1;
        cell *p1_k = p1_m + p1_m->num_cells;
        module *fm = find_module(q->pl, C_STR(q, p1_m));
        if (fm) m = fm;
        p1 = p1_k;
    }
    
    char keybuf[1024];
    if (is_atom(p1)) snprintf(keybuf, sizeof(keybuf), "%s:%s", m->name, C_STR(q, p1));
    else if (is_integer(p1)) snprintf(keybuf, sizeof(keybuf), "%s:%lld", m->name, (long long)get_smallint(p1));
    else snprintf(keybuf, sizeof(keybuf), "%s:opaque", m->name);
    
    lua_pushstring(L, keybuf);
    prolog_to_lua(L, q, p2, p2_ctx);
    lua_settable(L, -3);
    lua_settop(L, top);
    return true;
}

bool bif_lua_get_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    lua_State *L = get_lua_vm(q);
    int top = lua_gettop(L);
    lua_getglobal(L, "_PROLOG_STORE");
    
    module *m = q->pl->global_bb ? q->pl->user_m : q->st.m;
    if (is_compound(p1) && p1->val_off == g_colon_s && p1->arity == 2) {
        cell *p1_m = p1 + 1;
        cell *p1_k = p1_m + p1_m->num_cells;
        module *fm = find_module(q->pl, C_STR(q, p1_m));
        if (fm) m = fm;
        p1 = p1_k;
    }
    
    char keybuf[1024];
    if (is_atom(p1)) snprintf(keybuf, sizeof(keybuf), "%s:%s", m->name, C_STR(q, p1));
    else if (is_integer(p1)) snprintf(keybuf, sizeof(keybuf), "%s:%lld", m->name, (long long)get_smallint(p1));
    else snprintf(keybuf, sizeof(keybuf), "%s:opaque", m->name);
    
    lua_pushstring(L, keybuf);
    lua_gettable(L, -2);
    
    if (lua_isnil(L, -1)) {
        lua_settop(L, top);
        return false;
    }
    
    cell *res = lua_to_prolog(L, -1, q);
    bool ok = unify(q, p2, p2_ctx, res, q->st.curr_fp);
    lua_settop(L, top);
    return ok;
}

bool bif_lua_yield_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    lua_State *L = get_lua_vm(q);
    if (!q->retry) {
        if (is_atom(p1)) lua_getglobal(L, C_STR(q, p1));
        else prolog_to_lua(L, q, p1, p1_ctx);
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
        lua_setglobal(L, "_CURRENT_TABLE");
        lua_pushnil(L);
        lua_setglobal(L, "_CURRENT_KEY");
    }
    lua_getglobal(L, "_CURRENT_TABLE");
    lua_getglobal(L, "_CURRENT_KEY");
    if (lua_next(L, -2)) {
        lua_pushvalue(L, -2);
        lua_setglobal(L, "_CURRENT_KEY");
        cell *res = lua_to_prolog(L, -1, q);
        bool ok = unify(q, p2, p2_ctx, res, q->st.curr_fp);
        lua_pop(L, 3);
        if (ok) return retry_choice(q);
    }
    return false;
}

bool bif_lua_state_visit_1(query *q) {
    GET_FIRST_ARG(p1, any);
    lua_State *L = get_lua_vm(q);
    lua_getglobal(L, "_PROLOG_STATE");
    prolog_to_lua(L, q, p1, p1_ctx);
    lua_pushvalue(L, -1);
    lua_gettable(L, -3);
    
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    lua_settable(L, -3);
    lua_pop(L, 1);
    return true;
}

bool bif_lua_state_clear_0(query *q) {
    lua_State *L = get_lua_vm(q);
    lua_newtable(L);
    lua_setglobal(L, "_PROLOG_STATE");
    return true;
}

// --- Native Built-ins ---

bool bif_lua_fib_2(query *q) {
    GET_FIRST_ARG(p1, integer);
    GET_NEXT_ARG(p2, any);
    long long n = (long long)get_smallint(p1);
    long long a = 1, b = 1;
    for (long long i = 0; i < n; i++) {
        long long tmp = a; a = b; b = tmp + b;
    }
    cell res;
    make_int(&res, a);
    return unify(q, p2, p2_ctx, &res, q->st.curr_fp);
}

bool bif_lua_gcd_3(query *q) {
    GET_FIRST_ARG(p1, integer);
    GET_NEXT_ARG(p2, integer);
    GET_NEXT_ARG(p3, any);
    long long a = llabs(get_smallint(p1));
    long long b = llabs(get_smallint(p2));
    while (b) { a %= b; long long t = a; a = b; b = t; }
    cell res;
    make_int(&res, a);
    return unify(q, p3, p3_ctx, &res, q->st.curr_fp);
}

bool bif_lua_is_prime_1(query *q) {
    GET_FIRST_ARG(p1, integer);
    long long n = get_smallint(p1);
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (long long i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}

bool bif_lua_union_3(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    GET_NEXT_ARG(p3, any);
    lua_State *L = get_lua_vm(q);
    int top = lua_gettop(L);
    lua_checkstack(L, 100);
    lua_newtable(L); // Set
    lua_newtable(L); // Result
    int res_idx = 1;
    
    prolog_to_lua(L, q, p1, p1_ctx);
    if (lua_istable(L, -1)) {
        int n = (int)lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            lua_pushvalue(L, -1);
            lua_gettable(L, top + 1);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_pushvalue(L, -1);
                lua_pushboolean(L, 1);
                lua_settable(L, top + 1);
                lua_pushvalue(L, -1);
                lua_rawseti(L, top + 2, res_idx++);
            } else lua_pop(L, 1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    
    prolog_to_lua(L, q, p2, p2_ctx);
    if (lua_istable(L, -1)) {
        int n = (int)lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            lua_pushvalue(L, -1);
            lua_gettable(L, top + 1);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_pushvalue(L, -1);
                lua_pushboolean(L, 1);
                lua_settable(L, top + 1);
                lua_pushvalue(L, -1);
                lua_rawseti(L, top + 2, res_idx++);
            } else lua_pop(L, 1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    
    cell *res = lua_to_prolog_list(L, top + 2, q);
    lua_settop(L, top);
    return unify(q, p3, p3_ctx, res, q->st.curr_fp);
}

bool bif_lua_intersection_3(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    GET_NEXT_ARG(p3, any);
    lua_State *L = get_lua_vm(q);
    int top = lua_gettop(L);
    lua_checkstack(L, 100);
    lua_newtable(L); // Set
    lua_newtable(L); // Result
    int res_idx = 1;
    
    prolog_to_lua(L, q, p1, p1_ctx);
    if (lua_istable(L, -1)) {
        int n = (int)lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            lua_pushboolean(L, 1);
            lua_settable(L, top + 1);
        }
    }
    lua_pop(L, 1);
    
    prolog_to_lua(L, q, p2, p2_ctx);
    if (lua_istable(L, -1)) {
        int n = (int)lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            lua_pushvalue(L, -1);
            lua_gettable(L, top + 1);
            if (!lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_pushvalue(L, -1);
                lua_rawseti(L, top + 2, res_idx++);
            } else lua_pop(L, 1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    
    cell *res = lua_to_prolog_list(L, top + 2, q);
    lua_settop(L, top);
    return unify(q, p3, p3_ctx, res, q->st.curr_fp);
}

bool bif_lua_powerset_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    lua_State *L = get_lua_vm(q);
    int top = lua_gettop(L);
    lua_checkstack(L, 100);
    lua_newtable(L); // Result
    lua_newtable(L); // empty set
    lua_rawseti(L, -2, 1);
    
    prolog_to_lua(L, q, p1, p1_ctx);
    if (lua_istable(L, -1)) {
        int n = (int)lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            int cur_len = (int)lua_rawlen(L, top + 1);
            for (int j = 1; j <= cur_len; j++) {
                lua_rawgeti(L, top + 1, j);
                int set_len = (int)lua_rawlen(L, -1);
                lua_newtable(L);
                for (int k = 1; k <= set_len; k++) {
                    lua_rawgeti(L, -2, k);
                    lua_rawseti(L, -2, k);
                }
                lua_pushvalue(L, -3);
                lua_rawseti(L, -2, set_len + 1);
                lua_rawseti(L, top + 1, cur_len + j);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    cell *res = lua_to_prolog(L, top + 1, q);
    lua_settop(L, top);
    return unify(q, p2, p2_ctx, res, q->st.curr_fp);
}

builtins g_lua_bifs[] = {
    { .name = "lua_eval", .arity = 1, .fn = bif_lua_eval_1, .help = "+script", .iso = false },
    { .name = "lua_call", .arity = 3, .fn = bif_lua_call_3, .help = "+func, +args, -res", .iso = false },
    { .name = "lua_set", .arity = 2, .fn = bif_lua_set_2, .help = "+key, +val", .iso = false },
    { .name = "lua_get", .arity = 2, .fn = bif_lua_get_2, .help = "+key, -val", .iso = false },
    { .name = "lua_yield", .arity = 2, .fn = bif_lua_yield_2, .help = "+table, -val", .iso = false },
    { .name = "state_visit", .arity = 1, .fn = bif_lua_state_visit_1, .help = "+term", .iso = false },
    { .name = "state_clear", .arity = 0, .fn = bif_lua_state_clear_0, .help = "", .iso = false },
    { .name = "fib", .arity = 2, .fn = bif_lua_fib_2, .help = "+n, -res", .iso = false },
    { .name = "powerset", .arity = 2, .fn = bif_lua_powerset_2, .help = "+list, -res", .iso = false },
    { .name = "union", .arity = 3, .fn = bif_lua_union_3, .help = "+l1, +l2, -res", .iso = false },
    { .name = "intersection", .arity = 3, .fn = bif_lua_intersection_3, .help = "+l1, +l2, -res", .iso = false },
    { .name = "gcd", .arity = 3, .fn = bif_lua_gcd_3, .help = "+a, +b, -res", .iso = false },
    { .name = "is_prime", .arity = 1, .fn = bif_lua_is_prime_1, .help = "+n", .iso = false },
    { 0 }
};
#else
builtins g_lua_bifs[] = { { 0 } };
#endif

#ifndef USE_LUA
void init_lua_vm(prolog *pl) { (void)pl; }
lua_State *get_lua_vm(query *q) { (void)q; return NULL; }
void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx) { (void)L; (void)q; (void)c; (void)c_ctx; }
cell *lua_to_prolog(lua_State *L, int index, query *q) { (void)L; (void)index; (void)q; return NULL; }
bool call_lua_function(query *q, cell *c, pl_ctx c_ctx) { (void)q; (void)c; (void)c_ctx; return false; }
void lua_vm_lock() {}
void lua_vm_unlock() {}
#endif
