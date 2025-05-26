#include <stdio.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <syslog.h>

using namespace std;

void lanzar_demonio(){
    pid_t pidDemonio = fork();
    if(pidDemonio < 0){
        cout << "Error creando proceso hijo demonio" << endl;
        exit(EXIT_FAILURE);
    }

    if(pidDemonio > 0){
        exit(EXIT_SUCCESS); // termina el proceso padre pidHijo2
    }

    // config demonio
    setsid(); // nueva sesion
    chdir("/"); // cambiar directorio de trabajo
    close(STDIN_FILENO); // cerrar stdin
    close (STDOUT_FILENO); // cerrar stdout
    close(STDERR_FILENO); // cerrar stderr
    open("/dev/null", O_RDONLY);  // stdin
    open("/dev/null", O_WRONLY);  // stdout
    open("/dev/null", O_WRONLY);  // stderr

    openlog("mi_demonio", LOG_PID, LOG_DAEMON);

    while(true){
        syslog(LOG_INFO, "Demonio activo (PID: %d)", getpid());
        sleep(5);
    }
}

void mostrarAyuda() {
    cout << "Uso: ./ejercicio1.cpp [OPCIONES]" << endl;
    cout << "Opciones:" << endl;
    cout << "  -h          Muestra este mensaje de ayuda" << endl;
    cout << endl;
    cout << "Descripción:" << endl;
    cout << "  Este programa crea procesos hijos, nietos y un demonio." << endl;
    cout << "  El nieto 2 se deja como zombie intencionalmente para demostración." << endl;
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./programa       # Ejecución normal" << endl;
    cout << "  ./programa -h    # Muestra ayuda" << endl;
}

int main(int argc, char *argv[]){

    // verificar parametro -h
    if (argc > 1 && strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        mostrarAyuda();
        return 0;
    }
    cout << "Iniciando proceso padre (PID: " << getpid() << ")" << endl;
    pid_t pidHijo1, pidHijo2;

    // Hijo 1
    pidHijo1 = fork();

    if(pidHijo1 < 0){
        cout << "Error creando proceso hijo" << endl;
        return 1;
    } else if(pidHijo1 == 0){
        // Proceso hijo 1
        cout << "Hijo1 (PID: " << getpid() << ") ejecutando tarea normal." << endl;

        // Creamos nieto 1
        pid_t pidNieto1 = fork();
        if(pidNieto1 < 0){
            cout << "Error creando proceso nieto" << endl;
            return 1;
        } else if(pidNieto1 == 0){
            // Proceso nieto 1
            cout << "Nieto1 (PID: " << getpid() << ") ejecutando tarea normal." << endl;
            sleep(5); // Simulamos una tarea
            cout << "Nieto1 (PID: " << getpid() << ") terminando." << endl;
            _exit(0);
        }

        // Creamos nieto 2 y que quede como zombie
        pid_t pidNieto2 = fork();
        if(pidNieto2 < 0){
            cout << "Error creando proceso nieto" << endl;
            return 1;
        } else if(pidNieto2 == 0){
            // Proceso nieto 2
            cout << "Nieto2 (PID: " << getpid() << ") ejecutando tarea normal." << endl;
            sleep(5); // Simulamos una tarea
            cout << "Nieto2 (PID: " << getpid() << ") terminando." << endl;
            _exit(0);
        }

        // Creamos nieto 3
        pid_t pidNieto3 = fork();
        if(pidNieto3 < 0){
            cout << "Error creando proceso nieto" << endl;
            return 1;
        } else if(pidNieto3 == 0){
            // Proceso nieto 3
            cout << "Nieto3 (PID: " << getpid() << ") ejecutando tarea normal." << endl;
            sleep(5); // Simulamos una tarea
            cout << "Nieto3 (PID: " << getpid() << ") terminando." << endl;
            _exit(0);
        }

        waitpid(pidNieto1, NULL, 0);  // Espera al primer nieto
        waitpid(pidNieto3, NULL, 0);  // Espera al tercer nieto

        cout << "Hijo1 (PID: " << getpid() << ") esperando 30 segundos (nieto2 será zombie)." << endl;
        sleep(30);

        _exit(0);
    }
    // Hijo 2
    pidHijo2 = fork();

    if(pidHijo2 < 0){
        cout << "Error creando proceso hijo" << endl;
        return 1;
    } else if(pidHijo2 == 0){
        // Proceso hijo 2
        cout << "Hijo2 (PID: " << getpid() << ") lanzando demonio..." << endl;
        lanzar_demonio();
        
    }
    
    // Proceso padre
    // Código del padre (solo el padre llega aquí)
    cout << "Esperando a que los hijos terminen..." << endl;
    waitpid(pidHijo1, NULL, 0);  // Espera al primer hijo
    waitpid(pidHijo2, NULL, 0);  // Espera al segundo hijo

    cout << "Padre: Todos los hijos terminaron." << endl;
    cout << "Mis hijos fueron: " << pidHijo1 << " y " << pidHijo2 << endl;

    // Implementacion de espera a pulsar una tecla antes de finalizar
    cout << "Presiona Enter para finalizar el programa..." << endl;
    cin.ignore();  // Espera a que el usuario presione Enter
    cout << "Finalizando proceso padre." << endl;

    return 0;  // El padre termina normalmente
}
