#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

#define LINHAS 10000
#define COLUNAS 10000
#define CORES 8
#define THREADS 4
#define TAMANHO_MACROBLOCO_COLUNA 1000
#define TAMANHO_MACROBLOCO_LINHA 1000

typedef struct matrix {
    int **data;
    int rows, cols;
} Matrix;

typedef struct macro_bloco {
    int comeco_linha, fim_linha, comeco_coluna, fim_coluna;
} Macrobloco;

typedef struct thread_ctx {
    int total_blocos,
        blocos_por_linha,
        blocos_por_coluna,
        bloco_tamanho_linha,
        bloco_tamanho_coluna;
} ThreadCtx;

Matrix g_matrix;
int g_contador_global = 0;
int g_proximo_bloco = 0;
pthread_mutex_t g_mutex_contador;
pthread_mutex_t g_mutex_fila;


static inline int min_int(int a, int b) { return (a < b) ? a : b; }
static inline int ceil_div(int a, int b) { return (a + b - 1) / b; }

void desalocar_ponteiros_matrix() {
    for (int i = 0; i < g_matrix.rows; i++)
        free(g_matrix.data[i]);

    free(g_matrix.data);
}

void inicializar_matrix_aleatoria(const int rows, const int cols) {
g_matrix.data = malloc(sizeof(int*) * rows);
if (!g_matrix.data) return;

for (int i = 0; i < rows; i++) {
        g_matrix.data[i] = malloc(sizeof(int) * cols);
        if (!g_matrix.data[i])  {
            for (int j = 0; j < i; j++) free(g_matrix.data[j]);
            free(g_matrix.data);
            return;
        }

        for (int k = 0; k < cols; k++) g_matrix.data[i][k] = rand() % 32000;
}

g_matrix.rows = rows;
g_matrix.cols = cols;
}


bool ehPrimo(const int n) {
    if ( n <= 1)
        return false;

    double limite = sqrt(n);
    for (int i = 2; i <= limite; i++) {
        if (n % i == 0) {
            return false;
        }
    }

    return true;
}

void calcula_macrobloco(Macrobloco *mb, const ThreadCtx *ctx, const int bloco_index) {
    int bloco_linha = bloco_index / ctx->blocos_por_coluna;
    int bloco_coluna = bloco_index % ctx->blocos_por_coluna;

    int comeco_linha = bloco_linha * ctx->bloco_tamanho_linha;
    int fim_linha = min_int(comeco_linha + ctx->bloco_tamanho_linha, g_matrix.rows);

    int comeco_coluna = bloco_coluna * ctx->bloco_tamanho_coluna;
    int fim_coluna = min_int(comeco_coluna + ctx->bloco_tamanho_coluna, g_matrix.cols);

    mb->comeco_coluna = comeco_coluna;
    mb->fim_coluna = fim_coluna;
    mb->comeco_linha = comeco_linha;
    mb->fim_linha = fim_linha;
}

void* thread_worker(void *arg) {
    ThreadCtx *ctx = (ThreadCtx*)arg;

    while (1) {
        pthread_mutex_lock(&g_mutex_fila);
        int bloco_id = g_proximo_bloco;
        g_proximo_bloco++;
        pthread_mutex_unlock(&g_mutex_fila);

        if (bloco_id >= ctx->total_blocos) break;

        Macrobloco mb;
        calcula_macrobloco(&mb, ctx, bloco_id);

        int contador_local = 0;
        for (int i = mb.comeco_linha; i < mb.fim_linha; i++) {
            for (int j = mb.comeco_coluna; j < mb.fim_coluna; j++) {
                if (ehPrimo(g_matrix.data[i][j])) contador_local++;
            }
        }

        pthread_mutex_lock(&g_mutex_contador);
        g_contador_global += contador_local;
        pthread_mutex_unlock(&g_mutex_contador);
    }

    return NULL;
}

int busca_paralela_alocacao_dinamica(pthread_t *threads, const int thread_count, const int tamanho_macrobloco_linha, const int tamanho_macrobloco_coluna) {
    g_contador_global = 0;
    g_proximo_bloco = 0;

    const int n_linhas_bloco = ceil_div(g_matrix.rows, tamanho_macrobloco_linha);
    const int n_colunas_bloco = ceil_div(g_matrix.cols, tamanho_macrobloco_coluna);
    const int total_blocos = n_colunas_bloco * n_linhas_bloco;

    // pthread_t *threads = malloc(sizeof(pthread_t) * thread_count);
    // if (!threads) return -1;

    pthread_mutex_init(&g_mutex_fila, NULL);
    pthread_mutex_init(&g_mutex_contador, NULL);

    ThreadCtx ctx = {
        .total_blocos = total_blocos,
        .bloco_tamanho_coluna = tamanho_macrobloco_coluna,
        .bloco_tamanho_linha = tamanho_macrobloco_linha,
        .blocos_por_coluna = n_colunas_bloco,
        .blocos_por_linha = n_linhas_bloco
    };

    for (int i = 0; i < thread_count; i++)
        pthread_create(&threads[i], NULL, thread_worker, &ctx);

    for (int i = 0; i < thread_count; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&g_mutex_fila);
    pthread_mutex_destroy(&g_mutex_contador);

    return g_contador_global;
}

