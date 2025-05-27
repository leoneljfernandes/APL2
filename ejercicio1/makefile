# Especifica el compilador
CC = g++

# Parámetros de compilación (incluyendo --std=c++17 como pediste)
CPARAMS = --std=c++17 -Wall -Wextra

# Nombre del archivo fuente
SRC = ejercicio1.cpp

# Nombre del ejecutable (opcional, puedes quitarlo si solo quieres el .o)
TARGET = ejercicio1

# Regla por defecto (compila ambos: el .o y el ejecutable)
all: ejercicio1.o $(TARGET)

# Regla para generar el objeto
ejercicio1.o: $(SRC)
	$(CC) $(CPARAMS) -c $(SRC) -o ejercicio1.o

# Regla para generar el ejecutable (opcional)
$(TARGET): ejercicio1.o
	$(CC) $(CPARAMS) ejercicio1.o -o $(TARGET)

# Limpiar archivos generados
clean:
	rm -f ejercicio1.o $(TARGET)