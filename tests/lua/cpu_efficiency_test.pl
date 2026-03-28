% CPU Efficiency Test: Reactive vs Threaded
% This test measures time AND simulated CPU work.

task_worker(ID) :-
    % Sleep simulates waiting for a socket (I/O)
    sleep(0.5).

run_efficiency_test(N) :-
    write('Launching 1000 reactive tasks...'), nl,
    get_time_ms(Start),
    forall(between(1, N, _), call_task(task_worker(_))),
    wait,
    get_time_ms(End),
    Time is (End - Start) / 1000,
    format('Total time for 1000 reactive tasks: ~3f sec~n', [Time]).

get_time_ms(T) :- statistics(wall, T).

:- run_efficiency_test(1000), halt.
