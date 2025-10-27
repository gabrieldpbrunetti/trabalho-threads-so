#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

#define LINHAS 90
#define COLUNAS 100
#define CORES 8
#define THREADS 12
#define TAMANHO_MACROBLOCO_COLUNA 3
#define TAMANHO_MACROBLOCO_LINHA 3

typedef struct {
    int **data;
    int rows, cols;
} Matrix;

typedef struct {
    int **matrix_data;
    int comeco_linha, fim_linha, comeco_coluna, fim_coluna;
} Macrobloco;

typedef struct {
    Macrobloco *blocos;
    int total_blocos;
    int *proximo_bloco_id;
    int *contador_global;
    pthread_mutex_t *mutex_fila;
    pthread_mutex_t *mutex_contador;
} ThreadCtx;

static inline int min_int(int a, int b) { return (a < b) ? a : b; }
static inline int ceil_div(int a, int b) { return (a + b - 1) / b; }

Matrix* init_matrix(const int rows, const int cols) {
    Matrix *m = malloc(sizeof(Matrix));
    if (!m) return NULL;

    m->data = malloc(sizeof(int*) * rows);
    if (!m->data) {
        free(m);
        return NULL;
    }

    m->rows = rows;
    m->cols = cols;

    for (int i = 0; i < rows; i++) {
        m->data[i] = malloc(sizeof(int) * cols);
        if (!m->data[i]) {
            for (int j = 0; j < i; j++) free(m->data[j]);
            free(m->data);
            free(m);
            return NULL;
        }
    }

    return m;
}

void free_matrix(Matrix *m) {
    if (!m) return;

    int **data = m->data;
    for (int i = 0; i < m->rows; i++) {
        free(data[i]);
    }
    free(data);
    free(m);
}

void matrix_inserir_numeros_aleatorios(Matrix *m) {
    Matrix matrix = *m;
    for (int i = 0; i < matrix.rows; i++) {
        int *row = matrix.data[i];
        for (int j = 0; j < matrix.cols; j ++) {
            row[j] = rand() % 32000;
        }
    }
}

Matrix* criar_matrix_aleatoria(const int rows, const int cols) {
    Matrix *m = init_matrix(rows, cols);
    if (!m) return NULL;
    matrix_inserir_numeros_aleatorios(m);

    return m;
}

void print_matrix(Matrix *m) {
    Matrix matrix = *m;
    for (int i = 0; i < matrix.rows; i++) {
        int *row = matrix.data[i];
        printf("Linha %d: ", i);
        for (int j = 0; j < matrix.cols; j++) {
            printf("%d ", row[j]);
        }
        printf("\n");
    }
}

bool ehPrimo(const int n) {
    if ( n <= 1)
        return false;

    for (int i = 2; i <= n / 2; i++) {
        if (n % i == 0) {
            return false;
        }
    }

    return true;
}

void* thread_worker(void *arg) {
    ThreadCtx *ctx = (ThreadCtx*)arg;

    while (1) {
        pthread_mutex_lock(ctx->mutex_fila);
        int bloco_id = *(ctx->proximo_bloco_id);
        ((*ctx->proximo_bloco_id))++;
        pthread_mutex_unlock(ctx->mutex_fila);

        if (bloco_id >= ctx->total_blocos) break;

        Macrobloco mb = ctx->blocos[bloco_id];

        int contador_local = 0;
        for (int i = mb.comeco_linha; i < mb.fim_linha; i++) {
            for (int j = mb.comeco_coluna; j < mb.fim_coluna; j++) {
                if (ehPrimo(mb.matrix_data[i][j])) contador_local++;
            }
        }

        pthread_mutex_lock(ctx->mutex_contador);
        *(ctx->contador_global) += contador_local;
        pthread_mutex_unlock(ctx->mutex_contador);
    }

    return NULL;
}

