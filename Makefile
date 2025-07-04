# Variables
CC = gcc
CFLAGS = -Wall

# Objetivos principales
all: broker feed mensajes


# Reglas para generar los binarios
broker: broker.o util.h
	$(CC) $(CFLAGS) -o manager manager.o

feed: feed.o util.h
	$(CC) $(CFLAGS) -o feed feed.o

# Reglas para generar archivos .o
broker.o: manager.c util.h
	$(CC) $(CFLAGS) -c manager.c -o manager.o

feed.o: feed.c util.h
	$(CC) $(CFLAGS) -c feed.c -o feed.o

# Regla para el archivo de mensajes
mensajes:
	touch mensajes.txt

# Limpiar archivos generados
clean:
	rm -f manager feed manager.o feed.o client_pipe_* server_pipe mensajes.txt
