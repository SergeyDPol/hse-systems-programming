CC = gcc
CFLAGS = -Wall -Wextra -O2

SRC_DIR = src
BIN_DIR = .

all: filestat hide nohup execarg udpchat

filestat: $(SRC_DIR)/filestat.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/filestat $(SRC_DIR)/filestat.c
hide: $(SRC_DIR)/hide.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/hide $(SRC_DIR)/hide.c
nohup: $(SRC_DIR)/nohup.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/nohup $(SRC_DIR)/nohup.c
execarg: $(SRC_DIR)/execarg.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/execarg $(SRC_DIR)/execarg.c
udpchat: $(SRC_DIR)/udpchat.c
	$(CC) $(CFLAGS) -o $(BIN_DIR)/udpchat $(SRC_DIR)/udpchat.c

.PHONY: all
