% test_gc_gain.pl - Analisando ganhos do GC Lua no Trealla
:- use_module(library(lists)).

stress_test(N) :-
    writeln('--- Benchmark: Global State Churn (N items) ---'),
    format('Iterações: ~w~n', [N]),
    
    % Teste 1: Database Nativo (Lento, fragmenta mais)
    get_time(T0),
    test_db(N),
    get_time(T1),
    D1 is T1 - T0,
    format('Prolog Assert/Retract: ~f s~n', [D1]),
    
    % Teste 2: Blackboard (Rápido, C puro, sem GC)
    get_time(T2),
    test_bb(N),
    get_time(T3),
    D2 is T3 - T2,
    format('Trealla Blackboard:    ~f s~n', [D2]),
    
    % Teste 3: Lua Store (Híbrido, gerenciado pelo Lua GC)
    get_time(T4),
    test_lua(N),
    get_time(T5),
    D3 is T5 - T4,
    format('Lua Store (Híbrido):   ~f s~n', [D3]),
    
    writeln('\n--- Uso de Memória Final ---'),
    statistics(heap, H), MB is H/1024/1024,
    format('Heap do Trealla: ~2f MB~n', [MB]).

% --- Versão Prolog Assert/Retract ---
:- dynamic(seen_db/2).
test_db(0) :- !.
test_db(N) :-
    Key is N mod 100,
    (retract(seen_db(Key, _)) ; true),
    assertz(seen_db(Key, N)),
    N1 is N - 1,
    test_db(N1).

% --- Versão Blackboard ---
test_bb(0) :- !.
test_bb(N) :-
    Key is N mod 100,
    bb_put(Key, N),
    N1 is N - 1,
    test_bb(N1).

% --- Versão Lua Store (Onde o GC do Lua brilha) ---
test_lua(0) :- !.
test_lua(N) :-
    Key is N mod 100,
    lua_set(Key, N),
    N1 is N - 1,
    test_lua(N1).

% --- Teste de Visitação de Estados (State Search) ---
% Simula o GC limpando rastros de busca
bench_visit(N) :-
    writeln('\n--- Benchmark: Seen-Set (State Visit) ---'),
    % Limpa o estado Lua
    state_clear,
    get_time(T0),
    loop_visit(N),
    get_time(T1),
    D is T1 - T0,
    format('Tempo State Visit (Lua GC): ~f s~n', [D]).

loop_visit(0) :- !.
loop_visit(N) :-
    % state_visit adiciona ao Lua e retorna true se for novo
    % Ideal para DFS/BFS em grafos gigantes sem sujar o heap do Prolog
    (state_visit(item(N)) -> true ; true),
    N1 is N - 1,
    loop_visit(N1).
