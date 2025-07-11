/*
# Integrantes del grupo:
# - Berti Rodrigo
# - Burnowicz Alejo
# - Fernandes Leonel
# - Federico Agustin
*/
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <algorithm>
#include <signal.h>
#include <csignal>
#include <cstring>

using namespace std;

#define NOMBRE_SEMAFORO_SERVIDOR "semaforoServidor"
#define NOMBRE_SEMAFORO_CLIENTE "semaforoCliente"
#define NOMBRE_SEMAFORO_CLIENTE_UNICO "semaforoClienteUnico"
#define NOMBRE_MEMORIA "miMemoria"
#define NOMBRE_MEMORIA_RESPUESTA "miMemoriaRespuesta"

struct RespuestaServidor {
    bool letraCorrecta;
    int intentosRestantes;
    bool partidaTerminada;
};

void mostrarAyuda() {
    cout << "Uso: ./cliente -n <nickname>" << endl;
    cout << "Descripción:" << endl;
    cout << "Cliente para el juego de adivinanza de frases." << endl;
    cout << "Opciones:" << endl;
    cout << " -h               Muestra este mensaje de ayuda" << endl;
    cout << " -n <nickname>    Nombre del jugador (requerido, max 20 chars)" << endl;
    cout << "Ejemplo:" << endl;
    cout << "  ./cliente -n jugador1" << endl;
}

