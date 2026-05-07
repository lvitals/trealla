% Demonstration of the new recursive mapping (Deep Mapping) between Prolog and Lua.
% Nested structures, lists, and compound terms are now faithfully converted.

setup_lua :-
    lua_eval('function echo(x) return x end'),
    lua_eval('function get_complex_table() 
        return {
            name = "Trealla",
            version = 2.0,
            features = {"ProLog", "Lua", "Parallel"},
            metadata = {
                author = "Leandro",
                tags = {1, 2, 3}
            }
        } 
    end').

% Test sending a complex term from Prolog to Lua and back.
test_prolog_to_lua :-
    ComplexTerm = person(name("Leandro"), 
                         skills(["Prolog", "C", "Lua"]), 
                         address(city("Lisbon"), zip(12345))),
    format("Sending to Lua: ~w~n", [ComplexTerm]),
    lua_call(echo, [ComplexTerm], Result),
    format("Received back: ~w~n", [Result]),
    (ComplexTerm == Result -> writeln("MATCH OK") ; writeln("MISMATCH")).

% Test receiving a complex nested Lua table.
test_lua_to_prolog :-
    writeln("Requesting complex table from Lua..."),
    lua_call(get_complex_table, [], Result),
    format("Resulting Prolog Term: ~q~n", [Result]),
    % Verify if specific fields are present
    member(name("Trealla"), Result),
    member(version(2.0), Result),
    writeln("Structure verification OK").

run_tests :-
    setup_lua,
    test_prolog_to_lua,
    nl,
    test_lua_to_prolog.
