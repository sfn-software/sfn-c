#include "proto.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs.h"
#include "md5.h"
#include "net.h"
#include "progress.h"

/* Return values from receive-side helpers.
 * Prefixed RC_ to avoid colliding with R_OK from <unistd.h>. */
#define RC_OK         0
#define RC_MD5_BAD    1
#define RC_FATAL    (-1)

/*
 * Copy file_size bytes from src to dest, in buffer_size chunks.
 * If hash is non-NULL, every byte is also fed through MD5.
 * Updates the progress bar but does not begin/end it.
 */
static int transfer_data(int src, int dest, off_t file_size,
                         int buffer_size, md5_ctx *hash) {
    char *buffer = malloc((size_t) buffer_size);
    if (buffer == NULL) return -1;

    off_t total = 0;
    while (total < file_size) {
        size_t want = (size_t) buffer_size;
        if ((off_t) want > file_size - total) {
            want = (size_t) (file_size - total);
        }
        ssize_t n = read(src, buffer, want);
        if (n == 0) {
            /* Unexpected EOF — caller decides if that's fatal. */
            free(buffer);
            return -1;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "\nUnable to read source: %s\n", strerror(errno));
            free(buffer);
            return -1;
        }
        if (write_total(dest, buffer, (size_t) n) < 0) {
            fprintf(stderr, "\nUnable to write data: %s\n", strerror(errno));
            free(buffer);
            return -1;
        }
        if (hash != NULL) {
            md5_update(hash, buffer, (size_t) n);
        }
        total += n;
        progress_update(total);
    }
    free(buffer);
    return 0;
}

/* ---- helpers shared across opcodes ---- */

static int read_size(int sock, off_t *out) {
    uint8_t buf[8];
    if (read_total(sock, buf, 8) != 8) {
        fprintf(stderr, "Unable to read file size\n");
        return -1;
    }
    *out = (off_t) read_int64_le(buf);
    return 0;
}

static int read_exec_bit(int sock, int *out) {
    uint8_t byte;
    if (read_total(sock, &byte, 1) != 1) {
        fprintf(stderr, "Unable to read executable bit\n");
        return -1;
    }
    *out = byte == 1;
    return 0;
}

static int compare_md5(const md5_ctx *computed_ctx, const char *received_hex,
                       const char *file_name) {
    md5_ctx tmp = *computed_ctx;
    uint8_t digest[MD5_DIGEST_SIZE];
    char hex[MD5_HEX_SIZE + 1];
    md5_final(&tmp, digest);
    md5_to_hex(digest, hex);

    if (strcmp(hex, received_hex) != 0) {
        fprintf(stderr, "MD5 mismatch for %s: expected %s, got %s\n",
                file_name, received_hex, hex);
        return RC_MD5_BAD;
    }
    return RC_OK;
}

/* Receive one file body into the destination directory, hashing along the way.
 * On success returns RC_OK and fills hash_out with the final MD5 context. */
static int receive_body(int sock, const char *dir, const char *file_name,
                        off_t size, int buffer_size, int executable,
                        md5_ctx *hash_out) {
    char *out_path = path_join(dir, file_name);
    if (out_path == NULL) return RC_FATAL;

    int fd = open_for_write(out_path);
    if (fd < 0) {
        free(out_path);
        return RC_FATAL;
    }

    md5_init(hash_out);
    progress_begin(file_name, size);
    int rc = transfer_data(sock, fd, size, buffer_size, hash_out);
    progress_end();
    close(fd);

    if (rc == 0 && executable) {
        if (set_executable(out_path) != 0) {
            fprintf(stderr, "Unable to mark %s executable: %s\n",
                    out_path, strerror(errno));
        }
    }
    free(out_path);
    return rc == 0 ? RC_OK : RC_FATAL;
}

/* ---- per-opcode receive handlers ---- */

static int recv_op_file(int sock, const char *base_dir, int buffer_size) {
    char *name = read_line(sock);
    if (name == NULL) return RC_FATAL;
    off_t size;
    if (read_size(sock, &size) != 0) { free(name); return RC_FATAL; }

    md5_ctx hash;
    int rc = receive_body(sock, base_dir, name, size, buffer_size, 0, &hash);
    free(name);
    return rc;
}

static int recv_op_md5_with_file(int sock, const char *base_dir, int buffer_size) {
    char *name = read_line(sock);
    if (name == NULL) return RC_FATAL;
    off_t size;
    if (read_size(sock, &size) != 0) { free(name); return RC_FATAL; }
    char *md5_hex = read_line(sock);
    if (md5_hex == NULL) { free(name); return RC_FATAL; }

    md5_ctx hash;
    int rc = receive_body(sock, base_dir, name, size, buffer_size, 0, &hash);
    if (rc == RC_OK) {
        rc = compare_md5(&hash, md5_hex, name);
    }
    free(name);
    free(md5_hex);
    return rc;
}

static int recv_op_file_with_md5(int sock, const char *base_dir, int buffer_size) {
    char *name = read_line(sock);
    if (name == NULL) return RC_FATAL;
    off_t size;
    if (read_size(sock, &size) != 0) { free(name); return RC_FATAL; }

    md5_ctx hash;
    int rc = receive_body(sock, base_dir, name, size, buffer_size, 0, &hash);
    if (rc != RC_OK) { free(name); return rc; }

    char *md5_hex = read_line(sock);
    if (md5_hex == NULL) { free(name); return RC_FATAL; }
    rc = compare_md5(&hash, md5_hex, name);

    free(name);
    free(md5_hex);
    return rc;
}

