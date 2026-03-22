% Cycle Detection and Pathfinding using Lua as a "Visited Set"

% --- The Graph (with a cycle) ---
edge(a, b).
edge(b, c).
edge(c, a). % The Cycle: a -> b -> c -> a
edge(c, d). % The Exit:  c -> d

% --- Lua Setup ---
setup_cycle_detection :-
    % Clear the visited set in Lua
    lua_eval("visited_nodes = {}"),
    lua_eval("function mark_visited(node) if visited_nodes[node] then return false end visited_nodes[node] = true return true end"),
    lua_eval("function clear_visited() visited_nodes = {} end").

% --- Path Predicate ---
% finds a path from X to Y without getting stuck in cycles.
path(X, Y) :-
    setup_cycle_detection,
    clear_visited,
    find_path(X, Y, []).

% Base case: direct edge
find_path(X, Y, Path) :-
    edge(X, Y),
    append(Path, [X, Y], FullPath),
    write('Found Path: '), write(FullPath), nl.

% Recursive case with Lua-based cycle detection
find_path(X, Y, Path) :-
    % 1. Mark current node as visited in Lua.
    % If already visited, this will fail (backtrack).
    lua_call(mark_visited, [X], true),
    
    % 2. Find an outgoing edge
    edge(X, Z),
    
    % 3. Continue searching from Z
    append(Path, [X], NewPath),
    find_path(Z, Y, NewPath).

% Helper to clear visited set from Prolog
clear_visited :-
    lua_eval("clear_visited()").

% --- Test Query ---
run_cycle_test :-
    write('Searching for path from a to d in cyclic graph...'), nl,
    (path(a, d) -> write('Test Passed: Path Found!'), nl ; write('Test Failed: Could not find path')),
    halt.
