/*
Integrantes del grupo:
- Berti Rodrigo
- Burnowicz Alejo
- Fernandes Leonel
- Federico Agustin
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "partida.h"
#include <sys/socket.h>
#include <unistd.h>
#define MAX_LEN 100
#define BUFFER_SIZE 2048
#define FALSE 0
#define TRUE 1


int partida(char *palabra,char* recv_buffer,char* send_buffer,int client_fd){
	int intentos = 0;
	char palabraJuego[MAX_LEN];
	char letra = 'a';
	int acierto;
	ssize_t n = 0;
	devolverPalabraJuego(palabraJuego,palabra);
    while (intentos < 5)
    {
	memset(send_buffer,0,BUFFER_SIZE);
	strcpy(send_buffer,palabraJuego);
	if( send(client_fd,send_buffer,strlen(send_buffer),0) < 0){
		perror("fallo la copia de la palabra o el envio");
		break;
	}
	memset(send_buffer,0,BUFFER_SIZE);

	memset(recv_buffer,0,BUFFER_SIZE);
	n = recv(client_fd,recv_buffer,BUFFER_SIZE,0);

	if(n<=0){
		perror("fallo la recepcion de la letra");
		return -1;
	}

	letra = recv_buffer[0];
	memset(recv_buffer,0,BUFFER_SIZE);
	acierto = 0;


	for (int i = 0; palabra[i] != '\0';i++){
		if(palabra[i] == letra &&  palabraJuego[i] != letra){
			palabraJuego[i] = letra;
			acierto++;
		}
	}

        if (!acierto){
            intentos++;
        }

        if (strcmp(palabraJuego, palabra) == 0){
            return TRUE;
        }
    }
    return FALSE;
}

void devolverPalabraJuego(char *destino, char *original) {
    int i;
    for (i = 0; original[i] != '\0'; i++) {
        destino[i] = (original[i] == ' ') ? ' ' : '_';
    }
    destino[i] = '\0';
}
