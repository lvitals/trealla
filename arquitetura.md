# Arquitetura do Trealla Prolog + Integração Lua

Este documento descreve a arquitetura interna do Trealla Prolog e como a integração com o motor Lua (Trealla-Lua Hybrid) expande suas capacidades originais.

---

## 1. Arquitetura Base do Trealla Prolog

O Trealla é um interpretador Prolog moderno e compacto, focado em eficiência de memória e velocidade. Diferente de implementações baseadas estritamente na WAM (Warren Abstract Machine), o Trealla utiliza uma abordagem baseada em **Células de Tamanho Fixo** e **Heap Segmentado**.

### 1.1 Células (Cells)
A unidade fundamental de dados é a `cell`, uma estrutura de **24 bytes** no sistema 64-bit.
- **Tags:** Cada célula possui uma tag (8 bits) que define seu tipo. Abaixo está a lista completa de tags extraída de `src/internal.h`:

| Tag | Valor | Descrição |
| :--- | :---: | :--- |
| **`TAG_EMPTY`** | 0 | Célula vazia ou não inicializada (usada em slots e trail). |
| **`TAG_VAR`** | 1 | Variável Prolog (referência com contexto ou anônima). |
| **`TAG_INTERNED`** | 2 | Átomo ou Functor internado na tabela de símbolos global. |
| **`TAG_CSTR`** | 3 | String pequena (Small String) otimizada diretamente na célula (< 16 bytes). |
| **`TAG_INT`** | 4 | Inteiro de 64 bits (inclui BigInt via `FLAG_INT_BIG`). |
| **`TAG_FLOAT`** | 5 | Número de ponto flutuante de precisão dupla (64-bit double). |
| **`TAG_RATIONAL`** | 6 | Número racional (aritmética de alta precisão). |
| **`TAG_INDIRECT`** | 7 | Ponteiro indireto para outro termo (evita cópias profundas). |
| **`TAG_BLOB`** | 8 | Objeto binário opaco ou string longa (ref-counted ou estática). |
| **`TAG_DBID`** | 9 | Identificador interno de banco de dados (referência a cláusula). |
| **`TAG_KVID`** | 10 | Identificador de par Chave-Valor (usado em Skip Lists/Maps). |
| **`TAG_END`** | 11 | Marcador de fim de termo ou sentinela. |

#### Observações Técnicas:
- **Termos Compostos:** Um termo composto (Compound) é definido por uma `TAG_INTERNED` com `arity > 0`.
- **Listas:** Uma lista é um caso especial de composto (`TAG_INTERNED`, `arity == 2`) onde o functor aponta para o símbolo `.`.
- **Referências:** Se uma `TAG_VAR` possui o flag `FLAG_VAR_REF` setado, ela atua como uma referência a um slot em um contexto específico (`val_ctx`).
- **Despacho Rápido:** O motor utiliza uma tabela de despacho (`g_disp`) indexada por estas tags para executar unificação e comparação sem condicionais complexas.
- **Num_cells:** Em termos compostos ou listas, a primeira célula indica quantas células subsequentes formam o termo completo, permitindo uma travessia linear rápida.
- **Interning:** Átomos e functores são "internados" em uma tabela de símbolos global, permitindo que a unificação seja uma simples comparação de ponteiros/offsets.

### 1.2 Gerenciamento de Memória
- **Heap Segmentado:** O Heap é alocado em páginas de tamanho variável ligadas em lista. Termos complexos são construídos em um `tmp_heap` (scratchpad) antes de serem copiados para uma página definitiva, garantindo contiguidade.
- **Blackboard:** Um sistema de armazenamento global baseado em Skip Lists, permitindo compartilhar dados entre diferentes queries ou threads de forma eficiente.

### 1.3 Skip Lists: A Estrutura de Indexação Central
O Trealla utiliza **Skip Lists** (`src/skiplist.c`) como sua principal estrutura de dados para armazenamento indexado e persistente. Elas são fundamentais para a Blackboard, o mapeamento de predicados em módulos e a gestão de atributos.

#### Detalhes Técnicos:
- **Estrutura de William Pugh:** A implementação segue o design clássico com `MAX_LEVELS` fixado em **16** (ideal para até $2^{16}$ elementos) e uma probabilidade de subida de nível de **0.5**.
- **Mecanismos de Concorrência:** Cada lista possui um `lock guard` (mutex/fastlock) para garantir a atomicidade de escritas.
- **Iteradores Estáveis:** Implementa `sliter`, que permite percorrer a lista mesmo enquanto outras threads realizam modificações, garantindo backtracking seguro.

#### Comparação de Performance: Skip List vs Lua Store
| Recurso | Trealla Skip List (Blackboard) | Lua Table (Lua Store) |
| :--- | :--- | :--- |
| **Complexidade** | $O(\log N)$ | $O(1)$ médio |
| **Pior Caso** | $O(N)$ (raro) | $O(N)$ (colisão extrema) |
| **Ordenação** | **Mantém as chaves ordenadas** | Não ordenada |
| **Uso de Memória** | Gerenciada manualmente (C) | Gerenciada por **GC** (Lua) |

