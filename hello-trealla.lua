local trealla = require("trealla")

-- Inicializar o motor Prolog
local pl = trealla.create()

-- Executar código Prolog
pl:eval("write('Hello from Trealla inside Lua 5.4!'), nl")

-- Opcional: destruir manualmente ou esperar o GC do Lua
pl:destroy()
