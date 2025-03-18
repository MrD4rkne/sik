#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <endian.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "sum-common.h"
#include "err.h"
#include "common.h"
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>

#define QUEUE_LENGTH  5
#define SOCK_TIMEOUT  4

#define BUFFER_SIZE 10000

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fatal("usage: %s <port>", argv[0]);
    }

    uint16_t port = read_port(argv[1]);

    // Create a socket. Buffer should not be allocated on the stack.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
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

    // Switch the socket to listening.
    if (listen(socket_fd, QUEUE_LENGTH) < 0) {
        syserr("listen");
    }

    // Find out what port the server is actually listening on.
    socklen_t lenght = (socklen_t) sizeof server_address;
    if (getsockname(socket_fd, (struct sockaddr *) &server_address, &lenght) < 0) {
        syserr("getsockname");
    }

    char* buffer = malloc(BUFFER_SIZE);
    if(buffer == NULL) {
        syserr("malloc");
    }

    for (;;) {
        printf("waiting on port %" PRIu16 "\n", ntohs(server_address.sin_port));

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
            time_t loop_start_time = time(NULL);

            memset(buffer, 0, BUFFER_SIZE); // Clean the buffer.

            ssize_t read_length = read(client_fd, buffer, BUFFER_SIZE);
            if (reaPRIu64 "e {
                    error("read");
                }
                break;
            }
            else if (read_length == 0) {
                printf("connection closed\n");
                break;
            }

            received_total += read_length;

            time_t end_time = time(NULL);
            printf("current bytes: %" PRIu64 " in %.2f seconds, total sum: %" PRIu64 ", time: %.2f seconds\n", 
                   (uint64_t)read_length, difftime(end_time, loop_start_time), received_total, difftime(end_time, start_time));
        }

        time_t end_time = time(NULL);
        double duration = difftime(end_time, start_time);
        printf("received %" PRIu64 " bytes in %.2f seconds\n", received_total, duration);
    }

    printf("exchange finished\n");

    close(socket_fd);
    return 0;
}
