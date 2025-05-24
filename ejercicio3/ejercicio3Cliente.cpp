
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#define NOMBRE_FIFO "miFifo"

using namespace std;

void mostrarAyuda() {
    cout << "Uso: ./ejercicio3Cliente.cpp [OPCIONES]" << endl;
    cout << "Descripción:" << endl;
    cout << "  Este programa simula una cola de impresion FIFO." << endl;
    cout << "  Este es el programa cliente el cual pondra en cola los archivos que indique." << endl;
    cout << "Opciones:" << endl;
    cout << "  -h                               Muestra este mensaje de ayuda" << endl;
    cout << " -i / --impresiones <num>          Cantidad de archivos a imprimir (Requerido). Valor entero positivo." << endl;
    cout << " -a / --archivo <ruta archivo>     Archivo a imprimir. (Requerido)." << endl;
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./programa -i 2 -a ./archivos/prueba         # Ejecución normal" << endl;
    cout << "  ./programa -h                                # Muestra ayuda" << endl;
}

void verificarParametros(int argc, char *argv[]) {
    bool tieneImpresiones = false;
    bool tieneArchivo = false;

    for(int i = 1; i < argc; i++) {  // Empezar en 1 para omitir el nombre del programa
        string arg = argv[i];
        
        if (arg == "-i" || arg == "--impresiones") {
            if (i + 1 >= argc) {
                throw runtime_error("Falta la cantidad de impresiones después de " + arg);
            }
            
            string valor = argv[i+1];
            if (valor.find_first_not_of("0123456789") != string::npos) {
                throw runtime_error("La cantidad de impresiones debe ser un número entero positivo");
            }
            
            tieneImpresiones = true;
            i++;  // Saltar el valor del parámetro
            
        } else if (arg == "-a" || arg == "--archivo") {
            if (i + 1 >= argc) {
                throw runtime_error("Falta la ruta del archivo después de " + arg);
            }
            
            string rutaArchivo = argv[i+1];
            
            // Verificar existencia del archivo
            ifstream archivo(rutaArchivo);
            if (!archivo.good()) {
                throw runtime_error("El archivo no existe o no se puede leer: " + rutaArchivo);
            }
            
            // Verificar que el archivo no esté vacío
            archivo.seekg(0, ios::end);
            if (archivo.tellg() == 0) {
                throw runtime_error("El archivo está vacío: " + rutaArchivo);
            }
            
            tieneArchivo = true;
            i++;  // Saltar el valor del parámetro
            
        } else if (arg == "-h" || arg == "--help") {
            // No es un error, pero detenemos la validación
            return;
        } else {
            throw runtime_error("Parámetro desconocido: " + arg);
        }
    }

    if (!tieneImpresiones) {
        throw runtime_error("Falta especificar la cantidad de impresiones (-i/--impresiones)");
    }
    
    if (!tieneArchivo) {
        throw runtime_error("Falta especificar el archivo (-a/--archivo)");
    }
}

int main(int argc, char *argv[]) {

    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        mostrarAyuda();
        return 0;
    }
    if (argc < 5) {
        cout << "Error: Faltan parámetros requeridos." << endl;
        mostrarAyuda();
        return 1;
    }
    // Verificamos que los parametros sean correctos
    verificarParametros(argc, argv);

    int cantImpresiones = atoi(argv[2]);
    string rutaArchivo = argv[4];

    // Creo el Fifo
    mkfifo(NOMBRE_FIFO, 0666);

    // Escribimos en el fifo publico

    ofstream fifo(NOMBRE_FIFO);
    string contenido = to_string(getpid()) + ":" + rutaArchivo;
    fifo << contenido << ends;
    fifo.close();

    // creo fifo privado para recibir respuesta
    string fifoPrivado = "/tmp/FIFO_" + to_string(getpid());
    mkfifo(fifoPrivado.c_str(), 0666);

    // Abrir el FIFO privado para lectura (se bloqueará hasta que el servidor lo abra para escritura)
    ifstream fifoRespuesta(fifoPrivado);
    string respuesta;
    
    // Leer la respuesta del servidor
    getline(fifoRespuesta, respuesta);
    fifoRespuesta.close();

    // Procesar la respuesta
    if (respuesta == "OK") {
        cout << "Impresión completada exitosamente." << endl;
    } else {
        // Asumimos que cualquier otra cosa es un mensaje de error
        cout << "Error en la impresión: " << respuesta << endl;
    }

    // Limpiar recursos - eliminar el FIFO privado
    unlink(fifoPrivado.c_str());

    return 0;
}    