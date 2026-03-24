-- Interactive AI Butler: Trealla Prolog (Brain) + Lua (Memory & IO)
package.cpath = package.cpath .. ";../../?.so"
local trealla = require("trealla")

print("\n====================================================")
print("   Welcome, Master. I am your AI Butler.")
print("   (Type 'exit' to quit)")
print("====================================================\n")

-- 1. Setup Prolog
local pl = trealla.create()
pl:eval("consult('ai_butler.pl')")

-- 2. Initial World State in Lua
pl:eval("lua_set(light, off)")
pl:eval("lua_set(heater, off)")
pl:eval("lua_set(ac, off)")

-- 3. Interactive Loop
while true do
    io.write("You: ")
    local input = io.read()
    
    if not input or input == "exit" then break end
    if input == "" then goto continue end

    -- Clean input: remove trailing spaces/newlines and dots
    input = input:gsub("[%.]", ""):lower()

    -- Send to Prolog Brain
    -- We use a formatted query to call interpret/2
    local query = string.format("interpret('%s', R), write(R), nl.", input)
    
    io.write("Butler: ")
    local ok = pl:eval(query)
    
    if not ok then
        print("I had a logical error processing that. Please try again.")
    end

    ::continue::
end

pl:destroy()
print("\nGoodbye, Master. Have a pleasant day.")
