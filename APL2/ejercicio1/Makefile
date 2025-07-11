# Archivos fuente
SOURCES = ejercicio1.c

# Nombre del programa
TARGET = ejercicio1

# Compilador
CC = gcc

# Opciones de compilación
CFLAGS = -Wall -g

# Objetivo principal
all: $(TARGET)

# Construir el programa
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Construir los objetos
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Objetivo para limpiar
clean:
	rm -f $(TARGET) $(SOURCES:.c=.o)