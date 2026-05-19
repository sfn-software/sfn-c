/*
 * Wire protocol shared with sfn-go-cli.
 *
 * Each file is preceded by a one-byte opcode that tells the receiver
 * which header fields to expect and whether an MD5 trailer follows.
 *
 *   OP_FILE              name\n  size(le64)  body
 *   OP_MD5_WITH_FILE     name\n  size(le64)  md5_hex\n  body
 *   OP_FILE_WITH_MD5     name\n  size(le64)  body  md5_hex\n
 *   OP_FILE_WITH_PATH    name\n  size(le64)  relDir\n  exec(1)  body  md5_hex\n
 *   OP_DONE              (no payload, marks end of stream)
 *
 * Sizes are little-endian 64-bit signed integers, matching Go's
 * encoding/binary with binary.LittleEndian on int64.
 */

#ifndef SIPHON_PROTO_H
#define SIPHON_PROTO_H

#include "fs.h"

#define OP_FILE              0x01
#define OP_DONE              0x02
#define OP_MD5_WITH_FILE     0x03
#define OP_FILE_WITH_MD5     0x04
#define OP_FILE_WITH_PATH    0x05

/* Send every entry in `files`, then OP_DONE. Each entry's base_dir is
 * used to derive the relative subdirectory sent over the wire.
 *
 * use_legacy=1 emits the bare OP_FILE format (no MD5, no path) for
 * compatibility with the original C version. use_legacy=0 emits
 * OP_FILE_WITH_PATH. */
int send_files(int sock, const file_list *files, int buffer_size, int use_legacy);

/* Receive files into directory until OP_DONE arrives.
 * MD5 mismatches print a warning and continue with the next file. */
int receive_files(int sock, const char *directory, int buffer_size);

#endif
