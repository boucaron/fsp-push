
#include "fsp.h"
#include "fsp_walk.h"
#include "fsp_rx.h"
#include "fsp_progress.h"

#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <openssl/evp.h>


// Progress Bar
void fsp_receiver_progressbar(fsp_receiver_state_t *rx, int force)
{
    /* Throttle:
       - every 100 files
       - or every 10 MB */
    int files_trigger = (rx->total_files % THRESHOLD_FILES) == 0;
    int bytes_trigger =
        rx->total_bytes >= rx->last_speed_bytes + (THRESHOLD_DATA<< 20);

    if ( force <= 0 ) {
        if (!files_trigger && !bytes_trigger)
            return;
    }

    /* --- Update throughput ONLY on byte trigger --- */
    if (bytes_trigger) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        uint64_t delta_bytes =
            rx->total_bytes - rx->last_speed_bytes;

        double delta_time =
            timespec_diff_sec(now, rx->last_speed_ts);

        if (delta_time > 0.0)
            rx->last_throughput = delta_bytes / delta_time;

        rx->last_speed_bytes = rx->total_bytes;
        rx->last_speed_ts = now;
    }

    char total_buf[64], recv_buf[64];

    fsp_print_size(rx->expected_total_bytes,
                   total_buf, sizeof(total_buf));

    fsp_print_size(rx->total_bytes,
                   recv_buf, sizeof(recv_buf));

    fprintf(stderr, "\r\033[2K");

    fprintf(stderr,
        ANSI_BOLD "Receiving:" ANSI_RESET " "
        "Files " ANSI_CYAN "%" PRIu64 "/%" PRIu64 ANSI_RESET "  "
        "Data " ANSI_GREEN "%s/%s" ANSI_RESET,
        rx->total_files,
        rx->expected_total_files,
        recv_buf,
        total_buf
    );

    /* --- Display speed + ETA --- */
    if (rx->last_throughput > 0.0) {

        char speed_buf[32];
        fsp_print_size((uint64_t)rx->last_throughput,
                       speed_buf, sizeof(speed_buf));

        fprintf(stderr,
                "  Speed " ANSI_MAGENTA "%s/s" ANSI_RESET,
                speed_buf);

        /* ETA */
        uint64_t remaining_bytes =
            rx->expected_total_bytes - rx->total_bytes;

        double eta_sec =
            (double)remaining_bytes / rx->last_throughput;

        int hours   = (int)(eta_sec / 3600);
        int minutes = (int)((eta_sec - hours * 3600) / 60);
        int seconds = (int)(eta_sec - hours * 3600 - minutes * 60);

        fprintf(stderr,
                "  ETA " ANSI_YELLOW "%02d:%02d:%02d" ANSI_RESET,
                hours, minutes, seconds);
    }

    fflush(stderr);
}
// -----------------------------------------------------------------------------
// State handlers
// -----------------------------------------------------------------------------
static int fsp_rx_handle_version(fsp_receiver_state_t *rx, FILE *fp) {
    char line[64];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_handle_version, no data error\n");
        return -1;
    }

    if (sscanf(line, "VERSION: %d", &rx->version) != 1) {
        fprintf(stderr,"fsp_rx_handle_version, waiting for VERSION: N\n");
        return -1;
    }
    if ( rx->version != FSP_VERSION) {
        fprintf(stderr, "fsp_rx_handle_version unsupported version %d\n", rx->version);
        return -1;
    }

    rx->state = FSP_RX_EXPECT_MODE;
    return 0;
}

