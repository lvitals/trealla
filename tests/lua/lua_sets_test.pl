% Test suite for Lua-accelerated Set operations
% Lua Math is now native. No import needed.

test_union :-
    write('Testing Set Union... '),
    union([1, 2, 3], [3, 4, 5], Result),
    % Result should contain 1, 2, 3, 4, 5 (order might vary depending on Lua pairs)
    (member(1, Result), member(2, Result), member(3, Result), member(4, Result), member(5, Result), length(Result, 5) 
    -> write('OK'), nl ; (write('FAILED: '), write(Result), nl)).

test_intersection :-
    write('Testing Set Intersection... '),
    intersection([1, 2, 3, 4], [3, 4, 5, 6], Result),
    % Result should be [3, 4]
    (member(3, Result), member(4, Result), length(Result, 2)
    -> write('OK'), nl ; (write('FAILED: '), write(Result), nl)).

test_powerset :-
    write('Testing Powerset (Set of all subsets)... '),
    powerset([a, b], Result),
    % Result for [a, b] should be [[],[a],[b],[a,b]]
    (member([], Result), member([a], Result), member([b], Result), member([a, b], Result), length(Result, 4)
    -> write('OK'), nl ; (write('FAILED: '), write(Result), nl)).

test_math_utils :-
    write('Testing Math Utils (GCD & Prime)... '),
    gcd(48, 18, 6),
    is_prime(97),
    \+ is_prime(100),
    write('OK'), nl.

run_set_tests :-
    write('--- LUA SETS & MATH TEST ---'), nl,
    test_union,
    test_intersection,
    test_powerset,
    test_math_utils,
    write('----------------------------'), nl,
    halt.
