-- Trealla Prolog Integration Example for LuaJIT
-- This example uses FFI to call the shared library (libtrealla.so)

local ffi = require("ffi")

-- Load the Trealla shared library
local lib = ffi.load("../../libtrealla.so")

-- Define the Trealla C API
ffi.cdef[[
    typedef struct prolog_ prolog;
    
    prolog *pl_create();
    void pl_destroy(prolog*);
    bool pl_eval(prolog*, const char *expr, bool interactive);
]]

print("--- Trealla Prolog FFI Example (LuaJIT) ---")

-- Initialize the engine
local pl = lib.pl_create()

-- Execute code
print("Calling Prolog from LuaJIT via FFI:")
lib.pl_eval(pl, "write('Hello from LuaJIT FFI!'), nl.", false)

-- Calculations also work through the hybrid motor
print("\nCalculating Fibonacci via Prolog engine:")
lib.pl_eval(pl, "X is fib(40), write('Result: '), write(X), nl.", false)

-- Cleanup
print("\nCleaning up...")
lib.pl_destroy(pl)
print("Test completed.")
