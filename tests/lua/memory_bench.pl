% Benchmark: Memory and Performance (Threads vs Async)

get_memory_usage(RSS) :-
    (  exists_file('/proc/self/statm')
    -> setup_call_cleanup(open('/proc/self/statm', read, Stream),
          (read_line_to_codes(Stream, Codes),
           append(PagesCodes, [32|_], Codes), % 32 is space
           atom_codes(PagesAtom, PagesCodes),
           atom_number(PagesAtom, Pages),
           RSS is Pages * 4 / 1024) % Convert pages to MB (assuming 4KB pages)
       , close(Stream))
    ;  RSS = 0).

thread_worker :- sleep(0.1).
async_worker :- sleep(0.1).

run_benchmarks :-
    N = 500,
    write('======================================================='), nl,
    write('   MEMORY & PERFORMANCE COMPARISON (N=500)'), nl,
    write('======================================================='), nl,
    
    % 1. BASELINE
    get_memory_usage(M0),
    statistics(heap, H0),
    format("Baseline Memory: ~2f MB | Heap: ~w bytes~n", [M0, H0]),
    nl,

    % 2. OS THREADS
    write('1. Testing OS Threads...'), nl,
    get_time_ms(T0),
    findall(Tid, (between(1, N, _), thread_create(thread_worker, Tid)), Tids),
    get_memory_usage(M1),
    statistics(heap, H1),
    forall(member(T, Tids), thread_join(T, _)),
    get_time_ms(T1),
    TimeThreads is (T1 - T0) / 1000,
    format("   Time: ~3f sec | Memory: ~2f MB | Heap: ~w bytes~n", [TimeThreads, M1, H1]),
    nl,

    % 3. ASYNC TASKS
    write('2. Testing Async Tasks (Trealla + Lua)...'), nl,
    get_time_ms(T2),
    forall(between(1, N, _), call_task(async_worker)),
    get_memory_usage(M2),
    statistics(heap, H2),
    wait,
    get_time_ms(T3),
    TimeAsync is (T3 - T2) / 1000,
    format("   Time: ~3f sec | Memory: ~2f MB | Heap: ~w bytes~n", [TimeAsync, M2, H2]),
    nl,
    
    write('--- ANALYSIS ---'), nl,
    MemDiff is M1 - M2,
    format("Async Tasks used ~2f MB LESS memory than OS Threads.~n", [MemDiff]),
    write('======================================================='), nl,
    halt.

get_time_ms(T) :- statistics(wall, T).
