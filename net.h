/*
 * Network and binary IO helpers.
 *
 * Sockets opened here behave like POSIX file descriptors, so the same
 * read_total / write_total helpers work for both files and sockets.
 */

#ifndef SIPHON_NET_H
#define SIPHON_NET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Open a TCP socket and either connect or listen+accept.
 * If ip is NULL, bind to INADDR_ANY and accept a single client.
 * If ip is non-NULL, resolve it and connect.
 * Returns the connected socket on success, or -1 on error. */
int open_socket(const char *ip, int port);

/* Read exactly n bytes. Returns n on success, 0 on EOF before any bytes,
 * a value < n if EOF arrived mid-read, or -1 on error. */
ssize_t read_total(int fd, void *buf, size_t n);

/* Write exactly n bytes. Returns n on success or -1 on error. */
ssize_t write_total(int fd, const void *buf, size_t n);

/* Read bytes until '\n' is seen. The newline is consumed but not stored.
 * Returns a malloc'd, NUL-terminated string (caller frees), or NULL on
 * EOF/error. */
char *read_line(int fd);

/* Encode/decode an int64 in little-endian wire format (matches Go's
 * encoding/binary with binary.LittleEndian). */
void write_int64_le(uint8_t out[8], int64_t value);
int64_t read_int64_le(const uint8_t in[8]);

#endif
