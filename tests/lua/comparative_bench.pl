% Trealla Pure Prolog vs Trealla + Lua: Comparative Benchmark
% This script measures the performance delta between pure logic and Lua-accelerated logic.

:- use_module(library(lists)).

% --- 1. MATH BENCHMARK (Fibonacci 32) ---

fib_prolog(0, 0) :- !.
fib_prolog(1, 1) :- !.
fib_prolog(N, R) :-
    N1 is N - 1, N2 is N - 2,
    fib_prolog(N1, R1),
    fib_prolog(N2, R2),
    R is R1 + R2.

setup_lua_math :-
    lua_eval("function fib_lua(n) local a, b = 0, 1 for i = 1, n do a, b = b, a + b end return a end").

bench_math :-
    write('1. Math (Fibonacci 32)'), nl,
    % Prolog
    get_time_ms(P0),
    fib_prolog(32, _),
    get_time_ms(P1),
    PTime is (P1 - P0) / 1000,
    format("   Pure Prolog:   ~3f sec~n", [PTime]),
    
    % Lua
    setup_lua_math,
    get_time_ms(L0),
    lua_call(fib_lua, [32], _),
    get_time_ms(L1),
    LTimeRaw is L1 - L0,
    % Avoid division by zero
    (LTimeRaw == 0 -> LTimeFix = 1 ; LTimeFix = LTimeRaw),
    LTime is LTimeRaw / 1000,
    Speedup is (PTime * 1000) / LTimeFix,
    format("   Trealla + Lua: ~3f sec (~2f x faster)~n", [LTime, Speedup]),
    nl.

% --- 2. DATABASE BENCHMARK (20k Records) ---

:- dynamic(prolog_db/2).

bench_db(N) :-
    write('2. Database Store/Get (20,000 records)'), nl,
    % Prolog
    retractall(prolog_db(_, _)),
    get_time_ms(P0),
    forall(between(1, N, I), assertz(prolog_db(I, val(I)))),
    forall(between(1, N, I), (prolog_db(I, _), true)),
    get_time_ms(P1),
    PTime is float(P1 - P0),
    format("   Pure Prolog (assertz): ~1f ms~n", [PTime]),
    
    % Lua
    get_time_ms(L0),
    forall(between(1, N, I), lua_set(I, val(I))),
    forall(between(1, N, I), (lua_get(I, _), true)),
    get_time_ms(L1),
    LTimeRaw is float(L1 - L0),
    (LTimeRaw == 0.0 -> LTimeFix = 1.0 ; LTimeFix = LTimeRaw),
    format("   Trealla + Lua (set/get): ~1f ms (~2f x faster)~n", [LTimeRaw, PTime / LTimeFix]),
    nl.

% --- 3. LIST PROCESSING (1M Elements) ---

generate_list(0, []) :- !.
generate_list(N, [N|T]) :- N1 is N - 1, generate_list(N1, T).

% Tail-recursive sum for Prolog to avoid stack overflow
sum_prolog(List, Sum) :- sum_prolog(List, 0, Sum).
sum_prolog([], Acc, Acc).
sum_prolog([H|T], Acc, Sum) :- Acc1 is Acc + H, sum_prolog(T, Acc1, Sum).

setup_lua_list :-
    lua_eval("function sum_lua(t) local s = 0 for _, v in ipairs(t) do s = s + v end return s end").

bench_list :-
    write('3. List Processing (Sum 100,000 elements)'), nl,
    N = 100000,
    findall(I, between(1, N, I), List),
    
    % Prolog
    get_time_ms(P0),
    sum_prolog(List, _),
    get_time_ms(P1),
    PTime is float(P1 - P0),
    format("   Pure Prolog (tail-rec): ~1f ms~n", [PTime]),
    
    % Lua
    setup_lua_list,
    get_time_ms(L0),
    lua_call(sum_lua, [List], _),
    get_time_ms(L1),
    LTimeRaw is float(L1 - L0),
    (LTimeRaw == 0.0 -> LTimeFix = 1.0 ; LTimeFix = LTimeRaw),
    format("   Trealla + Lua (iteração): ~1f ms (~2f x faster)~n", [LTimeRaw, PTime / LTimeFix]),
    nl.

% --- MAIN RUNNER ---

run_comparative_bench :-
    write('======================================================='), nl,
    write('   COMPARATIVE REPORT: PURE PROLOG VS TREALLA + LUA'), nl,
    write('======================================================='), nl,
    nl,
    bench_math,
    bench_db(20000),
    bench_list,
    write('======================================================='), nl,
    halt.

get_time_ms(T) :-
    statistics(wall, T).
