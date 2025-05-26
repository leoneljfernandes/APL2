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
    cout << "  ./programa        # Ejecución normal" << endl;
    cout << "  ./programa -h     # Muestra ayuda" << endl;
}

void manejarSenial(int signal) {
    unlink(NOMBRE_FIFO);
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, manejarSenial);
    signal(SIGTERM, manejarSenial);

    if (argc > 1 && strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        mostrarAyuda();
        return 0;
    }

    // Limpiamos la cola de impresión y resultados previos
    unlink(NOMBRE_FIFO);

    string LOG_TEMPORAL = "/tmp/impresiones.log";
    // Limpiamos el archivo de log
    try {
        ofstream log(LOG_TEMPORAL, ios::trunc);
        log.close();
    } catch (const exception& e) {
        cerr << "Error al limpiar el archivo de log: " << e.what() << endl;
        return 1;
    }

    // Creamos el FIFO público
    if (mkfifo(NOMBRE_FIFO, 0666) == -1) {
        perror("Error al crear el FIFO público");
        return 1;
    }

    // Abrimos el FIFO público en modo lectura
    int fd_fifo = open(NOMBRE_FIFO, O_RDONLY);
    if (fd_fifo == -1) {
        perror("Error al abrir el FIFO público");
        return 1;
    }

    // Buffer para lectura
    char buffer[1024];
    ssize_t bytes_leidos;

    while(true) {
        bytes_leidos = read(fd_fifo, buffer, sizeof(buffer)-1);
        if (bytes_leidos > 0) {
            buffer[bytes_leidos] = '\0';
            string mensaje(buffer);
            
            if (!mensaje.empty()) {
                size_t sep = mensaje.find(':');
                if (sep != string::npos) {
                    string pidCliente = mensaje.substr(0, sep);
                    string archivo = mensaje.substr(sep + 1);

                    // Eliminar posibles saltos de línea
                    archivo.erase(std::remove(archivo.begin(), archivo.end(), '\n'), archivo.end());
                    archivo.erase(std::remove(archivo.begin(), archivo.end(), '\r'), archivo.end());

                    // Simular impresión
                    cout << "Imprimiendo archivo: " << archivo << endl;
                    sleep(2); // Tiempo de impresión simulado

                    // Procesar archivo y log
                    ifstream archivoImpreso(archivo);
                    ofstream log(LOG_TEMPORAL, ios::app);

                    if (archivoImpreso && log) {
                        // Obtener tiempo actual
                        auto ahora = chrono::system_clock::now();
                        time_t tiempo_actual = chrono::system_clock::to_time_t(ahora);
                        tm* tiempo_local = localtime(&tiempo_actual);

                        // Formatear mensaje de log
                        ostringstream oss;
                        oss << "PID {" << pidCliente << "} imprimió el archivo {" << archivo
                            << "} el día {" << put_time(tiempo_local, "%d/%m/%Y")
                            << "} a las {" << put_time(tiempo_local, "%H:%M:%S") << "}";

                        log << oss.str() << endl;
                        log << archivoImpreso.rdbuf() << endl << endl;

                        // Enviar respuesta al cliente
                        string fifoRespuestaPrivado = "/tmp/FIFO_" + pidCliente;
                        ofstream respuesta(fifoRespuestaPrivado);
                        respuesta << "OK" << endl;
                        respuesta.close();
                    } else {
                        string fifoCliente = "/tmp/FIFO_" + pidCliente;
                        ofstream respuesta(fifoCliente);
                        respuesta << "Error al procesar el archivo" << endl;
                        respuesta.close();
                    }
                }
            }
        }
    }

    close(fd_fifo);
    unlink(NOMBRE_FIFO);
    return 0;
}