#include <endian.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "sum-common.h"
#include "err.h"
#include "common.h"
#include <sys/socket.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fatal("usage: %s <host> <port> <n> <k>\n", argv[0]);
    }

    char const *host = argv[1];
    uint16_t port = read_port(argv[2]);

    struct sockaddr_in server_address = get_server_address(host, port);
    char const *server_ip = inet_ntoa(server_address.sin_addr);
    uint16_t server_port = ntohs(server_address.sin_port);

    // Create a socket.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    long long n = atoll(argv[3]);
    long long k = atoll(argv[4]);

    char* message = (char*) malloc(k * sizeof(char));
    if (message == NULL) {
        syserr("malloc");
    }

    size_t message_length = k;

    socklen_t address_length = (socklen_t) sizeof(server_address);

    if(connect(socket_fd, (struct sockaddr *) &server_address, address_length) < 0) {
        syserr("connect");
    }

    for (long long i = 0; i<n; i++) {

        // Send a message.
        printf("Attempt %lld\n", i);
        
        ssize_t sent_length = write(socket_fd, message, message_length);
        if (sent_length < 0) {
            syserr("sendto");
        }
        else if ((size_t) sent_length != message_length) {
            fatal("incomplete sending");
        }

        printf("sent to %s:%" PRIu16 "\n", server_ip, server_port); 
    }

    free(message);

    close(socket_fd);
    return 0;
}