int busca_paralela(Matrix *m, const int tamanho_macrobloco_linha, const int tamanho_macrobloco_coluna) {

    const int n_linhas_bloco = ceil_div(m->rows, tamanho_macrobloco_linha);
    const int n_colunas_bloco = ceil_div(m->cols, tamanho_macrobloco_coluna);
    const int total_blocos = n_colunas_bloco * n_linhas_bloco;

    Macrobloco *blocos = (Macrobloco *)malloc(sizeof(Macrobloco) * total_blocos);
    if (!blocos) return -1;

    int k = 0;
    for (int bl = 0; bl < n_linhas_bloco; ++bl) {
        int comeco_linha = bl * tamanho_macrobloco_linha;
        int fim_linha = min_int(comeco_linha + tamanho_macrobloco_linha, m->rows);
        for (int bc = 0; bc < n_colunas_bloco; ++bc) {
            int comeco_coluna = bc * tamanho_macrobloco_coluna;
            int fim_coluna = min_int(comeco_coluna + tamanho_macrobloco_coluna, m->cols);

            blocos[k].matrix_data = m->data;
            blocos[k].comeco_coluna = comeco_coluna;
            blocos[k].fim_coluna = fim_coluna;
            blocos[k].comeco_linha = comeco_linha;
            blocos[k].fim_linha = fim_linha;
            k++;
        }
    }

    pthread_t threads[THREADS];
    int ids[THREADS];
    int contador_global = 0, proximo_bloco_id = 0;
    pthread_mutex_t mutex_fila, mutex_contador;

    pthread_mutex_init(&mutex_fila, NULL);
    pthread_mutex_init(&mutex_contador, NULL);

    ThreadCtx ctx;
    ctx.blocos = blocos;
    ctx.total_blocos = total_blocos;
    ctx.proximo_bloco_id = &proximo_bloco_id;
    ctx.contador_global = &contador_global;
    ctx.mutex_fila = &mutex_fila;
    ctx.mutex_contador = &mutex_contador;

    for (int i = 0; i < THREADS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, thread_worker, &ctx);
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex_fila);
    pthread_mutex_destroy(&mutex_contador);
    free(blocos);

    return contador_global;
}

int busca_serial(Matrix *m)  {
    Matrix matrix = *m;
    int count = 0;
    for (int i = 0; i < matrix.rows; i++) {
        int *row = matrix.data[i];
        for (int j = 0; j < matrix.cols; j++) {
            if (ehPrimo(row[j])) count++;
        }
    }
    return count;
}

int main() {
    srand(2004);
    struct timespec comeco_paralelo, fim_paralelo, comeco_serial, fim_serial;

    Matrix *matrix = init_matrix(LINHAS, COLUNAS);
    if (!matrix) {
        perror("Erro ao alocar matriz");
        return EXIT_FAILURE;
    }
    matrix_inserir_numeros_aleatorios(matrix);

    clock_gettime(CLOCK_MONOTONIC, &comeco_serial);
    int r1 = busca_serial(matrix);
    clock_gettime(CLOCK_MONOTONIC, &fim_serial);

    double tempo_serial = (fim_serial.tv_sec - comeco_serial.tv_sec) + (fim_serial.tv_nsec - comeco_serial.tv_nsec) / 1e9;

    clock_gettime(CLOCK_MONOTONIC, &comeco_paralelo);
    int r2 = busca_paralela(matrix, TAMANHO_MACROBLOCO_LINHA, TAMANHO_MACROBLOCO_COLUNA);
    clock_gettime(CLOCK_MONOTONIC, &fim_paralelo);

    double tempo_paralelo = (fim_paralelo.tv_sec - comeco_paralelo.tv_sec) + (fim_paralelo.tv_nsec - comeco_paralelo.tv_nsec) / 1e9;

    free_matrix(matrix);

    printf("Busca serial: %d\nBusca Paralela: %d\n", r1, r2);
    printf("Tempo serial: %lf\nTempo paralelo: %lf\n", tempo_serial, tempo_paralelo);


    return EXIT_SUCCESS;
}
