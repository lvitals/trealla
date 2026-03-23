% Benchmark: Fibonacci 40
% Comparing Pure Prolog vs Trealla-Lua

% --- Pure Prolog (Slow) ---
fib_prolog(0, 0) :- !.
fib_prolog(1, 1) :- !.
fib_prolog(N, R) :-
    N1 is N - 1, N2 is N - 2,
    fib_prolog(N1, R1),
    fib_prolog(N2, R2),
    R is R1 + R2.

% --- Lua Version (Fast via LuaJIT/Lua) ---
setup_lua :-
    lua_eval("function fib_lua(n) local a, b = 0, 1 for i = 1, n do a, b = b, a + b end return a end").

% --- Test Interface ---
run_pure_prolog(N) :-
    write('Running Fibonacci (Pure Prolog) N='), write(N), nl,
    time(fib_prolog(N, R)),
    write('Result: '), write(R), nl.

run_lua_prolog(N) :-
    write('Running Fibonacci (Trealla-LuaJIT) N='), write(N), nl,
    setup_lua,
    time(lua_call(fib_lua, [N], R)),
    write('Result: '), write(R), nl.
