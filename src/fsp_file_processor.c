#include "fsp_file_processor.h"
#include "fsp_progress.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h> 
#include <openssl/evp.h>



void fsp_file_processor_progressbar(fsp_walker_state_t *state) {

     /* Throttle updates:
       - every 100 files
       - or every 10 MB */
    int files_trigger = (state->total_files % 100) == 0;
    int bytes_trigger = 
        state->total_bytes >= state->last_speed_bytes + (10ULL << 20);

    if (!files_trigger && !bytes_trigger)
        return;

    /* --- Update throughput ONLY on byte trigger --- */
    if (bytes_trigger) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        uint64_t delta_bytes =
            state->total_bytes - state->last_speed_bytes;

        double delta_time =
            timespec_diff_sec(now, state->last_speed_ts);

        if (delta_time > 0.0)
            state->last_throughput = delta_bytes / delta_time;

        state->last_speed_bytes = state->total_bytes;
        state->last_speed_ts = now;
    }

    /* --- Formatting --- */

    char total_buf[64], sent_buf[64];
    fsp_print_size(state->dry_run->file_total_size,
                   total_buf, sizeof(total_buf));
    fsp_print_size(state->total_bytes,
                   sent_buf, sizeof(sent_buf));

    int is_terminal = isatty(fileno(stderr));

    if (is_terminal) {
        // Clear current line
        fprintf(stderr, "\r\033[2K");
    } else {
        // Non-terminal: just print a newline
        fprintf(stderr, "\n");
        fflush(stderr);
    }    

    fprintf(stderr,
        ANSI_BOLD "Progress:" ANSI_RESET " "
        "Files " ANSI_CYAN "%" PRIu64 "/%" PRIu64 ANSI_RESET "  "
        "Data " ANSI_GREEN "%s/%s" ANSI_RESET,
        state->total_files,
        state->dry_run->file_count,
        sent_buf,
        total_buf
    );

    /* --- Display last known speed --- */
    if (state->last_throughput > 0.0) {
        char speed_buf[32];
        fsp_print_size((uint64_t)state->last_throughput,
                       speed_buf, sizeof(speed_buf));
       fprintf(stderr,
            "  Speed " ANSI_MAGENTA "%s/s" ANSI_RESET,
            speed_buf);


        /* ETA */
        uint64_t remaining_bytes = state->dry_run->file_total_size - state->total_bytes;
        double eta_sec = (double)remaining_bytes / state->last_throughput;

        int hours   = (int)(eta_sec / 3600);
        int minutes = (int)((eta_sec - hours * 3600) / 60);
        int seconds = (int)(eta_sec - hours * 3600 - minutes * 60);

        fprintf(stderr,
                "  ETA " ANSI_YELLOW "%02d:%02d:%02d" ANSI_RESET,
                hours, minutes, seconds);     
    }

    fflush(stderr);
}


/*
 * Small file no chunk
 * 
 * READ FILE
 *  - SEND DATA
 *  - HASH DATA 
 * 
 * Hash of the file stored in file_hash
 * 
 */
