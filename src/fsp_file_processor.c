#include "fsp_file_processor.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <openssl/evp.h>


/* =========================================================================
 * SHA256 file + chunk hashing (single pass, EVP)
 * ========================================================================= */
static int
fsp_compute_file_and_chunks(const char *path,
                            fsp_file_entry_t *entry,
                            fsp_walker_state_t *state)
{
    if (!state->file_buf || state->file_buf_size == 0) {
        fprintf(stderr, "fsp_compute_file_and_chunks: invalid state buffer\n");
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return -1; }

    EVP_MD_CTX *file_ctx  = EVP_MD_CTX_new();
    EVP_MD_CTX *chunk_ctx = EVP_MD_CTX_new();
    if (!file_ctx || !chunk_ctx) { fclose(fp); EVP_MD_CTX_free(file_ctx); EVP_MD_CTX_free(chunk_ctx); return -1; }

    EVP_DigestInit_ex(file_ctx,  EVP_sha256(), NULL);
    EVP_DigestInit_ex(chunk_ctx, EVP_sha256(), NULL);

    uint8_t *buf = state->file_buf;
    size_t buf_size = state->file_buf_size;

    uint64_t chunk_bytes = 0;
    size_t   cap_chunks  = 0;
    unsigned char (*chunks)[SHA256_DIGEST_LENGTH] = NULL;

    while (!feof(fp)) {
        size_t n = fread(buf, 1, buf_size, fp);
        if (ferror(fp)) { perror("fread"); fclose(fp); EVP_MD_CTX_free(file_ctx); EVP_MD_CTX_free(chunk_ctx); free(chunks); return -1; }
        if (n == 0) break;

        EVP_DigestUpdate(file_ctx, buf, n);

        if (entry->size >= FSP_CHUNK_SIZE) {
            EVP_DigestUpdate(chunk_ctx, buf, n);
            chunk_bytes += n;

            if (chunk_bytes >= FSP_CHUNK_SIZE) {
                if (entry->num_chunks >= cap_chunks) {
                    size_t new_cap = cap_chunks ? cap_chunks * 2 : 16;
                    void *tmp = realloc(chunks, new_cap * sizeof(*chunks));
                    if (!tmp) { fclose(fp); EVP_MD_CTX_free(file_ctx); EVP_MD_CTX_free(chunk_ctx); free(chunks); return -1; }
                    chunks = tmp;
                    cap_chunks = new_cap;
                }

                EVP_DigestFinal_ex(chunk_ctx, chunks[entry->num_chunks], NULL);
                entry->num_chunks++;
                chunk_bytes = 0;
                EVP_DigestInit_ex(chunk_ctx, EVP_sha256(), NULL);
            }
        }
    }

    // Final partial chunk
    if (chunk_bytes > 0 && entry->size >= FSP_CHUNK_SIZE) {
        if (entry->num_chunks >= cap_chunks) {
            size_t new_cap = cap_chunks ? cap_chunks * 2 : 16;
            void *tmp = realloc(chunks, new_cap * sizeof(*chunks));
            if (!tmp) { fclose(fp); EVP_MD_CTX_free(file_ctx); EVP_MD_CTX_free(chunk_ctx); free(chunks); return -1; }
            chunks = tmp;
            cap_chunks = new_cap;
        }
        EVP_DigestFinal_ex(chunk_ctx, chunks[entry->num_chunks], NULL);
        entry->num_chunks++;
    }

    EVP_DigestFinal_ex(file_ctx, entry->file_hash, NULL);
    entry->chunk_hashes = chunks;
    entry->cap_chunks   = cap_chunks;

    fclose(fp);
    EVP_MD_CTX_free(file_ctx);
    EVP_MD_CTX_free(chunk_ctx);

    return 0;
}

/* =========================================================================
 * Debug helpers
 * ========================================================================= */
static void fsp_print_sha256(const unsigned char hash[SHA256_DIGEST_LENGTH])
{
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        fprintf(stderr,"%02x", hash[i]);
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void fsp_file_processor_init(fsp_walker_state_t *state)
{
    memset(&state->entries, 0, sizeof(state->entries));
    state->current_files = 0;
    state->current_bytes = 0;
}

/**
 * Main directory callback implementing **three-phase batching**
 */
int fsp_file_processor_process_directory(fsp_walker_state_t *state)
{
    fsp_dir_entries_t *entries = &state->entries;
    size_t total_files = entries->num_files;

    size_t file_index = 0;

    while (file_index < total_files) {
        size_t batch_files = 0;
        uint64_t batch_bytes = 0;

        // --- Build a batch ---
        while ((file_index + batch_files) < total_files &&
               (!state->max_files || batch_files < state->max_files) &&
               (!state->max_bytes || (batch_bytes + entries->files[file_index + batch_files].size) <= state->max_bytes))
        {
            batch_bytes += entries->files[file_index + batch_files].size;
            batch_files++;
        }

        if (batch_files == 0) {
            // Single huge file exceeds max_bytes threshold
            batch_files = 1;
            batch_bytes = entries->files[file_index].size;
        }

        // --- Phase 1: Send metadata (sizes) ---
        fprintf(stderr, "\n[PHASE 1] Sending metadata for batch starting at file %zu\n", file_index);
        for (size_t i = 0; i < batch_files; i++) {
            fsp_file_entry_t *f = &entries->files[file_index + i];
            fprintf(stderr,"  File: %s, Size: %llu\n", f->name, (unsigned long long)f->size);
        }

        // --- Phase 2: Compute SHA256 + send data ---
        fprintf(stderr, "[PHASE 2] Hashing and sending data...\n");
        for (size_t i = 0; i < batch_files; i++) {
            fsp_file_entry_t *f = &entries->files[file_index + i];
            char full_path[PATH_MAX];
            // snprintf(full_path, sizeof(full_path), "%s/%s", state->fullpath, f->name);
            if (fsp_path_join(state->fullpath, f->name, full_path, sizeof(full_path)) < 0) {
               fprintf(stderr, "Error: absolute file path truncated: %s/%s\n", state->fullpath, f->name);
               return -1;
            }

            if (fsp_compute_file_and_chunks(full_path, f, state) < 0) {
                fprintf(stderr, "Error hashing file: %s\n", full_path);
                return -1;
            }

            fprintf(stderr,"  Sent: %s (SHA256: ", f->name);
            fsp_print_sha256(f->file_hash);
            fprintf(stderr,")\n");
        }

        // --- Phase 3: Send metadata with hashes ---
        fprintf(stderr, "[PHASE 3] Sending metadata with hashes...\n");
        for (size_t i = 0; i < batch_files; i++) {
            fsp_file_entry_t *f = &entries->files[file_index + i];
            fprintf(stderr,"  File: %s, SHA256: ", f->name);
            fsp_print_sha256(f->file_hash);
            fprintf(stderr,"\n");
        }

        // --- Free chunk hashes for this batch to save memory ---
        for (size_t i = 0; i < batch_files; i++) {
            free(entries->files[file_index + i].chunk_hashes);
            entries->files[file_index + i].chunk_hashes = NULL;
            entries->files[file_index + i].num_chunks = 0;
            entries->files[file_index + i].cap_chunks = 0;
        }

        file_index += batch_files;
    }

    // Reset directory entries for next directory
    entries->num_files = 0;
    state->current_files = 0;
    state->current_bytes = 0;

    return 0;
}
