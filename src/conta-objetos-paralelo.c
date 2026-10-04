#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

typedef struct {
    int linha;
    int coluna;
} Coordenada;

typedef struct {
    int id;                 
    const int *matriz;      
    int *rotulos;           
                                
    int linhas;
    int colunas;
    int linha_inicial;      
    int linha_final;        
    int contagem_local;     
    int erro;               /* 1 se a thread falhou (ex.: sem memoria) */
} ArgumentoThread;

static int *ler_matriz(const char *caminho, int *out_linhas, int *out_colunas)
{
    FILE *arquivo;
    int linhas, colunas, i, j;
    int *matriz;

    arquivo = fopen(caminho, "r");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", caminho);
        return NULL;
    }

    if (fscanf(arquivo, "%d %d", &linhas, &colunas) != 2) {
        fprintf(stderr, "Erro: cabecalho invalido em '%s' (esperado: linhas colunas)\n", caminho);
        fclose(arquivo);
        return NULL;
    }

    if (linhas <= 0 || colunas <= 0) {
        fprintf(stderr, "Erro: dimensoes invalidas (%d x %d)\n", linhas, colunas);
        fclose(arquivo);
        return NULL;
    }

    matriz = (int *) malloc((size_t) linhas * (size_t) colunas * sizeof(int));
    if (matriz == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para a matriz (%d x %d)\n", linhas, colunas);
        fclose(arquivo);
        return NULL;
    }

    for (i = 0; i < linhas; i++) {
        for (j = 0; j < colunas; j++) {
            int valor;

            if (fscanf(arquivo, "%d", &valor) != 1) {
                fprintf(stderr, "Erro: dados insuficientes na linha %d do arquivo '%s'\n", i, caminho);
                free(matriz);
                fclose(arquivo);
                return NULL;
            }
            if (valor != 0 && valor != 1) {
                fprintf(stderr, "Erro: valor invalido '%d' na posicao (%d,%d); esperado 0 ou 1\n", valor, i, j);
                free(matriz);
                fclose(arquivo);
                return NULL;
            }
            matriz[i * colunas + j] = valor;
        }
    }

    fclose(arquivo);
    *out_linhas = linhas;
    *out_colunas = colunas;
    return matriz;
}

/*
 * Flood fill iterativo restrito a faixa [linha_inicial_faixa, linha_final_faixa).
 * A pilha e' fornecida pelo chamador e REUTILIZADA entre componentes: alocar
 * uma pilha do tamanho da faixa a cada componente custava um malloc/free
 * (mmap/munmap) por objeto e, pior, serializava as threads no kernel.
 * Cada celula e' empilhada no maximo uma vez (marcada ao empilhar), logo
 * a capacidade "celulas da faixa" e' suficiente.
 */
