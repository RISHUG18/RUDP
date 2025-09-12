# Simple Makefile for networking mini-project
# Builds: client, server

CC := gcc
CFLAGS := -Wall -Wextra -O2
LDFLAGS := 
LDLIBS_CLIENT := 
LDLIBS_SERVER := -lcrypto

# Sources
CLIENT_SRC := client.c
SERVER_SRC := server.c

# Outputs
CLIENT_BIN := client
SERVER_BIN := server

.PHONY: all clean debug release

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC) sham.h
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDLIBS_CLIENT)

$(SERVER_BIN): $(SERVER_SRC) sham.h
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS) $(LDLIBS_SERVER)

# Debug build with symbols
debug: CFLAGS += -g -O0
debug: clean all

# Release build (default CFLAGS already optimize)
release: clean all

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN) *.o *.out *.log
