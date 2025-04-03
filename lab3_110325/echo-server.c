#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#include "err.h"

#define QUEUE_LENGTH  5
#define SOCK_TIMEOUT  4

#define BUFFER_SIZE 100000

static uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port == 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fatal("usage: %s <port>", argv[0]);
    }

    uint16_t port = read_port(argv[1]);

    // Create a socket. Buffer should not be allocated on the stack.
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // Bind the socket to a concrete address.
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
        syserr("bind");
    }

    char* buffer = malloc(BUFFER_SIZE);
    if(buffer == NULL) {
        syserr("malloc");
    }

    for (;;) {
        struct sockaddr_in client_address;

        int client_fd = accept(socket_fd, (struct sockaddr *) &client_address,
                               &((socklen_t){sizeof(client_address)}));
        if (client_fd < 0) {
            syserr("accept");
        }

        char const *client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);
        printf("accepted connection from %s:%" PRIu16 "\n", client_ip, client_port);

        // Set timeouts for the client socket.
        struct timeval to = {.tv_sec = SOCK_TIMEOUT, .tv_usec = 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to);

        uint64_t received_total = 0;
        time_t start_time = time(NULL);

        for(;;) {
            memset(buffer, 0, BUFFER_SIZE); // Clean the buffer.

            ssize_t read_length = read(client_fd, buffer, BUFFER_SIZE);
            if (read_length < 0) {
                if (errno == EAGAIN) {
                    printf("timeout\n");
                }
                else {
                    perror("readn");
                }
                break;
            }
            else if (read_length == 0) {
                printf("connection closed\n");
                break;
            }
            else if ((size_t) read_length < sizeof buffer) {
                printf("connection closed without providing full data structure\n");
                break;
            }

            received_total += read_length;
        }

        time_t end_time = time(NULL);
        printf("received %" PRIu64 " bytes in %ld seconds\n", received_total, end_time - start_time);
    }

    printf("exchange finished\n");

    close(socket_fd);
    return 0;
}
