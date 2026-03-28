% Async Scheduler and Multi-threading Benchmark Suite
% This test measures the gains from VM isolation and the kpoll-based reactive scheduler.

:- use_module(library(lists)).

% --- SETUP LUA ---
setup_lua :-
    % Heavy loop for CPU stress
    lua_eval("function heavy_loop(n) local sum = 0 for i=1,n do sum = sum + i end return sum end").

% --- 1. MULTI-THREADING BENCHMARK ---
% Each thread has its own Lua VM, so they should run in parallel.

run_parallel_threads(N) :-
    setup_lua,
    get_time_ms(Start),
    findall(Tid, (between(1, N, _), thread_create(lua_call(heavy_loop, [10000000], _), Tid)), Tids),
    forall(member(T, Tids), thread_join(T, _)),
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("Parallel (N=~w): ~3f sec~n", [N, Time]).

run_sequential_lua(N) :-
    setup_lua,
    get_time_ms(Start),
    forall(between(1, N, _), lua_call(heavy_loop, [10000000], _)),
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("Sequential (N=~w): ~3f sec~n", [N, Time]).

% --- 2. ASYNC/REACTIVE SCHEDULER BENCHMARK ---
% Spawns many tasks that yield. Reactive scheduler should handle them with low overhead.

task_worker(_ID) :-
    % Sleep for 1 second.
    % If reactive, multiple tasks should take ~1s total if they yield correctly.
    sleep(1),
    % Do some minor work
    lua_call(heavy_loop, [100], _).

run_async_benchmark(N) :-
    setup_lua,
    get_time_ms(Start),
    forall(between(1, N, _), call_task(task_worker(_))),
    wait,
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("Async Tasks (N=~w, sleep 1s each): ~3f sec~n", [N, Time]).

% --- 3. STATE STORE BENCHMARK ---
% Comparing Lua-based state vs Prolog dynamic database.

:- dynamic(prolog_data/2).

bench_prolog_store(N) :-
    retractall(prolog_data(_, _)),
    get_time_ms(Start),
    forall(between(1, N, I), assertz(prolog_data(I, val(I)))),
    forall(between(1, N, I), (prolog_data(I, _), true)),
    get_time_ms(End),
    Time is float(End - Start),
    format("Prolog Store (assertz/query, N=~w): ~1f ms~n", [N, Time]).

bench_lua_store(N) :-
    get_time_ms(Start),
    forall(between(1, N, I), lua_set(I, val(I))),
    forall(between(1, N, I), (lua_get(I, _), true)),
    get_time_ms(End),
    Time is float(End - Start),
    format("Lua Store (lua_set/get, N=~w): ~1f ms~n", [N, Time]).

% --- MAIN RUNNER ---
run_all_benchmarks :-
    write('--- TREALLA + LUA PERFORMANCE & SCALABILITY REPORT ---'), nl,
    
    write('1. Multi-threading (VM Isolation Test)'), nl,
    run_sequential_lua(4),
    run_parallel_threads(4),
    nl,
    
    write('2. Async Scheduler (kpoll/Reactive Test)'), nl,
    % 10 tasks sleeping 1s.
    run_async_benchmark(10),
    nl,
    
    write('3. State Storage Efficiency'), nl,
    bench_prolog_store(5000),
    bench_lua_store(5000),
    nl,
    
    write('-------------------------------------------------------'), nl,
    halt.

get_time_ms(T) :-
    statistics(wall, T).
