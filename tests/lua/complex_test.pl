% Test script for recursive Prolog -> Lua conversion

setup_lua :-
    % Lua function to sum all numbers in a nested table structure
    lua_eval("function sum_recursive(t) local sum = 0 for k, v in pairs(t) do if type(v) == 'table' then sum = sum + sum_recursive(v) elseif type(v) == 'number' then sum = sum + v end end return sum end"),
    
    % Lua function to identify the functor of a Prolog compound (at index 0)
    lua_eval("function identify_functor(t) if type(t) == 'table' and t[0] then return 'The functor is: ' .. t[0] end return 'Not a compound' end").

% --- Tests ---

test_nested_sum(Result) :-
    setup_lua,
    ComplexList = [10, [20, 30], [[5, 5], 10]], % Total = 80
    lua_call(sum_recursive, [ComplexList], Result).

test_compound_functor(Msg) :-
    setup_lua,
    Compound = person(leandro, 30),
    lua_call(identify_functor, [Compound], Msg).

run_tests :-
    write('Running nested sum test... '),
    test_nested_sum(Sum),
    (Sum == 80 -> write('OK (Sum=80)'), nl ; (write('FAILED (Sum='), write(Sum), write(')'), nl)),
    
    write('Running compound functor test... '),
    test_compound_functor(Msg),
    (Msg == 'The functor is: person' -> write('OK'), nl ; (write('FAILED (Msg='), write(Msg), write(')'), nl)).