static int fsp_rx_handle_mode(fsp_receiver_state_t *rx, FILE *fp) {
    char line[PATH_MAX + 128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_handle_mode, no data error\n");
        return -1;
    }

    if (strncmp(line, "MODE: ", 6) != 0) {
        fprintf(stderr,"fsp_rx_handle_mode, waiting for MODE: N\n");
        return -1;
    }

    if (strcmp(line+6, "append") == 0) rx->mode = FSP_APPEND;
    else if (strcmp(line+6, "update") == 0) rx->mode = FSP_UPDATE;
    else if (strcmp(line+6, "safe") == 0) rx->mode = FSP_SAFE;
    else if (strcmp(line+6, "force") == 0) rx->mode = FSP_FORCE;
    else {
         fprintf(stderr, "fsp_rx_handle_mode: unknown mode %s", line);
         return -1;
    }

    rx->state = FSP_RX_STAT_BYTES;
    return 0;
}


static int fsp_rx_stat_bytes(fsp_receiver_state_t *rx, FILE *fp) {
    char line[PATH_MAX + 128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_stat_bytes, no data error\n");
        return -1;
    }

    size_t count = 0;
    if (sscanf(line, "STAT_BYTES: %zu", &count) != 1) {
        fprintf(stderr, "fsp_rx_stat_bytes, waiting for STAT_BYTES: N, got: %s\n", line);
        return -1;
    }
    rx->expected_total_bytes = count;

    rx->state = FSP_RX_STAT_FILES;
    return 0;
}

static int fsp_rx_stat_files(fsp_receiver_state_t *rx, FILE *fp) {
    char line[PATH_MAX + 128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_stat_files, no data error\n");
        return -1;
    }

    size_t count = 0;
    if (sscanf(line, "STAT_FILES: %zu", &count) != 1) {
        fprintf(stderr, "fsp_rx_stat_files, waiting for STAT_FILES: N, got: %s\n", line);
        return -1;
    }
    rx->expected_total_files = count;


    rx->state = FSP_RX_IDLE;
    return 0;
}



static int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p;
    
    if (!path || strlen(path) >= sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s", path);

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) < 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) < 0 && errno != EEXIST)
        return -1;

    return 0;
}



static int fsp_rx_handle_idle(fsp_receiver_state_t *rx, FILE *fp) {
    char line[PATH_MAX + 128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_handle_idle, no data error\n");
        return -1;
    } 

     // Skip empty lines
    char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') {
        rx->state = FSP_RX_DONE;
        return 0;
    }

    if (strncmp(line, "DIRECTORY: ", 11) == 0) {
        strncpy(rx->current_dir, line+11, sizeof(rx->current_dir)-1);
        rx->current_dir[sizeof(rx->current_dir)-1] = '\0';


        // Create directory with default permissions (honoring umask)
        char targetdir[PATH_MAX+3];
        snprintf(targetdir, sizeof(targetdir), "%s/%s", rx->target_path, rx->current_dir);
        targetdir[PATH_MAX-1] = '\0';
        if (mkdir_p(targetdir, 0755) < 0) {
            if (errno != EEXIST) {
                fprintf(stderr,"fsp_rx_handle_idle, DIRECTORY cannot be created for %s\n", targetdir);
                perror("mkdir_p");                
                return -1;
            }
        }


        rx->state = FSP_RX_EXPECT_FILE_LIST;
        return 0;
    }

    // In case multiple FILE_LIST calls in a directory
    if (strncmp(line, "FILE_LIST", 9) == 0 ) {
         // Move to next state: expect file count
        rx->state = FSP_RX_EXPECT_FILE_COUNT;
        return 0;
    }

    if (strcmp(line, "END") == 0) {
        rx->state = FSP_RX_DONE;
        fsp_receiver_progressbar(rx,1);
        fprintf(stderr,"\nEND found exiting nicely\n");
        exit(0);
        return 0;
    }

    fprintf(stderr, "Protocol error: unexpected line: '%s'\n", line);

    return -1;
}

