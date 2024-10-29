#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(void) {
  struct dirent **entry_list;
  int n = 0;
  int blk_cnt = 0;
  int chr_cnt = 0;
  int fifo_cnt = 0;
  int reg_cnt = 0;
  int dir_cnt = 0;
  int lnk_cnt = 0;
  int sock_cnt = 0;
  int err_cnt = 0;

  n = scandir(".", &entry_list, NULL, alphasort);
  if (n == -1) {
    if (errno == ENOMEM) {
      err(EXIT_FAILURE, "insufficient memory to complete the operation");
    }
    if (errno == EACCES) {
      err(EXIT_FAILURE, "cannot read directory contents, permission denied");
    }
    err(EXIT_FAILURE, "unknown error, please report the bug to the author");
  }

  while (n--) {
    struct stat entry;
    if (!strcmp(entry_list[n]->d_name, ".") ||
        !strcmp(entry_list[n]->d_name, "..")) {
      free(entry_list[n]);
      continue;
    }
    if (lstat(entry_list[n]->d_name, &entry) == -1) {
      warnx("could not process entry %s", entry_list[n]->d_name);
      err_cnt++;
      free(entry_list[n]);
      continue;
    }
    if (S_ISBLK(entry.st_mode)) {
      blk_cnt++;
    } else if (S_ISCHR(entry.st_mode)) {
      chr_cnt++;
    } else if (S_ISFIFO(entry.st_mode)) {
      fifo_cnt++;
    } else if (S_ISREG(entry.st_mode)) {
      reg_cnt++;
    } else if (S_ISDIR(entry.st_mode)) {
      dir_cnt++;
    } else if (S_ISLNK(entry.st_mode)) {
      lnk_cnt++;
    } else if (S_ISSOCK(entry.st_mode)) {
      sock_cnt++;
    } else {
      warnx("could not process entry %s", entry_list[n]->d_name);
      err_cnt++;
      free(entry_list[n]);
      err_cnt++;
    }
    free(entry_list[n]);
  }
  free(entry_list);

  printf("The directory contains:\n");
  printf("%d\tBlock special files\n", blk_cnt);
  printf("%d\tCharacter special files\n", chr_cnt);
  printf("%d\tFIFO special files\n", fifo_cnt);
  printf("%d\tRegular files\n", reg_cnt);
  printf("%d\tDirectories\n", dir_cnt);
  printf("%d\tSymbolic links\n", lnk_cnt);
  printf("%d\tSockets\n", sock_cnt);
  if (err_cnt) {
    printf("Failed to process %d entries\n", err_cnt);
  }
  exit(EXIT_SUCCESS);
}
