# Compilador
CXX = g++
# Opciones de compilación
CXXFLAGS = -Wall -Wextra -std=c++14

# Nombres de los ejecutables
SERVER = servidor
CLIENT = cliente

# Regla por defecto (compila ambos programas)
all: $(SERVER) $(CLIENT)

# Regla para compilar el servidor
$(SERVER): servidor.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

# Regla para compilar el cliente
$(CLIENT): cliente.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

# Limpiar archivos generados
clean:
	rm -f $(SERVER) $(CLIENT)

.PHONY: all clean