/*Carolina Lee - 10440304
Enrique Cipolla Martins - 10427834
Pedro Gabriel Guimarães Fernandes - 10437465*/
#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TABLE_SIZE 131071

void extract_url(const char* line, char* url) {
    const char* get_ptr = strstr(line, "GET ");
    if (!get_ptr) {
        // Tenta com aspas
        get_ptr = strstr(line, "\"GET ");
        if (!get_ptr) {
            url[0] = '\0';
            return;
        }
        get_ptr = get_ptr + 5;  // Pula "\"GET "
    } else {
        get_ptr = get_ptr + 4;  // Pula "GET "
    }
    
    const char* url_end = strchr(get_ptr, ' ');
    if (!url_end) {
        url[0] = '\0';
        return;
    }
    
    size_t url_len = url_end - get_ptr;
    if (url_len >= 512) url_len = 511;
    
    strncpy(url, get_ptr, url_len);
    url[url_len] = '\0';
}

void build_hash_table(HashTable* ht, const char* manifest_file) {
    FILE* f = fopen(manifest_file, "r");
    if (!f) {
        perror("Erro abrir manifest");
        exit(1);
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int count = 0;
    
    while ((read = getline(&line, &len, f)) != -1) {
        // Remove caracteres de fim de linha
        size_t last = strlen(line) - 1;
        while (last > 0 && (line[last] == '\n' || line[last] == '\r')) {
            line[last] = '\0';
            last--;
        }
        
        ht_put(ht, line);
        count++;
    }
    
    printf("Carregadas %d URLs do manifesto\n", count);
    free(line);
    fclose(f);
}

void process_log_sequential(HashTable* ht, const char* log_file) {
    FILE* f = fopen(log_file, "r");
    if (!f) {
        perror("Erro abrir log");
        exit(1);
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char url[512];
    long line_count = 0;
    int not_found = 0;
    
    while ((read = getline(&line, &len, f)) != -1) {
        extract_url(line, url);
        if (url[0] != '\0') {
            CacheNode* node = ht_get(ht, url);
            if (node) {
                node->hit_count++;
            } else {
                not_found++;
                if (not_found <= 5) {
                    printf("URL nao encontrada: '%s'\n", url);
                }
            }
        }
        line_count++;
        
        if (line_count % 1000000 == 0) {
            printf("Processadas %ld linhas...\n", line_count);
        }
    }
    
    printf("Total processado: %ld linhas\n", line_count);
    if (not_found > 0) {
        printf("ATENCAO: %d URLs nao encontradas na tabela\n", not_found);
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
    
    printf("Processando %s...\n", argv[1]);
    clock_t start = clock();
    process_log_sequential(ht, argv[1]);
    clock_t end = clock();
    
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tempo sequencial: %.4f segundos\n", time_spent);
    
    ht_save_results(ht, "results.csv");
    printf("Resultados salvos em results.csv\n");
    
    ht_destroy(ht);
    return 0;
}