void validarNickname(const string &nickname) {
    if (nickname.empty()) {
        cerr << "El nickname no puede estar vacío." << endl;
        exit(EXIT_FAILURE);
    }
    if (nickname.length() > 20) {
        cerr << "El nickname no puede tener más de 20 caracteres." << endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN);

    if (argc < 2) {
        mostrarAyuda();
        return 1;
    }

    string opcion = argv[1];
    if (opcion == "-h") {
        mostrarAyuda();
        return 0;
    } else if (opcion == "-n") {
        if (argc < 3) {
            cerr << "Falta el nombre de usuario." << endl;
            mostrarAyuda();
            return 1;
        }
        string nickname = argv[2];
        validarNickname(nickname);
        cout << "Nickname válido: " << nickname << endl;
    } else {
        cerr << "Opción no reconocida: " << opcion << endl;
        mostrarAyuda();
        return 1;
    }

    // Configurar memoria compartida
    int idMemoria = shm_open(NOMBRE_MEMORIA, O_CREAT | O_RDWR, 0600);
    if (idMemoria == -1) {
        cerr << "Error al crear la memoria compartida." << endl;
        return 1;
    }

    if (ftruncate(idMemoria, sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida." << endl;
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    char *letraADivinar = (char *)mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0);
    if (letraADivinar == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida." << endl;
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    int idMemoriaNickname = shm_open("miMemoriaNickname", O_CREAT | O_RDWR, 0600);
    if (idMemoriaNickname == -1) {
        cerr << "Error al crear la memoria para nickname." << endl;
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    if (ftruncate(idMemoriaNickname, 20 * sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria para nickname." << endl;
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    char *nicknameCliente = (char *)mmap(NULL, 20 * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaNickname, 0);
    if (nicknameCliente == MAP_FAILED) {
        cerr << "Error al mapear la memoria para nickname." << endl;
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    int idMemoriaRespuesta = shm_open(NOMBRE_MEMORIA_RESPUESTA, O_CREAT | O_RDWR, 0600);
    if (idMemoriaRespuesta == -1) {
        cerr << "Error al crear la memoria para respuesta." << endl;
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    if (ftruncate(idMemoriaRespuesta, sizeof(RespuestaServidor)) == -1) {
        cerr << "Error al definir el tamaño de la memoria para respuesta." << endl;
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    RespuestaServidor *respuesta = (RespuestaServidor *)mmap(NULL, sizeof(RespuestaServidor), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaRespuesta, 0);
    if (respuesta == MAP_FAILED) {
        cerr << "Error al mapear la memoria para respuesta." << endl;
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    // Configurar semáforos
    sem_t *semaforoServidor = sem_open(NOMBRE_SEMAFORO_SERVIDOR, O_CREAT, 0600, 0);
    if (semaforoServidor == SEM_FAILED) {
        cerr << "Error al crear semáforo servidor" << endl;
        munmap(respuesta, sizeof(RespuestaServidor));
        munmap(nicknameCliente, 20 * sizeof(char));
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        shm_unlink("miMemoriaNickname");
        shm_unlink(NOMBRE_MEMORIA);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        return 1;
    }

    sem_t *semaforoCliente = sem_open(NOMBRE_SEMAFORO_CLIENTE, O_CREAT, 0600, 0);
    if (semaforoCliente == SEM_FAILED) {
        cerr << "Error al crear semáforo cliente" << endl;
        sem_close(semaforoServidor);
        munmap(respuesta, sizeof(RespuestaServidor));
        munmap(nicknameCliente, 20 * sizeof(char));
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        shm_unlink("miMemoriaNickname");
        shm_unlink(NOMBRE_MEMORIA);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        return 1;
    }

    sem_t *semaforoClienteUnico = sem_open(NOMBRE_SEMAFORO_CLIENTE_UNICO, O_CREAT, 0600, 0);
    if (semaforoClienteUnico == SEM_FAILED) {
        cerr << "Error al crear semáforo cliente único" << endl;
        sem_close(semaforoServidor);
        sem_close(semaforoCliente);
        munmap(respuesta, sizeof(RespuestaServidor));
        munmap(nicknameCliente, 20 * sizeof(char));
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        shm_unlink("miMemoriaNickname");
        shm_unlink(NOMBRE_MEMORIA);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);
        return 1;
    }

    // Copiar nickname a memoria compartida
    strncpy(nicknameCliente, argv[2], 20);
    nicknameCliente[19] = '\0';
    cout << "Conectando al servidor con nickname: " << nicknameCliente << endl;

    // Notificar al servidor
    sem_post(semaforoServidor);

    // Esperar inicio de partida
    sem_wait(semaforoCliente);

    cout << "Partida iniciada. Ingrese letras para adivinar la frase." << endl;
    cout << "Escriba 'exit' para salir en cualquier momento." << endl;

    bool partidaActiva = true;

    while(partidaActiva) {
        cout << "Ingrese una letra: ";
        string entrada;
        cin >> entrada;

        if(entrada == "exit") {
            cout << "Saliendo del juego..." << endl;
            break;
        }

        if(entrada.length() != 1) {
            cout << "Por favor ingrese solo una letra." << endl;
            continue;
        }

        // Enviar letra al servidor
        *letraADivinar = entrada[0];
        sem_post(semaforoServidor);

        // Esperar respuesta del servidor
        sem_wait(semaforoCliente);

        // Procesar respuesta
        if(respuesta->letraCorrecta) {
            cout << "¡Correcto! La letra está en la frase." << endl;
        } else {
            cout << "La letra no está en la frase." << endl;
        }

        cout << "Intentos restantes: " << respuesta->intentosRestantes << endl;

        if(respuesta->partidaTerminada) {
            if(respuesta->intentosRestantes == 0) {
                cout << "¡Se agotaron los intentos! Fin de la partida." << endl;
            } else {
                cout << "¡Felicidades! ¡Has adivinado la frase!" << endl;
            }
            partidaActiva = false;
            
            // Notificar al servidor que recibimos el fin de partida
            sem_post(semaforoServidor);
        }
    }

    // Liberar recursos
    sem_close(semaforoServidor);
    sem_close(semaforoCliente);
    sem_close(semaforoClienteUnico);
    munmap(respuesta, sizeof(RespuestaServidor));
    munmap(nicknameCliente, 20 * sizeof(char));
    munmap(letraADivinar, sizeof(char));
    close(idMemoriaRespuesta);
    close(idMemoriaNickname);
    close(idMemoria);
    shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
    shm_unlink("miMemoriaNickname");
    shm_unlink(NOMBRE_MEMORIA);
    sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);

    cout << "Cliente finalizado correctamente." << endl;
    return 0;
}