#include <netdb.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "cb.h"
#include "err.h"

#define MAX_HOSTNAME_LEN  255
#define MAX_PORT_LEN      5

char *progName = NULL;

static void helpAndExit()
{
  fatal("invocation: %s UDPaddress UDPport TCPport", progName);
}

struct sockaddr_and_len {
  socklen_t addr_len;
  struct sockaddr_storage addr;
};

static struct sockaddr_and_len _get_first_matching_addr(const char *hostStr,
                                                        const char *portStr,
                                                        int family, int flags)
{
  struct addrinfo *ai;
  struct addrinfo hints = {.ai_family = family, .ai_socktype = 0,
                           .ai_protocol = 0, .ai_flags = flags};

  int gai_error = getaddrinfo(hostStr, portStr, &hints, &ai);
  if (gai_error != 0) {
    fprintf(stderr, "getaddrinfo failed: %s", gai_strerror(gai_error));
    helpAndExit();
  }

  struct sockaddr_and_len ret;
  ret.addr_len = ai->ai_addrlen;
  *(struct sockaddr_in *)&(ret.addr) = *(struct sockaddr_in *)(ai->ai_addr);
  freeaddrinfo(ai);

  return ret;
}

static struct sockaddr_and_len parseHostAndPort(const char *hostStr, const char *portStr)
{
  return _get_first_matching_addr(hostStr, portStr, AF_INET, 0);
}

static struct sockaddr_and_len getListeningAddress(const char *portStr)
{
  return _get_first_matching_addr(NULL, portStr, AF_INET, AI_PASSIVE);
}

bool cbIsEmpty(CircularBuffer *b)
{
  return cbGetContinuousCount(b) == 0;
}

// Parametry: adres i port UDP są do rozgłaszania i słuchania,
// a port tcp do nasłuchiwania.
int main(int argc, char *argv[])
{
  progName = argv[0];
  if (argc != 4) {
    helpAndExit();
  }

  struct sockaddr_and_len udp_sending_address = parseHostAndPort(argv[1], argv[2]);
  struct sockaddr_and_len udp_listening_address = getListeningAddress(argv[2]);
  struct sockaddr_and_len tcp_address = getListeningAddress(argv[3]);

  int tcp_listen_sock;
  if ((tcp_listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    syserr("socket");
  }

  if (bind(tcp_listen_sock, (struct sockaddr *)&(tcp_address.addr), tcp_address.addr_len) < 0) {
    syserr("bind");
  }

  if (listen(tcp_listen_sock, 1) < 0) {
    syserr("bind");
  }

  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof peer;
  int tcp_sock;
  if ((tcp_sock = accept(tcp_listen_sock, (struct sockaddr *)&peer, &peer_len)) < 0) {
    syserr("accept");
  }

  char peer_addr_str[MAX_HOSTNAME_LEN + 1];
  char peer_port_str[MAX_PORT_LEN + 1];
  int gai_error = getnameinfo((struct sockaddr *)&peer, peer_len, peer_addr_str, (socklen_t)(sizeof peer_addr_str),
                              peer_port_str, (socklen_t)(sizeof peer_port_str), NI_NUMERICHOST | NI_NUMERICSERV);
  if (gai_error != 0) {
    fprintf(stderr, "getnameinfo failed: %s", gai_strerror(gai_error));
    helpAndExit();
  }
  printf("TCP connection to %s:%s established.\n", peer_addr_str, peer_port_str);

  close(tcp_listen_sock);

  int udp_sock;
  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    syserr("UDP socket");
  }

  int enable = 1;
  if (setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
    syserr("setsockopt broadcast");
  }

  if (bind(udp_sock, (struct sockaddr *)&(udp_listening_address.addr), 
           udp_listening_address.addr_len) < 0) {
    syserr("UDP bind");
  }

  struct pollfd fds[2];
  fds[0].fd = tcp_sock;
  fds[0].events = POLLIN;
  fds[1].fd = udp_sock;
  fds[1].events = POLLIN;

  bool tcp_closed = false;

  CircularBuffer tcp_buffer, udp_buffer;
  cbInit(&tcp_buffer);
  cbInit(&udp_buffer);

  char buf[1024];
  ssize_t bytes_read;

  while ((!tcp_closed && !cbIsEmpty(&tcp_buffer)) || !cbIsEmpty(&udp_buffer)) {
    int ret = poll(fds, 2, -1);
    if (ret < 0) {
      syserr("poll");
    }

    // Check TCP socket
    if (fds[0].revents & POLLIN) {
      bytes_read = read(tcp_sock, buf, sizeof(buf));
      if (bytes_read < 0) {
        syserr("read from TCP socket");
      } else if (bytes_read == 0) {
        printf("TCP connection closed.\n");
        tcp_closed = true;
        fds[0].fd = -1; // Remove from polling
      } else {
        cbPushBack(&udp_buffer, buf, bytes_read);
      }
    }

    // Check UDP socket
    if (!tcp_closed && fds[1].revents & POLLIN) {
      bytes_read = recvfrom(udp_sock, buf, sizeof(buf), 0, NULL, NULL);
      if (bytes_read < 0) {
        syserr("recvfrom UDP socket");
      } else {
        cbPushBack(&tcp_buffer, buf, bytes_read);
      }
    }

    // Forward data from TCP buffer to UDP
    if (!cbIsEmpty(&udp_buffer)) {
      ssize_t bytes_to_send = cbBytesReady(&udp_buffer);
      if (bytes_to_send > sizeof(buf)) {
        bytes_to_send = sizeof(buf);
      }
      char* data = cbGetData(&udp_buffer);
      
      ssize_t bytes_sent = sendto(udp_sock, data, bytes_to_send, 0,
                               (struct sockaddr *)&(udp_sending_address.addr),
                               udp_sending_address.addr_len);
      if (bytes_sent < 0) {
        syserr("sendto UDP");
      }

      cbDropFront(&udp_buffer, bytes_sent);
    }

    // Forward data from UDP buffer to TCP
    if (!cbIsEmpty(&tcp_buffer) && !tcp_closed) {
      ssize_t bytes_to_send = cbBytesReady(&udp_buffer);
      if (bytes_to_send > sizeof(buf)) {
        bytes_to_send = sizeof(buf);
      }
      
      char* data = cbGetData(&tcp_buffer);
      
      ssize_t bytes_sent = write(tcp_sock, data, bytes_to_send);
      if (bytes_sent < 0) {
        syserr("write to TCP socket");
      }
      cbDropFront(&tcp_buffer, bytes_sent);

      if (bytes_sent == 0) {
        printf("TCP connection closed.\n");
        tcp_closed = true;
        fds[0].fd = -1; // Remove from polling
      }
    }
  }

  cbDestroy(&tcp_buffer);
  cbDestroy(&udp_buffer);

  close(udp_sock);

  close(tcp_sock);

  return 0;
}
