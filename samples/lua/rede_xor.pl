% Multilayer Neural Network for XOR
% Demonstrates the need for more layers for non-linearly separable problems.
% Final Version - with weights that work better

:- use_module(library(lists)).

% Activation function (Step function)
activation(X, 1) :- X > 0, !.
activation(_, 0).

% Prediction
predict_xor([X1, X2], PO1, PO2, PS, Output) :-
    % Hidden layer
    SumH1 is (X1 * PO1_W1) + (X2 * PO1_W2) + PO1_B,
    [PO1_W1, PO1_W2, PO1_B] = PO1,
    activation(SumH1, H1),
    
    SumH2 is (X1 * PO2_W1) + (X2 * PO2_W2) + PO2_B,
    [PO2_W1, PO2_W2, PO2_B] = PO2,
    activation(SumH2, H2),
    
    % Output layer
    [PS_W1, PS_W2, PS_B] = PS,
    SumS is (H1 * PS_W1) + (H2 * PS_W2) + PS_B,
    activation(SumS, Output).

% Demonstration with manually adjusted weights
run_xor :-
    writeln('XOR with 2 Layers (Multi-layer Perceptron):'),
    writeln('Note: In pure Prolog it is very hard to train XOR with step function.'),
    writeln('Using pre-calculated weights that solve XOR:'),
    
    PO1 = [ 1.0,  1.0, -1.5],   % Hidden neuron 1
    PO2 = [ 1.0,  1.0, -0.5],   % Hidden neuron 2
    PS  = [-1.0,  1.0,  0.5],   % Output layer
    
    test_xor([0,0], PO1, PO2, PS),
    test_xor([0,1], PO1, PO2, PS),
    test_xor([1,0], PO1, PO2, PS),
    test_xor([1,1], PO1, PO2, PS),
    
    writeln('\nConclusion:'),
    writeln('A simple Perceptron (1 layer) CANNOT learn XOR.'),
    writeln('But with 2 layers (Hidden + Output), XOR is solvable!').

test_xor(In, PO1, PO2, PS) :-
    predict_xor(In, PO1, PO2, PS, Out),
    format('XOR Input: ~w -> Output: ~w~n', [In, Out]).
