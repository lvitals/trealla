mark_nn :-
    Epocas = 1500,
    HSize = 32,
    NEx = 20,
    LearningRate = 0.01,

    writeln('=== BENCHMARK REDE NEURAL PESADA (BLACKBOARD) - TREALLA PROLOG ==='),
    format('Épocas : ~w~n', [Epocas]),
    format('Neurônios ocultos: ~w~n', [HSize]),
    format('Exemplos/época : ~w~n', [NEx]),
    format('Taxa aprendizado : ~w~n', [LearningRate]),
    nl,

    % Inicialização no Blackboard
    gera_pesos_oculta_aleatorios(HSize, PO_inicial),
    gera_pesos_saida_aleatorios(HSize, PS_inicial),
    bb_put(pesos_oculta, PO_inicial),
    bb_put(pesos_saida, PS_inicial),

    Entrada = [0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,0.9,0.8,0.7,0.6,0.5],
    SaidaDesejada = 1,

    % Erro Inicial
    feedforward_oculta(Entrada, PO_inicial, SO_i, _),
    feedforward_saida(SO_i, PS_inicial, SaidaInicial, _),
    ErroInicial is SaidaDesejada - SaidaInicial,
    format('Saída inicial: ~f~n', [SaidaInicial]),
    format('Erro inicial : ~f~n', [ErroInicial]), nl,

    get_time(T0),
    statistics(cputime, Cpu0),

    writeln('Iniciando treinamento otimizado com Blackboard...'), nl,

    treinar_epocas_bb(Epocas, Entrada, SaidaDesejada, NEx, LearningRate, 1),

    get_time(T1),
    statistics(cputime, Cpu1),

    Tempo is T1 - T0,
    Cpu is Cpu1 - Cpu0,

    writeln('\n=== RESULTADOS DO BENCHMARK ==='),
    format('Tempo total: ~f~n', [Tempo]),
    format('CPU: ~f~n', [Cpu]),

    bb_get(pesos_oculta, PO_final),
    bb_get(pesos_saida, PS_final),

    length(PO_final, Num_neurons),
    format('Neurônios ocultos: ~w~n', [Num_neurons]),

    % Validação final
    feedforward_oculta(Entrada, PO_final, SO_final, _),
    feedforward_saida(SO_final, PS_final, SaidaFinal, _),
    format('Saída após treino: ~f~n', [SaidaFinal]),
    ErroFinal is SaidaDesejada - SaidaFinal,
    format('Erro final: ~f~n', [ErroFinal]).

% =========================
% TREINAMENTO (BLACKBOARD)
% =========================

treinar_epocas_bb(0, _, _, _, _, _) :- !.
treinar_epocas_bb(N, Entrada, SD, NEx, LR, Epoca) :-
    N > 0,
    N1 is N - 1,
    treinar_exemplos_bb(NEx, Entrada, SD, LR),
    (Epoca mod 100 =:= 0 ->
        statistics(heap, Heap),
        MB is Heap / 1024 / 1024,
        format('Epoca ~w - Heap: ~2f MB~n', [Epoca, MB])
    ; true),
    Ep2 is Epoca + 1,
    !,
    treinar_epocas_bb(N1, Entrada, SD, NEx, LR, Ep2).

treinar_exemplos_bb(0, _, _, _) :- !.
treinar_exemplos_bb(N, Entrada, SD, LR) :-
    N > 0,
    N1 is N - 1,
    
    % Busca pesos do Blackboard
    bb_get(pesos_oculta, PO),
    bb_get(pesos_saida, PS),

    feedforward_oculta(Entrada, PO, SO, Somas),
    feedforward_saida(SO, PS, Saida, SomaSaida),
    
    Erro is SD - Saida,
    derivada_sigmoide(SomaSaida, DOut),
    GradOut is Erro * DOut,
    
    atualizar_pesos(PS, SO, PS_n, GradOut, LR),
    atualizar_oculta(PO, Somas, PS, GradOut, Entrada, LR, PO_n),

    % Atualização mutável no Blackboard
    bb_update(pesos_saida, _, PS_n),
    bb_update(pesos_oculta, _, PO_n),
    
    !,
    treinar_exemplos_bb(N1, Entrada, SD, LR).

