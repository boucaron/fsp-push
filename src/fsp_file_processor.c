#include "fsp_file_processor.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <openssl/evp.h>

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

static fsp_file_entry_t *
fsp_append_file_entry(fsp_walker_state_t *state,
                  const char *name,
                  uint64_t size,
                  uint32_t depth)
{
    fsp_dir_entries_t *entries = &state->entries;

    if (entries->num_files >= entries->cap_files) {
        size_t new_cap = entries->cap_files ? entries->cap_files * 2 : 64;
        fsp_file_entry_t *nf =
            realloc(entries->files, new_cap * sizeof(*nf));
        if (!nf) return NULL;
        entries->files = nf;
        entries->cap_files = new_cap;
    }

    fsp_file_entry_t *f = &entries->files[entries->num_files++];
    memset(f, 0, sizeof(*f));

    strncpy(f->name, name, sizeof(f->name) - 1);
    f->size  = size;
    f->depth = depth;

    return f;
}

/* =========================================================================
 * SHA256 file + chunk hashing (single pass, EVP)
 * ========================================================================= */

static int
fsp_compute_file_and_chunks(const char *path, fsp_file_entry_t *entry)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    const int use_chunks = (entry->size >= FSP_CHUNK_SIZE);

    EVP_MD_CTX *file_ctx = EVP_MD_CTX_new();
    EVP_MD_CTX *chunk_ctx = NULL;

    if (!file_ctx) {
        fclose(fp);
        return -1;
    }

    if (use_chunks) {
        chunk_ctx = EVP_MD_CTX_new();
        if (!chunk_ctx) {
            fclose(fp);
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }
    }

    EVP_DigestInit_ex(file_ctx, EVP_sha256(), NULL);
    if (use_chunks)
        EVP_DigestInit_ex(chunk_ctx, EVP_sha256(), NULL);

    uint8_t buf[1024 * 1024]; /* 1 MB buffer */
    size_t  n;

    uint64_t chunk_bytes = 0;
    size_t   cap_chunks  = 0;

    unsigned char (*chunks)[SHA256_DIGEST_LENGTH] = NULL;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        EVP_DigestUpdate(file_ctx, buf, n);

        if (use_chunks) {
            EVP_DigestUpdate(chunk_ctx, buf, n);
            chunk_bytes += n;

            if (chunk_bytes >= FSP_CHUNK_SIZE) {
                if (entry->num_chunks >= cap_chunks) {
                    size_t new_cap = cap_chunks ? cap_chunks * 2 : 16;
                    void *tmp = realloc(
                        chunks,
                        new_cap * sizeof(*chunks)
                    );
                    if (!tmp) {
                        fclose(fp);
                        EVP_MD_CTX_free(file_ctx);
                        EVP_MD_CTX_free(chunk_ctx);
                        free(chunks);
                        return -1;
                    }
                    chunks = tmp;
                    cap_chunks = new_cap;
                }

                EVP_DigestFinal_ex(
                    chunk_ctx,
                    chunks[entry->num_chunks],
                    NULL
                );
                entry->num_chunks++;
                chunk_bytes = 0;

                EVP_DigestInit_ex(chunk_ctx, EVP_sha256(), NULL);
            }
        }
    }

    /*
     * Final partial chunk ONLY if chunks are enabled
     */
    if (use_chunks && chunk_bytes > 0) {
        if (entry->num_chunks >= cap_chunks) {
            size_t new_cap = cap_chunks ? cap_chunks * 2 : 16;
            void *tmp = realloc(
                chunks,
                new_cap * sizeof(*chunks)
            );
            if (!tmp) {
                fclose(fp);
                EVP_MD_CTX_free(file_ctx);
                EVP_MD_CTX_free(chunk_ctx);
                free(chunks);
                return -1;
            }
            chunks = tmp;
            cap_chunks = new_cap;
        }

        EVP_DigestFinal_ex(
            chunk_ctx,
            chunks[entry->num_chunks],
            NULL
        );
        entry->num_chunks++;
    }

    EVP_DigestFinal_ex(file_ctx, entry->file_hash, NULL);

    if (use_chunks) {
        entry->chunk_hashes = chunks;
        entry->cap_chunks   = cap_chunks;
    } else {
        entry->chunk_hashes = NULL;
        entry->num_chunks   = 0;
        entry->cap_chunks   = 0;
    }

    fclose(fp);
    EVP_MD_CTX_free(file_ctx);
    if (chunk_ctx)
        EVP_MD_CTX_free(chunk_ctx);

    return 0;
}


