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

using namespace std;

#define NOMBRE_SEMAFORO_SERVIDOR "semaforoServidor"
#define NOMBRE_SEMAFORO_CLIENTE "semaforoCliente"
#define NOMBRE_SEMAFORO_CLIENTE_UNICO "semaforoClienteUnico"
#define NOMBRE_MEMORIA "miMemoria"
#define NOMBRE_MEMORIA_RESPUESTA "miMemoriaRespuesta"

// Estructura para la respuesta del servidor
struct RespuestaServidor {
    bool letraCorrecta;
    int intentosRestantes;
};

void mostrarAyuda() {
    cout << "Uso: ./ejercicio4Servidor.cpp [OPCIONES]" << endl;
    cout << "Descripción:" << endl;
    cout << "Este es el programa cliente el cual se conectara al servidor e intentara ir adivinando la frase." << endl;
    cout << "Opciones:" << endl;
    cout << " -h                               Muestra este mensaje de ayuda" << endl;
    cout << " -n / --nickname <nombre>         Nombre del usuario (Requerido)." << endl;    
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./ejercicio4Cliente -n nombrePrueba  #Ejecución normal" << endl;
    cout << "  ./ejercicio4Cliente -h               # Muestra ayuda" << endl;
}

void validarNickname(const string &nickname) {
    if (nickname.empty()) {
        cout << "El nickname no puede estar vacío." << endl;
        exit(EXIT_FAILURE);
    }
    if (nickname.length() > 20) {
        cout << "El nickname no puede tener más de 20 caracteres." << endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]){

    if (argc < 1){
        cout << "Parametros incompletos." << endl;
        mostrarAyuda();
    }

    string opcion = argv[1];
    if (opcion == "-h") {
        mostrarAyuda();
        return 0;
    } else if (opcion == "-n" || opcion == "--nickname") {
        if (argc < 3) {
            cout << "Falta el nombre de usuario." << endl;
            mostrarAyuda();
            return 1;
        }
        string nickname = argv[2];
        validarNickname(nickname);
        cout << "Nickname válido: " << nickname << endl;
    } else {
        cout << "Opción no reconocida: " << opcion << endl;
        mostrarAyuda();
        return 1;
    }

    // creo la memoria compartida a la que se van a conectar los clientes, es una variable char para la letra que deposita a adivinar de la frase
    int idMemoria = shm_open(NOMBRE_MEMORIA, O_CREAT | O_RDWR, 0600);
    if (idMemoria == -1) {
        cerr << "Error al crear la memoria compartida." << endl;
        return 1;
    }

    // declaramos el tamaño de la memoria compartida
    if (ftruncate(idMemoria, sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida." << endl;
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }
    // mapeamos la memoria compartida a una variable de nuestro programa
    char *letraADivinar = (char *)mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0);
    if (letraADivinar == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida." << endl;
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    // mem compartida para pasar el nickname del cliente conectado al servidor
    int idMemoriaNickname = shm_open("miMemoriaNickname", O_CREAT | O_RDWR, 0600);
    if (idMemoriaNickname == -1) {
        cerr << "Error al crear la memoria compartida para el nickname." << endl;
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }
    // declaramos el tamaño de la memoria compartida para el nickname
    if (ftruncate(idMemoriaNickname, 20 * sizeof(char)) == -1) { // 20 caracteres para el nickname
        cerr << "Error al definir el tamaño de la memoria compartida para el nickname." << endl;
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }
    // mapeamos la memoria compartida para el nickname
    char *nicknameCliente = (char *)mmap(NULL, 20 * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaNickname, 0);
    if (nicknameCliente == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida para el nickname." << endl;
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    // Memoria compartida para la respuesta del servidor
    int idMemoriaRespuesta = shm_open(NOMBRE_MEMORIA_RESPUESTA, O_CREAT | O_RDWR, 0600);
    if (idMemoriaRespuesta == -1) {
        cerr << "Error al crear la memoria compartida para la respuesta." << endl;
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    if (ftruncate(idMemoriaRespuesta, sizeof(RespuestaServidor)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida para la respuesta." << endl;
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    RespuestaServidor *respuesta = (RespuestaServidor *)mmap(NULL, sizeof(RespuestaServidor), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaRespuesta, 0);
    if (respuesta == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida para la respuesta." << endl;
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }    

    // creamos los semaforos
    //creo los semáforos que van a controlar el acceso a la memoria compartida
    sem_t *semaforoServidor = sem_open(NOMBRE_SEMAFORO_SERVIDOR, O_CREAT, 0600, 1);
    if (semaforoServidor == SEM_FAILED) {
        cerr << "Error al crear el semáforo del servidor." << endl;
        munmap(respuesta, sizeof(RespuestaServidor));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }
    sem_t *semaforoCliente = sem_open(NOMBRE_SEMAFORO_CLIENTE, O_CREAT, 0600, 0);
    if (semaforoCliente == SEM_FAILED) {
        cerr << "Error al crear el semáforo del cliente." << endl;
        sem_close(semaforoServidor);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        munmap(respuesta, sizeof(RespuestaServidor));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    // Semáforo para controlar cliente único
    sem_t *semaforoClienteUnico = sem_open(NOMBRE_SEMAFORO_CLIENTE_UNICO, O_CREAT, 0600, 1);
    if (semaforoClienteUnico == SEM_FAILED) {
        cerr << "Error al crear el semáforo de cliente único." << endl;
        sem_close(semaforoServidor);
        sem_close(semaforoCliente);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        munmap(respuesta, sizeof(RespuestaServidor));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    //espero servidor conectado
    cout << "Cliente iniciado. Esperando conexión al servidor..." << endl;
    // Esperar a que el servidor esté listo
    






    return 0;
}