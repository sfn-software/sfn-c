#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int open_for_read(const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Unable to open file %s: %s\n", file_path, strerror(errno));
    }
    return fd;
}

int open_for_write(const char *file_path) {
    int fd = open(file_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "Unable to open file %s: %s\n", file_path, strerror(errno));
    }
    return fd;
}

off_t file_size(const char *file_path) {
    struct stat st;
    if (stat(file_path, &st) != 0) return -1;
    return st.st_size;
}

int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

const char *file_basename(const char *file_path) {
    if (file_path == NULL) return NULL;
    /* Accept both '/' and '\' as separators so Windows-style names round-trip. */
    const char *last = file_path;
    for (const char *p = file_path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

char *path_dirname(const char *file_path) {
    if (file_path == NULL) return NULL;
    const char *last_slash = strrchr(file_path, '/');
    if (last_slash == NULL) return strdup("");
    if (last_slash == file_path) return strdup("/");  /* file in root */
    size_t len = (size_t) (last_slash - file_path);
    char *out = malloc(len + 1);
    if (out == NULL) return NULL;
    memcpy(out, file_path, len);
    out[len] = '\0';
    return out;
}

char *path_join(const char *directory, const char *name) {
    if (name == NULL) return NULL;
    if (directory == NULL || directory[0] == '\0') {
        return strdup(name);
    }
    size_t dir_len = strlen(directory);
    size_t name_len = strlen(name);
    int needs_sep = directory[dir_len - 1] != '/';

    char *out = malloc(dir_len + (needs_sep ? 1 : 0) + name_len + 1);
    if (out == NULL) return NULL;

    memcpy(out, directory, dir_len);
    if (needs_sep) out[dir_len++] = '/';
    memcpy(out + dir_len, name, name_len);
    out[dir_len + name_len] = '\0';
    return out;
}

/* Refuse paths that could escape the base directory. */
static int path_is_safe(const char *rel_path) {
    if (rel_path[0] == '/') return 0;
    /* Scan for ".." as a whole component. */
    const char *p = rel_path;
    while (*p != '\0') {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '\0' || p[2] == '/')) {
            return 0;
        }
        /* Skip to next component. */
        while (*p != '\0' && *p != '/') p++;
        if (*p == '/') p++;
    }
    return 1;
}

char *mkdir_p(const char *base, const char *rel_path) {
    if (rel_path == NULL || rel_path[0] == '\0') {
        return strdup(base != NULL ? base : "");
    }
    if (!path_is_safe(rel_path)) {
        fprintf(stderr, "Refusing unsafe path: %s\n", rel_path);
        return NULL;
    }

    char *full = path_join(base, rel_path);
    if (full == NULL) return NULL;

    /* Walk through each '/' and mkdir the prefix. */
    for (char *p = full + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(full, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "Unable to create directory %s: %s\n", full, strerror(errno));
                free(full);
                return NULL;
            }
            *p = '/';
        }
    }
    if (mkdir(full, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Unable to create directory %s: %s\n", full, strerror(errno));
        free(full);
        return NULL;
    }
    return full;
}

int is_executable(const char *file_path) {
    struct stat st;
    if (stat(file_path, &st) != 0) return 0;
    return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
}

int set_executable(const char *file_path) {
    struct stat st;
    if (stat(file_path, &st) != 0) return -1;
    mode_t mode = st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
    return chmod(file_path, mode);
}

char *path_rel_dir(const char *base_dir, const char *file_path) {
    if (base_dir == NULL || file_path == NULL) return NULL;

    /* Find the start of the basename — everything before it is the parent. */
    const char *last_slash = strrchr(file_path, '/');
    if (last_slash == NULL) {
        /* file_path has no directory component, so the parent is empty. */
        return strdup("");
    }
    size_t parent_len = (size_t) (last_slash - file_path);

    /* Compare against base_dir ignoring any trailing slash on the base. */
    size_t base_len = strlen(base_dir);
    while (base_len > 0 && base_dir[base_len - 1] == '/') base_len--;

    if (base_len == 0) {
        /* Empty base — the rel dir is the entire parent path. */
        char *out = malloc(parent_len + 1);
        if (out == NULL) return NULL;
        memcpy(out, file_path, parent_len);
        out[parent_len] = '\0';
        return out;
    }

    if (parent_len < base_len || memcmp(file_path, base_dir, base_len) != 0) {
        return NULL;  /* file_path not under base_dir */
    }
    if (parent_len == base_len) {
        return strdup("");
    }
    /* Right after the base must come '/', otherwise base was only a
     * string prefix (e.g. base="/foo" file="/foobar/x"). */
    if (file_path[base_len] != '/') return NULL;

    size_t rel_start = base_len + 1;
    size_t rel_len = parent_len - rel_start;
    char *out = malloc(rel_len + 1);
    if (out == NULL) return NULL;
    memcpy(out, file_path + rel_start, rel_len);
    out[rel_len] = '\0';
    return out;
}

/* ---- file list ---- */

void file_list_init(file_list *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int file_list_add(file_list *list, const char *file_path, const char *base_dir) {
    if (list->count == list->capacity) {
        int new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        file_entry *grown = realloc(list->items, (size_t) new_cap * sizeof(file_entry));
        if (grown == NULL) return -1;
        list->items = grown;
        list->capacity = new_cap;
    }
    char *fp = strdup(file_path);
    char *bd = strdup(base_dir);
    if (fp == NULL || bd == NULL) {
        free(fp);
        free(bd);
        return -1;
    }
    list->items[list->count].file_path = fp;
    list->items[list->count].base_dir = bd;
    list->count++;
    return 0;
}

void file_list_free(file_list *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].file_path);
        free(list->items[i].base_dir);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Recursive worker. `current` is the directory being scanned right now;
 * `base` is the original root and is what gets stored on each entry. */
static int scan_dir_recursive(const char *base, const char *current, file_list *list) {
    DIR *dir = opendir(current);
    if (dir == NULL) {
        fprintf(stderr, "Unable to open directory %s: %s\n", current, strerror(errno));
        return -1;
    }

    int rc = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        char *full = path_join(current, name);
        if (full == NULL) { rc = -1; break; }

        struct stat st;
        if (stat(full, &st) != 0) {
            fprintf(stderr, "Unable to stat %s: %s\n", full, strerror(errno));
            free(full);
            rc = -1;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            rc = scan_dir_recursive(base, full, list);
        } else if (S_ISREG(st.st_mode)) {
            if (file_list_add(list, full, base) != 0) rc = -1;
        }
        free(full);
        if (rc != 0) break;
    }
    closedir(dir);
    return rc;
}

int scan_dir(const char *base, file_list *list) {
    return scan_dir_recursive(base, base, list);
}
