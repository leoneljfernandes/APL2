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
#define BUFFER_SIZE 2048
#define FALSE 0
#define TRUE 1

int main(int argc, char *argv[]){

////////////////////////////////////////////////////////////////////////////
if(argc > 7) {
        printf("parametros invalidos\n");
        return 1;
 }

char serverIp[40];
char nickName[40];
int portNumber;
struct sockaddr_in socketAddr;

int i = 0;
bool port = FALSE;
bool b_nickname = FALSE;
bool server = FALSE;
while(i <= argc)
{
	if(argv[i] != NULL) {
	        if(strcmp(argv[i],"-help") == 0 || strcmp(argv[i],"-h") == 0){
			printf("-puerto o -p para indicar el peurto del servidor de destino\n");
			printf("-servidor o -s para indicar la ipv4 del servidor de destino\n");
			printf("-nickname o -n para indicar el nickname del cliente\n");
			return 0;
	        }

		if(strcmp(argv[i],"-puerto") == 0 || strcmp(argv[i],"-p") == 0){
			portNumber = atoi(argv[i+1]);
			i++;
			port = TRUE;
		}

		if(strcmp(argv[i],"-servidor") == 0 || strcmp(argv[i],"-s") ==0){
			strcpy(serverIp,argv[i+1]);
			int res = inet_pton(AF_INET, serverIp,&(socketAddr.sin_addr));
			if( res <= 0 ){
				printf("direccion ipv4 invalida: %s\n",serverIp);
				return 1;
			}
			i++;
			server = TRUE;
		}

		if(strcmp(argv[i],"-nickname") == 0 || strcmp(argv[i],"-n") == 0) {
			strcpy(nickName,argv[i+1]);
			i++;
			b_nickname = TRUE;
		}
	}




i++;
}

if(b_nickname == FALSE){
	printf("parametro nickname invalido o faltante\n");
	return 1;
}
if(server == FALSE){
	printf("parametro server invalido o faltante\n");
	return 1;
}
if(port == FALSE){
	printf("parametro puerto invalido o faltante\n");
	return 1;
}

////////////////////////////////////////////////////////////////////////////

int sockfd;
struct sockaddr_in server_addr;
char send_buffer[BUFFER_SIZE];
char recv_buffer[BUFFER_SIZE];

sockfd = socket(AF_INET,SOCK_STREAM,0);

if(sockfd < 0){
	perror("fallo la creacion del socket");
	exit(EXIT_FAILURE);
}

server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(portNumber);
server_addr.sin_addr.s_addr = INADDR_ANY;
if(inet_pton(AF_INET,serverIp,&server_addr.sin_addr) <= 0){
	perror("dirección de server invalida");
	close(sockfd);
	exit(EXIT_FAILURE);
}

if(connect(sockfd, (struct sockaddr*)&server_addr,sizeof(server_addr)) < 0){
	perror("fallo al establecer la conexion");
	close(sockfd);
	exit(EXIT_FAILURE);
}

printf("Conectado al servidor en %s:%d\n",serverIp, portNumber);
ssize_t n = 0;

n = recv(sockfd,recv_buffer,BUFFER_SIZE,0);

if(n<=0){
	perror("fallo algo");
}

if(strcmp(recv_buffer,"INICIO_PARTIDA") != 0){
	perror("no llego el inicio partida");
}

memset(send_buffer,0,BUFFER_SIZE);
snprintf(send_buffer,BUFFER_SIZE,"%s", nickName);
send(sockfd,send_buffer,strlen(send_buffer),0);
memset(send_buffer,0,BUFFER_SIZE);

bool continuar = TRUE;
while(continuar){

	while(1){
		memset(recv_buffer,0,BUFFER_SIZE);
		n = recv(sockfd,recv_buffer,BUFFER_SIZE,0);
		if(n<0){
			perror("fallo la entrada de la palabra");
			close(sockfd);
			return 1;
		}

		if(n==0){
			printf("el servidor se desconectó\n");
			close(sockfd);
			return 1;
		}

		if(strcmp("PERDISTE",recv_buffer)==0){
			printf("Perdiste la partida\n");
			break;
		}

		if(strcmp("GANASTE",recv_buffer)==0){
			printf("Ganaste la partida\n");
			break;
		}

		printf("[server]: %s\n",recv_buffer);
		memset(recv_buffer,0,BUFFER_SIZE);


		printf("> ");
		fflush(stdout);
		fflush(stdin);
		char entrada[5];
		if(scanf("%s",entrada)<1){
			perror("fallo la entrada de la letra");
			return 1;
		}
		strcpy(send_buffer,entrada);
		fflush(stdin);
		fflush(stdout);

		//remover newline del mensaje
		send_buffer[strcspn(send_buffer, "\n")] = '\0';

		if(strcmp(send_buffer,"exit") == 0 || strcmp(send_buffer,"EXIT") == 0){
			printf("desconectando...\n");
			//aca va la logica de desconexion de cliente controlada
			break;
		}

		if (send(sockfd,send_buffer,strlen(send_buffer),0) < 0 ){
			perror("fallo el envio del mensaje al server");
			close(sockfd);
			return 1;
		}
	memset(send_buffer,0,BUFFER_SIZE);
	}

	memset(recv_buffer,0,BUFFER_SIZE);
	printf("Terminó su juego, seguir jugando? [Y/N]: ");
	char letra[5];
	TAG:
	fflush(stdout);
	fflush(stdin);
	if(scanf("%s",letra)<1){
		perror("fallo el escaneo de la letra");
		return 1;
	}
	switch(letra[0]){
		case 'Y':
		case 'y':
			memset(send_buffer,0,BUFFER_SIZE);
			snprintf(send_buffer,BUFFER_SIZE,"SEGUIR");
			send(sockfd,send_buffer,strlen(send_buffer),0);
			memset(send_buffer,0,BUFFER_SIZE);
		break;
		case 'N':
		case 'n':
			memset(send_buffer,0,BUFFER_SIZE);
                        snprintf(send_buffer,BUFFER_SIZE,"FINALIZAR");
                        send(sockfd,send_buffer,strlen(send_buffer),0);
                        memset(send_buffer,0,BUFFER_SIZE);
			continuar = FALSE;
		break;
		default:
			printf("letra invalida, por favor ingrese nuevamente\n >");
		goto TAG;
	}
}

close(sockfd);
return 0;


}