/* =========================================================================
 * Public API
 * ========================================================================= */

void
fsp_file_processor_init(fsp_walker_state_t *state)
{
    memset(&state->entries, 0, sizeof(state->entries));
    state->current_files = 0;
    state->current_bytes = 0;
}

int
fsp_file_processor_process_file(const char *full_path,
                                const char *rel_path,
                                const struct stat *st,
                                fsp_walker_state_t *state)
{
    fsp_file_entry_t *entry =
        fsp_append_file_entry(state, rel_path, st->st_size,
                          state->current_depth);
    if (!entry)
        return -1;

    if (fsp_compute_file_and_chunks(full_path, entry) < 0) {
        fprintf(stderr, "hash failed: %s\n", full_path);
        return -1;
    }

    state->current_files++;
    state->current_bytes += st->st_size;

    if ((state->max_files && state->current_files >= state->max_files) ||
        (state->max_bytes && state->current_bytes >= state->max_bytes)) {
        return fsp_file_processor_flush_batch(state);
    }

    return 0;
}

static void fsp_print_sha256(const unsigned char hash[SHA256_DIGEST_LENGTH])
{
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        fprintf(stderr,"%02x", hash[i]);
}



static void fsp_file_processor_flush_batch_debug(fsp_walker_state_t *state) {
    fprintf(stderr,"\n=== FILE_LIST SEND ===\n");
    fprintf(stderr,"Files      : %zu\n", state->entries.num_files);
    fprintf(stderr,"Total bytes: %llu\n",
           (unsigned long long)state->current_bytes);
    fprintf(stderr,"----------------------\n");

    for (size_t i = 0; i < state->entries.num_files; i++) {
        fsp_file_entry_t *f = &state->entries.files[i];

        fprintf(stderr,"File #%zu\n", i + 1);
        fprintf(stderr,"  Path   : %s\n", f->name);
        fprintf(stderr,"  Size   : %llu bytes\n",
               (unsigned long long)f->size);
        fprintf(stderr,"  Depth  : %u\n", f->depth);

        fprintf(stderr,"  SHA256 : ");
        fsp_print_sha256(f->file_hash);
        fprintf(stderr,"\n");

        if (f->num_chunks > 0) {
            fprintf(stderr,"  Chunks : %llu (size=%llu bytes)\n",
                   (unsigned long long)f->num_chunks,
                   (unsigned long long)FSP_CHUNK_SIZE);

            for (uint64_t c = 0; c < f->num_chunks; c++) {
                fprintf(stderr,"    [%3llu] ",
                       (unsigned long long)c);
                fsp_print_sha256(f->chunk_hashes[c]);
                fprintf(stderr,"\n");
            }
        }
        fprintf(stderr,"\n");
    }

    fprintf(stderr,"=== END FILE_LIST ===\n");
}

int
fsp_file_processor_flush_batch(fsp_walker_state_t *state)
{
    if (state->entries.num_files == 0)
        return 0;

    fprintf(stderr, ">>> Sending batch: %zu files, %llu bytes\n",
           state->entries.num_files,
           (unsigned long long)state->current_bytes);
    fsp_file_processor_flush_batch_debug(state);                 

    for (size_t i = 0; i < state->entries.num_files; i++) {
        free(state->entries.files[i].chunk_hashes);
        state->entries.files[i].chunk_hashes = NULL;
        state->entries.files[i].num_chunks   = 0;
        state->entries.files[i].cap_chunks   = 0;
    }

    state->entries.num_files = 0;
    state->current_files = 0;
    state->current_bytes = 0;

    return 0;
}
