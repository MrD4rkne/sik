#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "err.h"
#include "common.h"
#include "protocol.h"

#define BUF_SIZE (4096 * 64)

int main(int argc, char* argv[]) {
  if (argc != 4 && argc != 5)
    fatal("usage: %s <host> <port> <file_name> [<malice>]", argv[0]);

  const char* host = argv[1];
  uint16_t port = read_port(argv[2]);
  struct sockaddr_in server_address = get_server_address(host, port);

  char* file_name = argv[3];
  int file_fd;
  if ((file_fd = open(file_name, O_RDONLY)) < 0) {
    syserr("open file");
  }

  const long malice = argc == 4 ? 0 : atol(argv[4]);

  const char* name = basename(file_name);
  size_t name_len = strlen(name);
  if (name_len == 0) {
    fatal("will not send a directory");
  }
  else if (name_len > UINT16_MAX) {
    fatal("file name too long");
  }

  struct stat file_info;
  if (fstat(file_fd, &file_info) < 0) {
    syserr("fstat");
  }
  off_t size = file_info.st_size;

  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 0) {
      syserr("cannot create a socket");
  }

  if (connect(socket_fd, (struct sockaddr *) &server_address,
              (socklen_t) sizeof(server_address)) < 0) {
    syserr("cannot connect to the server");
  }

  printf("connected to %s:%" PRIu16 "\n", host, port);

  data_header header = {
    htonl((uint32_t) size),
    htons((uint16_t) name_len)
  };

  if (writen(socket_fd, &header, sizeof header) < 0) {
    syserr("writen header");
  }

  if (writen(socket_fd, name, name_len) < 0) {
    syserr("writen name");
  }

  static char buf[BUF_SIZE];

  if (malice < 0 && (size_t) size > (size_t) -malice) {
    size += malice;
  }

  while (size > 0) {
    size_t len = (size_t)size < sizeof buf ? (size_t)size : sizeof buf;
    ssize_t n = readn(file_fd, buf, len);
    if (n < 0) {
      syserr("read from file");
    }
    if (writen(socket_fd, buf, n) < 0) {
      syserr("write to socket");
    }
    size -= n;
    usleep(1000);
  }

  if (malice > 0 && (size_t) malice <= sizeof buf) {
    if (writen(socket_fd, buf, malice) < 0) {
      syserr("write to socket");
    }
  }

  close(socket_fd);
  close(file_fd);

  return 0;
}
