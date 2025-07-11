# Configuración común
CXX = g++
CXXFLAGS = --std=c++17 -Wall -Wextra

# Nombres de los ejecutables
CLIENTE = cliente
SERVIDOR = servidor

# Regla principal: compila ambos programas
all: $(CLIENTE) $(SERVIDOR)

# Compilar el Cliente
$(CLIENTE): ejercicio3Cliente.cpp
	$(CXX) $(CXXFLAGS) ejercicio3Cliente.cpp -o $(CLIENTE)

# Compilar el Servidor
$(SERVIDOR): ejercicio3Servidor.cpp
	$(CXX) $(CXXFLAGS) ejercicio3Servidor.cpp -o $(SERVIDOR)

# Limpiar (borrar ejecutables)
clean:
	rm -f $(CLIENTE) $(SERVIDOR)