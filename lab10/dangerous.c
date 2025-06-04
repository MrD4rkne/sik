#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void exploited(void) {
  printf("Jestem wyzyskiwany.\n");
  exit(1);
}

int main() {
  char buff[16];
  printf("Adres funkcji exploited: %p\n", exploited);
  printf("Adres bufora na stosie: %p\n", buff);
  ssize_t r = read(0, buff, 512);
  printf("Liczba przeczytanych bajtów: %zd\n", r);
}
