#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 65507

int setup_socket(in_port_t port) {
  int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock_fd == -1) {
    err(EXIT_FAILURE, "failed to open socket for communication");
  }
  int on = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
    err(EXIT_FAILURE, "failed to set broadcast option on the socket");
  }
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    err(EXIT_FAILURE, "failed to set address reusal option on the socket");
  }
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
    err(EXIT_FAILURE, "failed to set port reusal option on the socket");
  }
  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = port;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
    err(EXIT_FAILURE,
        "failed to assign a local socket address %hu to the socket",
        port);
  }
  return sock_fd;
}

int setup_epoll(int sock_fd) {
  // Create epoll with the socket and stdin
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    err(EXIT_FAILURE, "could not create epoll for input monitoring");
  }

  struct epoll_event sock_event = {
      .events = EPOLLIN,
      .data.fd = sock_fd,
  };
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &sock_event) < 0) {
    err(EXIT_FAILURE, "could not add socket to epoll");
  }
  struct epoll_event stdin_event = {
      .events = EPOLLIN,
      .data.fd = STDIN_FILENO,
  };
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &stdin_event) < 0) {
    err(EXIT_FAILURE, "could not add socket to epoll");
  }
  return epoll_fd;
}

int send_message(int sock_fd, char *message_buffer, in_port_t port) {
  // Set the broadcast addr
  struct sockaddr_in broadcast_addr;
  memset(&broadcast_addr, 0, sizeof(broadcast_addr));
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_port = port;
  broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
  // Read message from stdin, due to buffering will get a \n terminated line
  ssize_t message_length =
      read(STDIN_FILENO, message_buffer, MAX_MESSAGE_SIZE - 1);
  if (message_length < 0) {
    err(EXIT_FAILURE, "could not read stdin");
  }
  message_buffer[message_length] = '\0';
  if (strncmp(message_buffer, "\\q", 2) == 0) {
    return 1;
  }
  ssize_t sent_bytes =
      sendto(sock_fd, message_buffer, (size_t)message_length, 0,
             (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
  if (sent_bytes < 0) {
    err(EXIT_FAILURE, "could not send message");
  }
  printf("me: %s", message_buffer);
  return 0;
}

void receive_message(int sock_fd, char *message_buffer) {
  struct sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);
  ssize_t message_length =
      recvfrom(sock_fd, message_buffer, MAX_MESSAGE_SIZE - 1, 0,
               (struct sockaddr *)&sender_addr, &addr_len);
  if (message_length < 0) {
    err(EXIT_FAILURE, "could not read message from the network");
  }
  message_buffer[message_length] = '\0';
  char sender_addr_string[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_addr_string,
            INET_ADDRSTRLEN);
  printf("%s: %s", sender_addr_string, message_buffer);
}

// This code works if the host doesn't have unconventional network
// configuration. Works in Docker containers.

int main(int argc, char **argv) {
  // Argument processing
  if (argc < 2) {
    errx(EXIT_FAILURE, "usage: %s {port}", argv[0]);
  }

  // Get the port number
  in_port_t port = htons((short)strtol(argv[1], NULL, 0));
  if (errno) {
    err(EXIT_FAILURE, "please provide a valid port");
  }

  // Set up the socket and create a buffer for the messages
  int sock_fd = setup_socket(port);
  char *message_buffer = malloc(sizeof(char) * MAX_MESSAGE_SIZE);

  int epoll_fd = setup_epoll(sock_fd);

  while (true) {
    struct epoll_event incoming_event;
    int epoll_status = epoll_wait(epoll_fd, &incoming_event, 1, -1);
    if (epoll_status < 0) {
      err(EXIT_FAILURE, "error while waiting for input");
    }
    if (incoming_event.data.fd == STDIN_FILENO) {
      if (send_message(sock_fd, message_buffer, port)) {
        break;
      }
    } else {
      receive_message(sock_fd, message_buffer);
    }
  }
  free(message_buffer);
  close(sock_fd);
  close(epoll_fd);
  return EXIT_SUCCESS;
}
