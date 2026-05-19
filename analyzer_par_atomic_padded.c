#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "hash_table.h"

#define HASH_SIZE 131071

// Fase 1: Construção da Estrutura (Sequencial)
HashTable* construir_tabela_manifest(const char *manifest_path) {
    HashTable *ht = ht_create(HASH_SIZE);
    if (ht == NULL) {
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(manifest_path, "r");
    if (file == NULL) {
        exit(EXIT_FAILURE);
    }

    char url[256];
    while (fscanf(file, "%255s", url) == 1) {
        ht_put(ht, url);
    }

    fclose(file);
    return ht;
}

// Fase 2: Processamento dos Logs (Paralelo)
void processar_logs_padded(HashTable *ht, char **log_lines, size_t total_lines) {
    #pragma omp parallel for
    for (size_t i = 0; i < total_lines; i++) {
        char method[16], url[256], protocol[16];
        char *req_start = strchr(log_lines[i], '"');
        if (req_start != NULL) {
            if (sscanf(req_start + 1, "%15s %255s %15s", method, url, protocol) == 3) {
                CacheNode *node = ht_get(ht, url);
                if (node != NULL) {
                    #pragma omp atomic update
                    node->hit_count++;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <arquivo_de_log>\n", argv[0]);
        return 1;
    }

    // 1. Inicializa a tabela a partir do manifest
    clock_t t1 = clock();
    HashTable *ht = construir_tabela_manifest("manifest.txt");
    clock_t t2 = clock();

    // 2. Carrega o log para a memória (vetor de strings)
    FILE *f = fopen(argv[1], "r");
    if (!f) exit(EXIT_FAILURE);

    char **log_lines = malloc(10000000 * sizeof(char *));
    char buffer[1024];
    size_t total_lines = 0;

    while (fgets(buffer, sizeof(buffer), f)) {
        log_lines[total_lines++] = strdup(buffer);
    }
    fclose(f);

    // 3. Executa o processamento paralelo
    clock_t t3 = clock();
    processar_logs_padded(ht, log_lines, total_lines);
    clock_t t4 = clock();

    // 4. Salva os resultados para validação
    ht_save_results(ht, "results.csv");
    clock_t t5 = clock();

    // Limpeza de memória
    for (size_t i = 0; i < total_lines; i++) free(log_lines[i]);
    free(log_lines);
    ht_destroy(ht);

    printf("Construir manifest: %.4f s\n", (double)(t2 - t1) / CLOCKS_PER_SEC);
    printf("Processar logs:     %.4f s\n", (double)(t4 - t3) / CLOCKS_PER_SEC);
    printf("Salvar resultados:  %.4f s\n", (double)(t5 - t4) / CLOCKS_PER_SEC);
    printf("Total:              %.4f s\n", (double)(t5 - t1) / CLOCKS_PER_SEC);

    return EXIT_SUCCESS;
}