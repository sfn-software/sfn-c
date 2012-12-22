/* 
 * File:   main.c
 * Author: solkin
 *
 * Created on December 19, 2012, 10:53 PM
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#define BLOCK_FILE_START 0x01
#define BLOCK_FILE_END   0x02
#define BLOCK_FILE_FAIL  0x02

size_t buffer_size = 0x1000;

/** Main methods **/
int open_file(const char *file_path, int flags);
int open_socket(const char *ip);
int send_file(const char *file_path, const char *ip);
int load_file(const char *directory, const char *ip);
int transfer_data(int src, int dest, off_t file_seek, off_t file_size);

/** Socket utils */
void *read_data(int src, char stop_byte, off_t stop_size);

/** File utils **/
off_t fsize(const char *file_path);
const char *fname(const char *file_path);
char *fpath(const char *file_name, const char *directory);

/** Progress methods **/
static void setup_bar(const char *file_name, off_t file_size);
static inline void show_bar(size_t total_read);

/** Progress static variables **/
static int bar_percent, bar_c;
static const char *bar_size_metrics;
static char bar_progress[22];
static const char *bar_file_name;
static off_t bar_file_size;
static size_t bar_total_read;
static size_t bar_total_read_bytes;

/*
 * 
 */
int main(int argc, char** argv) {

  // load_file("/home/solkin/Desktop/", NULL);
  send_file("/home/solkin/Downloads/jdk-6u37-linux-i586.bin", "127.0.0.1");

  return (EXIT_SUCCESS);
}

int open_file(const char *file_path, int flags) {
  /** Opening file **/
  int mode = 0644;
  if (flags & O_CREAT > 0) {
    mode = 0777;
  }
  int a_file = open(file_path, flags, mode);
  if (a_file == -1) {
    fprintf(stderr, "Unable to open file %s:\n%s", file_path,
            strerror(errno));
    return EXIT_FAILURE;
  }
  return a_file;
}

int open_socket(const char *ip) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr, "Unable to open socket:\n%s", strerror(errno));
    return EXIT_FAILURE;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(3214);
  if (ip != NULL) {
    addr.sin_addr.s_addr = inet_addr(ip); // inet_addr("77.108.234.195"); // htonl(INADDR_LOOPBACK);
    if (connect(sock, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
      fprintf(stderr, "Unable to connect to socket:\n%s", strerror(errno));
      return EXIT_FAILURE;
    }
    return sock;
  } else {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
      fprintf(stderr, "Unable to bind socket:\n%s", strerror(errno));
      return EXIT_FAILURE;
    }
    /** Listening for clients **/
    struct sockaddr_in cli_addr;
    int cli_len;
    /** Waiting for client to connect **/
    listen(sock, 1);
    cli_len = sizeof (cli_addr);
    /** Accepting client connection **/
    int cli_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_len);
    if (cli_sock < 0) {
      fprintf(stderr, "Unable to connect to client:\n%s", strerror(errno));
      return EXIT_FAILURE;
    }
    return cli_sock;
  }
}

