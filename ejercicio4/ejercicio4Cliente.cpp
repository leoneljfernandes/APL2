#include <fcntl.h>
#include <iostream>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

#define NOMBRE_SEMAFORO_SERVIDOR "semaforoServidor"
#define NOMBRE_SEMAFORO_CLIENTE "semaforoCliente"
#define NOMBRE_MEMORIA "miMemoria"

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




    return 0;
}