% =========================
% RANDOM (TAIL RECURSIVE)
% =========================

gera_lista_aleatoria(N, L) :- gera_lista_aleatoria(N, [], L).
gera_lista_aleatoria(0, L, L) :- !.
gera_lista_aleatoria(N, Acc, L) :-
    N > 0, N1 is N - 1,
    random(R), X is (R * 2) - 1,
    gera_lista_aleatoria(N1, [X|Acc], L).

gera_pesos_oculta_aleatorios(N, L) :- gera_pesos_oculta_aleatorios(N, [], L).
gera_pesos_oculta_aleatorios(0, L, L) :- !.
gera_pesos_oculta_aleatorios(N, Acc, L) :-
    N > 0, N1 is N - 1,
    gera_lista_aleatoria(15, P),
    gera_pesos_oculta_aleatorios(N1, [P|Acc], L).

gera_pesos_saida_aleatorios(HSize, Ps) :- gera_lista_aleatoria(HSize, Ps).

% =========================
% MATH
% =========================

sigmoide(X, Y) :- Y is 1 / (1 + exp(-X)).
derivada_sigmoide(X, D) :- sigmoide(X, S), D is S * (1 - S).

dot(L1, L2, S) :- dot(L1, L2, 0, S).
dot([], [], S, S) :- !.
dot([X|Xs], [W|Ws], Acc, S) :- Acc1 is Acc + X*W, dot(Xs, Ws, Acc1, S).

feedforward_oculta(Entradas, Pesos, Saidas, Somas) :-
    feedforward_oculta(Entradas, Pesos, [], Saidas_rev, [], Somas_rev),
    reverse(Saidas_rev, Saidas),
    reverse(Somas_rev, Somas).
feedforward_oculta(_, [], S_Acc, S_Acc, Soma_Acc, Soma_Acc) :- !.
feedforward_oculta(Entradas, [P|Ps], S_Acc, Saidas, Soma_Acc, Somas) :-
    dot(Entradas, P, Soma), sigmoide(Soma, S),
    feedforward_oculta(Entradas, Ps, [S|S_Acc], Saidas, [Soma|Soma_Acc], Somas).

feedforward_saida(SO, PS, Saida, Soma) :- dot(SO, PS, Soma), sigmoide(Soma, Saida).

% =========================
% UPDATE (TAIL RECURSIVE)
% =========================

atualizar_pesos(W, X, NW, G, LR) :-
    atualizar_pesos(W, X, [], NW_rev, G, LR),
    reverse(NW_rev, NW).
atualizar_pesos([], [], NW, NW, _, _) :- !.
atualizar_pesos([W|Ws], [X|Xs], Acc, NW, G, LR) :-
    W_n is W + LR * G * X,
    atualizar_pesos(Ws, Xs, [W_n|Acc], NW, G, LR).

atualizar_oculta(PO, Somas, PS, GOut, Entrada, LR, PO_n) :-
    atualizar_oculta(PO, Somas, PS, GOut, Entrada, LR, [], PO_n_rev),
    reverse(PO_n_rev, PO_n).
atualizar_oculta([], [], [], _, _, _, PO_n, PO_n) :- !.
atualizar_oculta([P|Ps], [Soma|Ss], [WOut|WOuts], GOut, Entrada, LR, Acc, PO_n) :-
    derivada_sigmoide(Soma, D),
    G_local is GOut * WOut * D,
    atualizar_camada_oculta_pesos(P, Entrada, G_local, LR, P_n),
    atualizar_oculta(Ps, Ss, WOuts, GOut, Entrada, LR, [P_n|Acc], PO_n).

atualizar_camada_oculta_pesos(W, X, G, LR, NW) :-
    atualizar_camada_oculta_pesos(W, X, G, LR, [], NW_rev),
    reverse(NW_rev, NW).
atualizar_camada_oculta_pesos([], [], _, _, NW, NW) :- !.
atualizar_camada_oculta_pesos([W|Ws], [X|Xs], G, LR, Acc, NW) :-
    W_n is W + LR * G * X,
    atualizar_camada_oculta_pesos(Ws, Xs, G, LR, [W_n|Acc], NW).