int send_file(const char *file_path, const char *ip) {
  char block_type;
  int file, sock;
  /** Opening file descriptor */
  if ((file = open_file(file_path, O_RDONLY)) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  /** Opening socket descriptor */
  if ((sock = open_socket(ip)) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  /** Sending block, file name and file size **/
  block_type = BLOCK_FILE_START; // Maybe, range? 
  write(sock, &block_type, 1);
  const char *file_name = fname(file_path);
  write(sock, file_name, strlen(file_name));
  write(sock, &"\n", 1);
  off_t file_size = fsize(file_path);
  /** Checking for file size unavailable **/
  if (file_size == -1) {
    return EXIT_FAILURE;
  }
  write(sock, &file_size, 8);
  setup_bar(file_name, file_size);
  int trans_cond = transfer_data(file, sock, 0, file_size);
  /** Checking for transfer condition **/
  if (trans_cond == EXIT_FAILURE) {
    /** Reporting another client about error **/
    block_type = BLOCK_FILE_FAIL;
  } else {
    /** Sending end-block **/
    block_type = BLOCK_FILE_END;
  }
  write(sock, &block_type, 1);
  /** Closing streams **/
  close(file);
  shutdown(sock, SHUT_RDWR);
  return EXIT_SUCCESS;
}

int load_file(const char *directory, const char *ip) {
  char block_type;
  int file, sock;
  /** Opening socket descriptor */
  if ((sock = open_socket(ip)) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  /** Sending block, file name and file size **/
  read(sock, &block_type, 1);
  if (block_type == BLOCK_FILE_START) { // Maybe, range? 
    /** Reading file name **/
    const char *file_name = (const char*) read_data(sock, '\n', 0);
    /** Concatinating into file path **/
    char *file_path = fpath(file_name, directory);
    /** Reading file size **/
    off_t file_size = *((unsigned long *) read_data(sock, 0, 8));
    /** Opening file descriptor */
    if ((file = open_file(file_path, O_CREAT | O_WRONLY)) == EXIT_FAILURE) {
      return EXIT_FAILURE;
    }
    setup_bar(file_name, file_size);
    int trans_cond = transfer_data(sock, file, 0, file_size);
    /** Checking for transfer condition **/
    if (trans_cond == EXIT_FAILURE) {
      /** Reporting another client about error **/
      block_type = BLOCK_FILE_FAIL;
    } else {
      /** Sending end-block **/
      block_type = BLOCK_FILE_END;
    }
    /** Reading final block type */
    read(sock, &block_type, 1);
    if (block_type == BLOCK_FILE_START) {
      /** Another file */
    } else if (block_type == BLOCK_FILE_END) {
      /** All files are received */
    } else if (block_type == BLOCK_FILE_FAIL) {
      /** Another client has failed */
    }
    block_type = BLOCK_FILE_END;
    write(sock, &block_type, 1);
    /** Closing streams **/
    close(file);
    shutdown(sock, SHUT_RDWR);
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

int transfer_data(int src, int dest, off_t file_seek, off_t file_size) {
  int bytes_read;
  int bytes_written;
  size_t total_read;
  void *buffer = malloc(buffer_size);
  /** Seek **/
  lseek(src, file_seek, SEEK_SET);
  int b_size = buffer_size;
  /** Reading and sending file **/
  for (total_read = file_seek; total_read < file_size;) {
    if (file_size - total_read < buffer_size) {
      b_size = file_size - total_read;
    } else {
      b_size = buffer_size;
    }
    bytes_read = read(src, buffer, b_size);
    total_read += bytes_read;
    show_bar(total_read);
    if (bytes_read == 0) {
      /** No more data **/
      printf("\n");
      return EXIT_SUCCESS;
    }
    if (bytes_read < 0) {
      /** Error while reading data **/
      fprintf(stderr, "\nUnable to read source:\n%s", strerror(errno));
      return EXIT_FAILURE;
    }
    // void *p = buffer;
    while (bytes_read > 0) {
      bytes_written = write(dest, buffer, bytes_read);
      if (bytes_written <= 0) {
        /** Error while writing data **/
        fprintf(stderr, "\nUnable to write data:\n%s",
                strerror(errno));
        return EXIT_FAILURE;
      }
      bytes_read -= bytes_written;
      buffer += bytes_written;
    }
    buffer -= bytes_written;
  }
  /** Deallocate buffer **/
  free(buffer);
  return EXIT_FAILURE;
}

void *read_data(int src, char stop_byte, off_t stop_size) {
  int size = 1;
  char *data = (char*) malloc(size);
  char buffer;
  /** Reading char by char **/
  while (read(src, &buffer, 1) > 0) {
    /** Comparing byte **/
    if (stop_byte != 0 && buffer == stop_byte) {
      break;
    }
    /** Checking for length **/
    if (stop_size != 0 && size == stop_size) {
      break;
    }
    /** Copying bytes **/
    memcpy(data + size - 1, &buffer, 1);
    size += 1;
    /** Shrinking size **/
    data = (char*) realloc(data, size);
  }
  /** Stop byte **/
  buffer = '\0';
  memcpy(data + size - 1, &buffer, 1);
  return data;
}

off_t fsize(const char *file_path) {
  struct stat stat_struct;
  /** Obtain file info **/
  if (stat(file_path, &stat_struct) == 0) {
    return stat_struct.st_size;
  }
  return -1;
}

const char *fname(const char *file_path) {
  if (file_path) {
    char *slash_index;
    /** Unix FS */
    slash_index = strstr(file_path, "/");
    if (slash_index != NULL) {
      return strrchr(file_path, '/') + 1;
    }
    /** Windows FS */
    slash_index = strstr(file_path, "\\");
    if (slash_index != NULL) {
      return strrchr(file_path, '\\') + 1;
    }
  }
  return NULL;
}

char *fpath(const char *file_name, const char *directory) {
  /** Checking for parameters are specified  */
  if (file_name && directory) {
    /** Allocating memory **/
    char *file_path = (char*) malloc(strlen(directory) + strlen(file_name) + 1);
    /** Concatinating **/
    strcat(file_path, directory);
    strcat(file_path, file_name);
    strcat(file_path, "\0");
    return file_path;
  }
  return NULL;
}

static void setup_bar(const char *file_name, off_t file_size) {
  bar_file_name = file_name;
  bar_file_size = file_size;
}

static inline void show_bar(size_t total_read) {
  bar_total_read_bytes = total_read;
  /** Calculating metrics **/
  if (total_read < 1024) {
    bar_size_metrics = "Byte";
  }
  if (total_read >= 1024) {
    total_read /= 1024;
    bar_size_metrics = "KiB ";
  }
  if (total_read >= 1024) {
    total_read /= 1024;
    bar_size_metrics = "MiB ";
  }
  if (total_read >= 1024) {
    total_read /= 1024;
    bar_size_metrics = "GiB ";
  }
  if (total_read >= 1024) {
    total_read /= 1024;
    bar_size_metrics = "TiB ";
  }
  /** Checking for something changed **/
  if (bar_total_read != total_read
          || ((bar_total_read_bytes * 100) / bar_file_size) != bar_percent) {
    /** Updating values **/
    bar_total_read = total_read;
    bar_percent = (bar_total_read_bytes * 100) / bar_file_size;
    /** Padding **/
    for (bar_c = 0; bar_c < 22; bar_c += 1) {
      if (bar_c * 100 / 22 <= bar_percent) {
        bar_progress[bar_c] = '#';
      } else {
        bar_progress[bar_c] = '-';
      }
    }
    /** Output **/
    printf(" %-32s %4d %4s [%s] %3d %\r", bar_file_name, bar_total_read,
            bar_size_metrics, bar_progress, bar_percent);
  }
}