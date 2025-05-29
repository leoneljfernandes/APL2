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

// Variables globales para manejo de señales
volatile sig_atomic_t finalizar_servidor = 0;
volatile sig_atomic_t partida_en_curso = 0;

void mostrarAyuda() {
    cout << "Uso: ./ejercicio4Servidor.cpp [OPCIONES]" << endl;
    cout << "Descripción:" << endl;
    cout << "Este es el programa servidor tomara una frase aleatoria de un archivo "
         << "y la enviara al cliente para ser adivinada." << endl;
    cout << "Opciones:" << endl;
    cout << " -h                               Muestra este mensaje de ayuda" << endl;
    cout << " -a / --archivo <ruta archivo>     Archivo de donde tomar las frases. (Requerido), cada frase debe estar en una linea." << endl;
    cout << " -c / --cantidad <num>             Cantidad de intentos por partida (Requerido). Valor entero positivo." << endl;
    
    cout << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./ejercicio4Servidor -a ./archivos/prueba -c 2   #Ejecución normal" << endl;
    cout << "  ./ejercicio4Servidor -h                          # Muestra ayuda" << endl;
}

void verificarParametros(int argc, char *argv[]) {
    for(int i = 1; i < argc; i++) {  // Empezar en 1 para omitir el nombre del programa
        string arg = argv[i];
        
        if (arg == "-c" || arg == "--cantidad") {
            if (i + 1 >= argc) {
                throw runtime_error("Falta la cantidad de intentos después de " + arg);
            }
            
            string valor = argv[i+1];
            if (valor.find_first_not_of("0123456789") != string::npos) {
                throw runtime_error("La cantidad de intentos debe ser un número entero positivo");
            }

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
            
            i++;  // Saltar el valor del parámetro
            
        } else if (arg == "-h" || arg == "--help") {
            // No es un error, pero detenemos la validación
            return;
        } else {
            throw runtime_error("Parámetro desconocido: " + arg);
        }
    }
}

vector<string> obtenerFrases(string rutaArchivo){
    try{
        ifstream archivo(rutaArchivo);
        if (!archivo.is_open()) {
            throw runtime_error("No se pudo abrir el archivo: " + rutaArchivo);
        }

        string frase;
        vector<string> frasesDisponibles;

        while (getline(archivo, frase)) {
            if (!frase.empty()) {  // Aseguramos que la frase no esté vacía
                frasesDisponibles.push_back(frase);
            }
        }

        archivo.close();

        if (frasesDisponibles.empty()) {
            throw runtime_error("El archivo no contiene frases válidas.");
        }

        return frasesDisponibles;

    } catch (const exception &e) {
        cerr << "Error al leer el archivo: " << e.what() << endl;
        return {};
    }
    
}

// Manejador de señales
void manejarSenhal(int sig) {
    if (sig == SIGUSR1) {
        if (!partida_en_curso) {
            finalizar_servidor = 1;
        }
    } else if (sig == SIGUSR2) {
        finalizar_servidor = 1;
    }
}

// Función para comparar y ordenar el ranking
bool compararClientes(const pair<string, int>& a, const pair<string, int>& b) {
    // Ordenar por intentos (menor primero) y luego por nombre
    if (a.second != b.second) {
        return a.second < b.second;
    }
    return a.first < b.first;
}

