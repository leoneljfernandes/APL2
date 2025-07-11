CC = g++

CLIENT_SRC = cliente.cpp
SERVER_SRC = servidor.cpp

CLIENT_BIN = cliente
SERVER_BIN = servidor

.PHONY: all clean

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) -o $(CLIENT_BIN) $(CLIENT_SRC)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) -o $(SERVER_BIN) $(SERVER_SRC)

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)