static int fsp_compute_small_file(const char *path,
                                  fsp_file_entry_t *entry,
                                  fsp_walker_state_t *state)
{
    if (!state->file_buf || state->file_buf_size == 0) {
        fprintf(stderr, "fsp_compute_small_file: invalid state buffer\n");
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    EVP_MD_CTX *file_ctx = EVP_MD_CTX_new();
    if (!file_ctx) {
        fclose(fp);
        return -1;
    }

    if (EVP_DigestInit_ex(file_ctx, EVP_sha256(), NULL) != 1) {
        fclose(fp);
        EVP_MD_CTX_free(file_ctx);
        return -1;
    }

    uint8_t *buf = state->file_buf;
    size_t buf_size = state->file_buf_size;    

    for (;;) {
        size_t n = fread(buf, 1, buf_size, fp);
        if (n == 0) {
            if (ferror(fp)) {
                perror("fread");
                fclose(fp);
                EVP_MD_CTX_free(file_ctx);
                return -1;
            }
            break; // EOF
        }

        state->total_bytes += n;

        if (EVP_DigestUpdate(file_ctx, buf, n) != 1) {
            fprintf(stderr, "EVP_DigestUpdate failed\n");
            fclose(fp);
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }
        
        // Push into big protocol buffer (handles aggregation + flushing)
        if (fsp_bw_push(&state->protowritebuf, buf, n) < 0) {
            fprintf(stderr, "fsp_bw_push failed\n");
            fclose(fp);
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }
        
                
    }

    // Store final hash
    EVP_DigestFinal_ex(file_ctx, entry->file_hash, NULL);

    fclose(fp);
    EVP_MD_CTX_free(file_ctx);
    return 0;
}


/*
    Big file Hash
    Cut the file by 128MB offsets
    Hash individually in chunks
    Finally take all SHA256 of each chunks and compute the SHA256 of it 
     aka. Merkle Level 0

     Hash of the Merkle Level 0 stored in file_hash
     Individual hashes of the chunks

*/
static int fsp_compute_big_file(const char *path,
                                fsp_file_entry_t *entry,
                                fsp_walker_state_t *state)
{
    if (!state->file_buf || state->file_buf_size == 0) {
        fprintf(stderr, "fsp_compute_big_file: invalid state buffer\n");
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    uint64_t total_chunks = (entry->size + FSP_CHUNK_SIZE - 1) / FSP_CHUNK_SIZE;

    // Ensure chunk_hashes array has enough capacity
    if (entry->cap_chunks < total_chunks) {
        unsigned char (*tmp)[SHA256_DIGEST_LENGTH] = realloc(entry->chunk_hashes,
                                                total_chunks * sizeof(*entry->chunk_hashes));
        if (!tmp) {
            perror("realloc");
            fclose(fp);
            return -1;
        }
        entry->chunk_hashes = tmp;
        entry->cap_chunks = total_chunks;
    }

    entry->num_chunks = 0;

    EVP_MD_CTX *chunk_ctx  = EVP_MD_CTX_new();
    EVP_MD_CTX *merkle_ctx = EVP_MD_CTX_new();
    if (!chunk_ctx || !merkle_ctx) {
        fclose(fp);
        EVP_MD_CTX_free(chunk_ctx);
        EVP_MD_CTX_free(merkle_ctx);
        return -1;
    }

    EVP_DigestInit_ex(merkle_ctx, EVP_sha256(), NULL);

    uint8_t *buf = state->file_buf;
    size_t buf_size = state->file_buf_size;    
    uint64_t remaining_bytes = entry->size;

    while (remaining_bytes > 0) {
        EVP_DigestInit_ex(chunk_ctx, EVP_sha256(), NULL);

        uint64_t chunk_bytes = remaining_bytes < FSP_CHUNK_SIZE ? remaining_bytes : FSP_CHUNK_SIZE;
        uint64_t processed = 0;

        while (processed < chunk_bytes) {
            size_t to_read = buf_size;
            if (to_read > chunk_bytes - processed)
                to_read = (size_t)(chunk_bytes - processed);

            size_t n = fread(buf, 1, to_read, fp);
            if (n == 0) {
                if (ferror(fp)) {
                    perror("fread");
                    fclose(fp);
                    EVP_MD_CTX_free(chunk_ctx);
                    EVP_MD_CTX_free(merkle_ctx);
                    return -1;
                }
                break;
            }

            state->total_bytes += n;
            fsp_file_processor_progressbar(state);

            // Update SHA256 for this chunk
            if (EVP_DigestUpdate(chunk_ctx, buf, n) != 1) {
                fprintf(stderr, "EVP_DigestUpdate failed\n");
                fclose(fp);
                EVP_MD_CTX_free(chunk_ctx);
                EVP_MD_CTX_free(merkle_ctx);
                return -1;
            }

            // Push to big output buffer (handles aggregation + flushing)
            if (fsp_bw_push(&state->protowritebuf, buf, n) < 0) {
                fprintf(stderr, "fsp_bw_push failed\n");
                fclose(fp);
                EVP_MD_CTX_free(chunk_ctx);
                EVP_MD_CTX_free(merkle_ctx);
                return -1;
            }
           
            processed += n;
        }

        // Finalize chunk hash
        EVP_DigestFinal_ex(chunk_ctx, entry->chunk_hashes[entry->num_chunks], NULL);

        // Update Merkle Level 0
        EVP_DigestUpdate(merkle_ctx, entry->chunk_hashes[entry->num_chunks], SHA256_DIGEST_LENGTH);

        entry->num_chunks++;
        remaining_bytes -= chunk_bytes;
    }

    // Final Merkle hash
    EVP_DigestFinal_ex(merkle_ctx, entry->file_hash, NULL);

    fclose(fp);
    EVP_MD_CTX_free(chunk_ctx);
    EVP_MD_CTX_free(merkle_ctx);

    return 0;
}


/* =========================================================================
 * SHA256 file hashing 
 * 
 * If file is under 128MB there is no chunk and the hash is the file hash
 * Otherwise, it is the Merkle 0 hash of the chunks: all chunks are SHA256 
 *  hashes made individually (can be parallelized) and the resulting hash
 *  is the hash of the chunks' hashes 
 * 
 * ========================================================================= */
static int
fsp_compute_file_and_chunks(const char *path,
                            fsp_file_entry_t *entry,
                            fsp_walker_state_t *state)
{
    int ret;
    if ( entry->size > FSP_CHUNK_SIZE ) {
        ret = fsp_compute_big_file(path, entry, state);
    } else {
        ret = fsp_compute_small_file(path, entry, state);
    }
    return ret;
}

/* =========================================================================
 * Debug helpers
 * ========================================================================= */
/* static void fsp_print_sha256(const unsigned char hash[SHA256_DIGEST_LENGTH])
{
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        fprintf(stderr,"%02x", hash[i]);
} */

/* =========================================================================
 * Public API
 * ========================================================================= */

void fsp_file_processor_init(fsp_walker_state_t *state)
{
    memset(&state->entries, 0, sizeof(state->entries));
    state->current_files = 0;
    state->current_bytes = 0;
}


static int fsp_send_file_entry_buf(fsp_buf_writer_t *bw,
                                   const fsp_file_entry_t *entry)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Push fixed-size fields into the buffer
    size_t len = strnlen(entry->name, NAME_MAX);
    if (len > UINT16_MAX) return -1;
    uint16_t name_len_be = htobe16((uint16_t)len);
    if (fsp_bw_push(bw, &name_len_be, sizeof(name_len_be)) < 0) return -1; // Send name size
    if (fsp_bw_push(bw, entry->name, len) < 0) return -1;

    uint64_t size_be = htobe64(entry->size);
    if (fsp_bw_push(bw, &size_be, sizeof(size_be)) < 0) return -1; // Send filesize
    if (fsp_bw_push(bw, entry->file_hash, SHA256_DIGEST_LENGTH) < 0) return -1;

    uint64_t num_chunks_be = htobe64(entry->num_chunks); // Send numchunks
    if (fsp_bw_push(bw, &num_chunks_be, sizeof(num_chunks_be)) < 0) return -1;

    // Push dynamic chunk hashes, if present
    if (entry->num_chunks > 0 && entry->chunk_hashes) {        
        size_t chunks_size = entry->num_chunks * SHA256_DIGEST_LENGTH;
        if (fsp_bw_push(bw, entry->chunk_hashes, chunks_size) < 0) return -1;
    }

    return 0;
}

/**
 * Main directory callback implementing **three-phase batching**
 */
int fsp_file_processor_process_directory(fsp_walker_state_t *state)
{
    fsp_dir_entries_t *entries = &state->entries;
    size_t total_files = entries->num_files;

    size_t file_index = 0;

#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif


    // --- SEND Directory: <directoryname> \n ---
    char dir_line[PATH_MAX + 32]; // extra space for prefix and newline
    int na = snprintf(dir_line, sizeof(dir_line), "DIRECTORY: %s\n", state->relpath);
    if (na < 0 || (size_t)na >= sizeof(dir_line)) {
        fprintf(stderr, "fsp_send_directory: snprintf error\n");
        return -1;
    }        
    int ret = fsp_bw_push(&state->protowritebuf, dir_line, na);
    if ( ret < 0 ) {
        return ret;
    }
    ret = fsp_bw_flush(&state->protowritebuf);
    if (ret < 0) {
        return ret;
    }

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
        // Send FILE_LIST header ---
        const char *header = "FILE_LIST\n";
        size_t header_len = strlen(header);        
        ret = fsp_bw_push(&state->protowritebuf, header, header_len);
        if ( ret < 0 ) {
            return ret;
        }
        ret = fsp_bw_flush(&state->protowritebuf);
        if (ret < 0) {
            return ret;
        }

        // --- Send FILES: <count>\n header ---
        char files_line[64];
        int n = snprintf(files_line, sizeof(files_line), "FILES: %zu\n", batch_files);
        if (n < 0 || (size_t)n >= sizeof(files_line)) {
            fprintf(stderr, "fsp_send_file_list_batch: snprintf error\n");
            return -1;
        }      
        ret =  fsp_bw_push(&state->protowritebuf, files_line, n);
        if ( ret < 0 ) {
            return ret;
        }
        ret = fsp_bw_flush(&state->protowritebuf);
        if (ret < 0) {
            return ret;
        }

        // fprintf(stderr, "\n[PHASE 1] Sending metadata for batch starting at file %zu\n", file_index);                
        for (size_t i = 0; i < batch_files; i++) {
            fsp_file_entry_t *f = &entries->files[file_index + i];
            // VERBOSE
            // fprintf(stderr,"  File: %s, Size: %llu\n", f->name, (unsigned long long)f->size);
           
            int ret = fsp_send_file_entry_buf(&state->protowritebuf, f);
            if ( ret < 0 ) {
                return ret;
            }
        }
        ret = fsp_bw_flush(&state->protowritebuf);
        if ( ret < 0 ) { 
            return ret; 
        }

        // --- Phase 2: Compute SHA256 + send data ---
        // fprintf(stderr, "[PHASE 2] Hashing and sending data...\n");
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

            // VERBOSE
            // fprintf(stderr,"  Sent: %s (SHA256: ", f->name);
            // fsp_print_sha256(f->file_hash);
            // fprintf(stderr,")\n");

            state->total_files++;

            // DEBUG
            fsp_file_processor_progressbar(state);

        }
        ret = fsp_bw_flush(&state->protowritebuf);
        if ( ret < 0 ) { 
            return ret; 
        }


        // --- Phase 3: Send metadata with hashes ---
        // --- Send FILES: <count>\n header ---
        char hfiles_line[64];
        int nn = snprintf(hfiles_line, sizeof(hfiles_line), "HASH FILES: %zu\n", batch_files);
        if (nn < 0 || (size_t)nn >= sizeof(hfiles_line)) {
            fprintf(stderr, "fsp_send_file_list_batch: snprintf error\n");
            return -1;
        }        
        ret =  fsp_bw_push(&state->protowritebuf, hfiles_line, nn);
        if ( ret < 0 ) {
            return ret;
        }
        ret = fsp_bw_flush(&state->protowritebuf);
        if (ret < 0) {
            return ret;
        }

        // fprintf(stderr, "[PHASE 3] Sending metadata with hashes...\n");
        for (size_t i = 0; i < batch_files; i++) {
            fsp_file_entry_t *f = &entries->files[file_index + i];

            int ret = fsp_send_file_entry_buf(&state->protowritebuf, f);
            if ( ret < 0 ) {
                return ret;
            }

          // VERBOSE
          //  fprintf(stderr,"  File: %s, SHA256: ", f->name);
          //  fsp_print_sha256(f->file_hash);
          //  fprintf(stderr,"\n");
        }    
        ret = fsp_bw_flush(&state->protowritebuf);
        if ( ret < 0 ) { 
            return ret; 
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
