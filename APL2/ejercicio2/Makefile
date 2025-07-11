# Especifica el compilador
CC = g++

# Parámetros de compilación (incluyendo --std=c++17 como pediste)
CPARAMS = --std=c++17 -Wall -Wextra

# Nombre del archivo fuente
SRC = ejercicio2.cpp

# Nombre del ejecutable (opcional, puedes quitarlo si solo quieres el .o)
TARGET = ejercicio2

# Regla por defecto (compila ambos: el .o y el ejecutable)
all: ejercicio2.o $(TARGET)

# Regla para generar el objeto
ejercicio2.o: $(SRC)
	$(CC) $(CPARAMS) -c $(SRC) -o ejercicio2.o

# Regla para generar el ejecutable (opcional)
$(TARGET): ejercicio2.o
	$(CC) $(CPARAMS) ejercicio2.o -o $(TARGET)

# Limpiar archivos generados
clean:
	rm -f ejercicio2.o $(TARGET)