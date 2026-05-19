/*
 * Filesystem helpers: paths, sizes, mode bits, directory creation,
 * and a small list type for collecting files to send.
 */

#ifndef SIPHON_FS_H
#define SIPHON_FS_H

#include <sys/types.h>

/* Open a file for reading (existing) or writing (create+truncate). */
int open_for_read(const char *file_path);
int open_for_write(const char *file_path);

/* File size in bytes, or -1 on error. */
off_t file_size(const char *file_path);

/* True if path exists and is a directory. */
int is_directory(const char *path);

/* Return the basename portion of file_path (no allocation, points into input). */
const char *file_basename(const char *file_path);

/* Return the directory portion of file_path with any trailing slash stripped.
 *   "/foo/bar/baz" -> "/foo/bar"
 *   "baz"          -> ""
 *   "/baz"         -> "/"
 * Caller frees. */
char *path_dirname(const char *file_path);

/* Join directory + name with a single '/' between them.
 * Returns a malloc'd string (caller frees), or NULL on allocation failure.
 * Empty directory means "current working directory" — just returns name. */
char *path_join(const char *directory, const char *name);

/* Compute the directory of file_path relative to base_dir.
 * Assumes file_path lives under base_dir.
 *   base="/a/b", file="/a/b/c.txt"     -> ""
 *   base="/a/b", file="/a/b/d/c.txt"   -> "d"
 *   base="/a/b", file="/a/b/d/e/c.txt" -> "d/e"
 * Returns a malloc'd string (caller frees), or NULL if file_path is
 * not under base_dir. */
char *path_rel_dir(const char *base_dir, const char *file_path);

/* Create directory and any missing parents (like `mkdir -p`).
 * rel_path is interpreted under base. Rejects paths that escape base
 * (absolute paths or any ".." component). Returns the joined absolute
 * directory as a malloc'd string on success, or NULL on error. */
char *mkdir_p(const char *base, const char *rel_path);

/* True if any of user/group/other has the execute bit set. */
int is_executable(const char *file_path);

/* Add the execute bit for user/group/other. */
int set_executable(const char *file_path);

/* ---- file list: a growable array of (file_path, base_dir) pairs ----
 *
 * Each entry pairs a file with the directory it belongs to. For a file
 * passed on the command line, base_dir is its parent. For a file found
 * while walking a directory, base_dir is the original directory the
 * walk started from — this preserves the subpath when sending. */

typedef struct {
    char *file_path;
    char *base_dir;
} file_entry;

typedef struct {
    file_entry *items;
    int count;
    int capacity;
} file_list;

void file_list_init(file_list *list);
int file_list_add(file_list *list, const char *file_path, const char *base_dir);
void file_list_free(file_list *list);

/* Recursively walk base and add every regular file to list, each with
 * its base_dir set to base. Returns 0 on success, -1 on error. */
int scan_dir(const char *base, file_list *list);

#endif