static int fsp_rx_handle_file_list(fsp_receiver_state_t *rx, FILE *fp) {
    char line[128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_handle_file_list, no data error\n");
        return -1;
    }

    if (strcmp(line, "FILE_LIST") != 0) {
        fprintf(stderr, "fsp_rx_handle_file_list, waiting for FILE_LIST, got: %s\n", line);
        return -1;
    }

    // Move to next state: expect file count
    rx->state = FSP_RX_EXPECT_FILE_COUNT;
    return 0;
}

static int fsp_rx_handle_file_count(fsp_receiver_state_t *rx, FILE *fp) {
    char line[128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_handle_file_count, no data error\n");
        return -1;
    }

    size_t count = 0;
    if (sscanf(line, "FILES: %zu", &count) != 1) {
        fprintf(stderr, "fsp_rx_handle_file_count, waiting for FILES: N, got: %s\n", line);
        return -1;
    }

    if ( count > FSP_MAX_FILES_PER_LIST ) {
        fprintf(stderr, "fsp_rx_handle_file_count, attempt to push more files than FSP_MAX_FILES_PER_LIST\n");
        return -1;
    }

    rx->expected_files = count;
    rx->files_received = 0;

    // Allocate entries if needed
    if (rx->entries_capacity < count) {
        fsp_file_entry_t *new_entries = (fsp_file_entry_t*)realloc(rx->entries, count * sizeof(fsp_file_entry_t));
        if (!new_entries) {
            perror("realloc");
            fprintf(stderr, "fsp_rx_handle_file_count, cannot realloc entries\n");
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
        if (fsp_rx_read_full(fp, name_len_buf, sizeof(name_len_buf)) < 0) {
            fprintf(stderr,"fsp_rx_handle_file_metadata cannot read name length for entry %ld\n", i);
            return -1;
        }
        uint16_t name_len = ((uint16_t)name_len_buf[0] << 8) | name_len_buf[1];

        // Optional safety: limit path length
        if (name_len > NAME_MAX) {            
            fprintf(stderr,"fsp_rx_handle_file_metadata name_len too big for entry %ld\n", i);
            return -1;
        }

        // 2️ Read filename
        if (fsp_rx_read_full(fp, entry->name, name_len) < 0) {
            fprintf(stderr,"fsp_rx_handle_file_metadata cannot read entry->name for entry %ld\n", i);
            return -1;
        }
        entry->name[name_len] = '\0'; // null-terminate

        // 3️ Read file size (uint64_t, network byte order)
        uint64_t size_be;
        if (fsp_rx_read_full(fp, &size_be, sizeof(size_be)) < 0) {
            fprintf(stderr,"fsp_rx_handle_file_metadata cannot read filesize for entry %ld\n", i);
            return -1;
        }
        entry->size = be64toh(size_be);

        // 4️ Read file hash -- PLACEHOLDER
        if (fsp_rx_read_full(fp, entry->file_hash, SHA256_DIGEST_LENGTH) < 0) {
            fprintf(stderr,"fsp_rx_handle_file_metadata cannot read file_hash placeholder for entry %ld\n", i);
            return -1;
        }

        // 5️ Read number of chunks (uint64_t, network byte order) -- SHOULD BE ZERO AT THIS STAGE
        uint64_t num_chunks_be;
        if (fsp_rx_read_full(fp, &num_chunks_be, sizeof(num_chunks_be)) < 0)  {
            fprintf(stderr,"fsp_rx_handle_file_metadata cannot read num_chunks placeholder for entry %ld\n", i);
            return -1;
        }
        entry->num_chunks = be64toh(num_chunks_be);

        // Optional safety: impose maximum number of chunks
       
        // 6️ Allocate chunk hashes array if needed
        if (entry->num_chunks > 0) {
            entry->chunk_hashes = malloc(entry->num_chunks * SHA256_DIGEST_LENGTH);
            if (!entry->chunk_hashes) {
                fprintf(stderr,"fsp_rx_handle_file_metadata cannot malloc num_chunks placeholder for entry %ld\n", i);
                return -1;
            }
            entry->cap_chunks = entry->num_chunks;

            // Read chunk hashes from the stream
            if (fsp_rx_read_full(fp, entry->chunk_hashes, entry->num_chunks * SHA256_DIGEST_LENGTH) < 0) {
                free(entry->chunk_hashes);
                entry->chunk_hashes = NULL;
                fprintf(stderr,"fsp_rx_handle_file_metadata cannot read chunks placeholder for entry %ld\n", i);
                return -1;
            }
        } else {
            entry->chunk_hashes = NULL;
            entry->cap_chunks = 0;
        }
       


        // DEBUG
        if (  rx->verbose == 1 ) {
            fprintf(stderr, "fsp_rx_handle_file_metadata - Received file: %s (%lu bytes, %lu chunks)\n", 
                entry->name, entry->size, entry->num_chunks);
        }
    }

    // Move to next state: file data
    rx->state = FSP_RX_EXPECT_FILE_DATA;
    return 0;
}


// The caller is responsible for closing the out file
static int fsp_rx_handle_small_file(fsp_receiver_state_t *rx, fsp_file_entry_t *entry, FILE *fp, FILE *out) {

    EVP_MD_CTX *file_ctx = EVP_MD_CTX_new();
    if (!file_ctx) {
        fprintf(stderr,"fsp_rx_handle_small_file cannot create EVP_MD_CTX_new\n");
        return -1;
    }

    if (EVP_DigestInit_ex(file_ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(file_ctx);
        fprintf(stderr,"fsp_rx_handle_small_file cannot EVP_DigestInit_ex\n");
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
            fprintf(stderr, "fsp_rx_handle_small_file, unexpected EOF while reading small file\n");
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }

        rx->total_bytes += n;
        remaining -= n;
        fsp_receiver_progressbar(rx,0);

        if (EVP_DigestUpdate(file_ctx, buf, n) != 1) {
            fprintf(stderr, "fsp_rx_handle_small_file, EVP_DigestUpdate failed\n");
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }

        size_t w = fwrite(buf, 1, n, out);
        if (w != n) {
            perror("fwrite");
            fprintf(stderr, "fsp_rx_handle_small_file, fwrite failed\n");
            EVP_MD_CTX_free(file_ctx);
            return -1;
        }
    }

    // Store final SHA256 hash
    if (EVP_DigestFinal_ex(file_ctx, entry->file_hash, NULL) != 1) {
        fprintf(stderr, "fsp_rx_handle_small_file, EVP_DigestFinal_ex failed\n");
        EVP_MD_CTX_free(file_ctx);
        return -1;
    }

    EVP_MD_CTX_free(file_ctx);
    return 0;
}

// The caller is responsible for closing the out file
static int fsp_rx_handle_chunked_file(fsp_receiver_state_t *rx,
                                      fsp_file_entry_t *entry,
                                      FILE *fp,
                                      FILE *out) {
    uint8_t *buf = rx->proto_buf;
    size_t buf_size = rx->proto_buf_size;
    uint64_t remaining = entry->size;

    EVP_MD_CTX *chunk_ctx  = EVP_MD_CTX_new();
    EVP_MD_CTX *merkle_ctx = EVP_MD_CTX_new();
    if (!chunk_ctx || !merkle_ctx) {
        EVP_MD_CTX_free(chunk_ctx);
        EVP_MD_CTX_free(merkle_ctx);
        fprintf(stderr,"fsp_rx_handle_chunked_file EVP_MD_CTX_new failed\n");
        return -1;
    }

    int ret = EVP_DigestInit_ex(merkle_ctx, EVP_sha256(), NULL);
    if ( ret != 1 ) {
         fprintf(stderr,"fsp_rx_handle_chunked_file EVP_DigestInit_ex failed\n");
         return -1;
    }
    entry->num_chunks = 0;

    while (remaining > 0) {
        uint64_t chunk_bytes = remaining < FSP_CHUNK_SIZE ? remaining : FSP_CHUNK_SIZE;
        uint64_t processed = 0;

        ret = EVP_DigestInit_ex(chunk_ctx, EVP_sha256(), NULL);
        if ( ret != 1 ) {
            fprintf(stderr,"fsp_rx_handle_chunked_file EVP_DigestInit_ex failed\n");
            return -1;
        }

        while (processed < chunk_bytes) {
            size_t to_read = buf_size;
            if (to_read > chunk_bytes - processed)
                to_read = (size_t)(chunk_bytes - processed);

            size_t n = fread(buf, 1, to_read, fp);
            if (n == 0) {
                if (ferror(fp)) perror("fread");
                else fprintf(stderr, "fsp_rx_handle_chunked_file, Unexpected EOF in chunk\n");

                EVP_MD_CTX_free(chunk_ctx);
                EVP_MD_CTX_free(merkle_ctx);
                return -1;
            }

            rx->total_bytes += n;
            processed += n;

            fsp_receiver_progressbar(rx,0);

            if (EVP_DigestUpdate(chunk_ctx, buf, n) != 1) {
                EVP_MD_CTX_free(chunk_ctx);
                EVP_MD_CTX_free(merkle_ctx);
                fprintf(stderr, "fsp_rx_handle_chunked_file, EVP_DigestUpdate failed\n");
                return -1;
            }

            if (fwrite(buf, 1, n, out) != n) {
                perror("fwrite");
                EVP_MD_CTX_free(chunk_ctx);
                EVP_MD_CTX_free(merkle_ctx);
                fprintf(stderr, "fsp_rx_handle_chunked_file, fwrite failed\n");
                return -1;
            }
        }

        uint8_t chunk_hash[SHA256_DIGEST_LENGTH];
        ret = EVP_DigestFinal_ex(chunk_ctx, chunk_hash, NULL);
        if ( ret != 1 ) {
            fprintf(stderr, "fsp_rx_handle_chunked_file, EVP_DigestFinal_ex failed\n");
            return -1;
        }

        // Ensure capacity for chunk hashes
        if (entry->num_chunks >= entry->cap_chunks) {
            size_t new_cap = entry->cap_chunks ? entry->cap_chunks * 2 : 16;
            unsigned char (*tmp)[SHA256_DIGEST_LENGTH] =
                realloc(entry->chunk_hashes, new_cap * sizeof(*entry->chunk_hashes));
            if (!tmp) {
                perror("realloc chunk_hashes");
                EVP_MD_CTX_free(chunk_ctx);
                EVP_MD_CTX_free(merkle_ctx);
                fprintf(stderr, "fsp_rx_handle_chunked_file, realloc failed\n");
                return -1;
            }
            entry->chunk_hashes = tmp;
            entry->cap_chunks = new_cap;
        }

        // Store the chunk hash
        memcpy(entry->chunk_hashes[entry->num_chunks], chunk_hash, SHA256_DIGEST_LENGTH);
        entry->num_chunks++;

        // Update Merkle Level 0 hash
        ret = EVP_DigestUpdate(merkle_ctx, chunk_hash, SHA256_DIGEST_LENGTH);
        if ( ret != 1 ) {
            fprintf(stderr, "fsp_rx_handle_chunked_file, EVP_DigestUpdate failed\n");
            return -1;
        }

        remaining -= chunk_bytes;
    }

    // Final Merkle hash stored in entry->file_hash
    ret = EVP_DigestFinal_ex(merkle_ctx, entry->file_hash, NULL);
    if ( ret != 1 ) {
        fprintf(stderr, "fsp_rx_handle_chunked_file, EVP_DigestFinal_ex failed\n");
        return -1;
    }

    EVP_MD_CTX_free(chunk_ctx);
    EVP_MD_CTX_free(merkle_ctx);

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
            fprintf(stderr,"fsp_rx_handle_file_data, fopen failed for %s\n", filepath);
            return -1;
        }
       
        if (entry->size > FSP_CHUNK_SIZE) {
           if (fsp_rx_handle_chunked_file(rx, entry, fp, out) < 0) {
                fclose(out);
                fprintf(stderr,"fsp_rx_handle_file_data, fsp_rx_handle_chunked_file filepath failed %s\n", filepath);
                return -1;
            }
        } else {
             if (fsp_rx_handle_small_file(rx, entry, fp, out) < 0) {
                fprintf(stderr,"fsp_rx_handle_file_data, fsp_rx_handle_small_file filepath failed %s\n", filepath);
                fclose(out);
                return -1;
            }            
        }

        fclose(out);
    }

    // After reading all file data, transition to Phase 3 (hashes)
    rx->state = FSP_RX_EXPECT_FILE_HASHES_COUNT;
    return 0;
}