int main(int argc, char *argv[]){

    if (argc < 5){
        cout << "Parametros incompletos." << endl;
        mostrarAyuda();
    }

    string opcion = argv[1];
    if(opcion == "-h" || opcion == "--help") {
        mostrarAyuda();
        return 0;
    }

    verificarParametros(argc, argv);

    string rutaArchivo = argv[2];
    int cantidadIntentos = stoi(argv[3]);

    // Configurar manejo de señales
    signal(SIGINT, SIG_IGN);  // Ignorar Ctrl-C
    signal(SIGUSR1, manejarSenhal);
    signal(SIGUSR2, manejarSenhal);

    // leemos el archivo y obtenemos todas las frases disponibles
    vector<string> frasesDisponibles = obtenerFrases(rutaArchivo);

    if (frasesDisponibles.empty()) {
        cerr << "No se pudieron obtener frases del archivo." << endl;
        return 1;
    }
    
    //datos cliente ganador
    string nombreClienteGanador = "";
    int tiempoClienteGanador = 0;

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

    // verificamos que no este corriendo otro servidor
    if (sem_trywait(semaforoServidor) == -1) {
        cerr << "Ya hay un servidor corriendo." << endl;
        sem_close(semaforoServidor);
        sem_close(semaforoCliente);
        sem_close(semaforoClienteUnico);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);
        munmap(respuesta, sizeof(RespuestaServidor));
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        shm_unlink("miMemoriaNickname");
        munmap(letraADivinar, sizeof(char));
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    // ranking de clientes que guarda nombre y cantidad de intentos
    vector<pair<string, int>> rankingClientes;

    //esperar conexion de cliente
    cout << "Servidor iniciado. Esperando conexión de clientes..." << endl;
    while (!finalizar_servidor) {
        // Elegir una frase aleatoria de las disponibles
        if(sem_wait(semaforoClienteUnico)==-1){
            cerr << "Error al esperar por cliente" << endl;
            break;
        }

        partida_en_curso=1;

        int indiceAleatorio = rand() % frasesDisponibles.size();
        string fraseAdivinar = frasesDisponibles[indiceAleatorio];
        string fraseAdivinarAUsar = fraseAdivinar;
        int intentosUtilizados = 0;
        bool adivinoFrase = false;

        // remuevo espacios en blanco
        fraseAdivinarAUsar.erase(std::remove_if(fraseAdivinarAUsar.begin(), 
            fraseAdivinarAUsar.end(), [](unsigned char c) { return std::isspace(c); }), fraseAdivinarAUsar.end());        
        // Esperar a que el cliente adivine la letra
        while ((intentosUtilizados < cantidadIntentos) && !adivinoFrase && !finalizar_servidor) {
            // espero cliente escriba una letra en la memoria compartida
            sem_wait(semaforoCliente);

            if (finalizar_servidor) {
                sem_post(semaforoServidor);
                break;
            }

            //leo letra que el cliente deposita en memoria
            char letra = *letraADivinar;
            bool letraCorrecta = false;

            // verifico si la letra es parte de la frase
            if(fraseAdivinar.find(letra) != string::npos){
                letraCorrecta = true;
                cout << "El cliente adivino la letra: " << letra << endl;
                fraseAdivinarAUsar.erase(std::remove(fraseAdivinarAUsar.begin(), fraseAdivinarAUsar.end(), letra), fraseAdivinarAUsar.end());
                    
                if(fraseAdivinarAUsar.empty()){
                    cout << "El cliente: " << string(nicknameCliente) << "adivino la frase: " << fraseAdivinar << endl;
                    if(intentosUtilizados < tiempoClienteGanador || tiempoClienteGanador == 0){
                        nombreClienteGanador= string(nicknameCliente);
                        tiempoClienteGanador =  intentosUtilizados;
                        cout << "El cliente " << string(nicknameCliente) << " ha ganado con " << intentosUtilizados << " intentos." << endl;
                        rankingClientes.push_back(make_pair(string(nicknameCliente), intentosUtilizados));
                        sem_post(semaforoServidor); // Libero el semáforo del servidor
                        adivinoFrase = true;
                    }
                }
            }
            // Preparar respuesta para el cliente
            respuesta->letraCorrecta = letraCorrecta;
            respuesta->intentosRestantes = cantidadIntentos - (intentosUtilizados + 1);

            intentosUtilizados++;
            sem_post(semaforoServidor); // Libero el semáforo del servidor para permitir que el cliente continúe
        }

        if(!adivinoFrase && !finalizar_servidor){
            rankingClientes.push_back(make_pair(string(nicknameCliente), intentosUtilizados));
        }

        partida_en_curso = 0;
        sem_post(semaforoClienteUnico); // libero semaforo para permitir nuevo cliente

        if(finalizar_servidor){
            break;
        }
    }
        // Limpieza antes de salir
    cout << "\nResultados finales:" << endl;
    for (const auto& cliente : rankingClientes) {
        cout << "Cliente: " << cliente.first << " - Intentos: " << cliente.second << endl;
    }

    // muestro ranking:
    // Mostrar ranking ordenado
    cout << "\n=== RANKING DE CLIENTES ===" << endl;
    cout << "Posición\tNombre\t\tIntentos" << endl;
    cout << "--------------------------------" << endl;
    
    // Ordenar el ranking
    sort(rankingClientes.begin(), rankingClientes.end(), compararClientes);
    
    // Imprimir el ranking ordenado
    for (size_t i = 0; i < rankingClientes.size(); ++i) {
        cout << i + 1 << "\t\t" << rankingClientes[i].first << "\t\t" 
             << rankingClientes[i].second << endl;
    }

    if (!rankingClientes.empty()) {
        cout << "\n¡Felicidades al ganador " << rankingClientes[0].first 
             << " con solo " << rankingClientes[0].second << " intentos!" << endl;
    } else {
        cout << "\nNo se registraron clientes en esta sesión." << endl;
    }

    // Liberar recursos
    sem_close(semaforoServidor);
    sem_close(semaforoCliente);
    sem_close(semaforoClienteUnico);
    sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);
    munmap(respuesta, sizeof(RespuestaServidor));
    shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
    munmap(nicknameCliente, 20 * sizeof(char));
    shm_unlink("miMemoriaNickname");
    munmap(letraADivinar, sizeof(char));
    shm_unlink(NOMBRE_MEMORIA);
    
    return 0;
}