int busca_paralela(const int tamanho_macrobloco_linha, const int tamanho_macrobloco_coluna, const int n_threads) {
    g_contador_global = 0;
    g_proximo_bloco = 0;

    const int n_linhas_bloco = ceil_div(g_matrix.rows, tamanho_macrobloco_linha);
    const int n_colunas_bloco = ceil_div(g_matrix.cols, tamanho_macrobloco_coluna);
    const int total_blocos = n_colunas_bloco * n_linhas_bloco;

    pthread_t threads[n_threads];

    pthread_mutex_init(&g_mutex_fila, NULL);
    pthread_mutex_init(&g_mutex_contador, NULL);

    ThreadCtx ctx = {
        .total_blocos = total_blocos,
        .bloco_tamanho_coluna = tamanho_macrobloco_coluna,
        .bloco_tamanho_linha = tamanho_macrobloco_linha,
        .blocos_por_coluna = n_colunas_bloco,
        .blocos_por_linha = n_linhas_bloco
    };

    for (int i = 0; i < n_threads; i++)
        pthread_create(&threads[i], NULL, thread_worker, &ctx);

    for (int i = 0; i < n_threads; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_destroy(&g_mutex_fila);
    pthread_mutex_destroy(&g_mutex_contador);

    return g_contador_global;
}

int busca_serial()  {

    int count = 0;
    for (int i = 0; i < g_matrix.rows; i++) {
        int *row = g_matrix.data[i];
        for (int j = 0; j < g_matrix.cols; j++) {
            if (ehPrimo(row[j])) count++;
        }
    }
    return count;
}

void benchmark() {
    struct timespec comeco_paralelo, fim_paralelo, comeco_serial, fim_serial;

    FILE *output_file = fopen("./benchmark.csv", "w");
    if (!output_file) {
        puts("Erro ao abrir arquivo de output do benchmark");
        return;
    }

    fprintf(output_file, "tamanho_matriz,tamanho_macrobloco,threads,tempo_serial,tempo_paralelo,speedup,eficiencia,resultado_serial,resultado_paralelo\n");

    int tamanho_matrizes[] = {100, 1000, 3000, 5000, 8000, 10000};
    int tamanho_blocos[] = {10, 50, 100, 250, 500, 1000};
    int thread_count[] = {1, 2, 3, 4, 8, 12};

    int n_tamanho_matrizes = sizeof(tamanho_matrizes) / sizeof(tamanho_matrizes[0]);
    int n_tamanho_blocos = sizeof(tamanho_blocos) / sizeof(tamanho_blocos[0]);
    int n_threads = sizeof(thread_count) / sizeof(thread_count[0]);

    for (int i = 0; i < n_tamanho_matrizes; i++) {
        int tamanho_matriz = tamanho_matrizes[i];
        inicializar_matrix_aleatoria(tamanho_matriz, tamanho_matriz);

        clock_gettime(CLOCK_MONOTONIC, &comeco_serial);
        int resultado_serial = busca_serial();
        clock_gettime(CLOCK_MONOTONIC, &fim_serial);
        double tempo_serial = (fim_serial.tv_sec - comeco_serial.tv_sec) + (fim_serial.tv_nsec - comeco_serial.tv_nsec) / 1e9;

        for (int j = 0; j < n_tamanho_blocos; j ++) {
            int tamanho_bloco = tamanho_blocos[j];
            for (int k = 0; k < n_threads; k++) {

                int n_threads = thread_count[k];
                pthread_t *threads = malloc(sizeof(pthread_t) * n_threads);
                clock_gettime(CLOCK_MONOTONIC, &comeco_paralelo);
                int resultado_paralelo = busca_paralela_alocacao_dinamica(threads, n_threads, tamanho_bloco, tamanho_bloco);
                clock_gettime(CLOCK_MONOTONIC, &fim_paralelo);

                double tempo_paralelo = (fim_paralelo.tv_sec - comeco_paralelo.tv_sec) + (fim_paralelo.tv_nsec - comeco_paralelo.tv_nsec) / 1e9;
                double speedup = tempo_serial / tempo_paralelo;
                double eficiencia = speedup / n_threads;

                free(threads);
                fprintf(output_file, "%d,%d,%d,%lf,%lf,%lf,%lf,%d,%d\n",
                    tamanho_matriz, tamanho_bloco, n_threads, tempo_serial ,tempo_paralelo, speedup, eficiencia, resultado_serial, resultado_paralelo);
            }
        }
    }
    fclose(output_file);
}

int main() {
    srand(2004);
    struct timespec comeco_paralelo, fim_paralelo, comeco_serial, fim_serial;

    benchmark();
    return 0;

    inicializar_matrix_aleatoria(LINHAS, COLUNAS);

    clock_gettime(CLOCK_MONOTONIC, &comeco_serial);
    int r1 = busca_serial();
    clock_gettime(CLOCK_MONOTONIC, &fim_serial);


    clock_gettime(CLOCK_MONOTONIC, &comeco_paralelo);
    int r2 = busca_paralela(THREADS, TAMANHO_MACROBLOCO_LINHA, TAMANHO_MACROBLOCO_COLUNA);
    clock_gettime(CLOCK_MONOTONIC, &fim_paralelo);

    double tempo_serial = (fim_serial.tv_sec - comeco_serial.tv_sec) + (fim_serial.tv_nsec - comeco_serial.tv_nsec) / 1e9;
    double tempo_paralelo = (fim_paralelo.tv_sec - comeco_paralelo.tv_sec) + (fim_paralelo.tv_nsec - comeco_paralelo.tv_nsec) / 1e9;
    double speedup = tempo_serial / tempo_paralelo;
    double eficiencia = speedup / THREADS;

    printf("Busca serial: %d\nBusca paralela: %d\n\n", r1, r2);
    printf("Tempo serial: %lf\nTempo paralelo: %lf\n\n", tempo_serial, tempo_paralelo);

    printf("Speedup: %lf - Eficiencia: %lf\n", speedup, eficiencia);

    desalocar_ponteiros_matrix();
    return EXIT_SUCCESS;
}