static void flood_fill_faixa(const int *matriz, int *rotulos, int colunas,
                              int linha_inicial_faixa, int linha_final_faixa,
                              int linha_origem, int coluna_origem, int rotulo,
                              Coordenada *pilha)
{
    static const int delta_linha[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    static const int delta_coluna[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
    int topo;

    topo = 0;
    pilha[topo].linha = linha_origem;
    pilha[topo].coluna = coluna_origem;
    topo++;
    rotulos[linha_origem * colunas + coluna_origem] = rotulo;

    while (topo > 0) {
        Coordenada atual;
        int k;
        topo--;
        atual = pilha[topo];
        for (k = 0; k < 8; k++) {
            int nl = atual.linha + delta_linha[k];
            int nc = atual.coluna + delta_coluna[k];
            if (nl < linha_inicial_faixa || nl >= linha_final_faixa) {
                continue;
            }
            if (nc < 0 || nc >= colunas) {
                continue;
            }
            if (rotulos[nl * colunas + nc] != 0) {
                continue;
            }
            if (matriz[nl * colunas + nc] != 1) {
                continue;
            }

            rotulos[nl * colunas + nc] = rotulo;
            pilha[topo].linha = nl;
            pilha[topo].coluna = nc;
            topo++;
        }
    }
}

static void *trabalhador(void *arg_generico)
{
    ArgumentoThread *arg = (ArgumentoThread *) arg_generico;
    Coordenada *pilha;
    int i, j;
    int proximo_rotulo_local;

    /* Uma unica pilha por thread, alocada fora do laco de componentes. */
    pilha = (Coordenada *) malloc((size_t) (arg->linha_final - arg->linha_inicial) *
                                  (size_t) arg->colunas * sizeof(Coordenada));
    if (pilha == NULL) {
        arg->erro = 1;
        return NULL;
    }

    proximo_rotulo_local = 1;

    for (i = arg->linha_inicial; i < arg->linha_final; i++) {
        for (j = 0; j < arg->colunas; j++) {
            int pos = i * arg->colunas + j;

            if (arg->matriz[pos] == 1 && arg->rotulos[pos] == 0) {
                flood_fill_faixa(arg->matriz, arg->rotulos, arg->colunas,
                                  arg->linha_inicial, arg->linha_final,
                                  i, j, proximo_rotulo_local, pilha);
                proximo_rotulo_local++;
            }
        }
    }

    arg->contagem_local = proximo_rotulo_local - 1;
    free(pilha);
    return NULL;
}

static int uf_find(int *pai, int x)
{
    while (pai[x] != x) {
        pai[x] = pai[pai[x]];  
        x = pai[x];
    }
    return x;
}

static void uf_union(int *pai, int *tamanho, int a, int b)
{
    int raiz_a, raiz_b;

    raiz_a = uf_find(pai, a);
    raiz_b = uf_find(pai, b);

    if (raiz_a == raiz_b) {
        return;
    }
    if (tamanho[raiz_a] < tamanho[raiz_b]) {
        pai[raiz_a] = raiz_b;
        tamanho[raiz_b] += tamanho[raiz_a];
    } else {
        pai[raiz_b] = raiz_a;
        tamanho[raiz_a] += tamanho[raiz_b];
    }
}

/*
 * Retorna o numero de objetos, ou -1 em caso de erro.
 *
 * Fases:
 *   1. (PARALELA)   cada thread rotula os componentes da sua faixa com
 *                   rotulos locais 1..contagem_local.
 *   2. (sequencial) deslocamento = soma das contagens das faixas anteriores;
 *                   rotulo global = rotulo local + deslocamento[faixa].
 *                   NAO reescrevemos a matriz de rotulos: o deslocamento e'
 *                   aplicado so' nas celulas de fronteira, quando lidas.
 *   3. (sequencial) Union-Find sobre as celulas de fronteira entre faixas
 *                   vizinhas (apenas 2 linhas por fronteira: custo O(colunas)).
 *   4. (sequencial) objetos = numero de raizes do Union-Find. Cada rotulo
 *                   global existe porque tem >= 1 celula, entao contar raizes
 *                   equivale a contar objetos distintos, sem varrer a matriz.
 */
static int contar_objetos_paralelo(const int *matriz, int linhas, int colunas,
                                    int num_trabalhadores)
{
    pthread_t *threads = NULL;
    ArgumentoThread *args = NULL;
    int *rotulos = NULL;
    int *deslocamento = NULL;
    int *pai = NULL;
    int *tamanho_conjunto = NULL;
    int total_rotulos_globais;
    int j, t, rc;
    int criadas = 0;
    int falhou = 0;
    int total_objetos = -1;

    if (num_trabalhadores > linhas) {
        num_trabalhadores = linhas;
    }
    if (num_trabalhadores < 1) {
        num_trabalhadores = 1;
    }

    threads = (pthread_t *) malloc((size_t) num_trabalhadores * sizeof(pthread_t));
    args = (ArgumentoThread *) malloc((size_t) num_trabalhadores * sizeof(ArgumentoThread));
    deslocamento = (int *) malloc((size_t) num_trabalhadores * sizeof(int));
    rotulos = (int *) calloc((size_t) linhas * (size_t) colunas, sizeof(int));

    if (threads == NULL || args == NULL || deslocamento == NULL || rotulos == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas da versao paralela\n");
        goto limpeza;
    }

    {
        int linhas_base = linhas / num_trabalhadores;
        int resto = linhas % num_trabalhadores;
        int linha_atual = 0;

        for (t = 0; t < num_trabalhadores; t++) {
            int tamanho_faixa = linhas_base + (t < resto ? 1 : 0);

            args[t].id = t;
            args[t].matriz = matriz;
            args[t].rotulos = rotulos;
            args[t].linhas = linhas;
            args[t].colunas = colunas;
            args[t].linha_inicial = linha_atual;
            args[t].linha_final = linha_atual + tamanho_faixa;
            args[t].contagem_local = 0;
            args[t].erro = 0;

            linha_atual += tamanho_faixa;
        }
    }

    /*
     * A thread principal tambem trabalha: ela processa a faixa 0 enquanto as
     * demais (1..p-1) rodam em pthreads. Com p trabalhadores sao criadas
     * apenas p-1 threads, e nao ha um nucleo ocioso esperando no join.
     */
    for (t = 1; t < num_trabalhadores; t++) {
        rc = pthread_create(&threads[t], NULL, trabalhador, &args[t]);
        if (rc != 0) {
            fprintf(stderr, "Erro: pthread_create falhou para a thread %d (codigo %d)\n", t, rc);
            falhou = 1;
            break;
        }
        criadas++;
    }

    if (!falhou) {
        trabalhador(&args[0]);
    }

    /* Sempre faz join das threads efetivamente criadas, mesmo em caso de erro. */
    for (t = 1; t <= criadas; t++) {
        rc = pthread_join(threads[t], NULL);
        if (rc != 0) {
            fprintf(stderr, "Erro: pthread_join falhou para a thread %d (codigo %d)\n", t, rc);
            falhou = 1;
        }
    }
    if (falhou) {
        goto limpeza;
    }
    for (t = 0; t < num_trabalhadores; t++) {
        if (args[t].erro) {
            fprintf(stderr, "Erro: o trabalhador %d falhou (memoria insuficiente)\n", t);
            goto limpeza;
        }
    }

    deslocamento[0] = 0;
    for (t = 1; t < num_trabalhadores; t++) {
        deslocamento[t] = deslocamento[t - 1] + args[t - 1].contagem_local;
    }
    total_rotulos_globais = deslocamento[num_trabalhadores - 1] +
                            args[num_trabalhadores - 1].contagem_local;

    pai = (int *) malloc((size_t) (total_rotulos_globais + 1) * sizeof(int));
    tamanho_conjunto = (int *) malloc((size_t) (total_rotulos_globais + 1) * sizeof(int));
    if (pai == NULL || tamanho_conjunto == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas de consolidacao (union-find)\n");
        goto limpeza;
    }

    {
        int i;
        for (i = 0; i <= total_rotulos_globais; i++) {
            pai[i] = i;
            tamanho_conjunto[i] = 1;
        }
    }

    /* Fronteiras: ultima linha da faixa t x primeira linha da faixa t+1. */
    for (t = 0; t < num_trabalhadores - 1; t++) {
        int linha_cima = args[t].linha_final - 1;
        int linha_baixo = args[t + 1].linha_inicial;

        for (j = 0; j < colunas; j++) {
            int pos_cima = linha_cima * colunas + j;
            int dc;

            if (matriz[pos_cima] != 1) {
                continue;
            }
            for (dc = -1; dc <= 1; dc++) {
                int nc = j + dc;
                int pos_baixo;

                if (nc < 0 || nc >= colunas) {
                    continue;
                }
                pos_baixo = linha_baixo * colunas + nc;
                if (matriz[pos_baixo] != 1) {
                    continue;
                }
                uf_union(pai, tamanho_conjunto,
                         rotulos[pos_cima] + deslocamento[t],
                         rotulos[pos_baixo] + deslocamento[t + 1]);
            }
        }
    }

    /* Objetos = numero de raizes (rotulos globais vao de 1 a total). */
    total_objetos = 0;
    {
        int i;
        for (i = 1; i <= total_rotulos_globais; i++) {
            if (pai[i] == i) {
                total_objetos++;
            }
        }
    }

limpeza:
    free(tamanho_conjunto);
    free(pai);
    free(deslocamento);
    free(rotulos);
    free(args);
    free(threads);

    return total_objetos;
}

int main(int argc, char *argv[])
{
    int *matriz;
    int linhas, colunas, objetos, num_trabalhadores;
    struct timespec inicio, fim;
    double tempo_ms;

    if (argc < 3) {
        fprintf(stderr, "Uso: %s <arquivo_matriz> <num_trabalhadores>\n", argv[0]);
        return EXIT_FAILURE;
    }

    num_trabalhadores = atoi(argv[2]);
    if (num_trabalhadores < 1) {
        fprintf(stderr, "Erro: numero de trabalhadores deve ser >= 1\n");
        return EXIT_FAILURE;
    }

    matriz = ler_matriz(argv[1], &linhas, &colunas);
    if (matriz == NULL) {
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &inicio);
    objetos = contar_objetos_paralelo(matriz, linhas, colunas, num_trabalhadores);
    clock_gettime(CLOCK_MONOTONIC, &fim);

    if (objetos < 0) {
        free(matriz);
        return EXIT_FAILURE;
    }

    tempo_ms = (double) (fim.tv_sec - inicio.tv_sec) * 1000.0 +
               (double) (fim.tv_nsec - inicio.tv_nsec) / 1000000.0;

    printf("Arquivo: %s\n", argv[1]);
    printf("Dimensoes: %d x %d\n", linhas, colunas);
    printf("Versao: paralela (pthreads)\n");
    printf("Trabalhadores solicitados: %d\n", num_trabalhadores);
    printf("Objetos encontrados: %d\n", objetos);
    printf("Tempo: %.3f ms\n", tempo_ms);

    free(matriz);
    return EXIT_SUCCESS;
}
