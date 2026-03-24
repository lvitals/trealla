#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "trealla.h"

// Metatable name for Prolog objects
#define TREALLA_PROLOG "Trealla.Prolog"

// create() -> prolog_ptr
static int l_pl_create(lua_State *L) {
    prolog *pl = pl_create();
    prolog **udata = (prolog **)lua_newuserdatauv(L, sizeof(prolog *), 0);
    *udata = pl;
    luaL_getmetatable(L, TREALLA_PROLOG);
    lua_setmetatable(L, -2);
    return 1;
}

// pl:eval("query") -> bool
static int l_pl_eval(lua_State *L) {
    prolog **udata = (prolog **)luaL_checkudata(L, 1, TREALLA_PROLOG);
    const char *expr = luaL_checkstring(L, 2);
    bool interactive = lua_toboolean(L, 3);
    bool ok = pl_eval(*udata, expr, interactive);
    lua_pushboolean(L, ok);
    return 1;
}

// pl:destroy()
static int l_pl_destroy(lua_State *L) {
    prolog **udata = (prolog **)luaL_checkudata(L, 1, TREALLA_PROLOG);
    if (*udata) {
        pl_destroy(*udata);
        *udata = NULL;
    }
    return 0;
}

static const struct luaL_Reg trealla_methods[] = {
    {"eval", l_pl_eval},
    {"destroy", l_pl_destroy},
    {"__gc", l_pl_destroy},
    {NULL, NULL}
};

static const struct luaL_Reg trealla_funcs[] = {
    {"create", l_pl_create},
    {NULL, NULL}
};

int luaopen_trealla(lua_State *L) {
    // Create metatable for Prolog userdata
    luaL_newmetatable(L, TREALLA_PROLOG);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, trealla_methods, 0);

    // Create the module table
    luaL_newlib(L, trealla_funcs);
    return 1;
}
