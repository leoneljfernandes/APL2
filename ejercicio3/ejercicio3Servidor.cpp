#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <csignal>
#include <string.h>
#include <algorithm>

#define NOMBRE_FIFO "miFifo"
#define TIMEOUT_SEGUNDOS 5  // Tiempo de espera para considerar inactividad

using namespace std;

void mostrarAyuda() {
    cout << "Uso: ./ejercicio3Servidor.cpp [OPCIONES]" << endl;
    cout << "Descripción:" << endl;
    cout << "  Este programa simula una cola de impresion FIFO." << endl;
    cout << "  Este es el programa servidor el cual tomara los " << endl;
    cout << "  archivos que le envien los clientes y los imprimira y procesara." << endl;
    cout << "Opciones:" << endl;
    cout << "  -h                               Muestra este mensaje de ayuda" << endl;
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./ejercicio3Servidor        # Ejecución normal" << endl;
    cout << "  ./ejercicio3Servidor -h     # Muestra ayuda" << endl;
}


int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        mostrarAyuda();
        return 0;
    }

    unlink(NOMBRE_FIFO);
    string LOG_TEMPORAL = "/tmp/impresiones.log";
    
    // Limpiar archivo de log
    ofstream log(LOG_TEMPORAL, ios::trunc);
    log.close();

    // Crear el FIFO
    if (mkfifo(NOMBRE_FIFO, 0666) == -1) {
        perror("Error al crear el FIFO");
        return 1;
    }

    // Abrir el FIFO en modo lectura/escritura para evitar bloqueos
    int fd = open(NOMBRE_FIFO, O_RDWR);
    if (fd == -1) {
        perror("Error al abrir el FIFO");
        return 1;
    }

        // Convertir descriptor a ifstream
    FILE* fifo_file = fdopen(fd, "r");
    if (!fifo_file) {
        perror("Error al crear FILE*");
        close(fd);
        return 1;
    }

    cout << "Servidor iniciado (se cerrará después de " << TIMEOUT_SEGUNDOS 
         << " segundos de inactividad)" << endl;

    time_t ultima_actividad = time(nullptr);
    string linea;


    cout << "Servidor de impresión iniciado. Esperando trabajos..." << endl;

    while (true) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), fifo_file)) {
            ultima_actividad = time(nullptr);
            linea = buffer;
            
            size_t sep = linea.find(':');
            if (sep != string::npos) {
                string pidCliente = linea.substr(0, sep);
                string archivo = linea.substr(sep + 1);
                archivo.erase(remove(archivo.begin(), archivo.end(), '\n'), archivo.end());

                cout << "Procesando: " << archivo << endl;
                sleep(2); // Simular impresión

                // Registrar en log
                ofstream log(LOG_TEMPORAL, ios::app);
                ifstream archivo_leer(archivo);
                if (log && archivo_leer) {
                    auto ahora = chrono::system_clock::now();
                    time_t t = chrono::system_clock::to_time_t(ahora);
                    log << "PID " << pidCliente << " | Archivo: " << archivo 
                        << " | Fecha: " << put_time(localtime(&t), "%F %T") << "\n"
                        << archivo_leer.rdbuf() << "\n\n";
                }

                // Responder al cliente
                ofstream respuesta("/tmp/FIFO_" + pidCliente);
                if (respuesta) respuesta << "OK" << endl;
            }
        } else {
            // Verificar timeout
            if (difftime(time(nullptr), ultima_actividad) > TIMEOUT_SEGUNDOS) {
                cout << "Timeout de inactividad. Cerrando servidor..." << endl;
                break;
            }
            usleep(100000); // Esperar 100ms antes de reintentar
        }
    // Limpieza
    fclose(fifo_file);
    close(fd);
    unlink(NOMBRE_FIFO);
    cout << "Servidor cerrado correctamente" << endl;
    return 0;
    }
}
