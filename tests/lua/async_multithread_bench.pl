% Trealla + Lua: Extreme Stress & Scalability Test Suite
% This suite pushes the boundaries of the async scheduler and multi-threaded isolation.

:- use_module(library(lists)).
:- use_module(library(statistics)).

% --- SETUP LUA ---
setup_lua :-
    % Much heavier computation
    lua_eval("function stress_math(n) local s = 0 for i=1,n do s = s + math.sqrt(i) * math.sin(i) end return s end"),
    lua_eval("function lua_store_bulk(n) for i=1,n do lua_set(i, {id=i, val='data'..i}) end end").

% --- 1. MULTI-THREADING SCALABILITY ---
% Use a massive number of iterations to overcome thread creation overhead.
stress_threads(Threads, Iterations) :-
    setup_lua,
    get_time_ms(Start),
    % Distribute work among threads
    WorkPerThread is Iterations // Threads,
    findall(Tid, (between(1, Threads, _), thread_create(lua_call(stress_math, [WorkPerThread], _), Tid)), Tids),
    forall(member(T, Tids), thread_join(T, _)),
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("  ~w Threads (~w iter/thread): ~3f sec~n", [Threads, WorkPerThread, Time]).

run_threading_report :-
    write('1. Threading Scalability (Parallel Lua VMs)'), nl,
    TotalWork = 40000000,
    stress_threads(1, TotalWork),
    stress_threads(2, TotalWork),
    stress_threads(4, TotalWork),
    nl.

% --- 2. MASSIVE ASYNC CONCURRENCY (SCHEDULER STRESS) ---
% Spawns thousands of reactive tasks to test kpoll efficiency.

async_worker(ID) :-
    % Random jitter sleep between 0.1 and 0.5s to stress the reactive wakeup
    % In Trealla, we'll use a mix of yields
    sleep(0.1),
    lua_call(stress_math, [1000], _),
    (0 is ID mod 2 -> sleep(0.1) ; yield),
    lua_call(stress_math, [1000], _).

run_async_stress(N) :-
    setup_lua,
    get_time_ms(Start),
    format("  Launching ~w reactive tasks... ", [N]),
    forall(between(1, N, I), call_task(async_worker(I))),
    wait,
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("Done in ~3f sec~n", [Time]).

% --- 3. HIGH-VOLUME STATE STORAGE (MEMORY STRESS) ---
% Comparing Prolog Dynamic DB vs Lua Backing Store with 50k records.

:- dynamic(prolog_bench/2).

run_storage_stress(N) :-
    write('3. High-Volume State Storage (50,000 records)'), nl,
    % Prolog
    retractall(prolog_bench(_, _)),
    get_time_ms(P0),
    forall(between(1, N, I), assertz(prolog_bench(I, data(I)))),
    forall(between(1, N, I), (prolog_bench(I, _), true)),
    get_time_ms(P1),
    PTime is float(P1 - P0),
    format("  Prolog Store: ~1f ms~n", [PTime]),
    
    % Lua
    get_time_ms(L0),
    forall(between(1, N, I), lua_set(I, data(I))),
    forall(between(1, N, I), (lua_get(I, _), true)),
    get_time_ms(L1),
    LTime is float(L1 - L0),
    format("  Lua Store:    ~1f ms (~2f x faster)~n", [LTime, PTime / LTime]),
    nl.

% --- MAIN RUNNER ---
run_extreme_benchmarks :-
    write('======================================================='), nl,
    write('   TREALLA + LUA EXTREME STRESS REPORT'), nl,
    write('======================================================='), nl,
    nl,
    run_threading_report,
    
    write('2. Massive Async Concurrency (kpoll Stress)'), nl,
    run_async_stress(100),   % Warmup
    run_async_stress(1000),  % Real stress
    nl,
    
    run_storage_stress(50000),
    
    write('======================================================='), nl,
    halt.

get_time_ms(T) :-
    statistics(wall, T).
