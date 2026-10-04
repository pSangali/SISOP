# Contagem paralela de objetos em matriz binária

Trabalho prático de Sistemas Operacionais (PUCRS, 2026/II) — contagem de objetos
(componentes conexos, **conectividade 8**) em uma matriz binária, com versão
sequencial e versão paralela com **Pthreads**, em **ANSI C (C89/C90)**.

**Autoria:** Pedro Marques Sangali, Gabriel Kirchmann Kondach, Thiago Farias dos Santos
**Código externo:** nenhum. O gerador de matrizes (`tests/gerar_matriz.py`) usa só a biblioteca padrão do Python.

## Estrutura

```
README.md
Makefile
src/conta-objetos-sequencial.c   versão sequencial (referência)
src/conta-objetos-paralelo.c     versão paralela (Pthreads)
tests/ex1..ex5.txt               5 matrizes obrigatórias do enunciado
tests/gerar_matriz.py            gerador de matrizes aleatórias
tests/rodar_testes.sh            testes de correção (seq x paralelo)
tests/benchmark.sh               medição de desempenho
results/                         resultados (testes e desempenho)
slides/apresentacao.pdf          slides
```

## Compilação e execução

Requer Linux/macOS, `cc`/`gcc`, `make`, `python3` (só para testes/benchmark).

```
make                 # compila em bin/ com -std=c89 -Wall -Wextra -pedantic -pthread
make test            # 5 matrizes obrigatórias + 180 comparações aleatórias (36 matrizes x 5 contagens de threads) seq x paralelo
make tsan            # ThreadSanitizer (detector de corridas de dados)
make bench           # gera matriz 6000x6000 e mede desempenho -> results/desempenho.csv
```

Execução manual:

```
bin/sequencial tests/ex3.txt
bin/paralelo   tests/ex3.txt 4      # 4 trabalhadores (threads)
```

Formato do arquivo: primeira linha `LINHAS COLUNAS`, depois a matriz de 0/1 separada por espaços.
Matriz própria: `python3 tests/gerar_matriz.py 1000 1000 0.4 7 > minha.txt`
(linhas, colunas, densidade, semente).

## Arquitetura

### Versão sequencial
Varre a matriz; ao achar célula `1` não visitada, executa **flood fill iterativo**
(pilha explícita, sem recursão, então não estoura a pilha em matrizes grandes) com os
8 vizinhos, marcando as células em um vetor `visitado`, e incrementa o contador.

### Versão paralela
1. **Decomposição por faixas de linhas.** A matriz é dividida em `p` faixas
   contíguas (diferença máxima de 1 linha entre faixas). `p` é o 2º argumento do
   programa; se `p` > nº de linhas, usa-se `p = linhas`.
   Das `p` faixas, a thread principal processa a faixa 0 e cria `p-1` threads para as demais.
2. **Rotulagem local (paralela).** Cada thread executa flood fill 8-conectado
   *restrito à sua faixa*, dando rótulos locais 1..k às suas componentes. Cada thread
   escreve só nas células da própria faixa e lê só a matriz (somente leitura), então
   **não há dados compartilhados escritos concorrentemente** e nenhum mutex é necessário.
   A sincronização é o `pthread_join`, que garante que todas terminaram (e que suas
   escritas são visíveis) antes da consolidação.
3. **Rótulos globais (sequencial).** `rótulo global = rótulo local + deslocamento da faixa`,
   com deslocamento = soma das contagens das faixas anteriores. Assim cada componente local tem ID único.
4. **Consolidação nas fronteiras (sequencial, O(colunas) por fronteira).** Para cada par de
   faixas vizinhas, compara-se a última linha da faixa `t` com a primeira da faixa `t+1`:
   cada célula `(i,j)` com `1` é unida (Union-Find) às células `1` em `(i+1, j-1)`, `(i+1, j)`
   e `(i+1, j+1)`, cobrindo vertical **e diagonais**.
5. **Resultado:** número de raízes do Union-Find = número de objetos.

