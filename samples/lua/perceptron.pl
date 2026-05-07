% Perceptron with Multiple Epochs - Trealla Prolog
% Simplified implementation for educational and benchmarking purposes.

:- use_module(library(lists)).

% Activation function (Step function)
activation(X, 1) :- X > 0, !.
activation(_, 0).

% Prediction (dot product + activation)
predict([X1, X2], [W1, W2, B], Output) :-
    Sum is (X1 * W1) + (X2 * W2) + B,
    activation(Sum, Output).

% Weight update (W = W + LR * (Target - Output) * Input)
update_weights(Target, Output, LR, [X1, X2], [W1, W2, B], [NW1, NW2, NB]) :-
    Error is Target - Output,
    NW1 is W1 + LR * Error * X1,
    NW2 is W2 + LR * Error * X2,
    NB is B + LR * Error.

% Dataset (AND Gate)
dataset([0,0], 0).
dataset([0,1], 0).
dataset([1,0], 0).
dataset([1,1], 1).

% Training with multiple epochs
train(Epocas, PesosFinais) :-
    LR = 0.1,
    PesosIniciais = [0.0, 0.0, 0.0],   % Starting from zero (better to see evolution)
    epoch_loop(Epocas, PesosIniciais, LR, PesosFinais),
    write('Final weights after '), write(Epocas), write(' epochs: '), writeln(PesosFinais),
    test_all(PesosFinais).

% Epoch loop
epoch_loop(0, W, _, W) :- !.
epoch_loop(N, W, LR, FinalW) :-
    one_epoch(W, LR, NW),
    N1 is N - 1,
    epoch_loop(N1, NW, LR, FinalW).

% One full epoch (the 4 AND examples)
one_epoch(W0, LR, W_final) :-
    dataset(In1, T1), predict(In1, W0, O1), update_weights(T1, O1, LR, In1, W0, W1),
    dataset(In2, T2), predict(In2, W1, O2), update_weights(T2, O2, LR, In2, W1, W2),
    dataset(In3, T3), predict(In3, W2, O3), update_weights(T3, O3, LR, In3, W2, W3),
    dataset(In4, T4), predict(In4, W3, O4), update_weights(T4, O4, LR, In4, W3, W_final).

% Testing
test_all(W) :-
    format('Testing results:~n', []),
    test_one([0,0], W),
    test_one([0,1], W),
    test_one([1,0], W),
    test_one([1,1], W).

test_one(In, W) :-
    predict(In, W, Out),
    format('Input: ~w -> Output: ~w~n', [In, Out]).
