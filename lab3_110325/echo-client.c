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

#include "err.h"

static uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port == 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

static struct sockaddr_in get_server_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;   // IPv4
    send_address.sin_addr.s_addr =       // IP address
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

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
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
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
    connect(socket_fd, (struct sockaddr *) &server_address, address_length);

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
