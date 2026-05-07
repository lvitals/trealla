:- use_module(library(lists)).

% Load the Lua script
:- lua_eval('dofile("samples/lua/parallel_calc.lua")').

% Execute heavy calculations in parallel using the Worker Pool.
% Each 'call_task' dispatch the goal to a worker thread.
run_parallel(NumTasks, Intensity) :-
    format("Spawning ~w parallel tasks with intensity ~w...~n", [NumTasks, Intensity]),
    statistics(walltime, [_|0]),
    spawn_tasks(NumTasks, Intensity, _Tasks),
    format("Tasks spawned. Waiting for completion...~n", []),
    wait, % The scheduler processes the workers
    format("All tasks finished.~n", []),
    statistics(walltime, [_, Elapsed]),
    format("Total elapsed time: ~w ms~n", [Elapsed]).

spawn_tasks(0, _, []) :- !.
spawn_tasks(N, Intensity, [task(N)|T]) :-
    % call_task/1 creates a task that will be executed in the Worker Pool.
    % Each worker will use its own isolated lua_vm.
    call_task((
        lua_call(intensive_calc, [N, Intensity], Res),
        format("[Prolog Worker ~w] Result: ~w~n", [N, Res])
    )),
    N1 is N - 1,
    spawn_tasks(N1, Intensity, T).

% Example usage:
% ?- run_parallel(8, 1000000).

