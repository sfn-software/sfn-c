#include "progress.h"

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BAR_WIDTH 22

static const char *cur_file_name;
static off_t cur_total_bytes;
static off_t last_bytes_done;
static int last_percent;
static const char *last_unit;

static void format_size(off_t bytes, off_t *out_value, const char **out_unit) {
    static const char *units[] = {"Byte", "KiB ", "MiB ", "GiB ", "TiB "};
    int i = 0;
    while (bytes >= 1024 && i < 4) {
        bytes /= 1024;
        i++;
    }
    *out_value = bytes;
    *out_unit = units[i];
}

void progress_begin(const char *file_name, off_t total_bytes) {
    cur_file_name = file_name;
    cur_total_bytes = total_bytes;
    last_bytes_done = -1;
    last_percent = -1;
    last_unit = NULL;
}

void progress_update(off_t bytes_done) {
    off_t scaled;
    const char *unit;
    format_size(bytes_done, &scaled, &unit);

    int percent = cur_total_bytes > 0
                  ? (int) ((bytes_done * 100) / cur_total_bytes)
                  : 100;

    /* Redraw only when something a user can see has changed. */
    if (scaled == last_bytes_done && percent == last_percent && unit == last_unit) {
        return;
    }
    last_bytes_done = scaled;
    last_percent = percent;
    last_unit = unit;

    char bar[BAR_WIDTH + 1];
    for (int i = 0; i < BAR_WIDTH; i++) {
        bar[i] = (i * 100 / BAR_WIDTH) <= percent ? '#' : '-';
    }
    bar[BAR_WIDTH] = '\0';

    struct winsize tty;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty) != 0 || tty.ws_col == 0) {
        tty.ws_col = 80;
    }
    int name_width = (int) tty.ws_col - 42;
    if (name_width < 1) name_width = 1;

    /* Truncate the name to the available width without allocating. */
    int name_len = (int) strlen(cur_file_name);
    int shown_len = name_len < name_width ? name_len : name_width;

    printf(" %-*.*s %4lld %4s [%s] %3d %%\r",
           name_width, shown_len, cur_file_name,
           (long long) scaled, unit, bar, percent);
    fflush(stdout);
}

void progress_end(void) {
    printf("\n");
    fflush(stdout);
}
