#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>

void exploited(void) {
  printf("Jestem wyzyskiwany.\n");
  exit(1);
}

int process(int fd){
  char buff[16];
  printf("Adres funkcji exploited: %p\n", exploited);
  printf("Adres bufora na stosie: %p\n", buff);
  ssize_t r = read(fd, buff, 512);
  if(r == 0){
    return 1;
  }
  if(r < 0){
    perror("read");
    exit(EXIT_FAILURE);
  }
  printf("Liczba przeczytanych bajtów: %zd\n", r);
  write(fd, buff, r);
  return 0;
}

int main() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0; // let the OS choose the port

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  if (listen(listen_fd, 5) < 0) {
    perror("listen");
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  // Get the port assigned by the OS
  if (getsockname(listen_fd, (struct sockaddr *)&addr, &addrlen) < 0) {
    perror("getsockname");
    close(listen_fd);
    exit(EXIT_FAILURE);
  }
  printf("Listening on port %d\n", ntohs(addr.sin_port));

  int accept_fd = accept(listen_fd, NULL, NULL);
  if (accept_fd < 0) {
    perror("accept");
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  while(process(accept_fd) == 0);

  close(accept_fd);
  close(listen_fd);
  return 0;
}
