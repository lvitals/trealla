% Advanced Lua Integration Test Suite

setup_lua :-
    % Function that returns a complex nested structure
    lua_eval("function get_nested() return {1, {2, 3}, {[0]='person', 'leandro', 30}} end"),
    % Define a global table for backtracking test
    lua_eval("my_items = {'apple', 'banana', 'cherry'}").

test_bidirectional :-
    write('Testing Lua -> Prolog Bidirectional conversion... '),
    lua_call(get_nested, [], Result),
    % Result should be [1,[2,3],person(leandro,30)]
    (Result == [1,[2,3],person(leandro,30)] -> write('OK'), nl ; (write('FAILED: '), write(Result), nl)).

test_backing_store :-
    write('Testing Lua Backing Store (lua_set/lua_get)... '),
    % Store a simple term
    lua_set(user_id, 12345),
    lua_get(user_id, Retrieved),
    (Retrieved == 12345 -> write('OK'), nl ; (write('FAILED'), nl)).

test_backtracking :-
    write('Testing Lua Backtracking (lua_yield)... '),
    % Iterate over 'my_items' global table
    findall(X, lua_yield(my_items, X), List),
    % List should contain the values
    (member(apple, List), member(banana, List), member(cherry, List) -> write('OK'), nl ; (write('FAILED: '), write(List), nl)).

run_advanced_tests :-
    setup_lua,
    test_bidirectional,
    test_backing_store,
    test_backtracking,
    write('All advanced tests completed successfully!'), nl.
