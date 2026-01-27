#include "fsp_fs.h"

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int fsp_list_dir(const char *dirpath, fsp_list_cb_t cb, void *user) {
    DIR *d = opendir(dirpath);
    if (!d) {
        perror("opendir");
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *name = de->d_name;

        /* Skip . and .. */
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        char full[PATH_MAX];
        if (snprintf(full, sizeof(full), "%s/%s", dirpath, name) >= (int)sizeof(full)) {
            fprintf(stderr, "fsp-send: path too long: %s/%s\n", dirpath, name);
            closedir(d);
            return -1;
        }

        struct stat st;
        if (lstat(full, &st) != 0) {
            perror("lstat");
            closedir(d);
            return -1;
        }

        /* Reject symlinks explicitly */
        if (S_ISLNK(st.st_mode)) {
            fprintf(stderr, "fsp-send: skipping symlink: %s\n", full);
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            if (cb(name, FSP_ENTRY_FILE, user) != 0) {
                closedir(d);
                return -1;
            }
        } else if (S_ISDIR(st.st_mode)) {
            if (cb(name, FSP_ENTRY_DIR, user) != 0) {
                closedir(d);
                return -1;
            }
        } else {
            /* Skip special files */
            fprintf(stderr, "fsp-send: skipping special file: %s\n", full);
            continue;
        }
    }

    if (closedir(d) != 0) {
        perror("closedir");
        return -1;
    }

    return 0;
}