static int fsp_rx_handle_hashfiles_count(fsp_receiver_state_t *rx, FILE *fp) {
    char line[128];
    if (fsp_rx_readline(fp, line, sizeof(line)) < 0) {
        fprintf(stderr,"fsp_rx_handle_hashfiles_count, no data error\n");
        return -1;
    }

    size_t count = 0;
    if (sscanf(line, "HASH FILES: %zu", &count) != 1) {
        fprintf(stderr, "fsp_rx_handle_hashfiles_count, waiting HASH FILES: N, got: %s\n", line);
        return -1;
    }

    if ( count > FSP_MAX_FILES_PER_LIST ) {
        fprintf(stderr, "fsp_rx_handle_hashfiles_count, attempt to push more files than FSP_MAX_FILES_PER_LIST\n");
        return -1;
    }

    rx->state = FSP_RX_EXPECT_FILE_HASHES;
    return 0;
}



static int fsp_rx_handle_file_hashes(fsp_receiver_state_t *rx, FILE *fp) {


    int file_received = 0 ;
    for (size_t i = rx->files_received; i < rx->expected_files; i++) {
        fsp_file_entry_t *entry = &rx->entries[i];

        // 1 Read filename length (uint16_t, network byte order)
        uint16_t name_len_be;
        if (fsp_rx_read_full(fp, &name_len_be, sizeof(name_len_be)) < 0) {
            fprintf(stderr, "fsp_rx_handle_file_hashes, name_len failed for entry %ld\n", i);
            return -1;
        }
        uint16_t name_len = be16toh(name_len_be);

        // Optional safety: limit path length
        if (name_len > NAME_MAX) {
            fprintf(stderr, "fsp_rx_handle_file_hashes, name_len too big for entry %ld\n", i);
            return -1;
        }

        // 2️ Read filename and cross check
        char     name[NAME_MAX + 1];
        memset(name, 0, sizeof(name));
        if (fsp_rx_read_full(fp, name, name_len) < 0) {
            fprintf(stderr, "fsp_rx_handle_file_hashes cannot read name for entry %ld\n", i);
            return -1;
        }
        if ( strcmp(entry->name, name) != 0 ) {
            fprintf(stderr, "fsp_rx_handle_file_hashes entry->name not matching %s for entry %ld\n", entry->name, i);
            return -1;
        }

        // 3️ Read file size (uint64_t, network byte order) and cross check
        uint64_t size_be;
        if (fsp_rx_read_full(fp, &size_be, sizeof(size_be)) < 0) {
            fprintf(stderr, "fsp_rx_handle_file_hashes cannot read size for entry %ld: %s\n", i, entry->name);
            return -1;
        }
        uint64_t size = be64toh(size_be);
        if (entry->size != size) {
            fprintf(stderr, "fsp_rx_handle_file_hashes entry->size not matching %ld for entry %ld: %s\n", entry->size, i, entry->name);
            return -1;        
        }

        // 4️ Read file hash - REAL DATA
        unsigned char file_hash[SHA256_DIGEST_LENGTH];        
        if (fsp_rx_read_full(fp, file_hash, SHA256_DIGEST_LENGTH) < 0) {
            fprintf(stderr, "fsp_rx_handle_file_hashes cannot read  file_hash for entry %ld: %s\n", i, entry->name);
            return -1;
        }
        // cross check file_hash entry->file_hash
        if ( size > 0 && memcmp(file_hash, entry->file_hash, SHA256_DIGEST_LENGTH) != 0) {
            fprintf(stderr, "fsp_rx_handle_file_hashes hash not matching file_hash for entry %ld: %s\n", i, entry->name);
            return -1;
        }

        // 5️ Read number of chunks (uint64_t, network byte order) - NOT ZERO IF REAL DATA & cross check
        uint64_t num_chunks_be;
        if (fsp_rx_read_full(fp, &num_chunks_be, sizeof(num_chunks_be)) < 0) {
            fprintf(stderr, "fsp_rx_handle_file_hashes cannot read num_chunks %ld: %s\n", i, entry->name);
            return -1;
        }
        uint64_t num_chunks = be64toh(num_chunks_be);
        if (entry->num_chunks != num_chunks) {
            fprintf(stderr, "fsp_rx_handle_file_hashes num_chunks not matching for %ld: %s\n", i, entry->name);
            return -1;
        }
    

        // Optional safety: impose maximum number of chunks
       
        // 6️ Allocate chunk hashes array if needed
        if (num_chunks > 0) {
            unsigned char *chunk_hashes = malloc(num_chunks * SHA256_DIGEST_LENGTH);
            if (!chunk_hashes) {
                fprintf(stderr, "fsp_rx_handle_file_hashes cannot malloc %ld: %s\n", i, entry->name);
                return -1;            
            }

            if (fsp_rx_read_full(fp, chunk_hashes, num_chunks * SHA256_DIGEST_LENGTH) < 0) {
                fprintf(stderr, "fsp_rx_handle_file_hashes cannot fsp_rx_read_full %ld: %s\n", i, entry->name);
                free(chunk_hashes);
                return -1;
            }

            // Cross-check against metadata copy
            if (memcmp(chunk_hashes, entry->chunk_hashes, num_chunks * SHA256_DIGEST_LENGTH) != 0) {
                fprintf(stderr, "fsp_rx_handle_file_hashes cannot match chunk hashes %ld: %s\n", i, entry->name);
                free(chunk_hashes);
                return -1;
            }

            // If we allocated a temporary buffer, free it
            free(chunk_hashes);
        }         

        // Optional: psychokouak mode, we reread the writen file
        //  And we ensure that the SHA256 and the chunks are correct


        // DEBUG
        if (  rx->verbose == 1 ) {
            fprintf(stderr, "fsp_rx_handle_file_hashes - Cross check OK : %s (%lu bytes, %lu chunks)\n", 
                entry->name, entry->size, entry->num_chunks);
        }
       
        // Eveything is fine and cross checked        
        file_received++;
    }

    rx->files_received += file_received;
    rx->total_files += file_received;
    fsp_receiver_progressbar(rx,0);

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
        case FSP_RX_STAT_BYTES:
            return fsp_rx_stat_bytes(rx, fp);
        case FSP_RX_STAT_FILES:
            return fsp_rx_stat_files(rx, fp);        
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
        case FSP_RX_EXPECT_FILE_HASHES_COUNT:
            return fsp_rx_handle_hashfiles_count(rx, fp);
        case FSP_RX_EXPECT_FILE_HASHES:
            return fsp_rx_handle_file_hashes(rx, fp);
        case FSP_RX_DONE:
            fsp_receiver_progressbar(rx,1);
            exit(0);
            return 0; // Already done
        default:
            fprintf(stderr,"fsp_receiver_process_line: Unknown state \n");
            return -1;
    }
}