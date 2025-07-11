
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

#define NOMBRE_FIFO "miFifo"

using namespace std;

void mostrarAyuda()
{
    cout << "Uso: ./ejercicio3Cliente.cpp [OPCIONES]" << endl;
    cout << "Descripción:" << endl;
    cout << "  Este programa simula una cola de impresion FIFO." << endl;
    cout << "  Este es el programa cliente el cual pondra en cola los archivos que indique." << endl;
    cout << "Opciones:" << endl;
    cout << "  -h                               Muestra este mensaje de ayuda" << endl;
    cout << " -a / --archivo <ruta archivo>     Archivo a imprimir. (Requerido)." << endl;
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./ejercicio3Cliente -a ./archivos/prueba         # Ejecución normal" << endl;
    cout << "  ./ejercicio3Cliente -h                           # Muestra ayuda" << endl;
}

void verificarParametros(int argc, char *argv[])
{
    bool tieneArchivo = false;

    for (int i = 1; i < argc; i++)
    { // Empezar en 1 para omitir el nombre del programa
        string arg = argv[i];

        if (arg == "-a" || arg == "--archivo")
        {
            if (i + 1 >= argc)
            {
                throw runtime_error("Falta la ruta del archivo después de " + arg);
            }

            string rutaArchivo = argv[i + 1];

            // Verificar existencia del archivo
            ifstream archivo(rutaArchivo);
            if (!archivo.good())
            {
                throw runtime_error("El archivo no existe o no se puede leer: " + rutaArchivo);
            }

            // Verificar que el archivo no esté vacío
            archivo.seekg(0, ios::end);
            if (archivo.tellg() == 0)
            {
                throw runtime_error("El archivo está vacío: " + rutaArchivo);
            }

            tieneArchivo = true;
            i++; // Saltar el valor del parámetro
        }
        else if (arg == "-h" || arg == "--help")
        {
            // No es un error, pero detenemos la validación
            return;
        }
        else
        {
            throw runtime_error("Parámetro desconocido: " + arg);
        }
    }

    if (!tieneArchivo)
    {
        throw runtime_error("Falta especificar el archivo (-a/--archivo)");
    }
}

bool servidorDisponible()
{
    // Verificar si el FIFO existe
    if (access(NOMBRE_FIFO, F_OK) == -1)
    {
        return false;
    }

    // Intentar abrir en modo no bloqueante para verificar disponibilidad
    int fd = open(NOMBRE_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd == -1)
    {
        // Si el error es ENXIO, significa que no hay servidor leyendo
        if (errno == ENXIO)
        {
            return false;
        }
        return false;
    }
    close(fd);
    return true;
}

int main(int argc, char *argv[])
{

    // verifico si faltan parametros o no se paso ninguno
    if (argc < 3)
    {
        cout << "Error: Faltan parámetros requeridos." << endl;
        mostrarAyuda();
        return 1;
    }

    if ((argc > 1 && strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))
    {
        mostrarAyuda();
        return 0;
    }

    verificarParametros(argc, argv);

    // Verificar disponibilidad del servidor antes de continuar
    if (!servidorDisponible())
    {
        cerr << "Error: No hay servidor disponible. Asegúrese de que el servidor esté ejecutándose." << endl;
        return 1;
    }

    string rutaArchivo = argv[2];

    // Crear FIFO privado antes de enviar la solicitud
    string fifoPrivado = "/tmp/FIFO_" + to_string(getpid());
    mkfifo(fifoPrivado.c_str(), 0666);

    // Abrir FIFO público y enviar solicitud
    ofstream fifo(NOMBRE_FIFO);
    if (!fifo)
    {
        cerr << "Error al abrir el FIFO público" << endl;
        unlink(fifoPrivado.c_str());
        return 1;
    }

    fifo << getpid() << ":" << rutaArchivo << endl;
    fifo.close();

    // Esperar respuesta del servidor
    ifstream fifoRespuesta(fifoPrivado);
    string respuesta;

    if (getline(fifoRespuesta, respuesta))
    {
        if (respuesta == "OK")
        {
            cout << "Impresión completada exitosamente." << endl;
        }
        else
        {
            cout << "Error en la impresión: " << respuesta << endl;
        }
    }
    else
    {
        cout << "No se recibió respuesta del servidor" << endl;
    }

    fifoRespuesta.close();
    unlink(fifoPrivado.c_str());

    return 0;
}
