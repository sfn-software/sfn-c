#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int open_socket(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Unable to open socket: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);

    if (ip != NULL) {
        struct addrinfo *addr_info;
        if (getaddrinfo(ip, NULL, NULL, &addr_info) != 0) {
            fprintf(stderr, "Unable to resolve host name: %s\n", strerror(errno));
            close(sock);
            return -1;
        }
        addr = *(struct sockaddr_in *) addr_info->ai_addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t) port);
        freeaddrinfo(addr_info);

        if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            fprintf(stderr, "Unable to connect to socket: %s\n", strerror(errno));
            close(sock);
            return -1;
        }
        return sock;
    }

    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Unable to bind socket: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    if (listen(sock, 1) < 0) {
        fprintf(stderr, "Unable to listen on socket: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int cli_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_len);
    if (cli_sock < 0) {
        fprintf(stderr, "Unable to connect to client: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    /* Stop listening once we have our single peer. */
    close(sock);
    return cli_sock;
}

ssize_t read_total(int fd, void *buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char *) buf + total, n - total);
        if (r == 0) return (ssize_t) total;  /* EOF */
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t) r;
    }
    return (ssize_t) total;
}

ssize_t write_total(int fd, const void *buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, (const char *) buf + total, n - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t) w;
    }
    return (ssize_t) total;
}

char *read_line(int fd) {
    size_t capacity = 64;
    size_t length = 0;
    char *data = malloc(capacity);
    if (data == NULL) return NULL;

    char byte;
    while (1) {
        ssize_t r = read(fd, &byte, 1);
        if (r == 0 || r < 0) {
            free(data);
            return NULL;
        }
        if (byte == '\n') {
            data[length] = '\0';
            return data;
        }
        /* Keep one byte free for the terminator. */
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *bigger = realloc(data, capacity);
            if (bigger == NULL) {
                free(data);
                return NULL;
            }
            data = bigger;
        }
        data[length++] = byte;
    }
}

void write_int64_le(uint8_t out[8], int64_t value) {
    uint64_t u = (uint64_t) value;
    for (int i = 0; i < 8; i++) {
        out[i] = (uint8_t) (u >> (i * 8));
    }
}

int64_t read_int64_le(const uint8_t in[8]) {
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) {
        u |= ((uint64_t) in[i]) << (i * 8);
    }
    return (int64_t) u;
}
