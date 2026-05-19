/*
 * Siphon — send files over a direct TCP connection.
 *
 * Two modes:
 *   --listen                accept one peer, exchange files, exit
 *   --connect <host>        connect to a listening peer
 *
 * The listening side reads first, then sends. The connecting side sends
 * first, then reads. Both sides agree on the wire format described in proto.h.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "fs.h"
#include "net.h"
#include "proto.h"

#define DEFAULT_PORT        3214
#define DEFAULT_BUFFER_SIZE 0x1400

static void show_help(void) {
    printf("Usage:\n\n");
    printf("    siphon --listen [options]\n");
    printf("    siphon --connect <address> [options]\n\n");
    printf("Options:\n\n");
    printf("    --version,   -v     Show Siphon version and exit.\n");
    printf("    --help,      -h     Show this text and exit.\n");
    printf("    --port,      -p     Use specified port. Defaults to %d.\n", DEFAULT_PORT);
    printf("    --file,      -f     Send a file or recursively send a directory's contents.\n"
           "                        Use \"-f file1 -f dir1\" to send multiple items.\n");
    printf("    --buffer,    -b     Use specified buffer size in bytes.\n"
           "                        Defaults to %d bytes.\n", DEFAULT_BUFFER_SIZE);
    printf("    --directory, -d     Use specified directory to store received files.\n"
           "                        Format is: /home/user/folder/.\n");
    printf("    --legacy,    -L     Send using the old protocol (no MD5, no path).\n"
           "                        Use this when talking to pre-v2 siphon peers.\n\n");
}

static void show_version(void) {
    printf("Siphon - Utility to send files via direct connection, written in C\n");
    printf("TomClaw Software\n");
    printf("Version 2.0\n");
}

/* Add a single -f argument to the file list, picking the right strategy
 * based on whether the path points to a regular file or a directory. */
static int add_file_arg(file_list *files, const char *arg) {
    if (is_directory(arg)) {
        return scan_dir(arg, files);
    }
    /* Regular file: base_dir is its parent so the recipient gets it flat. */
    char *parent = path_dirname(arg);
    if (parent == NULL) return -1;
    int rc = file_list_add(files, arg, parent);
    free(parent);
    return rc;
}

int main(int argc, char **argv) {
    file_list files;
    file_list_init(&files);

    char *host = NULL;
    const char *directory = "";
    int port = DEFAULT_PORT;
    int buffer_size = DEFAULT_BUFFER_SIZE;
    int use_legacy = 0;
    int mode_set = 0;  /* 1 once -l or -c is seen */

    static struct option long_options[] = {
        {"listen",    no_argument,       0, 'l'},
        {"connect",   required_argument, 0, 'c'},
        {"version",   no_argument,       0, 'v'},
        {"help",      no_argument,       0, 'h'},
        {"port",      required_argument, 0, 'p'},
        {"file",      required_argument, 0, 'f'},
        {"buffer",    required_argument, 0, 'b'},
        {"directory", required_argument, 0, 'd'},
        {"legacy",    no_argument,       0, 'L'},
        {0, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "lc:vhp:f:b:d:L", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'l':
                host = NULL;
                mode_set = 1;
                break;
            case 'c':
                host = optarg;
                mode_set = 1;
                break;
            case 'v':
                show_version();
                file_list_free(&files);
                return EXIT_SUCCESS;
            case 'h':
                show_help();
                file_list_free(&files);
                return EXIT_SUCCESS;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                if (add_file_arg(&files, optarg) != 0) {
                    fprintf(stderr, "Unable to add %s\n", optarg);
                    file_list_free(&files);
                    return EXIT_FAILURE;
                }
                break;
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 'd':
                directory = optarg;
                break;
            case 'L':
                use_legacy = 1;
                break;
            case '?':
                /* getopt_long has already printed an error message. */
                file_list_free(&files);
                return EXIT_FAILURE;
            default:
                show_help();
                file_list_free(&files);
                return EXIT_FAILURE;
        }
    }

    if (!mode_set || optind < argc) {
        fprintf(stderr, "You must specify --listen or --connect.\n\n");
        show_help();
        file_list_free(&files);
        return EXIT_FAILURE;
    }

    int sock = open_socket(host, port);
    if (sock < 0) {
        file_list_free(&files);
        return EXIT_FAILURE;
    }

    /* Listener reads first, then sends. Client does the opposite. */
    int rc;
    if (host != NULL) {
        rc = send_files(sock, &files, buffer_size, use_legacy);
        if (rc == 0) rc = receive_files(sock, directory, buffer_size);
    } else {
        rc = receive_files(sock, directory, buffer_size);
        if (rc == 0) rc = send_files(sock, &files, buffer_size, use_legacy);
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
    file_list_free(&files);
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
