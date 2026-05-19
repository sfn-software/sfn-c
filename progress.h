/*
 * Single-file progress bar that overdraws one line on the terminal.
 *
 * Usage:
 *   progress_begin(name, total_bytes);
 *   progress_update(bytes_done);   // call repeatedly
 *   progress_end();                // prints final newline
 */

#ifndef SIPHON_PROGRESS_H
#define SIPHON_PROGRESS_H

#include <sys/types.h>

void progress_begin(const char *file_name, off_t total_bytes);
void progress_update(off_t bytes_done);
void progress_end(void);

#endif
