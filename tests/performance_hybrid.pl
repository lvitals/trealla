% Lua Math and State are now native. No imports needed.

% --- 1. Math Stress (Now Idiomatic) ---
stress_math(N, R) :-
    fib(N, R). % Pure Prolog look, JIT speed.

% --- 2. Graph Stress ---
generate_edges(0) :- !.
generate_edges(N) :-
    N1 is N - 1,
    assertz(edge(N, N1)),
    generate_edges(N1).

path_hybrid(X, Y) :-
    state_visit(X),
    (edge(X, Y) ; (edge(X, Z), path_hybrid(Z, Y))).

% --- 3. Database Stress ---
stress_db_insert(0) :- !.
stress_db_insert(N) :-
    lua_set(N, val(N)),
    N1 is N - 1,
    stress_db_insert(N1).

stress_db_get(0) :- !.
stress_db_get(N) :-
    lua_get(N, _),
    N1 is N - 1,
    stress_db_get(N1).

% --- Main Runner ---
run_hybrid_benchmark :-
    write('--- IDIOMATIC HYBRID BENCHMARK (Trealla + Lua) ---'), nl,
    
    write('1. Math (fib/2 - Idiomatic JIT): '),
    time(stress_math(40, _)),
    
    write('2. Generating 1000 Edges... '),
    generate_edges(1000), assertz(edge(0, 1000)),
    write('Done.'), nl,
    
    write('3. Pathfinding in Large Cyclic Graph (Idiomatic O(1)): '),
    state_clear,
    time(path_hybrid(1000, 500)),
    
    write('4. Database: Inserting 5000 records into Lua: '),
    time(stress_db_insert(5000)),
    
    write('5. Database: Fetching 5000 records from Lua: '),
    time(stress_db_get(5000)),
    
    write('---------------------------------------'), nl,
    halt.
