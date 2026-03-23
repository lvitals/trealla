% --- 1. Math Stress (Pure Recursive) ---
fib_pure(0, 0) :- !.
fib_pure(1, 1) :- !.
fib_pure(N, R) :-
    N1 is N - 1, N2 is N - 2,
    fib_pure(N1, R1),
    fib_pure(N2, R2),
    R is R1 + R2.

% --- 2. Graph Stress (Pure List State) ---
generate_edges(0) :- !.
generate_edges(N) :-
    N1 is N - 1,
    assertz(edge(N, N1)),
    generate_edges(N1).

% Pure Pathfinding with Visited List (O(N) search)
path_pure(X, Y, Visited) :-
    edge(X, Y).
path_pure(X, Y, Visited) :-
    edge(X, Z),
    \+ member(Z, Visited),
    path_pure(Z, Y, [Z|Visited]).

% --- 3. Database Stress (assertz/1) ---
:- dynamic(data/2).
stress_db_insert(0) :- !.
stress_db_insert(N) :-
    assertz(data(N, val(N))),
    N1 is N - 1,
    stress_db_insert(N1).

stress_db_get(0) :- !.
stress_db_get(N) :-
    data(N, _),
    N1 is N - 1,
    stress_db_get(N1).

% --- Main Runner ---
run_pure_benchmark :-
    write('--- PURE PROLOG BENCHMARK ---'), nl,
    
    write('1. Math (Fibonacci 25 - Pure is slow): '),
    time(fib_pure(25, _)),
    
    write('2. Generating 1000 Edges... '),
    generate_edges(1000), assertz(edge(0, 1000)),
    write('Done.'), nl,
    write('3. Pathfinding in Large Cyclic Graph (List Search): '),
    time(path_pure(1000, 500, [1000])),
    
    write('4. Database: Inserting 5000 records (assertz): '),
    time(stress_db_insert(5000)),
    write('5. Database: Fetching 5000 records (query): '),
    time(stress_db_get(5000)),
    
    write('-----------------------------'), nl,
    halt.
