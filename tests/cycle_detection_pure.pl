% Idiomatic Pure Prolog Cycle Detection
% This version uses a list to track visited nodes.
% No external state, no Lua, 100% portable.

% --- The Graph ---
edge(a, b).
edge(b, c).
edge(c, a). % Cycle
edge(c, d). % Exit

% path(Start, End)
path(X, Y) :-
    find_path(X, Y, [X]). % Initialize visited list with the start node

% Base case: Direct connection
find_path(X, Y, _Visited) :-
    edge(X, Y),
    write('Found Path Tail: '), write([X, Y]), nl.

% Recursive case: Check if Z was already visited before recursing
find_path(X, Y, Visited) :-
    edge(X, Z),
    \+ member(Z, Visited), % PURE PROLOG CHECK: O(N) complexity
    find_path(Z, Y, [Z|Visited]).

% Helper test predicate
run_pure_test :-
    write('Searching for path from a to d (Pure Prolog)...'), nl,
    (path(a, d) -> write('Test Passed!') ; write('Test Failed')), nl,
    halt.
