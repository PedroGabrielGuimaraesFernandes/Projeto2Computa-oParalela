#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define TABLE_SIZE 131071

// Precisamos da função hash - vamos replicar a mesma lógica
static size_t hash_djb2(const char* str, size_t size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % size;
}

void extract_url(const char* line, char* url) {
    const char* get_ptr = strstr(line, "GET ");
    if (!get_ptr) return;
    const char* url_start = get_ptr + 4;
    const char* url_end = strchr(url_start, ' ');
    if (!url_end) return;
    size_t url_len = url_end - url_start;
    strncpy(url, url_start, url_len);
    url[url_len] = '\0';
}

void build_hash_table(HashTable* ht, const char* manifest_file) {
    FILE* f = fopen(manifest_file, "r");
    if (!f) { perror("manifest"); exit(1); }
    
    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        line[strcspn(line, "\n")] = '\0';
        ht_put(ht, line);
    }
    free(line);
    fclose(f);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <log_file>\n", argv[0]);
        return 1;
    }
    
    printf("Construindo tabela hash...\n");
    HashTable* ht = ht_create(TABLE_SIZE);
    build_hash_table(ht, "manifest.txt");
    
    // Criar array de locks para cada bucket
    omp_lock_t* bucket_locks = (omp_lock_t*)malloc(sizeof(omp_lock_t) * TABLE_SIZE);
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        omp_init_lock(&bucket_locks[i]);
    }
    
    printf("Carregando log...\n");
    FILE* f = fopen(argv[1], "r");
    if (!f) { perror("log"); return 1; }
    
    char** lines = malloc(sizeof(char*) * 10000000);
    size_t line_count = 0;
    size_t cap = 0;
    
    while (getline(&lines[line_count], &cap, f) != -1) {
        line_count++;
        lines = realloc(lines, sizeof(char*) * (line_count + 1));
        cap = 0;
        lines[line_count] = NULL;
    }
    fclose(f);
    
    printf("Processando %zu linhas com BUCKET LOCKS...\n", line_count);
    double start = omp_get_wtime();
    
    #pragma omp parallel for
    for (size_t i = 0; i < line_count; i++) {
        char url[512];
        extract_url(lines[i], url);
        
        // Calcula o bucket
        size_t bucket = hash_djb2(url, TABLE_SIZE);
        
        // Lock específico do bucket
        omp_set_lock(&bucket_locks[bucket]);
        
        CacheNode* node = ht_get(ht, url);
        if (node) {
            node->hit_count++;
        }
        
        omp_unset_lock(&bucket_locks[bucket]);
        
        free(lines[i]);
    }
    
    double end = omp_get_wtime();
    printf("Tempo bucket locks: %.4f segundos\n", end - start);
    
    // Limpeza
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        omp_destroy_lock(&bucket_locks[i]);
    }
    free(bucket_locks);
    free(lines);
    
    ht_save_results(ht, "results.csv");
    ht_destroy(ht);
    
    return 0;
}