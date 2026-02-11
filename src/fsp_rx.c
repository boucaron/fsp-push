
#include "fsp.h"
#include "fsp_walk.h"
#include "fsp_rx.h"

#include <sys/stat.h>
#include <errno.h>

#include <openssl/evp.h>

// -----------------------------------------------------------------------------
// State handlers
// -----------------------------------------------------------------------------
static int fsp_rx_handle_version(fsp_receiver_state_t *rx, FILE *fp) {
    char line[64];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) return -1;

    if (sscanf(line, "VERSION: %d", &rx->version) != 1) return -1;

    rx->state = FSP_RX_EXPECT_MODE;
    return 0;
}

static int fsp_rx_handle_mode(fsp_receiver_state_t *rx, FILE *fp) {
    char line[PATH_MAX + 128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) return -1;

    if (strncmp(line, "MODE: ", 6) != 0) return -1;

    if (strcmp(line+6, "overwrite") == 0) rx->mode = FSP_OVERWRITE_ALWAYS;
    else if (strcmp(line+6, "skip") == 0) rx->mode = FSP_SKIP_IF_EXISTS;
    else if (strcmp(line+6, "hash") == 0) rx->mode = FSP_OVERWRITE_IF_HASH_DIFFERS;
    else if (strcmp(line+6, "fail") == 0) rx->mode = FSP_FAIL_IF_EXISTS;
    else {
         fprintf(stderr, "fsp_rx_handle_mode: unknown mode %s", line);
         return -1;
    }

    rx->state = FSP_RX_IDLE;
    return 0;
}

static int fsp_rx_handle_idle(fsp_receiver_state_t *rx, FILE *fp) {
    char line[PATH_MAX + 128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) return -1;

    if (strncmp(line, "DIRECTORY: ", 11) == 0) {
        strncpy(rx->current_dir, line+11, sizeof(rx->current_dir)-1);
        rx->current_dir[sizeof(rx->current_dir)-1] = '\0';


        // Create directory with default permissions (honoring umask)
        char targetdir[PATH_MAX+3];
        snprintf(targetdir, sizeof(targetdir), "%s/%s", rx->target_path, rx->current_dir);
        targetdir[PATH_MAX-1] = '\0';
        if (mkdir(targetdir, 0755) < 0) {
            if (errno != EEXIST) {
                perror("mkdir");
                return -1;
            }
        }


        rx->state = FSP_RX_EXPECT_FILE_LIST;
        return 0;
    }

    if (strcmp(line, "END") == 0) {
        rx->state = FSP_RX_DONE;
        return 0;
    }

    return -1;
}

static int fsp_rx_handle_file_list(fsp_receiver_state_t *rx, FILE *fp) {
    char line[128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) return -1;

    if (strcmp(line, "FILE_LIST") != 0) {
        fprintf(stderr, "Expected FILE_LIST, got: %s\n", line);
        return -1;
    }

    // Move to next state: expect file count
    rx->state = FSP_RX_EXPECT_FILE_COUNT;
    return 0;
}

static int fsp_rx_handle_file_count(fsp_receiver_state_t *rx, FILE *fp) {
    char line[128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) return -1;

    size_t count = 0;
    if (sscanf(line, "FILES: %zu", &count) != 1) {
        fprintf(stderr, "Expected FILES: N, got: %s\n", line);
        return -1;
    }

    if ( count > FSP_MAX_FILES_PER_LIST ) {
        fprintf(stderr, "Attempt to push more files than FSP_MAX_FILES_PER_LIST\n");
        return -1;
    }

    rx->expected_files = count;
    rx->files_received = 0;

    // Allocate entries if needed
    if (rx->entries_capacity < count) {
        fsp_file_entry_t *new_entries = (fsp_file_entry_t*)realloc(rx->entries, count * sizeof(fsp_file_entry_t));
        if (!new_entries) {
            perror("realloc");
            return -1;
        }
        rx->entries = new_entries;
        rx->entries_capacity = count;
    }

    // Zero out entries
    for (size_t i = 0; i < count; i++) {
        memset(&rx->entries[i], 0, sizeof(fsp_file_entry_t));
    }

    // Move to next state: expect file metadata
    rx->state = FSP_RX_EXPECT_FILE_METADATA;
    return 0;
}