**Por que somar contagens locais não basta:** um objeto que cruza uma fronteira seria
contado uma vez por faixa. O Union-Find funde esses rótulos em um só.

**Encontro de quatro blocos / diagonais (ex. 3):** com faixas de linhas só existem fronteiras
horizontais, e a diagonal é tratada pelos vizinhos `j±1` acima. Como o Union-Find é
transitivo, um objeto que atravessa 3 ou mais faixas (ex. 4 e 5) também é unificado.
Para blocos 2x2/3x3, o caso "encontro de 4 blocos" é equivalente a um objeto que cruza uma fronteira
na diagonal; as 5 matrizes foram testadas com 2, 3, 4 e 8 faixas, que inclui
cortes passando por cima de todos os objetos relevantes.

**Trechos paralelos × sequenciais:** a rotulagem (custo O(L·C), dominante) é paralela; os passos
3–5 são sequenciais e custam só O(C·p + nº de rótulos), desprezíveis perante a fase 2.

**Ausência de problemas de concorrência:** sem escrita compartilhada → sem condição de
corrida, sem atualização perdida e sem deadlock (não há locks). Verificado com
ThreadSanitizer (`make tsan`), sem avisos.

**Tratamento de erros:** verificação de `fopen/fscanf/malloc/calloc`, `pthread_create` e
`pthread_join`; em falha as threads criadas recebem join e toda a memória é liberada.

## Resultados

### Matrizes obrigatórias (`results/testes_obrigatorios.txt`)

| Ex | Dimensões | Esperado | Sequencial | Paralelo (2/3/4/8 threads) |
|----|-----------|----------|------------|----------------------------|
| 1  | 5x5       | 3        | 3          | 3 / 3 / 3 / 3 |
| 2  | 6x8       | 4        | 4          | 4 / 4 / 4 / 4 |
| 3  | 8x8       | 5        | 5          | 5 / 5 / 5 / 5 |
| 4  | 9x12      | 6        | 6          | 6 / 6 / 6 / 6 |
| 5  | 12x12     | 7        | 7          | 7 / 7 / 7 / 7 |

Além disso, 180 comparações com matrizes aleatórias (tamanhos, densidades e nº de threads variados) coincidem com o sequencial.

### Desempenho (`results/desempenho.csv`)

Matriz 6000x6000, densidade 0,40, semente 42 (567.486 objetos). CPU com 12 threads de hardware,
`-O2`. Valor representativo: **mediana de 7 execuções** (só o tempo de contagem; leitura do arquivo
fica fora). Aceleração S = T_seq / T_par.

| Versão     | Threads | Tempo (ms) | Aceleração |
|------------|---------|-----------:|-----------:|
| Sequencial | 1       | 772,6      | 1,00 |
| Paralela   | 1       | 799,8      | 0,97 |
| Paralela   | 2       | 457,2      | 1,69 |
| Paralela   | 4       | 293,8      | 2,63 |
| Paralela   | 6       | 230,8      | 3,35 |
| Paralela   | 8       | 202,8      | 3,81 |
| Paralela   | 12      | 166,1      | 4,65 |

**Análise.** A aceleração cresce com o nº de threads, mas fica abaixo da ideal (4,65x com 12).
Motivos: (i) a consolidação e a alocação do vetor de rótulos são sequenciais (Lei de Amdahl);
(ii) a faixa de cada thread varre memória de uma matriz de 144 MB, então o gargalo passa a ser a
largura de banda de memória, que não escala linearmente com os núcleos; (iii) os 12 "núcleos" podem
ser threads de hardware (SMT) que compartilham unidades de execução. Com 1 thread a versão paralela é
ligeiramente **mais lenta** (0,97x) que a sequencial: paga o vetor extra de rótulos e a consolidação sem
ganho de concorrência. Nas matrizes obrigatórias (≤144 células) a paralela também é mais lenta,
pois o custo de criar threads supera o trabalho útil; por isso há uma matriz grande para a medição.
Os números variam com a máquina; reproduza com `make bench`.
