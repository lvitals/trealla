:- initialization(lua_eval('function test_list(n) local t = {}; for i=1,n do t[i] = i*1.1 end; return t end')).

test :-
    writeln('Calling lua_call...'),
    lua_call(test_list, [100], L),
    writeln('Got list'),
    length(L, Len),
    writeln(len(Len)),
    % writeln(L),
    writeln('Done').
