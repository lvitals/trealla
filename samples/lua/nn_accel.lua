-- Acelerador Matemático Estável
function sigmoide(x)
    return 1 / (1 + math.exp(-x))
end

function derivada_sigmoide(x)
    local s = 1 / (1 + math.exp(-x))
    return s * (1 - s)
end

function dot(l1, l2)
    local s = 0
    local n = #l1
    for i = 1, n do
        s = s + (l1[i] * l2[i])
    end
    return s
end

-- Retorna a tabela diretamente para o lua_call
function update_weights(w, x, g, lr)
    local nw = {}
    local n = #w
    for i = 1, n do
        nw[i] = w[i] + lr * g * x[i]
    end
    return nw
end
