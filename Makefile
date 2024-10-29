CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC_DIR = src
BIN_DIR = .

all: filestat

filestat: $(SRC_DIR)/filestat.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/filestat $(SRC_DIR)/filestat.c

.PHONY: all
