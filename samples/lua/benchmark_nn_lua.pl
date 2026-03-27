% benchmark_nn_lua.pl - Versão Híbrida Estável (Trealla + Lua)
:- use_module(library(lists)).

% Carrega o acelerador Lua na inicialização
:- initialization(lua_eval('require("samples/lua/nn_accel")')).

mark_nn_lua :-
    Epocas = 1500, HSize = 32, NEx = 20, LR = 0.01,
    writeln('=== BENCHMARK REDE NEURAL (HYBRID STABLE) ==='),
    format('Épocas : ~w | Neurônios : ~w~n', [Epocas, HSize]),

    % Inicialização no Lua Store (O(1))
    gera_pesos_oculta_aleatorios(HSize, PO_inicial),
    gera_pesos_saida_aleatorios(HSize, PS_inicial),
    lua_set(pesos_oculta, PO_inicial),
    lua_set(pesos_saida, PS_inicial),

    Entrada = [0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,0.9,0.8,0.7,0.6,0.5],
    SD = 1.0,

    % Erro Inicial
    feedforward_oculta_lua(Entrada, PO_inicial, SO_i, _),
    SomaS is dot(SO_i, PS_inicial),
    SaidaInicial is sigmoide(SomaS),
    format('Saída inicial: ~f~n', [SaidaInicial]),

    get_time(T0),
    treinar_epocas_lua(Epocas, Entrada, SD, NEx, LR, 1),
    get_time(T1),

    Tempo is T1 - T0,
    writeln('\n=== RESULTADOS DO BENCHMARK (HYBRID) ==='),
    format('Tempo total: ~f~n', [Tempo]),
    
    lua_get(pesos_oculta, PO_final),
    lua_get(pesos_saida, PS_final),
    feedforward_oculta_lua(Entrada, PO_final, SO_final, _),
    SomaF is dot(SO_final, PS_final),
    SaidaFinal is sigmoide(SomaF),
    format('Saída após treino: ~f~n', [SaidaFinal]),
    format('Erro final: ~f~n', [SD - SaidaFinal]).

% --- Treinamento Híbrido ---
treinar_epocas_lua(0, _, _, _, _, _) :- !.
treinar_epocas_lua(N, Entrada, SD, NEx, LR, Epoca) :-
    N > 0, N1 is N - 1,
    treinar_exemplos_lua(NEx, Entrada, SD, LR),
    (Epoca mod 100 =:= 0 ->
        statistics(heap, H), MB is H/1024/1024,
        format('Epoca ~w - Heap: ~2f MB~n', [Epoca, MB]) ; true),
    EpNext is Epoca + 1,
    !, treinar_epocas_lua(N1, Entrada, SD, NEx, LR, EpNext).

treinar_exemplos_lua(0, _, _, _) :- !.
treinar_exemplos_lua(N, Entrada, SD, LR) :-
    N > 0, N1 is N - 1,
    
    % Acesso O(1)
    lua_get(pesos_oculta, PO),
    lua_get(pesos_saida, PS),

    % Feedforward (Números via is/2, seguro e rápido)
    feedforward_oculta_lua(Entrada, PO, SO, Somas),
    SomaS is dot(SO, PS),
    Saida is sigmoide(SomaS),
    
    Erro is SD - Saida,
    GradOut is Erro * derivada_sigmoide(SomaS),
    
    % Atualização de Pesos (Listas via lua_call/3, seguro e estável)
    lua_call(update_weights, [PS, SO, GradOut, LR], PS_n),
    atualizar_oculta_lua(PO, Somas, PS, GradOut, Entrada, LR, PO_n),

    lua_set(pesos_saida, PS_n),
    lua_set(pesos_oculta, PO_n),
    !, treinar_exemplos_lua(N1, Entrada, SD, LR).

% --- Camada Oculta ---
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

% --- Auxiliares ---
gera_pesos_oculta_aleatorios(0, []) :- !.
gera_pesos_oculta_aleatorios(N, [P|Ps]) :-
    N > 0, N1 is N - 1,
    gera_lista_aleatoria(15, P),
    gera_pesos_oculta_aleatorios(N1, Ps).

gera_pesos_saida_aleatorios(HSize, Ps) :- gera_lista_aleatoria(HSize, Ps).

gera_lista_aleatoria(N, L) :- gera_lista_aleatoria(N, [], L).
gera_lista_aleatoria(0, L, L) :- !.
gera_lista_aleatoria(N, Acc, L) :-
    N > 0, N1 is N - 1,
    random(R), X is (R * 2) - 1,
    gera_lista_aleatoria(N1, [X|Acc], L).
