// index.c — Staging area implementation
#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

// Stubs — implement below
int index_load(Index *index) {
    index->count = 0;
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0; // No index yet is fine

    char hex[HASH_HEX_SIZE + 1];
    while (index->count < MAX_INDEX_ENTRIES) {
        if (fscanf(f, "%u %64s %llu %u %511s",
                   &index->entries[index->count].mode,
                   hex,
                   &index->entries[index->count].mtime_sec,
                   &index->entries[index->count].size,
                   index->entries[index->count].path) != 5) {
            break;
        }
        if (hex_to_hash(hex, &index->entries[index->count].hash) != 0) {
            fclose(f);
            return -1;
        }
        index->count++;
    }
    fclose(f);
    return 0;
}

static int compare_entries(const void *a, const void *b) {
    return strcmp(((IndexEntry*)a)->path, ((IndexEntry*)b)->path);
}

int index_save(const Index *index) {
    IndexEntry *sorted_entries = malloc(index->count * sizeof(IndexEntry));
    if (!sorted_entries) return -1;
    memcpy(sorted_entries, index->entries, index->count * sizeof(IndexEntry));
    qsort(sorted_entries, index->count, sizeof(IndexEntry), compare_entries);

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", INDEX_FILE);

    FILE *f = fopen(tmp_path, "w");
    if (!f) { free(sorted_entries); return -1; }

    char hex[HASH_HEX_SIZE + 1];
    for (int i = 0; i < index->count; i++) {
        hash_to_hex(&sorted_entries[i].hash, hex);
        if (fprintf(f, "%u %s %llu %u %s\n",
                    sorted_entries[i].mode,
                    hex,
                    sorted_entries[i].mtime_sec,
                    sorted_entries[i].size,
                    sorted_entries[i].path) < 0) {
            fclose(f); unlink(tmp_path); free(sorted_entries); return -1;
        }
    }

    if (fflush(f) != 0) { fclose(f); unlink(tmp_path); free(sorted_entries); return -1; }
    if (fsync(fileno(f)) != 0) { fclose(f); unlink(tmp_path); free(sorted_entries); return -1; }
    fclose(f);

    if (rename(tmp_path, INDEX_FILE) != 0) { free(sorted_entries); return -1; }
    free(sorted_entries);
    return 0;
}

int index_add(Index *index, const char *path) { (void)index; (void)path; return -1; }
