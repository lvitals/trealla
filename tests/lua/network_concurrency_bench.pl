% Benchmark: OS Threads vs Reactive Async Tasks
% Simulates 1,000 concurrent "network" operations using sleep as latency.

:- use_module(library(lists)).

% --- 1. PURE PROLOG: OS THREADS ---
% This creates real OS threads via pthread_create.

thread_worker :-
    sleep(0.01). % Simulate network latency

run_threads(N) :-
    format("   Creating ~w OS Threads... ", [N]),
    get_time_ms(Start),
    findall(Tid, (between(1, N, _), thread_create(thread_worker, Tid)), Tids),
    format("Joining... "),
    forall(member(T, Tids), thread_join(T, _)),
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("Done in ~3f sec~n", [Time]).

% --- 2. TREALLA + LUA: ASYNC TASKS ---
% This creates light-weight tasks managed by the kpoll scheduler.

async_worker :-
    sleep(0.01). % This YIELDS to the kpoll scheduler

run_async(N) :-
    format("   Creating ~w Async Tasks... ", [N]),
    get_time_ms(Start),
    forall(between(1, N, _), call_task(async_worker)),
    wait, % The reactive scheduler takes over here
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format("Done in ~3f sec~n", [Time]).

% --- MAIN RUNNER ---

run_concurrency_bench :-
    N = 1000,
    write('======================================================='), nl,
    write('   CONCURRENCY BENCHMARK: OS THREADS VS ASYNC TASKS'), nl,
    write('   Scenario: 1,000 concurrent "network" requests'), nl,
    write('======================================================='), nl,
    nl,
    
    write('1. Pure Prolog (OS Threads - Heavyweight)'), nl,
    (catch(run_threads(N), E, (write('FAILED: '), write(E), nl))),
    nl,
    
    write('2. Trealla + Lua (Async Tasks - Lightweight/Reactive)'), nl,
    run_async(N),
    nl,
    
    write('--- OBSERVATION ---'), nl,
    write('Notice the CPU usage during execution:'), nl,
    write('Threads will spike CPU due to kernel context switching.'), nl,
    write('Async Tasks will keep CPU near 0% while waiting for I/O.'), nl,
    write('======================================================='), nl,
    halt.

get_time_ms(T) :-
    statistics(wall, T).