static int fsp_rx_handle_file_metadata(fsp_receiver_state_t *rx, FILE *fp) {
  

    for (size_t i = rx->files_received; i < rx->expected_files; i++) {
        fsp_file_entry_t *entry = &rx->entries[i];

        // 1 Read filename length (uint16_t, network byte order)
        uint8_t name_len_buf[2];
        if (fsp_rx_read_full(fp, name_len_buf, sizeof(name_len_buf)) < 0) return -1;
        uint16_t name_len = ((uint16_t)name_len_buf[0] << 8) | name_len_buf[1];

        // Optional safety: limit path length
        if (name_len > NAME_MAX) return -1;

        // 2️ Read filename
        if (fsp_rx_read_full(fp, entry->name, name_len) < 0) return -1;
        entry->name[name_len] = '\0'; // null-terminate

        // 3️ Read file size (uint64_t, network byte order)
        uint64_t size_be;
        if (fsp_rx_read_full(fp, &size_be, sizeof(size_be)) < 0) return -1;
        entry->size = be64toh(size_be);

        // 4️ Read file hash -- PLACEHOLDER
        if (fsp_rx_read_full(fp, entry->file_hash, SHA256_DIGEST_LENGTH) < 0) return -1;

        // 5️ Read number of chunks (uint64_t, network byte order) -- SHOULD BE ZERO AT THIS STAGE
        uint64_t num_chunks_be;
        if (fsp_rx_read_full(fp, &num_chunks_be, sizeof(num_chunks_be)) < 0) return -1;
        entry->num_chunks = be64toh(num_chunks_be);

        // Optional safety: impose maximum number of chunks
       
        // 6️ Allocate chunk hashes array if needed
        if (entry->num_chunks > 0) {
            entry->chunk_hashes = malloc(entry->num_chunks * SHA256_DIGEST_LENGTH);
            if (!entry->chunk_hashes) return -1;
            entry->cap_chunks = entry->num_chunks;

            // Read chunk hashes from the stream
            if (fsp_rx_read_full(fp, entry->chunk_hashes, entry->num_chunks * SHA256_DIGEST_LENGTH) < 0) {
                free(entry->chunk_hashes);
                entry->chunk_hashes = NULL;
                return -1;
            }
        } else {
            entry->chunk_hashes = NULL;
            entry->cap_chunks = 0;
        }
       


        // DEBUG
        fprintf(stderr, "fsp_rx_handle_file_metadata - Received file: %s (%lu bytes, %lu chunks)\n", 
            entry->name, entry->size, entry->num_chunks);
    }

    // Move to next state: file data
    rx->state = FSP_RX_EXPECT_FILE_DATA;
    return 0;
}


