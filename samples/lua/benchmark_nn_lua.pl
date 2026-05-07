% benchmark_nn_lua.pl - Stable Hybrid Version (Trealla + Lua)
:- use_module(library(lists)).

% Load the Lua accelerator at initialization
:- initialization(lua_eval('require("samples/lua/nn_accel")')).

mark_nn_lua :-
    Epocas = 1500, HSize = 32, NEx = 20, LR = 0.01,
    writeln('=== NEURAL NETWORK BENCHMARK (HYBRID STABLE) ==='),
    format('Epochs : ~w | Neurons : ~w~n', [Epocas, HSize]),

    % Initialization in Lua Store (O(1))
    gera_pesos_oculta_aleatorios(HSize, PO_inicial),
    gera_pesos_saida_aleatorios(HSize, PS_inicial),
    lua_set(pesos_oculta, PO_inicial),
    lua_set(pesos_saida, PS_inicial),

    Entrada = [0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,0.9,0.8,0.7,0.6,0.5],
    SD = 1.0,

    % Initial Error
    feedforward_oculta_lua(Entrada, PO_inicial, SO_i, _),
    SomaS is dot(SO_i, PS_inicial),
    SaidaInicial is sigmoide(SomaS),
    format('Initial output: ~f~n', [SaidaInicial]),

    get_time(T0),
    treinar_epocas_lua(Epocas, Entrada, SD, NEx, LR, 1),
    get_time(T1),

    Tempo is T1 - T0,
    writeln('\n=== BENCHMARK RESULTS (HYBRID) ==='),
    format('Total time: ~f~n', [Tempo]),
    
    lua_get(pesos_oculta, PO_final),
    lua_get(pesos_saida, PS_final),
    feedforward_oculta_lua(Entrada, PO_final, SO_final, _),
    SomaF is dot(SO_final, PS_final),
    SaidaFinal is sigmoide(SomaF),
    format('Output after training: ~f~n', [SaidaFinal]),
    format('Final error: ~f~n', [SD - SaidaFinal]).

% --- Hybrid Training ---
treinar_epocas_lua(0, _, _, _, _, _) :- !.
treinar_epocas_lua(N, Entrada, SD, NEx, LR, Epoca) :-
    N > 0, N1 is N - 1,
    treinar_exemplos_lua(NEx, Entrada, SD, LR),
    (Epoca mod 100 =:= 0 ->
        statistics(heap, H), MB is H/1024/1024,
        format('Epoch ~w - Heap: ~2f MB~n', [Epoca, MB]) ; true),
    EpNext is Epoca + 1,
    !, treinar_epocas_lua(N1, Entrada, SD, NEx, LR, EpNext).

treinar_exemplos_lua(0, _, _, _) :- !.
treinar_exemplos_lua(N, Entrada, SD, LR) :-
    N > 0, N1 is N - 1,
    
    % O(1) Access
    lua_get(pesos_oculta, PO),
    lua_get(pesos_saida, PS),

    % Feedforward (Numbers via is/2, safe and fast)
    feedforward_oculta_lua(Entrada, PO, SO, Somas),
    SomaS is dot(SO, PS),
    Saida is sigmoide(SomaS),
    
    Erro is SD - Saida,
    GradOut is Erro * derivada_sigmoide(SomaS),
    
    % Weight Update (Lists via lua_call/3, safe and stable)
    lua_call(update_weights, [PS, SO, GradOut, LR], PS_n),
    atualizar_oculta_lua(PO, Somas, PS, GradOut, Entrada, LR, PO_n),

    lua_set(pesos_saida, PS_n),
    lua_set(pesos_oculta, PO_n),
    !, treinar_exemplos_lua(N1, Entrada, SD, LR).

% --- Hidden Layer ---
feedforward_oculta_lua(Entrada, Pesos, Saidas, Somas) :-
    feedforward_oculta_aux(Entrada, Pesos, [], Saidas, [], Somas).

feedforward_oculta_aux(_, [], S_Acc, Saidas, Soma_Acc, Somas) :-
    reverse(S_Acc, Saidas), reverse(Soma_Acc, Somas).
feedforward_oculta_aux(Entrada, [P|Ps], S_Acc, Saidas, Soma_Acc, Somas) :-
    Soma is dot(Entrada, P),
    S is sigmoide(Soma),
    feedforward_oculta_aux(Entrada, Ps, [S|S_Acc], Saidas, [Soma|Soma_Acc], Somas).

atualizar_oculta_lua(PO, Somas, PS, GOut, Entrada, LR, PO_n) :-
    atualizar_oculta_aux(PO, Somas, PS, GOut, Entrada, LR, [], PO_rev),
    reverse(PO_rev, PO_n).

atualizar_oculta_aux([], [], [], _, _, _, Acc, Acc).
atualizar_oculta_aux([P|Ps], [Soma|Ss], [WOut|WOuts], GOut, Entrada, LR, Acc, PO_n) :-
    G_local is GOut * WOut * derivada_sigmoide(Soma),
    lua_call(update_weights, [P, Entrada, G_local, LR], P_n),
    atualizar_oculta_aux(Ps, Ss, WOuts, GOut, Entrada, LR, [P_n|Acc], PO_n).

% --- Helpers ---
gera_pesos_oculta_aleatorios(0, []) :- !.
gera_pesos_oculta_aleatorios(N, [P|Ps]) :-
    N > 0, N1 is N - 1,
    gera_pesos_aleatorios(15, P),
    gera_pesos_oculta_aleatorios(N1, Ps).

gera_pesos_saida_aleatorios(HSize, P) :-
    gera_pesos_aleatorios(HSize, P).

gera_pesos_aleatorios(0, []) :- !.
gera_pesos_aleatorios(N, [W|Ws]) :-
    N > 0, N1 is N - 1,
    W is (random(1000) / 5000) - 0.1,
    gera_pesos_aleatorios(N1, Ws).

dot([], [], 0).
dot([H1|T1], [H2|T2], Res) :-
    dot(T1, T2, R1),
    Res is R1 + (H1 * H2).

sigmoide(X, Res) :- Res is 1 / (1 + exp(-X)).
sigmoide(X) :- Res is 1 / (1 + exp(-X)), return(Res).

derivada_sigmoide(X, Res) :-
    S is 1 / (1 + exp(-X)),
    Res is S * (1 - S).
