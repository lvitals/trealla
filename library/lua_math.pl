:- module(lua_math, [fib/2, factorial/2, gcd/3, is_prime/1, union/3, intersection/3, powerset/2]).

% Initialization: Load all Lua-side implementations in a single string line
init_lua_math :-
    lua_eval("function lua_fib(n) local a, b = 0, 1 for i = 1, n do a, b = b, a + b end return a end; function lua_fact(n) local r = 1 for i = 2, n do r = r * i end return r end; function lua_gcd(a, b) while b ~= 0 do a, b = b, a % b end return a end; function lua_is_prime(n) if n < 2 then return false end for i = 2, math.sqrt(n) do if n % i == 0 then return false end end return true end; function lua_union(l1, l2) local res, seen = {}, {} for _, v in ipairs(l1) do if not seen[v] then table.insert(res, v); seen[v] = true end end for _, v in ipairs(l2) do if not seen[v] then table.insert(res, v); seen[v] = true end end return res end; function lua_intersection(l1, l2) local res, seen = {}, {} for _, v in ipairs(l1) do seen[v] = true end for _, v in ipairs(l2) do if seen[v] then table.insert(res, v); seen[v] = nil end end return res end; function lua_powerset(l) local res = {{}} for _, elem in ipairs(l) do local new_sets = {} for _, set in ipairs(res) do local copy = {table.unpack(set)} table.insert(copy, elem) table.insert(new_sets, copy) end for _, s in ipairs(new_sets) do table.insert(res, s) end end return res end").

:- initialization(init_lua_math).

% --- Idiomatic Prolog Wrappers ---
fib(N, R) :- integer(N), lua_call(lua_fib, [N], R).
factorial(N, R) :- integer(N), lua_call(lua_fact, [N], R).
gcd(A, B, R) :- integer(A), integer(B), lua_call(lua_gcd, [A, B], R).
is_prime(N) :- integer(N), lua_call(lua_is_prime, [N], true).
union(L1, L2, R) :- is_list(L1), is_list(L2), lua_call(lua_union, [L1, L2], R).
intersection(L1, L2, R) :- is_list(L1), is_list(L2), lua_call(lua_intersection, [L1, L2], R).
powerset(L, R) :- is_list(L), lua_call(lua_powerset, [L], R).
