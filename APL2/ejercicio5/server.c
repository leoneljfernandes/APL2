/*
Integrantes del grupo:
- Berti Rodrigo
- Burnowicz Alejo
- Fernandes Leonel
- Federico Agustin
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <signal.h>
#include "cliente_handler.h"
#include "partida.h"
#define FALSE 0
#define TRUE 1
#define MAX_FRASES 3
#define MAX_LINEA 30

typedef struct {
	char nickName[2048];
	int* puntuacion;
	int* cantFrases;
	char** frases;
	int* cliente_id;
	int* contadorDeThreads;
} ParametrosThreadGame;


bool termProcess = TRUE;

void sigusrHandler(){
	termProcess = FALSE;
}


int main(int argc, char *argv[]){
signal(SIGINT,sigusrHandler);
signal(SIGUSR1,sigusrHandler);

/////////////////////////////////////////////////////////////////////
if(argc > 7) {
        printf("parametros invalidos\n");
        return 1;
}


int serverPort = -1;
int maxUsers = -1;
char* filePath = NULL;

int i = 0;
bool port = FALSE;
bool users = FALSE;
bool b_file = FALSE;
while(i < argc)
{
	if(argv[i] != NULL) {
	        if(strcmp(argv[i],"-help") == 0 || strcmp(argv[i],"-h") == 0){
			printf("-puerto o -p seguido del numero de puerto\n");
			printf("-usuarios o -u seguido del numero maximo de usuarios que maneja el server\n");
			printf("-archivo o -a seguido del path donde se encuentra el archivo con las palabras\n");
			return 0;
	        }

		if(strcmp(argv[i],"-puerto") == 0 || strcmp(argv[i],"-p") == 0){
			serverPort = atoi(argv[i+1]);
			i++;
			port = TRUE;
		}

		if(strcmp(argv[i],"-usuarios") == 0 || strcmp(argv[i],"-u") ==0){
			maxUsers = atoi(argv[i+1]);
			i++;
			users = TRUE;
		}

		if(strcmp(argv[i],"-archivo") == 0 || strcmp(argv[i],"-a") == 0) {
			filePath = argv[i+1];
			i++;
			b_file = TRUE;
		}
	}




i++;
}

if(users == FALSE){
	printf("parametro usuarios invalido o faltante\n");
	return 1;
}
if(b_file == FALSE){
	printf("parametro archivo invalido o faltante\n");
	return 1;
}
if(port == FALSE){
	printf("parametro puerto invalido o faltante\n");
	return 1;
}

////////////////////////////////////////////////////////////////////////////
FILE *file = fopen(filePath, "r");
if (!file){
	perror("Error abriendo archivo");
	return 1;
}
char *frases[MAX_FRASES];
char bufferArchivo[MAX_LINEA];
int cantidadFrases = 0;

while (fgets(bufferArchivo, MAX_LINEA, file) && cantidadFrases < MAX_FRASES){
	// Eliminar el salto de línea final si existe
	bufferArchivo[strcspn(bufferArchivo, "\n")] = '\0';
	// Reservar memoria y copiar la línea
	frases[cantidadFrases] = strdup(bufferArchivo);
	if (!frases[cantidadFrases]){
		perror("Fallo al reservar memoria");
		fclose(file);
		return 1;
	}
	cantidadFrases++;
}

fclose(file);
////////////////////////////////////////////////////////////////////////////
int server_fd;
struct sockaddr_in server_addr, client_addr;
socklen_t addr_len = sizeof(client_addr);

server_fd = socket(AF_INET,SOCK_STREAM,0);

if (server_fd == -1 ) {
	perror("error creando el socket");
	exit(EXIT_FAILURE);
}

server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = INADDR_ANY;
server_addr.sin_port = htons(serverPort);

if (bind(server_fd,(struct sockaddr*)&server_addr, sizeof(server_addr)) <0){
	perror("fallo al bindear el socket");
	close(server_fd);
	exit(EXIT_FAILURE);
}

if (listen(server_fd, maxUsers) < 0) {
	perror("Listen failed");
	close(server_fd);
	exit(EXIT_FAILURE);
}

printf("servidor a la escucha en el puerto: %d\n",serverPort);


int contadorDeThreads = 0;
TIMEOUT:
while (termProcess){
	while(contadorDeThreads<maxUsers){
		int* client_fd = malloc(sizeof(int));

		if (!client_fd) {
			perror("fallo el malloc para alojar el cliente");
			continue;
		}

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds);

		struct timeval timeout = {5, 0}; // 5 segundos

		int result = select(server_fd + 1, &readfds, NULL, NULL, &timeout);
		if (result == 0) {
		    //printf("Timeout: no se detectó una conexión entrante.\n");
		    	free(client_fd);
			goto TIMEOUT;

		} else if (result < 0) {
		    perror("select");
		} else {
		*client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
		}
		
		if (*client_fd < 0){
			perror("fallo aceptar el cliente");
			free(client_fd);
			continue;
		}

		printf("Conexion aceptada de %s:%d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

		pthread_t tid;

		ParametrosThreadGame params;

	        params.cliente_id = client_fd;
		params.cantFrases = &cantidadFrases;
		params.frases = frases;
		params.puntuacion = malloc(sizeof(int));
		params.contadorDeThreads = &contadorDeThreads;
	        *params.puntuacion=0;
		if(pthread_create(&tid,NULL,handle_client,&params) != 0){
			perror("fallo la creacion del thread");
			close(*client_fd);
			free(client_fd);
		} else {
			contadorDeThreads++;
			pthread_detach(tid);
		}
	}


}

FILE* fi = fopen("/tmp/resultado","r");

if (fi == NULL) {
	perror("Error opening file");
        return 1;
}
char line[20];
int maxValue = 0;
int first = 1;
char maxWord[20];
    while (fgets(line, sizeof(line), fi)) {
        int value;
        char word[20];

        // Remove newline if present
        line[strcspn(line, "\n")] = 0;

        if (sscanf(line, "%d|%s", &value, word) == 2) {
            if (first || value > maxValue) {
                maxValue = value;
                strncpy(maxWord, word, 20);
                maxWord[19] = '\0';
                first = 0;
            }
        }
    }

if (first) {
	printf("archivo vacio.\n");
} else {
	printf("El puntaje mas alto fue de: %s con %d\n",maxWord, maxValue);
}

fclose(fi);
remove("/tmp/resultado");
close(server_fd);
return 0;
}
