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
#define BUFFER_SIZE 1024

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


/*
int main(int argc, char *argv[]) {

    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        mostrarAyuda();
        return 0;
    }

    // Limpiamos la cola de impresion y resultados previos
    unlink(NOMBRE_FIFO);

    string LOG_TEMPORAL = "/tmp/impresiones.log"; // creo log
    // Limpiamos el archivo de log
    try{
        ofstream log(LOG_TEMPORAL, ios::trunc);
        log.close();
    }catch (const exception& e) {
        cout << "Error al limpiar el archivo de log: " << e.what() << endl;
    }
    // creo el archivo fifo
    mkfifo(NOMBRE_FIFO, 0666);

    // Leemos el fifo de uno a la vez
    while(true){
        ifstream fifo(NOMBRE_FIFO);
        string buffer;

        while(getline(fifo, buffer)){

            if(!buffer.empty()){
                size_t sep = buffer.find(':');
                if(sep != string:: npos){
                    string pidCliente = buffer.substr(0, sep);
                    string archivo = buffer.substr(sep + 1);

                    //simular impresion
                    cout << "Imprimiendo archivo: " << archivo << endl;
                    sleep(2); // Simulamos un tiempo de impresión de 2 segundos

                    // abro archivo y log en append para escribir
                    ifstream archivoImpreso(archivo);
                    ofstream log(LOG_TEMPORAL, ios::app);

                    if(archivoImpreso && log){
                        // Obtener el tiempo actual
                        auto ahora = std::chrono::system_clock::now();
                        std::time_t tiempo_actual = std::chrono::system_clock::to_time_t(ahora);
                        // Convertir a estructura tm para formateo
                        std::tm* tiempo_local = std::localtime(&tiempo_actual);
                        // Crear un stringstream para formatear
                        std::ostringstream oss;

                        // Formatear según lo solicitado
                        oss << "PID {" << pidCliente << "} imprimió el archivo {" << archivo
                            << "} el día {" << std::put_time(tiempo_local, "%d/%m/%Y")
                            << "} a las {" << std::put_time(tiempo_local, "%H:%M:%S") << "}";

                        log << oss.str() << endl;
                        log << archivoImpreso.rdbuf() << endl << endl;
                        
                        string fifoRespuestaPrivado = "/tmp/FIFO_" + pidCliente;
                        ofstream respuesta(fifoRespuestaPrivado);
                        respuesta << "OK" << endl;
                        respuesta.close();
                        unlink(fifoRespuestaPrivado.c_str()); // Eliminar FIFO privado

                        

                    }else{
                        // Enviar mensaje de error al cliente
                        string fifoCliente = "/tmp/FIFO_" + pidCliente;
                        ofstream respuesta(fifoCliente);
                        respuesta << "Error al procesar el archivo" << endl;
                        respuesta.close();
                        unlink(fifoCliente.c_str()); // Eliminar FIFO privado
                    }

                }
            }

        }
        //cierro fifo publico
        fifo.clear();
        fifo.close();

    }
    

}
*/

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

    char buffer[BUFFER_SIZE];
    ssize_t bytes_leidos;

    cout << "Servidor de impresión iniciado. Esperando trabajos..." << endl;

    while (true) {
        bytes_leidos = read(fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_leidos > 0) {
            buffer[bytes_leidos] = '\0';
            string mensaje(buffer);
            
            size_t sep = mensaje.find(':');
            if (sep != string::npos) {
                string pidCliente = mensaje.substr(0, sep);
                string archivo = mensaje.substr(sep + 1);
                
                // Eliminar posibles saltos de línea
                archivo.erase(remove(archivo.begin(), archivo.end(), '\n'), archivo.end());

                cout << "Imprimiendo archivo: " << archivo << " para cliente " << pidCliente << endl;
                sleep(2); // Simular tiempo de impresión

                // Procesar archivo
                ifstream archivoImpreso(archivo);
                ofstream log(LOG_TEMPORAL, ios::app);

                if (archivoImpreso && log) {
                    auto ahora = chrono::system_clock::now();
                    time_t tiempo_actual = chrono::system_clock::to_time_t(ahora);
                    tm* tiempo_local = localtime(&tiempo_actual);
                    
                    ostringstream oss;
                    oss << "PID {" << pidCliente << "} imprimió el archivo {" << archivo
                        << "} el día {" << put_time(tiempo_local, "%d/%m/%Y")
                        << "} a las {" << put_time(tiempo_local, "%H:%M:%S") << "}";

                    log << oss.str() << endl;
                    log << archivoImpreso.rdbuf() << endl << endl;
                    
                    // Responder al cliente
                    string fifoRespuesta = "/tmp/FIFO_" + pidCliente;
                    ofstream respuesta(fifoRespuesta);
                    if (respuesta) {
                        respuesta << "OK" << endl;
                    }
                    respuesta.close();
                } else {
                    string fifoRespuesta = "/tmp/FIFO_" + pidCliente;
                    ofstream respuesta(fifoRespuesta);
                    if (respuesta) {
                        respuesta << "Error al procesar el archivo" << endl;
                    }
                    respuesta.close();
                }
            }
        } else if (bytes_leidos == -1) {
            perror("Error al leer del FIFO");
            break;
        }
    }

    close(fd);
    unlink(NOMBRE_FIFO);
    return 0;
}