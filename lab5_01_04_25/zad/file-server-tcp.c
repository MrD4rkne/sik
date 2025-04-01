#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <endian.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "err.h"
#include "common.h"
#include "protocol.h"

#define QUEUE_LENGTH    5
#define BUFFER_SIZE     1024

#define boilerplate(ip, port, msg, ...) \
"client [%s:%" PRIu16 "]" msg, ip, port, ##__VA_ARGS__

static ssize_t read_until(int client_fd, size_t bytes_to_read, void* buffer){
    size_t bytes_read = 0;
    while (bytes_read < bytes_to_read) {
        ssize_t result = read(client_fd, buffer + bytes_read, bytes_to_read - bytes_read);
        if (result < 0) {
            return -1;
        } else if (result == 0) {
            break;
        }
        bytes_read += result;
    }

    return bytes_read;
}

static size_t write_all(int client_fd, size_t bytes_to_write, const void* buffer){
    size_t bytes_written = 0;
    while (bytes_written < bytes_to_write) {
        ssize_t result = write(client_fd, buffer + bytes_written, bytes_to_write - bytes_written);
        if (result < 0) {
            return -1;
        }
        bytes_written += result;
    }

    return bytes_written;
}

static char* get_file_content(int client_fd, uint32_t file_length, const char* ip, uint16_t port) {
    char* buffer = malloc(file_length);
    if (buffer == NULL) {
        return NULL;
    }

    ssize_t bytes_read = read_until(client_fd, file_length, buffer);
    if (bytes_read < 0) {
        fprintf(stderr, boilerplate(ip, port," Failed to read file content\n"));
        free(buffer);
        return NULL;
    }
    if (bytes_read < file_length) {
        fprintf(stderr, boilerplate(ip, port," Incomplete file data received\n"));
        free(buffer);
        return NULL;
    }

    return buffer;
}

typedef struct message {
    char const *client_ip;
    uint16_t client_port;
    int client_fd;
} message_t;

void *handle_connection(void *message_ptr) {
    message_t message = *((message_t *) message_ptr);
    free(message_ptr);

    data_header msg;
    ssize_t bytes_read = read_until(message.client_fd, sizeof(msg), &msg);
    if (bytes_read < 0) {
        perror("read");
        close(message.client_fd);
        pthread_exit(NULL);
    } else if (bytes_read != sizeof(msg)) {
        fprintf(stderr, boilerplate(message.client_ip, message.client_port," Incomplete message received\n"));
        close(message.client_fd);
        pthread_exit(NULL);
    }

    // Convert fields from network byte order to host byte order
    msg.file_length= ntohl(msg.file_length);
    msg.name_length= ntohs(msg.name_length);

    char* file_name = malloc(msg.name_length + 1);
    if (file_name == NULL) {
        perror("malloc");
        close(message.client_fd);
        pthread_exit(NULL);
    }

    bytes_read = read_until(message.client_fd, msg.name_length, file_name);
    if (bytes_read < 0) {
        perror("read");
        free(file_name);
        close(message.client_fd);
        pthread_exit(NULL);
    } else if (bytes_read != msg.name_length) {
        fprintf(stderr, boilerplate(message.client_ip, message.client_port, " Incomplete file name received\n"));
        free(file_name);
        close(message.client_fd);
        pthread_exit(NULL);
    }

    printf(boilerplate(message.client_ip, message.client_port," size=[%d] file=[%.*s]\n", 
           msg.file_length, msg.name_length, file_name));

    sleep(1);

    char* buffer = get_file_content(message.client_fd, msg.file_length, message.client_ip, message.client_port);
    if (buffer == NULL) {
        free(file_name);
        close(message.client_fd);
        pthread_exit(NULL);
    }

    close(message.client_fd);
    printf(boilerplate(message.client_ip, message.client_port," has sent its file of size=[%d]\n", msg.file_length));

    // Save the file to disk
    FILE* file = fopen(file_name, "wxb");
    if (file == NULL) {
        if (errno == EEXIST) {
            fprintf(stderr, boilerplate(message.client_ip, message.client_port," File already exists: %s\n", file_name));
        } else {
            fprintf(stderr, boilerplate(message.client_ip, message.client_port," Could not open file: %s\n", file_name));
            perror("fopen");
        }

        free(buffer);
        free(file_name);
        pthread_exit(NULL);
    }

    size_t bytes_written = write_all(fileno(file), msg.file_length, buffer);
    if (bytes_written < msg.file_length) {
        fprintf(stderr, boilerplate(message.client_ip, message.client_port," Writing to file failed: %s\n", file_name));
        fclose(file);
        free(buffer);
        free(file_name);
        pthread_exit(NULL);
    }

    fclose(file);
    free(buffer);
    free(file_name);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fatal("usage: %s <port>", argv[0]);
    }

    uint16_t port = read_port(argv[1]);

    // Create a socket.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // Bind the socket to a concrete address.
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof server_address) < 0) {
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

    printf("server is listening on port %" PRIu16 "\n", ntohs(server_address.sin_port));

    for (;;) {
        struct sockaddr_in client_address;
        int client_fd = accept(socket_fd, (struct sockaddr *) &client_address,
                               &((socklen_t) {sizeof client_address}));
        if (client_fd < 0) {
            syserr("accept");
        }

        char const *client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);

        message_t* msg = malloc(sizeof (message_t));
        if (msg == NULL) {
            error("malloc - could not allocate memory for msg");
            
            if(close(client_fd) < 0){
                syserr("close");
            }

            continue;
        }
        
        msg->client_ip = client_ip;
        msg->client_port = client_port;
        msg->client_fd = client_fd;

        pthread_t thread;
        if (pthread_create(&thread, 0, handle_connection, msg) != 0) {
            error("pthread_create - could not create thread");
            free(msg);
            if (close(client_fd) < 0) {
                syserr("close");
            }
            continue;
        }
        else if (pthread_detach(thread) != 0) {
            syserr("pthread_detach");
        }
    }
    close(socket_fd);
    exit(0);
}
