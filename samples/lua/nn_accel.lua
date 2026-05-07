-- Stable Mathematical Accelerator

function update_weights(weights, inputs, gradient, lr)
    local new_weights = {}
    for i = 1, #weights do
        new_weights[i] = weights[i] + (lr * gradient * inputs[i])
    end
    return new_weights
end

function dot_product_layer(input, num_neurons)
    local results = {}
    for i = 1, num_neurons do
        local sum = 0
        for j = 1, #input do
            sum = sum + (input[j] * 0.5)
        end
        results[i] = sum
    end
    return results
end

function activate_layer(hidden)
    local results = {}
    for i = 1, #hidden do
        results[i] = 1 / (1 + math.exp(-hidden[i]))
    end
    return results
end
