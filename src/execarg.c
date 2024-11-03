#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    err(EXIT_FAILURE, "please provide a program to run");
  }
  int out_fd = open("./out.txt", O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP);
  if (out_fd == -1) {
    err(EXIT_FAILURE, "could not open file to redirect command output");
  }
  int err_fd = open("./err.txt", O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP);
  if (err_fd == -1) {
    err(EXIT_FAILURE, "could not open file to redirect command err");
  }
  int redir_status = dup2(out_fd, STDOUT_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not redirect program stdout");
  }
  int backup_err = dup(STDERR_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not create backup err fd");
  }
  redir_status = dup2(err_fd, STDERR_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not redirect program stderr");
  }
  if (execvp(argv[1], argv + 1) == -1) {
    dup2(backup_err, STDERR_FILENO);
    if (errno == ENOENT) {
      err(EXIT_FAILURE, "no such executable found in $PATH");
    }
  }
  dup2(backup_err, STDERR_FILENO);
  err(EXIT_FAILURE, "could not execute specified program");
}
