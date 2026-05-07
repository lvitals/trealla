% Demonstrates the efficiency of the new Timer Heap (O(log N)).
% We launch thousands of tasks that "sleep" for different durations.

stress_timers(Count) :-
    format("Launching ~w async tasks with timers...~n", [Count]),
    statistics(walltime, [_|0]),
    launch_tasks(Count),
    wait,
    statistics(walltime, [_, Elapsed]),
    format("Finished processing ~w timers in ~w ms.~n", [Count, Elapsed]).

launch_tasks(0) :- !.
launch_tasks(N) :-
    % Random wait time between 1 and 5 seconds
    RandomWait is 1 + (N mod 5),
    call_task((
        sleep(RandomWait)
        % format("Task ~w woke up after ~w s~n", [N, RandomWait])
    )),
    N1 is N - 1,
    launch_tasks(N1).

% Example usage:
% ?- stress_timers(2000).