// The caller is responsible for closing the out file
static int fsp_rx_handle_small_file(fsp_receiver_state_t *rx, fsp_file_entry_t *entry, FILE *fp, FILE *out) {

    EVP_MD_CTX *file_ctx = EVP_MD_CTX_new();
    if (!file_ctx) return -1;

    if (EVP_DigestInit_ex(file_ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(file_ctx);
        return -1;
    }

    uint8_t *buf = rx->proto_buf;
    size_t buf_size = rx->proto_buf_size;
    size_t remaining = entry->size;

    while (remaining > 0) {
        size_t chunk = (remaining < buf_size) ? remaining : buf_size;
        size_t n = fread(buf, 1, chunk, fp);
        if (n == 0) {
            if (ferror(fp)) {
                perror("fread");
                EVP_MD_CTX_free(file_ctx);
                return -1;
            }
            fprintf(stderr, "Unexpected EOF while reading small file\n");
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }

        rx->total_bytes += n;
        remaining -= n;

        if (EVP_DigestUpdate(file_ctx, buf, n) != 1) {
            fprintf(stderr, "EVP_DigestUpdate failed\n");
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }

        size_t w = fwrite(buf, 1, n, out);
        if (w != n) {
            perror("fwrite");
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }
    }

    // Store final SHA256 hash
    if (EVP_DigestFinal_ex(file_ctx, entry->file_hash, NULL) != 1) {
        fprintf(stderr, "EVP_DigestFinal_ex failed\n");
        EVP_MD_CTX_free(file_ctx);
        return -1;
    }

    EVP_MD_CTX_free(file_ctx);
    return 0;
}

// The caller is responsible for closing the out file
static int fsp_rx_handle_chunked_file(fsp_receiver_state_t *rx, fsp_file_entry_t *entry, FILE *fp, FILE *out) {


    return 0;
}


static int fsp_rx_handle_file_data(fsp_receiver_state_t *rx, FILE *fp) {
    if (rx->expected_files == 0) return -1;
    

    for (size_t i = 0; i < rx->expected_files; i++) {
        fsp_file_entry_t *entry = &rx->entries[i];

        if (entry->size == 0) continue; // skip empty files
        char filepath[PATH_MAX+512];
        if ( strlen(rx->current_dir) > 0 ) {
            snprintf(filepath, sizeof(filepath), "%s/%s/%s", rx->target_path, rx->current_dir, entry->name);
        } else {
            snprintf(filepath, sizeof(filepath), "%s/%s", rx->target_path, entry->name);
        }

        FILE *out = fopen(filepath, "wb");
        if (!out) {
            perror("fopen");
            return -1;
        }
       
        if (entry->size > FSP_CHUNK_SIZE) {
           if (fsp_rx_handle_chunked_file(rx, entry, fp, out) < 0) {
                fclose(out);
                return -1;
            }
        } else {
             if (fsp_rx_handle_small_file(rx, entry, fp, out) < 0) {
                fclose(out);
                return -1;
            }            
        }

        fclose(out);
    }

    // After reading all file data, transition to Phase 3 (hashes)
    rx->state = FSP_RX_EXPECT_FILE_HASHES;
    return 0;
}


static int fsp_rx_handle_file_hashes(fsp_receiver_state_t *rx, FILE *fp) {


    for (size_t i = rx->files_received; i < rx->expected_files; i++) {
        fsp_file_entry_t *entry = &rx->entries[i];

        // 1 Read filename length (uint16_t, network byte order)
        uint16_t name_len_be;
        if (fsp_rx_read_full(fp, &name_len_be, sizeof(name_len_be)) < 0) return -1;
        uint16_t name_len = be16toh(name_len_be);

        // Optional safety: limit path length
        if (name_len > NAME_MAX) return -1;

        // 2️ Read filename and cross check
        char     name[NAME_MAX + 1];
        memset(name, 0, sizeof(name));
        if (fsp_rx_read_full(fp, name, name_len) < 0) return -1;
        if ( strcmp(entry->name, name) != 0 ) return -1;

        // 3️ Read file size (uint64_t, network byte order) and cross check
        uint64_t size_be;
        if (fsp_rx_read_full(fp, &size_be, sizeof(size_be)) < 0) return -1;
        uint64_t size = be64toh(size_be);
        if (entry->size != size) return -1;        

        // 4️ Read file hash - REAL DATA
        unsigned char file_hash[SHA256_DIGEST_LENGTH];        
        if (fsp_rx_read_full(fp, file_hash, SHA256_DIGEST_LENGTH) < 0) return -1;
        // cross check file_hash entry->file_hash
        if ( memcmp(file_hash, entry->file_hash, SHA256_DIGEST_LENGTH) != 0) return -1;

        // 5️ Read number of chunks (uint64_t, network byte order) - NOT ZERO IF REAL DATA & cross check
        uint64_t num_chunks_be;
        if (fsp_rx_read_full(fp, &num_chunks_be, sizeof(num_chunks_be)) < 0) return -1;
        uint64_t num_chunks = be64toh(num_chunks_be);
        if (entry->num_chunks != num_chunks) return -1;
    

        // Optional safety: impose maximum number of chunks
       
        // 6️ Allocate chunk hashes array if needed
        if (num_chunks > 0) {
            unsigned char *chunk_hashes = malloc(num_chunks * SHA256_DIGEST_LENGTH);
            if (!chunk_hashes) return -1;            

            if (fsp_rx_read_full(fp, chunk_hashes, num_chunks * SHA256_DIGEST_LENGTH) < 0) {
                free(chunk_hashes);
                return -1;
            }

            // Cross-check against metadata copy
            if (memcmp(chunk_hashes, entry->chunk_hashes, num_chunks * SHA256_DIGEST_LENGTH) != 0) {
                free(chunk_hashes);
                return -1;
            }

            // If we allocated a temporary buffer, free it
            free(chunk_hashes);
        }         

        // Optional: psychokouak mode, we reread the writen file
        //  And we ensure that the SHA256 and the chunks are correct


        // DEBUG
        fprintf(stderr, "fsp_rx_handle_file_hashes - Cross check OK : %s (%lu bytes, %lu chunks)\n", 
            entry->name, entry->size, entry->num_chunks);
       
        // Eveything is fine and cross checked
        rx->files_received++;
    }


    // Done with this batch, go back to idle to receive new directory or END
    rx->state = FSP_RX_IDLE;
    return 0;
}


// -----------------------------------------------------------------------------
// Main dispatcher function
// -----------------------------------------------------------------------------
int fsp_receiver_process_line(fsp_receiver_state_t *rx, FILE *fp) {
    switch (rx->state) {
        case FSP_RX_EXPECT_VERSION:
            return fsp_rx_handle_version(rx, fp);
        case FSP_RX_EXPECT_MODE:
            return fsp_rx_handle_mode(rx, fp);
        case FSP_RX_IDLE:
            return fsp_rx_handle_idle(rx, fp);
        case FSP_RX_EXPECT_FILE_LIST:
            return fsp_rx_handle_file_list(rx, fp);
        case FSP_RX_EXPECT_FILE_COUNT:
            return fsp_rx_handle_file_count(rx, fp);
        case FSP_RX_EXPECT_FILE_METADATA:
            return fsp_rx_handle_file_metadata(rx, fp);
        case FSP_RX_EXPECT_FILE_DATA:
            return fsp_rx_handle_file_data(rx, fp);
        case FSP_RX_EXPECT_FILE_HASHES:
            return fsp_rx_handle_file_hashes(rx, fp);
        case FSP_RX_DONE:
            return 0; // Already done
        default:
            fprintf(stderr,"fsp_receiver_process_line: Unknown state \n");
            return -1;
    }
}