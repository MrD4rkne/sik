#include <errno.h>
#include <endian.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

#include "err.h"
#include "common.h"

#define BUFFER_SIZE 1024
#define QUEUE_LENGTH 5
#define TIMEOUT 5000
#define CONNECTIONS 3

static bool finish = false;

/* Termination signal handling. */
static void catch_int(int sig)
{
    finish = true;
    printf("signal %d catched so no new connections will be accepted\n", sig);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fatal("usage: %s <port> <control port>", argv[0]);
    }

    install_signal_handler(SIGINT, catch_int, SA_RESTART);

    uint16_t port = read_port(argv[1]);
    uint16_t control_port = read_port(argv[2]);
    printf("port: %" PRIu16 "\n", port);
    printf("control port: %" PRIu16 "\n", control_port);

    // Create a socket.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        syserr("cannot create a socket");
    }

    // Bind the socket to a concrete address.
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;                // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *)&server_address, (socklen_t)sizeof server_address) < 0)
    {
        syserr("bind");
    }

    // Switch the socket to listening.
    if (listen(socket_fd, QUEUE_LENGTH) < 0)
    {
        syserr("listen");
    }

    // Bind control port.
    int control_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (control_socket_fd < 0)
    {
        syserr("cannot create a control socket");
    }

    struct sockaddr_in control_address;
    control_address.sin_family = AF_INET;                // IPv4
    control_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
    control_address.sin_port = htons(control_port);

    if (bind(control_socket_fd, (struct sockaddr *)&control_address, (socklen_t)sizeof control_address) < 0)
    {
        syserr("bind control socket");
    }

    // Switch the control socket to listening.
    if (listen(control_socket_fd, QUEUE_LENGTH) < 0)
    {
        syserr("listen control socket");
    }

    // Find out what port the server is actually listening on.
    socklen_t lenght = (socklen_t)sizeof server_address;
    if (getsockname(socket_fd, (struct sockaddr *)&server_address, &lenght) < 0)
    {
        syserr("getsockname");
    }

    printf("listening on port %" PRIu16 "\n", ntohs(server_address.sin_port));

    socklen_t control_lenght = (socklen_t)sizeof control_address;
    if (getsockname(control_socket_fd, (struct sockaddr *)&control_address, &control_lenght) < 0)
    {
        syserr("getsockname control socket");
    }
    printf("control socket listening on port %" PRIu16 "\n", ntohs(control_address.sin_port));

    // Initialization of pollfd structures.
    struct pollfd poll_descriptors[CONNECTIONS + 1];

    // The main socket has index 0.
    poll_descriptors[0].fd = socket_fd;
    poll_descriptors[0].events = POLLIN;

    // The control socket has index 1.
    poll_descriptors[1].fd = control_socket_fd;
    poll_descriptors[1].events = POLLIN;

    for (int i = 2; i < CONNECTIONS + 1; ++i)
    {
        poll_descriptors[i].fd = -1;
        poll_descriptors[i].events = POLLIN;
    }
    size_t active_clients = 0;
    size_t total_clients = 0;
    static char buffer[BUFFER_SIZE];
    struct sockaddr_in client_address;

    int control_client_fd = -1;

    do
    {
        for (int i = 0; i < CONNECTIONS + 1; ++i)
        {
            poll_descriptors[i].revents = 0;
        }

        // After Ctrl-C the main socket is closed.
        if (finish && poll_descriptors[0].fd >= 0)
        {
            close(poll_descriptors[0].fd);
            poll_descriptors[0].fd = -1;
        }

        int poll_status = poll(poll_descriptors, CONNECTIONS + 1, TIMEOUT);
        if (poll_status == -1)
        {
            if (errno == EINTR)
            {
                printf("interrupted system call\n");
            }
            else
            {
                syserr("poll");
            }
        }
        else if (poll_status > 0)
        {
            if (!finish && (poll_descriptors[0].revents & POLLIN))
            {
                // New connection: new client is accepted.
                int client_fd = accept(poll_descriptors[0].fd,
                                       (struct sockaddr *)&client_address,
                                       &((socklen_t){sizeof client_address}));
                if (client_fd < 0)
                {
                    syserr("accept");
                }

                // Searching for a free slot.
                bool accepted = false;
                for (int i = 2; i < CONNECTIONS + 1; ++i)
                {
                    if (poll_descriptors[i].fd == -1)
                    {
                        printf("received new connection (%d)\n", i);
                        poll_descriptors[i].fd = client_fd;
                        poll_descriptors[i].events = POLLIN;
                        active_clients++;
                        total_clients++;
                        accepted = true;
                        break;
                    }
                }
                if (!accepted)
                {
                    close(client_fd);
                    printf("too many clients\n");
                }
                else
                {
                    char const *client_ip = inet_ntoa(client_address.sin_addr);
                    uint16_t client_port = ntohs(client_address.sin_port);
                    printf("accepted connection from %s:%" PRIu16 "\n",
                           client_ip, client_port);
                }
            }

            if (!finish && control_client_fd == -1 && (poll_descriptors[1].revents & POLLIN))
            {
                // New control connection: new client is accepted.
                int control_client = accept(poll_descriptors[1].fd,
                                            (struct sockaddr *)&client_address,
                                            &((socklen_t){sizeof client_address}));
                if (control_client < 0)
                {
                    syserr("accept control");
                }

                // Searching for a free slot.
                bool accepted = false;
                for (int i = 2; i < CONNECTIONS + 1; ++i)
                {
                    if (poll_descriptors[i].fd == -1)
                    {
                        printf("received new connection (%d)\n", i);
                        poll_descriptors[i].fd = control_client;
                        control_client_fd = i;
                        poll_descriptors[i].events = POLLIN;
                        accepted = true;
                        break;
                    }
                }
                if (!accepted)
                {
                    close(control_client);
                    printf("too many clients\n");
                    control_client_fd = -1;
                }
                else
                {
                    char const *client_ip = inet_ntoa(client_address.sin_addr);
                    uint16_t client_port = ntohs(client_address.sin_port);
                    printf("accepted control connection from %s:%" PRIu16 "\n",
                           client_ip, client_port);
                }
            }

            // Serve data connections.
            for (int i = 2; i < CONNECTIONS + 1; ++i)
            {
                if (poll_descriptors[i].fd != -1 && (poll_descriptors[i].revents & (POLLIN | POLLERR)))
                {
                    memset(buffer, 0, BUFFER_SIZE);

                    ssize_t received_bytes = read(poll_descriptors[i].fd, buffer, BUFFER_SIZE);
                    if (received_bytes < 0)
                    {
                        error("error when reading message from connection %d", i);
                        close(poll_descriptors[i].fd);
                        if (i == control_client_fd)
                        {
                            control_client_fd = -1;
                        }
                        else{
                            active_clients--;
                        }
                        poll_descriptors[i].fd = -1;
                    }
                    else if (received_bytes == 0)
                    {
                        printf("ending connection (%d)\n", i);
                        close(poll_descriptors[i].fd);
                        if (i == control_client_fd)
                        {
                            control_client_fd = -1;
                        }
                        else{
                            active_clients--;
                        }
                        poll_descriptors[i].fd = -1;
                    }
                    else
                    {
                        if (i != control_client_fd)
                        {
                            printf("received %zd bytes within connection (%d): '%.*s'\n",
                                received_bytes, i, (int)received_bytes, buffer);
                        }
                        else
                        {
                            if (strncmp(buffer, "count", 5) == 0)
                            {
                                printf("received count command\n");

                                ssize_t bytes = snprintf(buffer, BUFFER_SIZE, "number of active clients: %zu\ntotal number of clients: %zu\n", active_clients, total_clients);
                                if (bytes < 0)
                                {
                                    error("error when writing message to connection %d", i);
                                    close(poll_descriptors[i].fd);
                                    control_client_fd = -1;
                                    poll_descriptors[i].fd = -1;
                                    active_clients--;
                                }

                                //printf("sending %zu bytes response to connection (%d)\n", bytes, i);

                                size_t bytes_written = 0;
                                while (bytes_written < (size_t)bytes)
                                {
                                    printf("writing %zu bytes to connection (%d)\n", bytes - bytes_written, i);
                                    ssize_t bytesw = write(poll_descriptors[i].fd, buffer + bytes_written, bytes - bytes_written);
                                    if (bytesw < 0)
                                    {
                                        error("error when writing message to connection %d", i);
                                        close(poll_descriptors[i].fd);
                                        control_client_fd = -1;
                                        poll_descriptors[i].fd = -1;
                                        break;
                                    }
                                    if (bytesw == 0)
                                    {
                                        error("zero bytes written to connection %d", i);
                                        break;
                                    }

                                    bytes_written += bytesw;
                                    printf("written %zu bytes to connection (%d)\n", bytes_written, i);
                                }
                            }
                            else{
                                printf("received unknown command: '%.*s'\n", (int)received_bytes, buffer);
                            }

                            close(poll_descriptors[i].fd);
                            control_client_fd = -1;
                            poll_descriptors[i].fd = -1;
                        }
                    }
                }
            }
        }
        else
        {
            printf("%d milliseconds passed without any events\n", TIMEOUT);
        }
    } while (!finish || active_clients > 0);

    if (control_client_fd != -1)
    {
        close(poll_descriptors[control_client_fd].fd);
        poll_descriptors[control_client_fd].fd = -1;
    }

    if (poll_descriptors[0].fd >= 0)
    {
        close(poll_descriptors[0].fd);
    }
}