---

## 2. Arquitetura da Integração Lua (Hybrid Motor)

A integração do Lua no Trealla não é um simples "wrapper", mas uma fusão arquitetônica onde os dois motores compartilham o ciclo de vida da execução.

### 2.1 O Virtual Machine Híbrido
Quando compilado com `USE_LUA=1`, o objeto `query` do Trealla ganha acesso a um `lua_State` persistente ou sob demanda.
- **Conversão de Tipos Transparente:**
    - **Prolog -> Lua:** Termos Prolog são convertidos em tabelas Lua. Listas viram arrays indexados (1..N). Termos compostos `f(a,b)` viram tabelas `{ [0]="f", [1]="a", [2]="b" }`.
    - **Lua -> Prolog:** Tabelas Lua são reconstruídas no `tmp_heap` do Prolog, preservando a estrutura de listas e functores.

### 2.2 Transparent Arithmetic (Aritmética Híbrida)
O avaliador de expressões do Prolog (`is/2`, `>/2`) foi modificado para que, caso encontre uma função desconhecida (ex: `X is my_func(10)`), ele despache a chamada para o VM Lua.
- **Vantagem:** Permite usar a biblioteca matemática do Lua (ou LuaJIT) e bibliotecas C via FFI de dentro de expressões Prolog sem predicados auxiliares.

### 2.3 Global Backing Store (Lua Store)
Além da Blackboard nativa, o Trealla oferece `lua_set/2` e `lua_get/2`.
- **Performance:** Utiliza a implementação de Tabelas Hash do Lua em C ($O(1)$). Em testes de estresse com 500.000 iterações, o **Lua Store superou a Blackboard nativa** ($O(1)$ vs $O(\log N)$), sendo aproximadamente 2.7x mais rápido.
- **Persistência de Módulo:** Chaves são qualificadas por módulo (ex: `user:minha_chave`), garantindo isolamento.

---

## 3. Vantagens do Modelo Híbrido

### 3.1 Delegação de Garbage Collection (GC) e Eficiência de Heap
Uma das maiores vantagens arquitetônicas é o **GC Churn Reduction**. 
- No Prolog puro, o estado global dinâmico (`assert/retract`) pode fragmentar o banco de dados e exigir compactação.
- Ao usar o **Lua Store**, o Trealla delega a gestão de memória de objetos globais para o **GC Incremental do Lua**.
- **Resultado de Benchmark:** Em um teste de 500.000 atualizações de estado, o **Heap do Trealla permaneceu em 0.00 MB**. Isso prova que o motor Prolog permanece "leve", pois o Lua gerencia e limpa os termos temporários de forma autônoma, eliminando a degradação de performance em execuções de longa duração.

### 3.2 State Visit: Detecção de Duplicatas em Larga Escala
O predicado `state_visit/1` oferece uma vantagem arquitetônica crucial para algoritmos de busca (IA, roteamento, grafos):
- **Velocidade:** Capaz de processar **1.000.000 de termos por segundo**.
- **Sem Poluição:** Permite marcar estados visitados sem utilizar `assert/1`, o que evita "sujar" o banco de dados do Prolog com fatos temporários que precisariam ser limpos via `retractall/1`.
- **Uso:** Ideal para DFS/BFS em grafos gigantes onde a detecção de ciclos e duplicatas é necessária, mas a persistência no Prolog é indesejada.

### 3.3 Performance em Computação Numérica
O Prolog é inerentemente lento para loops aritméticos pesados devido à recursão e criação de frames de escolha.
- **Offloading:** Ao mover o cálculo vetorial (ex: Produto Escalar em Redes Neurais) para funções Lua, o ganho de performance chega a **350%**.
- **LuaJIT:** Se habilitado, as partes críticas são compiladas para código de máquina nativo (JIT), tornando o Prolog competitivo com linguagens compiladas para tarefas matemáticas.

### 3.4 Interoperabilidade Moderna
Através do Lua, o Trealla ganha acesso imediato a:
- C Libraries via **FFI**.
- Gestão de JSON complexo.
- Protocolos de rede modernos disponíveis no ecossistema Lua.

---

## 4. Desvantagens e Limitações

1.  **Context Switching:** Existe um pequeno overhead ao cruzar a fronteira Prolog <-> Lua (serialização de termos). Para operações triviais, o Prolog puro é mais rápido.
2.  **Concorrência:** O VM Lua padrão não é thread-safe por design. O Trealla gerencia instâncias do Lua, mas o compartilhamento de dados entre threads via Lua exige travas (locks) ou instâncias separadas.
3.  **Complexidade de Tipos:** A unificação Prolog é mais rica que o sistema de tipos do Lua. Informações como atributos de variáveis ou restrições (CLP) não são traduzidas nativamente para Lua.

---

## 5. Análise de Resultados (Benchmarks)

| Cenário | Prolog Puro | Híbrido (Lua) | Ganho |
| :--- | :--- | :--- | :--- |
| **Rede Neural (1500 épocas)** | 35.2s | 10.1s | **~3.5x** |
| **Global State Update (500k)** | 0.60s (BB) | 0.22s (Lua) | **~2.7x** |
| **Fibonacci (40)** | ~2.5s | ~0.01s (JIT) | **>200x** |

