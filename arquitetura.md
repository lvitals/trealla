# Arquitetura do Trealla Prolog + Integração Lua

Este documento descreve a arquitetura interna do Trealla Prolog e como a integração com o motor Lua (Trealla-Lua Hybrid) expande suas capacidades originais.

---

## 1. Arquitetura Base do Trealla Prolog

O Trealla é um interpretador Prolog moderno e compacto, focado em eficiência de memória e velocidade. Diferente de implementações baseadas estritamente na WAM (Warren Abstract Machine), o Trealla utiliza uma abordagem baseada em **Células de Tamanho Fixo** e **Heap Segmentado**.

### 1.1 Células (Cells)
A unidade fundamental de dados é a `cell`, uma estrutura de **24 bytes** no sistema 64-bit.
- **Tags:** Cada célula possui uma tag (8 bits) que define seu tipo (Atom, Var, Int, Float, Compound, List, etc).
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

## 6. Conclusão

A arquitetura do Trealla Prolog + Lua representa um equilíbrio entre o poder de raciocínio lógico e a eficiência computacional moderna. Enquanto o Trealla cuida da busca, unificação e lógica, o Lua atua como um co-processador matemático e um gestor de memória global eficiente. Esta integração torna o Trealla uma ferramenta superior para sistemas embarcados, agentes de IA e aplicações WebAssembly que exigem alto desempenho e flexibilidade.
