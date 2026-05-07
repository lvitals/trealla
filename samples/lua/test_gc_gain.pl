% Performance Test: State Storage and GC Efficiency
% Compares different ways of maintaining mutable state in Trealla

bench_gc_gain(N) :-
    format('Iterations: ~w~n', [N]),
    
    % Test 1: Pure Prolog (Assert/Retract - Heavy on index/memory)
    statistics(walltime, [_|0]),
    test_pure_prolog(N),
    statistics(walltime, [_, D1]),
    format('Pure Prolog (Assert/Retract): ~f s~n', [D1]),
    
    % Test 2: Blackboard (Fast, pure C, no GC)
    statistics(walltime, [_|0]),
    test_blackboard(N),
    statistics(walltime, [_, D2]),
    format('Blackboard (Pure C Store):    ~f s~n', [D2]),
    
    % Test 3: Lua Store (Hybrid, managed by Lua GC)
    statistics(walltime, [_|0]),
    test_lua_store(N),
    statistics(walltime, [_, D3]),
    format('Lua Store (Hybrid):   ~f s~n', [D3]),
    
    writeln('\n--- Final Memory Usage ---'),
    statistics(memory, [MemUsed, _]),
    format('Memory in use: ~w bytes~n', [MemUsed]).

% --- Pure Prolog Version (Assert/Retract) ---
test_pure_prolog(0) :- !.
test_pure_prolog(N) :-
    retractall(counter(_)),
    assertz(counter(N)),
    N1 is N - 1,
    test_pure_prolog(N1).

% --- Blackboard Version ---
test_blackboard(0) :- !.
test_blackboard(N) :-
    bb_put(counter, N),
    N1 is N - 1,
    test_blackboard(N1).

% --- Lua Store Version (Where Lua GC shines) ---
test_lua_store(0) :- !.
test_lua_store(N) :-
    lua_set(counter, N),
    N1 is N - 1,
    test_lua_store(N1).

% --- State Visitation Test (State Search) ---
test_state_visit(0) :- !.
test_state_visit(N) :-
    lua_state_visit(state(N)),
    N1 is N - 1,
    test_state_visit(N1).
