CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -pthread

CLIENT_SRC = cliente.c
SERVER_SRC = server.c
CLIENT_HANDLER_SRC = cliente_handler.c
PARTIDA_SRC = partida.c

CLIENT_BIN = cliente
SERVER_BIN = server

.PHONY: all clean

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

$(SERVER_BIN): $(SERVER_SRC) $(CLIENT_HANDLER_SRC) $(PARTIDA_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) $(CLIENT_HANDLER_SRC) $(PARTIDA_SRC) $(LDFLAGS)

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)

