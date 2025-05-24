#include <iostream>
#include <string>
#include <cstring>
#include <filesystem> // Para verificar el directorio (C++17)
#include <climits>
#include <cstdlib> // para strtol
#include <mutex>
#include <thread>
#include <vector>
#include <fstream>
#include <condition_variable>

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

void limpiarContenidoDirectorio(const string& directorio) {
    try {
        if (fs::exists(directorio) && fs::is_directory(directorio)) {
            for (const auto& entry : fs::directory_iterator(directorio)) {
                fs::remove_all(entry.path());
            }
        }
    } catch (const fs::filesystem_error& e) {
        cout << "Error al limpiar el directorio: " << e.what() << endl;
    }
}

struct datosCompartidos {
    mutex mtx;
    condition_variable cv_producido;  // Notifica cuando se produce un elemento
    condition_variable cv_consumido;  // Notifica cuando se consume un elemento
    vector<string> buffer;
    // vector de 1 a 50 por sucursal para guardar el peso
    vector<double> pesosPorSucursal = vector<double>(50, 0);
    int maxBufferSize = 10;
    int nextId = 1;
    int cantMaxPaquetes = 0;
    int cantPaquetesGenerados = 0;
    int cantPaquetesProcesados = 0;
    bool fin = false;
};

void generarPaquetes(string subDirectorio, datosCompartidos& datos, int maxPaquetesAGenerar){

    while(true){
        //pido mutex
        unique_lock<mutex> lock(datos.mtx);

        // esperar si el buffer esta lleno
        datos.cv_producido.wait(lock, [&datos] { 
            return datos.buffer.size() < datos.maxBufferSize || datos.fin; });


        // si ya se generaron los paquetes a generar o se alcanzo el maximo, salgo
        if(datos.cantPaquetesGenerados == datos.cantMaxPaquetes || datos.cantPaquetesGenerados == maxPaquetesAGenerar){
            datos.fin = true;
            datos.cv_producido.notify_all(); // notificar a los consumidores
            return;
        }
        
        // genero paquete
        string nombreArchivo = to_string(datos.nextId) + ".paq";

        double peso = (rand() % 300) + 1; // peso entre 1 y 300
        int destino = (rand() % 50) + 1; // destino entre 1 y 50   
        string contenido = to_string(datos.nextId) + ";" + to_string(peso) + ";" + to_string(destino);
        
        // creo el archivo
        try{
            ofstream archivo(subDirectorio + "/" + nombreArchivo);
            if (archivo.is_open()) {
                archivo << contenido;
                archivo.close();
                cout << "Generador: " << nombreArchivo << " creado." << endl;
            } else {
                cout << "Error al crear el archivo: " << nombreArchivo << endl;
            }
        } catch (const fs::filesystem_error& e) {
            cout << "Error al crear el archivo: " << e.what() << endl;
        }

        // actualizo el buffer
        datos.buffer.push_back(nombreArchivo);
        datos.cv_producido.notify_one(); // notificar a los consumidores
        datos.nextId++;
        datos.cantPaquetesGenerados++;

        lock.unlock(); //libero el mutex
        this_thread::sleep_for(std::chrono::milliseconds(100)); //simulo tiempo de generacion
        
        cout << "Generador: " << nombreArchivo << " generado." << endl;
        cout << "Cantidad de paquetes generados: " << datos.cantPaquetesGenerados << endl;

    }
}

