#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

const char *get_hide_dir() {
  const char *hide_dir = getenv("HIDE_DIR");
  if (hide_dir == NULL) {
    return "/home/.hide";
  }
  return hide_dir;
}

void setup_dir() {
  const char *hide_dir = get_hide_dir();
  struct stat entry;
  if (stat(hide_dir, &entry)) {
    if (errno == ENOENT) {
      warnx("warning: %s does not exist", hide_dir);
      mkdir(hide_dir, S_IRWXU | S_IRWXG | S_IXOTH);
      chown(hide_dir, 0, 0);
      return;
    }
    err(EXIT_FAILURE, "failed to check %s existence", hide_dir);
  }

  if (!S_ISDIR(entry.st_mode)) {
    errx(EXIT_FAILURE, "%s already exists and is not a directory", hide_dir);
  }

  if (entry.st_uid != 0 || entry.st_gid != 0) {
    warnx("warning: %s has wrong owner", hide_dir);
    chown(hide_dir, 0, 0);
  }

  if (entry.st_mode & S_IRWXU == S_IRWXU &&
      entry.st_mode & S_IRWXG == S_IRWXG &&
      entry.st_mode & S_IXOTH == S_IXOTH && !(entry.st_mode & S_IROTH) &&
      !(entry.st_mode & S_WOTH)) {
    warnx("warning: %s has wrong permissions", hide_dir);
    chmod(hide_dir, S_IRWXU | S_IRWXG | S_IXOTH);
  }
  return;
}

int main(int argc, char **argv) {
  if (geteuid() != 0) {
    errx(EXIT_FAILURE, "needs to be run with root priveleges");
  }
  if (argc < 2) {
    errx(EXIT_FAILURE, "please provide a file to hide");
  }
  setup_dir();
  int src = open(argv[1], O_RDONLY);
  if (src == -1) {
    if (errno == ENOENT) {
      err(EXIT_FAILURE, "file %s does not exist", argv[1]);
    }
    err(EXIT_FAILURE, "failed to open file %s", argv[1]);
  }
  struct stat src_stat;
  if (fstat(src, &src_stat)) {
    err(EXIT_FAILURE, "failed to get information about %s", argv[1]);
  }
  if (!S_ISREG(src_stat.st_mode)) {
    err(EXIT_FAILURE, "%s is not a regular file", argv[1]);
  }
  if (strlen(get_hide_dir()) + strlen(argv[1]) > PATH_MAX) {
    err(EXIT_FAILURE, "the resulting path is too long");
  }

  char *dst_path = malloc(sizeof(char) * PATH_MAX);
  snprintf(dst_path, PATH_MAX, "%s/%s", get_hide_dir(), argv[1]);
  int dst = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, src_stat.st_mode);

  if (dst == -1) {
    free(dst_path);
    err(EXIT_FAILURE, "failed to create file in hidden catalog, path too long");
  }
  off_t sent_bytes = 0;
  while (sent_bytes != src_stat.st_size) {
    ssize_t sent_session = sendfile(dst, src, NULL, src_stat.st_size);
    if (sent_session == -1) {
      free(dst_path);
      err(EXIT_FAILURE, "failed to copy %s to %s", argv[1], get_hide_dir());
    }
    sent_bytes += sent_session;
  }
  uid_t uid = getuid();
  gid_t gid = getgid();
  chown(dst_path, uid, gid);
  close(src);
  close(dst);
  unlink(argv[1]);
  free(dst_path);

  exit(EXIT_SUCCESS);
}
