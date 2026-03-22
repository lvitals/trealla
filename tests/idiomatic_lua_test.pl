% Idiomatic Test using the lua_state abstraction library
:- use_module(library(lua_state)).

% --- Graph with a Cycle ---
edge(a, b).
edge(b, c).
edge(c, a). % Cycle
edge(c, d). % Exit

% --- Idiomatic Pathfinding ---
% No extra Visited list in the arguments!
path(X, Y) :-
    state_clear,
    find_path(X, Y).

% Base case
find_path(X, Y) :-
    edge(X, Y),
    write('Found path reaching: '), write(Y), nl.

% Recursive case using O(1) state from Lua
find_path(X, Y) :-
    state_visit(X), % Marks X as visited or fails if already visited
    edge(X, Z),
    find_path(Z, Y).

% --- Test Execution ---
run_idiomatic_test :-
    write('Testing idiomatic pathfinding with lua_state library...'), nl,
    (path(a, d) -> write('Test Passed: Path Found!'), nl ; write('Test Failed'), nl),
    halt.
