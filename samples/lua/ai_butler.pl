% AI Butler Logic: NLP + Reasoning
:- use_module(library(dcgs)).
:- use_module(library(lists)).

% --- NLP Grammar (DCG) ---
% Parses sentences like "turn on the light" or "is the heater off?"
sentence(Action) --> command(Action).
sentence(Query)  --> question(Query).

command(set(Device, on))  --> [turn, on, the], device(Device).
command(set(Device, off)) --> [turn, off, the], device(Device).
command(set(Device, on))  --> [switch, on, the], device(Device).
command(set(Device, off)) --> [switch, off, the], device(Device).

question(is(Device, Val)) --> [is, the], device(Device), [Val, '?'].
question(status_all)      --> [what, is, the, current, status, '?'].

device(light)  --> [light].
device(heater) --> [heater].
device(ac)     --> [air, conditioner].

% --- Reasoning & Action ---
% handle(+Command, -Response)
handle(set(Device, Val), Response) :-
    lua_set(Device, Val),
    atomic_list_concat(['Understood. I have turned the', Device, Val], ' ', Response).

handle(is(Device, Val), Response) :-
    lua_get(Device, Current),
    (   Current == Val
    ->  atomic_list_concat(['Yes, the', Device, is, Val], ' ', Response)
    ;   atomic_list_concat(['No, actually the', Device, is, Current], ' ', Response)
    ).

handle(status_all, 'Here is the current state of the house:') :-
    write('--- Current State ---'), nl.

% Entry point for Lua
interpret(InputString, Response) :-
    atomic_list_concat(Words, ' ', InputString),
    (   phrase(sentence(Action), Words)
    ->  handle(Action, Response)
    ;   Response = 'I am sorry, I did not understand that command.'
    ).
