-- Trealla Prolog Integration Example for Lua 5.4
-- This example uses the native C module (trealla.so)

-- Adjust C path to find trealla.so in the root directory
package.cpath = package.cpath .. ";../../?.so"

local trealla = require("trealla")

print("--- Trealla Prolog Native Module Example (Lua 5.4) ---")

-- Initialize the Prolog engine
local pl = trealla.create()

-- Execute a simple Prolog goal
print("Executing simple write:")
pl:eval("write('Hello from Prolog inside Lua!'), nl.")

-- Demonstrate transparent hybrid math (Prolog calling Lua back)
print("\nExecuting JIT-accelerated math via Prolog:")
pl:eval("X is fib(40), write('Fibonacci(40) result: '), write(X), nl.")

-- Demonstrate state management
print("\nTesting Lua-based state visit from Prolog:")
pl:eval("state_clear, (state_visit(my_term) -> write('First visit OK') ; write('Failed')), nl.")
pl:eval("(state_visit(my_term) -> write('Second visit (should fail)') ; write('Second visit blocked correctly')), nl.")

print("\nCleaning up...")
pl:destroy()
print("Test completed.")
