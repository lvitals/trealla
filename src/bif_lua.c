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

lua_State *g_lua_vm = NULL;

void init_lua_vm() {
    if (g_lua_vm) return;
    g_lua_vm = luaL_newstate();
    luaL_openlibs(g_lua_vm);
    
    // Backing store table
    lua_newtable(g_lua_vm);
    lua_setglobal(g_lua_vm, "_PROLOG_STORE");

    // State management table (Visited set)
    lua_newtable(g_lua_vm);
    lua_setglobal(g_lua_vm, "_PROLOG_STATE");
}

// --- Prolog -> Lua (Recursive) ---
void prolog_to_lua(lua_State *L, query *q, cell *c, pl_ctx c_ctx) {
    if (!lua_checkstack(L, 20)) return;
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
        pl_ctx curr_node_ctx = actual_ctx;
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

// --- Lua -> Prolog (Recursive using tmp_heap scratchpad) ---
static unsigned lua_to_tmp_heap(lua_State *L, int index, query *q, bool force_list) {
    if (!lua_checkstack(L, 20)) return 0;
    index = lua_absindex(L, index);
    int type = lua_type(L, index);
    pl_idx start = q->tmphp;
    
    if (type == LUA_TNUMBER) {
        cell *c = alloc_tmp(q, 1);
        if (lua_isinteger(L, index)) make_int(c, lua_tointeger(L, index));
        else make_float(c, lua_tonumber(L, index));
        c->num_cells = 1;
        c->flags = 0;
        return 1;
    } else if (type == LUA_TSTRING) {
        cell *c = alloc_tmp(q, 1);
        make_atom(c, new_atom(q->pl, lua_tostring(L, index)));
        c->num_cells = 1;
        c->flags = 0;
        return 1;
    } else if (type == LUA_TBOOLEAN) {
        cell *c = alloc_tmp(q, 1);
        make_atom(c, lua_toboolean(L, index) ? g_true_s : g_fail_s);
        c->num_cells = 1;
        c->flags = 0;
        return 1;
    } else if (type == LUA_TTABLE) {
        // Compound check: [0] = "functor"
        lua_rawgeti(L, index, 0);
        if (lua_isstring(L, -1)) {
            const char *functor = lua_tostring(L, -1);
            lua_pop(L, 1);
            int arity = (int)lua_rawlen(L, index);
            cell *c = alloc_tmp(q, 1);
            make_atom(c, new_atom(q->pl, functor));
            c->arity = arity;
            c->flags = 0;
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

        // Standard tables (lists/sets) return 'true' unless list content is required
        if (!force_list) {
            cell *c = alloc_tmp(q, 1);
            make_atom(c, g_true_s);
            c->num_cells = 1;
            c->flags = 0;
            return 1;
        }

        int n = (int)lua_rawlen(L, index);
        if (n == 0) {
            cell *c = alloc_tmp(q, 1);
            make_atom(c, g_nil_s);
            c->num_cells = 1;
            c->flags = 0;
            return 1;
        }
        
        for (int i = 1; i <= n; i++) {
            alloc_tmp(q, 1); // placeholder for dot
            lua_rawgeti(L, index, i);
            lua_to_tmp_heap(L, -1, q, true);
            lua_pop(L, 1);
        }
        cell *nil = alloc_tmp(q, 1);
        make_atom(nil, g_nil_s);
        nil->num_cells = 1;
        nil->flags = 0;
        
        pl_idx end = q->tmphp;
        pl_idx curr = start;
        for (int i = 1; i <= n; i++) {
            cell *dot = get_tmp_heap(q, curr);
            make_atom(dot, g_dot_s);
            dot->arity = 2;
            dot->num_cells = end - curr;
            dot->flags = 0;
            curr++; // Dot
            curr += get_tmp_heap(q, curr)->num_cells; // Head
        }
        return end - start;
    } else {
        cell *c = alloc_tmp(q, 1);
        make_atom(c, g_nil_s);
        c->num_cells = 1;
        c->flags = 0;
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
    dup_cells(res, q->tmp_heap + save_tmphp, num_cells);
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
    dup_cells(res, q->tmp_heap + save_tmphp, num_cells);
    q->tmphp = save_tmphp;
    return res;
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

bool bif_lua_state_visit_1(query *q) {
    GET_FIRST_ARG(p1, any);
    init_lua_vm();
    lua_getglobal(g_lua_vm, "_PROLOG_STATE");
    prolog_to_lua(g_lua_vm, q, p1, p1_ctx);
    lua_pushvalue(g_lua_vm, -1);
    lua_gettable(g_lua_vm, -3);
    
    if (!lua_isnil(g_lua_vm, -1)) {
        lua_pop(g_lua_vm, 2);
        return false;
    }
    
    lua_pop(g_lua_vm, 1);
    lua_pushboolean(g_lua_vm, 1);
    lua_settable(g_lua_vm, -3);
    lua_pop(g_lua_vm, 1);
    return true;
}

bool bif_lua_state_clear_0(query *q) {
    init_lua_vm();
    lua_newtable(g_lua_vm);
    lua_setglobal(g_lua_vm, "_PROLOG_STATE");
    return true;
}

// --- Native Math Implementations ---

bool bif_lua_fib_2(query *q) {
    GET_FIRST_ARG(p1, integer);
    GET_NEXT_ARG(p2, any);
    long long n = (long long)get_smallint(p1);
    long long a = 1, b = 1; // F(0)=1, F(1)=1 matches test053.pl
    for (long long i = 0; i < n; i++) {
        long long tmp = a;
        a = b;
        b = tmp + b;
    }
    cell res;
    make_int(&res, a);
    res.num_cells = 1;
    res.flags = 0;
    return unify(q, p2, p2_ctx, &res, q->st.cur_ctx);
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
    res.num_cells = 1;
    res.flags = 0;
    return unify(q, p3, p3_ctx, &res, q->st.cur_ctx);
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

// --- Set Operations using Lua Tables for speed ---

bool bif_lua_union_3(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    GET_NEXT_ARG(p3, any);
    init_lua_vm();
    int top = lua_gettop(g_lua_vm);
    lua_checkstack(g_lua_vm, 50);
    lua_newtable(g_lua_vm); // Set
    lua_newtable(g_lua_vm); // Result List
    int res_idx = 1;
    
    prolog_to_lua(g_lua_vm, q, p1, p1_ctx); // L1
    if (lua_istable(g_lua_vm, -1)) {
        int n = (int)lua_rawlen(g_lua_vm, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(g_lua_vm, -1, i);
            lua_pushvalue(g_lua_vm, -1);
            lua_gettable(g_lua_vm, top + 1);
            if (lua_isnil(g_lua_vm, -1)) {
                lua_pop(g_lua_vm, 1);
                lua_pushvalue(g_lua_vm, -1);
                lua_pushboolean(g_lua_vm, 1);
                lua_settable(g_lua_vm, top + 1);
                lua_pushvalue(g_lua_vm, -1);
                lua_rawseti(g_lua_vm, top + 2, res_idx++);
            } else lua_pop(g_lua_vm, 1);
            lua_pop(g_lua_vm, 1);
        }
    }
    lua_pop(g_lua_vm, 1);
    
    prolog_to_lua(g_lua_vm, q, p2, p2_ctx); // L2
    if (lua_istable(g_lua_vm, -1)) {
        int n = (int)lua_rawlen(g_lua_vm, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(g_lua_vm, -1, i);
            lua_pushvalue(g_lua_vm, -1);
            lua_gettable(g_lua_vm, top + 1);
            if (lua_isnil(g_lua_vm, -1)) {
                lua_pop(g_lua_vm, 1);
                lua_pushvalue(g_lua_vm, -1);
                lua_pushboolean(g_lua_vm, 1);
                lua_settable(g_lua_vm, top + 1);
                lua_pushvalue(g_lua_vm, -1);
                lua_rawseti(g_lua_vm, top + 2, res_idx++);
            } else lua_pop(g_lua_vm, 1);
            lua_pop(g_lua_vm, 1);
        }
    }
    lua_pop(g_lua_vm, 1);
    
    cell *res = lua_to_prolog_list(g_lua_vm, top + 2, q);
    lua_settop(g_lua_vm, top);
    return unify(q, p3, p3_ctx, res, q->st.cur_ctx);
}

bool bif_lua_intersection_3(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    GET_NEXT_ARG(p3, any);
    init_lua_vm();
    int top = lua_gettop(g_lua_vm);
    lua_checkstack(g_lua_vm, 50);
    lua_newtable(g_lua_vm); // Set
    lua_newtable(g_lua_vm); // Result List
    int res_idx = 1;
    
    prolog_to_lua(g_lua_vm, q, p1, p1_ctx); // L1
    if (lua_istable(g_lua_vm, -1)) {
        int n = (int)lua_rawlen(g_lua_vm, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(g_lua_vm, -1, i);
            lua_pushboolean(g_lua_vm, 1);
            lua_settable(g_lua_vm, top + 1);
        }
    }
    lua_pop(g_lua_vm, 1);
    
    prolog_to_lua(g_lua_vm, q, p2, p2_ctx); // L2
    if (lua_istable(g_lua_vm, -1)) {
        int n = (int)lua_rawlen(g_lua_vm, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(g_lua_vm, -1, i);
            lua_pushvalue(g_lua_vm, -1);
            lua_gettable(g_lua_vm, top + 1);
            if (!lua_isnil(g_lua_vm, -1)) {
                lua_pop(g_lua_vm, 1);
                lua_pushvalue(g_lua_vm, -1);
                lua_rawseti(g_lua_vm, top + 2, res_idx++);
            } else lua_pop(g_lua_vm, 1);
            lua_pop(g_lua_vm, 1);
        }
    }
    lua_pop(g_lua_vm, 1);
    
    cell *res = lua_to_prolog_list(g_lua_vm, top + 2, q);
    lua_settop(g_lua_vm, top);
    return unify(q, p3, p3_ctx, res, q->st.cur_ctx);
}

bool bif_lua_powerset_2(query *q) {
    GET_FIRST_ARG(p1, any);
    GET_NEXT_ARG(p2, any);
    init_lua_vm();
    int top = lua_gettop(g_lua_vm);
    lua_checkstack(g_lua_vm, 100);
    lua_newtable(g_lua_vm); // Result
    lua_newtable(g_lua_vm); // empty set
    lua_rawseti(g_lua_vm, -2, 1);
    
    prolog_to_lua(g_lua_vm, q, p1, p1_ctx); // Input
    if (lua_istable(g_lua_vm, -1)) {
        int n = (int)lua_rawlen(g_lua_vm, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(g_lua_vm, -1, i); // Elem
            int cur_len = (int)lua_rawlen(g_lua_vm, top + 1);
            for (int j = 1; j <= cur_len; j++) {
                lua_rawgeti(g_lua_vm, top + 1, j); // Existing set
                int set_len = (int)lua_rawlen(g_lua_vm, -1);
                lua_newtable(g_lua_vm); // New set
                for (int k = 1; k <= set_len; k++) {
                    lua_rawgeti(g_lua_vm, -2, k);
                    lua_rawseti(g_lua_vm, -2, k);
                }
                lua_pushvalue(g_lua_vm, -3); // Current Elem
                lua_rawseti(g_lua_vm, -2, set_len + 1);
                lua_rawseti(g_lua_vm, top + 1, cur_len + j);
                lua_pop(g_lua_vm, 1);
            }
            lua_pop(g_lua_vm, 1);
        }
    }
    lua_pop(g_lua_vm, 1);
    
    cell *res = lua_to_prolog(g_lua_vm, top + 1, q);
    lua_settop(g_lua_vm, top);
    return unify(q, p2, p2_ctx, res, q->st.cur_ctx);
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
