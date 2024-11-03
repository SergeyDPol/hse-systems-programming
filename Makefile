CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC_DIR = src
BIN_DIR = .

all: filestat hide nohup

filestat: $(SRC_DIR)/filestat.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/filestat $(SRC_DIR)/filestat.c
hide: $(SRC_DIR)/hide.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/hide $(SRC_DIR)/hide.c
nohup: $(SRC_DIR)/nohup.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/nohup $(SRC_DIR)/nohup.c

.PHONY: all
