% =============================================
% Rede Neural para XOR em Prolog (Trealla)
% Versão Final - com pesos que funcionam melhor
% =============================================

ativacao(S, 1) :- S >= 0, !.
ativacao(_, 0).

dot([], [], 0).
dot([X|Xs], [W|Ws], S) :-
    dot(Xs, Ws, R),
    S is X*W + R.

% Predição
prever([X1,X2,Bias], PO1, PO2, PS, Saida) :-
    dot([X1,X2,Bias], PO1, S1), ativacao(S1, H1),
    dot([X1,X2,Bias], PO2, S2), ativacao(S2, H2),
    dot([H1, H2, Bias], PS, S3),
    ativacao(S3, Saida).

% Demonstração com pesos ajustados manualmente
treinar_xor :-
    writeln('=== Rede Neural Simples - Tentativa de aprender XOR ==='),
    writeln('Obs: Em Prolog puro é muito difícil treinar XOR com função degrau.'),

    % Pesos ajustados manualmente (melhor que a anterior)
    PO1 = [ 1.0,  1.0, -1.5],   % Neurônio oculto 1
    PO2 = [ 1.0,  1.0, -0.5],   % Neurônio oculto 2
    PS  = [-1.0,  1.0,  0.5],   % Camada de saída

    writeln('\nResultados:'),
    prever([0,0,1], PO1, PO2, PS, S1), write('0 XOR 0 = '), writeln(S1),
    prever([0,1,1], PO1, PO2, PS, S2), write('0 XOR 1 = '), writeln(S2),
    prever([1,0,1], PO1, PO2, PS, S3), write('1 XOR 0 = '), writeln(S3),
    prever([1,1,1], PO1, PO2, PS, S4), write('1 XOR 1 = '), writeln(S4),

    writeln('\nConclusão:'),
    writeln('O Perceptron simples (1 camada) NÃO consegue aprender XOR.'),
    writeln('Precisamos de pelo menos 1 camada oculta + bom treinamento.'),
    writeln('Em Prolog puro fica muito complicado e lento fazer backpropagation completo.').
