% =============================================
% Perceptron com Múltiplas Épocas - Trealla Prolog
% =============================================

ativacao(Soma, 1) :- Soma >= 0, !.
ativacao(_, 0).

perceptron(Entradas, Pesos, Saida) :-
    dot_product(Entradas, Pesos, Soma),
    ativacao(Soma, Saida).

dot_product([], [], 0).
dot_product([X|Xs], [W|Ws], Soma) :-
    dot_product(Xs, Ws, SomaResto),
    Soma is X*W + SomaResto.

atualizar_pesos(Entradas, Pesos, SaidaDesejada, Taxa, NovosPesos) :-
    perceptron(Entradas, Pesos, SaidaObtida),
    Erro is SaidaDesejada - SaidaObtida,
    atualizar_lista(Entradas, Pesos, Erro, Taxa, NovosPesos).

atualizar_lista([], [], _, _, []).
atualizar_lista([X|Xs], [W|Ws], Erro, Taxa, [NovoW|NovosWs]) :-
    NovoW is W + Taxa*Erro*X,
    atualizar_lista(Xs, Ws, Erro, Taxa, NovosWs).

% =============================================
% Treinamento com várias épocas
% =============================================

treinar :-
    PesosIniciais = [0.0, 0.0, 0.0],   % Começando do zero (melhor para ver evolução)
    Taxa = 0.2,
    Epocas = 10,

    treinar_epocas(Epocas, PesosIniciais, Taxa, PesosFinais),
    write('Pesos finais após '), write(Epocas), write(' épocas: '), writeln(PesosFinais),
    testar(PesosFinais).

% Loop de épocas
treinar_epocas(0, Pesos, _, Pesos).
treinar_epocas(N, PesosAtual, Taxa, PesosFinais) :-
    N > 0,
    N1 is N - 1,
    treinar_uma_epoca(PesosAtual, Taxa, PesosNovo),
    treinar_epocas(N1, PesosNovo, Taxa, PesosFinais).

% Uma época completa (os 4 exemplos do AND)
treinar_uma_epoca(Pesos, Taxa, PesosFinal) :-
    atualizar_pesos([0,0,1], Pesos, 0, Taxa, P1),
    atualizar_pesos([0,1,1], P1,     0, Taxa, P2),
    atualizar_pesos([1,0,1], P2,     0, Taxa, P3),
    atualizar_pesos([1,1,1], P3,     1, Taxa, PesosFinal).

testar(Pesos) :-
    writeln('=== Testes AND ==='),
    perceptron([0,0,1], Pesos, S1), write('0 AND 0 = '), writeln(S1),
    perceptron([0,1,1], Pesos, S2), write('0 AND 1 = '), writeln(S2),
    perceptron([1,0,1], Pesos, S3), write('1 AND 0 = '), writeln(S3),
    perceptron([1,1,1], Pesos, S4), write('1 AND 1 = '), writeln(S4).