static int recv_op_file_with_path(int sock, const char *base_dir, int buffer_size) {
    char *name = read_line(sock);
    if (name == NULL) return RC_FATAL;
    off_t size;
    if (read_size(sock, &size) != 0) { free(name); return RC_FATAL; }
    char *rel_dir = read_line(sock);
    if (rel_dir == NULL) { free(name); return RC_FATAL; }
    int executable;
    if (read_exec_bit(sock, &executable) != 0) {
        free(name); free(rel_dir); return RC_FATAL;
    }

    char *target_dir = mkdir_p(base_dir, rel_dir);
    if (target_dir == NULL) {
        free(name); free(rel_dir); return RC_FATAL;
    }

    md5_ctx hash;
    int rc = receive_body(sock, target_dir, name, size, buffer_size, executable, &hash);
    free(target_dir);
    if (rc != RC_OK) { free(name); free(rel_dir); return rc; }

    char *md5_hex = read_line(sock);
    if (md5_hex == NULL) { free(name); free(rel_dir); return RC_FATAL; }
    rc = compare_md5(&hash, md5_hex, name);

    free(name);
    free(rel_dir);
    free(md5_hex);
    return rc;
}

int receive_files(int sock, const char *directory, int buffer_size) {
    while (1) {
        uint8_t op;
        ssize_t r = read_total(sock, &op, 1);
        if (r == 0) return 0;  /* peer closed cleanly between files */
        if (r < 0) return -1;

        int rc;
        switch (op) {
            case OP_DONE:
                return 0;
            case OP_FILE:
                rc = recv_op_file(sock, directory, buffer_size);
                break;
            case OP_MD5_WITH_FILE:
                rc = recv_op_md5_with_file(sock, directory, buffer_size);
                break;
            case OP_FILE_WITH_MD5:
                rc = recv_op_file_with_md5(sock, directory, buffer_size);
                break;
            case OP_FILE_WITH_PATH:
                rc = recv_op_file_with_path(sock, directory, buffer_size);
                break;
            default:
                fprintf(stderr, "Unknown opcode: 0x%02x\n", op);
                return -1;
        }
        if (rc == RC_FATAL) return -1;
        /* RC_MD5_BAD: helper printed the warning, keep going. */
    }
}

/* ---- send side ---- */

static int send_one_legacy(int sock, const file_entry *entry, int buffer_size) {
    const char *file_path = entry->file_path;
    const char *name = file_basename(file_path);
    off_t size = file_size(file_path);
    if (size < 0) {
        fprintf(stderr, "Unable to stat %s\n", file_path);
        return -1;
    }

    uint8_t op = OP_FILE;
    if (write_total(sock, &op, 1) < 0) return -1;
    if (write_total(sock, name, strlen(name)) < 0) return -1;
    if (write_total(sock, "\n", 1) < 0) return -1;

    uint8_t size_buf[8];
    write_int64_le(size_buf, (int64_t) size);
    if (write_total(sock, size_buf, 8) < 0) return -1;

    int fd = open_for_read(file_path);
    if (fd < 0) return -1;

    progress_begin(name, size);
    int rc = transfer_data(fd, sock, size, buffer_size, NULL);
    progress_end();
    close(fd);
    return rc;
}

static int send_one_with_path(int sock, const file_entry *entry, int buffer_size) {
    const char *file_path = entry->file_path;
    const char *name = file_basename(file_path);
    off_t size = file_size(file_path);
    if (size < 0) {
        fprintf(stderr, "Unable to stat %s\n", file_path);
        return -1;
    }

    /* Relative dir under the recipient's target directory. Empty for a
     * file passed directly; a sub-path like "sub/inner" when the file
     * was discovered by walking a directory. */
    char *rel_dir = path_rel_dir(entry->base_dir, file_path);
    if (rel_dir == NULL) {
        fprintf(stderr, "Unable to compute relative path for %s\n", file_path);
        return -1;
    }

    uint8_t op = OP_FILE_WITH_PATH;
    if (write_total(sock, &op, 1) < 0) { free(rel_dir); return -1; }
    if (write_total(sock, name, strlen(name)) < 0) { free(rel_dir); return -1; }
    if (write_total(sock, "\n", 1) < 0) { free(rel_dir); return -1; }

    uint8_t size_buf[8];
    write_int64_le(size_buf, (int64_t) size);
    if (write_total(sock, size_buf, 8) < 0) { free(rel_dir); return -1; }

    if (write_total(sock, rel_dir, strlen(rel_dir)) < 0) { free(rel_dir); return -1; }
    if (write_total(sock, "\n", 1) < 0) { free(rel_dir); return -1; }
    free(rel_dir);

    uint8_t exec_byte = (uint8_t) (is_executable(file_path) ? 1 : 0);
    if (write_total(sock, &exec_byte, 1) < 0) return -1;

    int fd = open_for_read(file_path);
    if (fd < 0) return -1;

    md5_ctx hash;
    md5_init(&hash);
    progress_begin(name, size);
    int rc = transfer_data(fd, sock, size, buffer_size, &hash);
    progress_end();
    close(fd);
    if (rc < 0) return -1;

    uint8_t digest[MD5_DIGEST_SIZE];
    char hex[MD5_HEX_SIZE + 1];
    md5_final(&hash, digest);
    md5_to_hex(digest, hex);

    if (write_total(sock, hex, MD5_HEX_SIZE) < 0) return -1;
    if (write_total(sock, "\n", 1) < 0) return -1;
    return 0;
}

int send_files(int sock, const file_list *files, int buffer_size, int use_legacy) {
    for (int i = 0; i < files->count; i++) {
        const file_entry *entry = &files->items[i];
        int rc = use_legacy
                 ? send_one_legacy(sock, entry, buffer_size)
                 : send_one_with_path(sock, entry, buffer_size);
        if (rc < 0) return -1;
    }
    uint8_t op = OP_DONE;
    if (write_total(sock, &op, 1) < 0) return -1;
    return 0;
}
