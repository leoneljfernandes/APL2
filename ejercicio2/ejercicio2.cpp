#include <iostream>
#include <string>
#include <cstring>
#include <filesystem> // Para verificar el directorio (C++17)
#include <climits>
#include <cstdlib> // para strtol

namespace fs = std::filesystem;
using namespace std;

bool esEnteroPositivo(const std::string& s, int& valor) {
    char* end;
    long num = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || num <= 0 || num > INT_MAX) {
        return false;
    }
    valor = static_cast<int>(num);
    return true;
}

int validarParametros(int argc, char *argv[]) {
    std::string directorio;
    int generadores = 0, consumidores = 0, paquetes = 0;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--directorio") {
            if (i + 1 >= argc) {
                std::cerr << "Error: Falta la ruta del directorio después de " << arg << std::endl;
                return 0;
            }
            directorio = argv[++i];
        } else if (arg == "-g" || arg == "--generadores") {
            if (i + 1 >= argc || !esEnteroPositivo(argv[i + 1], generadores)) {
                std::cerr << "Error: Cantidad inválida de generadores después de " << arg << std::endl;
                return 0;
            }
            i++;
        } else if (arg == "-c" || arg == "--consumidores") {
            if (i + 1 >= argc || !esEnteroPositivo(argv[i + 1], consumidores)) {
                std::cerr << "Error: Cantidad inválida de consumidores después de " << arg << std::endl;
                return 0;
            }
            i++;
        } else if (arg == "-p" || arg == "--paquetes") {
            if (i + 1 >= argc || !esEnteroPositivo(argv[i + 1], paquetes)) {
                std::cerr << "Error: Cantidad inválida de paquetes después de " << arg << std::endl;
                return 0;
            }
            i++;
        } else {
            std::cerr << "Error: Parámetro desconocido: " << arg << std::endl;
            return 0;
        }
    }

    // Verificar que todos los parámetros requeridos estén presentes
    if (directorio.empty() || generadores == 0 || consumidores == 0 || paquetes == 0) {
        std::cerr << "Error: Faltan parámetros requeridos." << std::endl;
        return 0;
    }

    // Verificar si el directorio existe o es accesible
    if (!fs::exists(directorio) || !fs::is_directory(directorio)) {
        std::cerr << "Error: El directorio '" << directorio << "' no existe o no es válido." << std::endl;
        return 0;
    }

    return 1; // Válido
}

void mostrarAyuda() {
    cout << "Uso: ./ejercicio1.cpp [OPCIONES]" << endl;
    cout << "Opciones:" << endl;
    cout << "   -h                           Muestra este mensaje de ayuda" << endl;
    cout << "   -d / --directorio <path>     Ruta del directorio a analizar. (Requerido)" <<endl;
    cout << "   -g / --generadores <nro>     Cantidad de threads a ejecutar concurrentemente para generarlos archivos del directorio (Requerido). El número ingresado debe ser un entero positivo." << endl;
    cout << "   -c / --consumidores <nro>    Cantidad de threads a ejecutar concurrentemente para procesar los archivos del directorio (Requerido). El número ingresado debe ser un entero positivo." << endl;
    cout << "   -p / --paquetes <nro>        Cantidad de paquetes a generar (Requerido). El número ingresado debe ser un entero positivo." << endl;
    cout << endl;
    cout << "Descripción:" << endl;
    cout << "El programa crea los threads solicitados, tanto para los generadores como\n"
     << "los procesadores, y distribuye la cantidad de paquetes a generar entre ellos.\n"
     << "Estos hilos generarán archivos en el directorio de procesamiento con el\n"
     << "siguiente formato:\n\n"
     << "  Nombre del archivo: id.paq\n\n"
     << "  Contenido (una línea): id_paquete;peso;destino\n"
     << "    - Id_paquete: número secuencial (1-1.000)\n"
     << "    - Peso: valor decimal aleatorio (0-300 kg)\n"
     << "    - Destino: entero aleatorio (1-50, sucursal destino)\n" << endl;
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./programa -d ./archivos/ -g 2 -c 2 -p 10      # Ejecución normal" << endl;
    cout << "  ./programa -h    # Muestra ayuda" << endl;
}


int main(int argc, char *argv[]){

    // verificar parametro -h
    if (argc > 1 && strcmp(argv[1], "-h") == 0){
        mostrarAyuda();
        return 0;
    }

    if(!validarParametros (argc, argv)){
        cout << "Error: Parámetros inválidos." << endl;
        mostrarAyuda();
        return 1;
    }

    return 0;
}