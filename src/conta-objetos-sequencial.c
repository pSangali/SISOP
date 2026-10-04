#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    int linha;
    int coluna;
} Coordenada;

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

/* A pilha e' alocada uma unica vez pelo chamador e reutilizada em cada flood fill. */
static void flood_fill(const int *matriz, int *visitado, int linhas, int colunas,
                        int linha_inicial, int coluna_inicial, Coordenada *pilha)
{
    static const int delta_linha[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
    static const int delta_coluna[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
    int topo;

    topo = 0;
    pilha[topo].linha = linha_inicial;
    pilha[topo].coluna = coluna_inicial;
    topo++;
    visitado[linha_inicial * colunas + coluna_inicial] = 1;

    while (topo > 0) {
        Coordenada atual;
        int k;

        topo--;
        atual = pilha[topo];

        for (k = 0; k < 8; k++) {
            int nl = atual.linha + delta_linha[k];
            int nc = atual.coluna + delta_coluna[k];

            if (nl < 0 || nl >= linhas || nc < 0 || nc >= colunas) {
                continue;
            }
            if (visitado[nl * colunas + nc]) {
                continue;
            }
            if (matriz[nl * colunas + nc] != 1) {
                continue;
            }

            visitado[nl * colunas + nc] = 1;
            pilha[topo].linha = nl;
            pilha[topo].coluna = nc;
            topo++;
        }
    }
}

static int contar_objetos_sequencial(const int *matriz, int linhas, int colunas)
{
    int *visitado;
    Coordenada *pilha;
    int i, j;
    int total_objetos;

    visitado = (int *) calloc((size_t) linhas * (size_t) colunas, sizeof(int));
    pilha = (Coordenada *) malloc((size_t) linhas * (size_t) colunas * sizeof(Coordenada));
    if (visitado == NULL || pilha == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas do flood fill\n");
        exit(EXIT_FAILURE);
    }

    total_objetos = 0;
    for (i = 0; i < linhas; i++) {
        for (j = 0; j < colunas; j++) {
            if (matriz[i * colunas + j] == 1 && !visitado[i * colunas + j]) {
                flood_fill(matriz, visitado, linhas, colunas, i, j, pilha);
                total_objetos++;
            }
        }
    }

    free(pilha);
    free(visitado);
    return total_objetos;
}

int main(int argc, char *argv[])
{
    int *matriz;
    int linhas, colunas, objetos;
    struct timespec inicio, fim;
    double tempo_ms;

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo_matriz>\n", argv[0]);
        return EXIT_FAILURE;
    }

    matriz = ler_matriz(argv[1], &linhas, &colunas);
    if (matriz == NULL) {
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &inicio);
    objetos = contar_objetos_sequencial(matriz, linhas, colunas);
    clock_gettime(CLOCK_MONOTONIC, &fim);

    tempo_ms = (double) (fim.tv_sec - inicio.tv_sec) * 1000.0 +
               (double) (fim.tv_nsec - inicio.tv_nsec) / 1000000.0;

    printf("Arquivo: %s\n", argv[1]);
    printf("Dimensoes: %d x %d\n", linhas, colunas);
    printf("Versao: sequencial\n");
    printf("Objetos encontrados: %d\n", objetos);
    printf("Tempo: %.3f ms\n", tempo_ms);
    free(matriz);
    return EXIT_SUCCESS;
}
