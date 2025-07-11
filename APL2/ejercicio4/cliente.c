
/*
Integrantes del grupo :
-Berti Rodrigo
- Burnowicz Alejo
- Fernandes Leonel
- Federico Agustin
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define FALSE 0
#define TRUE 1

typedef struct
{
    char nickname[30];
    char ultimaLetra;
    char palabra[30];
    char palabraCamuflada[30];
    int intentos;
    char estadoPartida[30];

} SharedMemory;

void sigtermHandler()
{
	printf("SIGTERM recibido, cerrando cliente...\n");
    sem_t *cliente_lock = sem_open("CLIENTE_LOCK", 0);
    if (cliente_lock != SEM_FAILED)
    {
        sem_post(cliente_lock); // Liberar el semáforo de exclusión
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
    }
    
    printf("Recursos liberados correctamente.\n");
	exit(0);
}

int main(int argc, char *argv[])
{

    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, sigtermHandler);

    ////////////////////////////////////////////////////////////////////////////
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
	{
		printf("Opciones :\n");
		printf("--help o -h para mostrar la ayuda\n");
		printf("--name o -n seguido del nombre del cliente\n");
		printf("Ejemplo: ./cliente -n prueba o ./cliente -name prueba\n");
		return 0;
	}

    // verifico que los parametros sean correctos
    if (argc < 3)
    {
        printf("Faltan parametros requeridos\n");
        return 1;
    }

    char nickName[40];

    int i = 0;
    bool b_nickname = FALSE;
    while (i <= argc)
    {
        if (argv[i] != NULL)
        {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            { // muestra ayuda para el cliente
                printf("Opciones :\n");
                printf("--nickname o -n para indicar el nickname del cliente\n");
                printf("--help o -h para mostrar esta ayuda\n");
                printf("Ejemplo: ./cliente -n nickName\n");
                return 0;
            }
            // copia el nickname del cliente
            if (strcmp(argv[i], "-nickname") == 0 || strcmp(argv[i], "-n") == 0)
            {
                strcpy(nickName, argv[i + 1]);
                i++;
                b_nickname = TRUE;
            }
        }

        i++;
    }

    if (b_nickname == FALSE)
    {
        printf("parametro nickname invalido o faltante\n");
        return 1;
    }

    ////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////
    // Semáforo para asegurar que solo un cliente se ejecute a la vez
    sem_t *cliente_lock = sem_open("CLIENTE_LOCK", O_CREAT | O_EXCL, 0600, 1);

    if (cliente_lock == SEM_FAILED)
    {
        if (errno == EEXIST)
        {
            // El semáforo ya existe, intentamos abrirlo
            cliente_lock = sem_open("CLIENTE_LOCK", 0);
            if (cliente_lock == SEM_FAILED)
            {
                perror("Error al abrir el semáforo CLIENTE_LOCK");
                exit(1);
            }
        }
        else
        {
            perror("Error al crear el semáforo CLIENTE_LOCK");
            exit(1);
        }
    }

    // Intentamos adquirir el semáforo (espera 0 segundos para no bloquear)
    if (sem_trywait(cliente_lock) == -1)
    {
        if (errno == EAGAIN)
        {
            printf("Ya hay un cliente en ejecución. Solo se permite un cliente a la vez.\n");
            exit(1);
        }
        else
        {
            perror("Error al adquirir el semáforo CLIENTE_LOCK");
            exit(1);
        }
    }

    ////////////////////////////////////////////////////////////////////////////

    SharedMemory *memoriaCompartida;

    int sharedMemInt = shm_open("SHARED_MEM", O_RDWR, 0600);

    if (sharedMemInt == -1)
    {
        perror("No se pudo abrir la memoria compartida");
        if (errno == ENOENT)
        {
            printf("No hay servidor conectado\n");
        }
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
        exit(1);
    }

    if (ftruncate(sharedMemInt, sizeof(SharedMemory)) == -1) {
        perror("Error al ajustar el tamaño de la memoria compartida");
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
        exit(EXIT_FAILURE);
    }

    memoriaCompartida = (SharedMemory *)mmap(NULL, sizeof(SharedMemory), PROT_WRITE, MAP_SHARED, sharedMemInt, 0);

    strcpy(memoriaCompartida->nickname, nickName);

    printf("Nickname ingresado: %s\n", memoriaCompartida->nickname);

    close(sharedMemInt);

    sem_t *cliente = sem_open("CLIENTE", O_RDWR, 0600, 0);

    if (cliente == SEM_FAILED)
    {
        printf("error abriendo sem cliente\n");
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
        exit(1);
    }

    sem_t *servidor = sem_open("SERVIDOR", O_RDWR, 0600, 0);

    if (servidor == SEM_FAILED)
    {
        printf("error abriendo sem servidor\n");
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
        exit(1);
    }

    sem_t *finalizacion = sem_open("FINALIZACION", O_RDWR, 0600, 0);

    if (finalizacion == SEM_FAILED)
    {
        printf("error abriendo sem finalizacion\n");
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
        exit(1);
    }

    sem_t *mutex = sem_open("MUTEX", O_CREAT, 0600, 1);
    // no se pudo abrir el semaforo MUTEX
    if (mutex == SEM_FAILED)
    {
        perror("error abriendo sem mutex");
        if (errno == EEXIST)
        {
            printf("el semaforo MUTEX ya existe, no se puede abrir\n");
        }
        else if (errno == EACCES)
        {
            printf("el semaforo MUTEX no se puede abrir, no tengo permisos\n");
        }
        else
        {
            printf("error desconocido abriendo sem MUTEX\n");
        }
        sem_close(cliente_lock);
        sem_unlink("CLIENTE_LOCK");
        exit(1);
    }

    sem_post(cliente); // me conecto a server

    struct timespec ts;

TAG:
    while ((memoriaCompartida->intentos > 0) && (strcmp(memoriaCompartida->estadoPartida, "exit") != 0) && (strcmp(memoriaCompartida->estadoPartida, "finalizando") != 0))
    {
        // sem_wait(servidor); // espero a que el server haya seteado todo

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;

        if ((sem_timedwait(servidor, &ts) == -1))
        {
            goto TAG;
        }

        char palabraAEnviar[30] = "";

        fflush(stdin);
        fflush(stdout);

        printf("[server]: %s\n - Intentos restantes: %d\n", memoriaCompartida->palabraCamuflada, memoriaCompartida->intentos);

        fflush(stdin);
        fflush(stdout);
        if (scanf("%s", palabraAEnviar) != 1) {
            printf("Error al leer la entrada\n");
            continue; // o manejar el error adecuadamente
        }
        fflush(stdin);
        fflush(stdout);

        if (strcmp(palabraAEnviar, "exit") != 0)
        {
            memoriaCompartida->ultimaLetra = palabraAEnviar[0];
        }
        else
        {
            strcpy(memoriaCompartida->estadoPartida, palabraAEnviar);
            sem_post(cliente); // le aviso al servidor que meti exit asi lee la rta
            break;
        }

        sem_post(cliente); // le aviso a server que meti una palabra
    }

    if (strcmp(memoriaCompartida->estadoPartida, "exit") == 0)
    {
        printf("El servidor ha finalizado la partida.\n");
    }

    // Liberamos el semáforo de exclusión antes de salir
    sem_post(cliente_lock);
    sem_close(cliente_lock);
    sem_unlink("CLIENTE_LOCK");

    sem_post(finalizacion);

    return 0;
}