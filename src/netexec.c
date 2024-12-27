#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_ENQUEUED                                                           \
  100 // arbitrarily defined, in reality would be tweaked
      // based on the load
#define TIMEOUT 2000

void send_whole_file(int file_fd, int sock_fd,
                     char client_addr[INET_ADDRSTRLEN]) {
  struct stat filestat;
  fstat(file_fd, &filestat);
  off_t filesize = filestat.st_size;
  off_t offset = 0;
  ssize_t sent_bytes = 0;
  while (sent_bytes < filesize) {
    sent_bytes = sendfile(sock_fd, file_fd, &offset, filesize);
    if (sent_bytes < 0) {
      warn("failed to send a reply to request from %s", client_addr);
      return;
    }
  }
  return;
}

int setup_socket(in_port_t port) {
  int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_fd == -1) {
    err(EXIT_FAILURE, "failed to open socket for communication");
  }
  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = port;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
    err(EXIT_FAILURE,
        "failed to assign a local socket address %hu to the socket",
        ntohs(port));
  }
  if (listen(sock_fd, MAX_ENQUEUED) < 0) {
    err(EXIT_FAILURE, "failed to listen on port %hu", ntohs(port));
  }
  return sock_fd;
}

char **split_args(char *buffer, size_t buffer_size) {
  if (!buffer || !buffer_size) {
    return NULL;
  }

  size_t out_count = 0;
  char **result = NULL;
  size_t capacity = 0;
  char *current = buffer;
  char *start = NULL;

  while (current < buffer + buffer_size) {
    if (isspace(*current)) {
      if (start) {
        *current = '\0';
        start = NULL;
      }
    } else if (!start) {
      start = current;

      if (out_count == capacity) {
        capacity = capacity == 0 ? 4 : capacity * 2;
        char **new_result = (char **)realloc(result, capacity * sizeof(char *));
        if (!new_result) {
          free(result);
          return NULL;
        }
        result = new_result;
      }

      result[out_count] = start;
      (out_count)++;
    }
    current++;
  }
  if (out_count == capacity) {
    capacity = capacity == 0 ? 4 : capacity * 2;
    char **new_result = (char **)realloc(result, capacity * sizeof(char *));
    if (!new_result) {
      free(result);
      return NULL;
    }
    result = new_result;
  }

  result[out_count] = NULL;

  return result;
}

struct connection_info {
  int sock_fd;
  char connection_addr[INET_ADDRSTRLEN];
};

static void *handle_connection(void *arg) {
  struct connection_info *connection = (struct connection_info *)arg;
  int sock_fd = connection->sock_fd;
  int max_message_size = sysconf(_SC_ARG_MAX);
  char *message_buffer = malloc(max_message_size * sizeof(char));
  for (;;) {
    ssize_t bytes_read = read(sock_fd, message_buffer, max_message_size - 1);
    if (bytes_read == 0) {
      close(sock_fd);
      break;
    }
    if (bytes_read < 0) {
      warn("failed to process a connection to %s", connection->connection_addr);
      free(message_buffer);
      free(arg);
      return NULL;
    }
    message_buffer[bytes_read + 1] = '\0';
    char **argv = split_args(message_buffer, bytes_read);
    if (argv == NULL) {
      warn("failed to process a connection to %s", connection->connection_addr);
      free(message_buffer);
      free(arg);
      return NULL;
    }
    char out_filename[] = "outXXXXXX";
    char err_filename[] = "errXXXXXX";
    int out_fd = mkstemp(out_filename);
    if (out_fd == -1) {
      warn("failed to create a temporary file for stdout while processing %s",
           connection->connection_addr);
      free(message_buffer);
      free(argv);
      free(arg);
      return NULL;
    }
    int err_fd = mkstemp(err_filename);
    if (err_fd == -1) {
      warn("failed to create a temporary file for stderr while processing %s",
           connection->connection_addr);
      free(message_buffer);
      free(argv);
      free(arg);
      return NULL;
    }
    pid_t child_pid = fork();
    if (child_pid < 0) {
      warn("failed to fork while processing %s", connection->connection_addr);
      free(message_buffer);
      free(argv);
      free(arg);
      unlink(out_filename);
      unlink(err_filename);
      return NULL;
    }
    if (child_pid == 0) {
      dup2(out_fd, STDOUT_FILENO);
      dup2(err_fd, STDERR_FILENO);

      int exec_err = execvp(argv[0], argv + 1);
      if (exec_err) {
        err(EXIT_FAILURE, "failed to execute the requested command");
      }
    } else {
      int wstatus;
      int fd = syscall(SYS_pidfd_open, child_pid, 0);
      struct pollfd pfd = {fd, POLLIN, 0};
      int exited = poll(&pfd, 1, TIMEOUT);
      if (!exited) {
        kill(child_pid, SIGKILL);
      }
      close(fd);
      waitpid(child_pid, &wstatus, 0);
      dprintf(sock_fd, "Child exited with status %d\n", wstatus);
      if (wstatus == EXIT_SUCCESS) {
        send_whole_file(out_fd, sock_fd, connection->connection_addr);
      } else {
        send_whole_file(err_fd, sock_fd, connection->connection_addr);
      }
    }
    free(argv);
    unlink(out_filename);
    unlink(err_filename);
  }
  free(message_buffer);
  free(arg);
  return NULL;
}

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
  for (;;) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    int connection_fd =
        accept(sock_fd, (struct sockaddr *)&client_addr, &addr_len);

    if (connection_fd < 0) {
      warn("failed to establish a connection");
    } else {
      struct connection_info *connection =
          malloc(sizeof(struct connection_info));
      connection->sock_fd = connection_fd;
      inet_ntop(AF_INET, &(client_addr.sin_addr), connection->connection_addr,
                INET_ADDRSTRLEN);
      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, handle_connection,
                         (void *)connection)) {
        warn("failed to create a thread #%ld to process a connection to %s",
             thread_id, connection->connection_addr);
      }
    }
  }
  return EXIT_SUCCESS;
}
