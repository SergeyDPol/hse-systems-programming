#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    err(EXIT_FAILURE, "please provide a program to run");
  }
  int dev_null = open("/dev/null", O_RDWR);
  if (dev_null == -1) {
    err(EXIT_FAILURE, "could not open file to redirect command output");
  }
  signal(SIGHUP, SIG_IGN);
  if (errno > 0) {
    errx(EXIT_FAILURE, "could not register SIGHUP ignoring");
  }
  int redir_status = dup2(dev_null, STDIN_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not redirect program stdin");
  }
  redir_status = dup2(dev_null, STDOUT_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not redirect program stdout");
  }
  int backup_err = dup(STDERR_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not create backup err fd");
  }
  redir_status = dup2(dev_null, STDERR_FILENO);
  if (redir_status == -1) {
    err(EXIT_FAILURE, "could not redirect program stderr");
  }
  if (execvp(argv[1], argv + 1) == -1) {
    dup2(backup_err, STDERR_FILENO);
    if (errno == ENOENT) {
      err(EXIT_FAILURE, "no such executable found in $PATH");
    }
  }
  err(EXIT_FAILURE, "could not execute specified program");
}