---

## 7. Mapeamento da Biblioteca Padrão (Standard Library)

O Trealla Prolog vem acompanhado de uma robusta biblioteca padrão (`library/*.pl`), grande parte derivada de implementações consolidadas como SWI-Prolog e Scryer Prolog, focando em compatibilidade ISO e estruturas de dados avançadas.

Abaixo está o mapeamento arquitetônico dessas bibliotecas agrupadas por domínio:

### 7.1 Estruturas de Dados e Algoritmos
- **`lists.pl`**: Operações essenciais sobre listas (member, select, append, reverse, maplist, foldl). Otimizada para evitar stack overflows.
- **`assoc.pl` & `rbtrees.pl`**: Implementações de Dicionários / Árvores balanceadas (Red-Black trees) para buscas em tempo $O(\log N)$.
- **`ordsets.pl`**: Operações eficientes de conjuntos (união, interseção) usando listas ordenadas.
- **`pairs.pl`**: Manipulação de pares Chave-Valor (termos `Key-Value`), integrando perfeitamente com maplist e ordenação.
- **`heaps.pl`**: Filas de prioridade (min-heaps) úteis para algoritmos de roteamento como A* ou Dijkstra.
- **`ugraphs.pl`**: Representação de Grafos Não-direcionados, oferecendo fechamento transitivo e ordenação topológica.

### 7.2 Programação Lógica Avançada (Constraints e Reificação)
O Trealla brilha em sua implementação de lógicas avançadas que limitam o espaço de busca (backtracking):
- **`clpz.pl`**: CLP(Z) - *Constraint Logic Programming over Integers*. Escrita por Markus Triska (Scryer), permite resolver equações matemáticas declarativamente, de trás para frente.
- **`dif.pl` & `freeze.pl`**: *Coroutining* e restrições de desigualdade segura (`dif/2` garante que dois termos nunca se unifiquem).
- **`atts.pl`**: Variáveis atribuídas. O mecanismo de baixo nível que permite anexar metadados a variáveis Prolog (a fundação para CLP(Z) e `dif/2`).
- **`reif.pl`**: Lógica reificada (*Indexing dif/2*), essencial para evitar falhas silenciosas e escrever código Prolog reversível seguro.
- **`when.pl`**: Permite adiar a execução de um predicado até que certas condições sobre variáveis sejam atendidas (ex: até que X seja instanciado).

### 7.3 Entrada, Saída e Parsing (I/O & DCGs)
- **`dcgs.pl` & `pio.pl`**: *Definite Clause Grammars* e Pure I/O. Permite escrever parsers extremamente expressivos que leem e escrevem arquivos sem causar efeitos colaterais de estado (I/O preguiçoso e transparente).
- **`format.pl` & `charsio.pl`**: Predicados de formatação estilo printf (`format/2`) e conversão segura entre tipos primitivos e strings de caracteres.
- **`abnf.pl` & `json.pl`**: Parsers prontos para gramáticas formais (ABNF) e manipulação estruturada de JSON (geralmente usado em chamadas web/API).

### 7.4 Integração, Redes e Concorrência
O Trealla inclui bindings em C que expõem APIs do Sistema Operacional para o Prolog:
- **`concurrent.pl` & `threads.pl`**: Suporte a paralelismo real (`future/3`). Permite execução não bloqueante aproveitando CPUs multicore (depende de `USE_THREADS`).
- **`http.pl` & `sockets.pl` & `curl.pl`**: Stack completa para comunicação web. Inclui a capacidade de abrir sockets TCP de baixo nível, lançar um servidor HTTP básico, e fazer requisições via libcurl (`http_get`, `http_post`).
- **`sqlite3.pl`**: Integração nativa com bancos de dados SQLite, permitindo salvar o estado da aplicação de forma relacional.
- **`linda.pl`**: Implementação do modelo de concorrência "Linda" (*tuple spaces*), para comunicação entre processos/threads via mensagens globais de quadro negro (blackboard).
- **`raylib.pl`**: Demonstra a capacidade do Trealla (através de FFI ou C wrappers) de manipular janelas gráficas 2D/3D usando o motor Raylib.

---

## 8. Considerações Finais sobre a Arquitetura

O **Trealla Prolog** não é apenas um interpretador; é um motor lógico minimalista que adota a filosofia de "microkernel". Ele mantém o núcleo C pequeno (focado na máquina virtual, Células, e Skip Lists) enquanto delega funcionalidades ricas (como dicionários e HTTP) para a sua biblioteca padrão Prolog (`library/*.pl`).

Ao mesmo tempo, ao introduzir a **Camada Híbrida Lua**, o Trealla quebra a barreira computacional histórica do Prolog, permitindo que a mesma base de código acesse a velocidade de execução vetorial e o gerenciamento de memória em tempo real de um JIT compiler moderno. Esta fusão cria uma arquitetura robusta, capaz de atuar em microcontroladores, agentes autônomos e servidores web com concorrência real e estado seguro.
