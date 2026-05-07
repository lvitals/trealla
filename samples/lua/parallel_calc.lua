function intensive_calc(id, n)
    -- Demonstra que cada thread tem sua própria VM e pode rodar loops pesados
    local result = 0
    for i = 1, n do
        result = result + math.sqrt(i) * math.sin(i)
    end
    print(string.format("[Lua Worker %s] Calculation finished.", id))
    return result
end