void procesarPaquetes(string directorioProcesamiento, string subDirectorioProcesamiento,datosCompartidos& datos, int maxPaquetesAProcesar){
    while(true){
        //pido mutex
        unique_lock<mutex> lock(datos.mtx);

        if(datos.buffer.empty() && datos.fin){
                return;
        }
        if(datos.buffer.empty() ){
            continue;
        }

        //espero si el buffet esta vacio
        datos.cv_producido.wait(lock, [&datos]{
            return !datos.buffer.empty() || datos.fin; });   

        // obtengo archivos a procesar
        string nombreArchivo = datos.buffer.back();
        datos.buffer.pop_back();
        datos.cantPaquetesProcesados++;

        //notifico al productor que hay espacio en buffer
        datos.cv_consumido.notify_one();

        //libero mutex
        lock.unlock();

        //proceso archivo
        try{
            //muevo el archivo del subdirec a procesados
            std::filesystem::path origen = directorioProcesamiento + "/" + nombreArchivo;
            std::filesystem::path destino = subDirectorioProcesamiento + "/" + nombreArchivo;

            // Procesar peso por cada sucursal destino
            try{
                ifstream archivo(origen);
                if (archivo.is_open()) {
                    string linea;
                    while (getline(archivo, linea)) {
                        size_t pos1 = linea.find(';');
                        size_t pos2 = linea.find(';', pos1 + 1);
                        if (pos1 != string::npos && pos2 != string::npos) {
                            int destinoSucursal = stoi(linea.substr(pos2 + 1));
                            double peso = stod(linea.substr(pos1 + 1, pos2 - pos1 - 1));
                            datos.pesosPorSucursal[destinoSucursal - 1] += peso; // Acumular peso por sucursal
                        }
                    }
                    archivo.close();
                } else {
                    cout << "Error al abrir el archivo: " << origen << endl;
                }

                // Mostrar pesos por sucursal
                cout << "Pesos por sucursal:" << endl;
                for (size_t i = 0; i < datos.pesosPorSucursal.size(); ++i) {
                    cout << "Sucursal " << (i + 1) << ": " << datos.pesosPorSucursal[i] << " kg" << endl;
                }

            }catch(const exception& e){
                cout << "Error al procesar el archivo: " << e.what() << endl;
            }

            if(fs::exists(origen)){
                fs::rename(origen, destino);
                cout << "Consumidor: " << nombreArchivo << " procesado." << endl;
            } else {
                cout << "Error al procesar el archivo: " << nombreArchivo << endl;
            }
        }catch(const exception& e){
            cout << "Error al procesar el archivo: " << e.what() << endl;
        }

        //simulo proc
        this_thread::sleep_for(std::chrono::milliseconds(100)); //simulo tiempo de procesamiento
    }
}

int main(int argc, char *argv[]){

    // creo el subDirectorio de procesamiento como nulo
    string directorioProcesamiento = "";
    int cantGeneradores = 0;
    int cantConsumidores = 0;
    int cantPaquetes = 0;

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

    // ./ejercicio2 -d ./archivos/ -g 2 -c 2 -p 10
    // .. 0         1   2           3 4 5 6  7  8
    // asigno el directorio y le concateno al subdirectorio el nombre del directorio de procesamiento
    directorioProcesamiento = argv[2];
    string subDirectorioProcesamiento = directorioProcesamiento + "/procesados";
    // Crear directorios si no existen
    if(!fs::exists(directorioProcesamiento)) {
        fs::create_directory(directorioProcesamiento);
    }
    if(!fs::exists(subDirectorioProcesamiento)) {
        fs::create_directory(subDirectorioProcesamiento);
    }
    cantGeneradores = atoi(argv[4]);
    cantConsumidores = atoi(argv[6]);
    cantPaquetes = atoi(argv[8]);

    // limpiar directorio de procesamiento
    limpiarContenidoDirectorio(directorioProcesamiento);
    limpiarContenidoDirectorio(subDirectorioProcesamiento);

    // generar hilo productor que generara el archivo paquete para depositar en buffer
    // Genero vector de hilos generadores
    datosCompartidos datosCompartidos;
    datosCompartidos.cantMaxPaquetes = cantPaquetes;
    vector<thread> generadores;
    for(int i = 0; i < cantGeneradores; i++){
        generadores.push_back(thread(generarPaquetes, directorioProcesamiento, ref(datosCompartidos), 1000));
    }


    // generar hilo consumidor que procesara y consumira el paquete del buffer
    vector<thread> consumidores;
    for(int i = 0; i< cantConsumidores; i++){
        consumidores.emplace_back(procesarPaquetes, directorioProcesamiento, subDirectorioProcesamiento, ref(datosCompartidos), 1000);
    }

        // Esperar a que terminen los generadores
    for(auto& t : generadores) {
        t.join();
    }

    // Esperar a que terminen los consumidores
    for(auto& t : consumidores) {
        t.join();
    }

    cout << "Proceso completado." << endl;
    cout << "Cantidad total de paquetes generados: " << datosCompartidos.cantPaquetesGenerados << endl;
    cout << "Cantidad total de paquetes procesados: " << datosCompartidos.cantPaquetesProcesados << endl;
    cout << "Pesos por sucursal:" << endl;
    for (size_t i = 0; i < datosCompartidos.pesosPorSucursal.size(); ++i) {
        cout << "Sucursal " << (i + 1) << ": " << datosCompartidos.pesosPorSucursal[i] << " kg" << endl;
    }

    return 0;
}