:- module(lua_state, [state_clear/0, state_visit/1, state_visited/1]).

% Initialize Lua functions when the module is loaded
init_lua_fns :-
    lua_eval("prolog_state = {}; function lua_state_mark(t) if prolog_state[t] then return false end prolog_state[t] = true return true end; function lua_state_check(t) return prolog_state[t] == true end").

:- initialization(init_lua_fns).

% Clear the state
state_clear :- 
    lua_eval("prolog_state = {}").

% Mark a term as visited
state_visit(Term) :-
    nonvar(Term),
    lua_call(lua_state_mark, [Term], true).

% Check if a term was visited
state_visited(Term) :-
    nonvar(Term),
    lua_call(lua_state_check, [Term], true).
