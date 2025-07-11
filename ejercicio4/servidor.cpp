/*
# Integrantes del grupo:
# - Berti Rodrigo
# - Burnowicz Alejo
# - Fernandes Leonel
# - Federico Agustin
*/
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
#include <chrono>
#include <string.h>

using namespace std;

#define NOMBRE_SEMAFORO_SERVIDOR "semaforoServidor"
#define NOMBRE_SEMAFORO_CLIENTE "semaforoCliente"
#define NOMBRE_SEMAFORO_CLIENTE_UNICO "semaforoClienteUnico"
#define NOMBRE_MEMORIA "miMemoria"
#define NOMBRE_MEMORIA_RESPUESTA "miMemoriaRespuesta"

void mostrarAyuda() {
    cout << "Uso: ./servidor [OPCIONES]" << endl;
    cout << "Descripción:" << endl;
    cout << "Servidor para el juego de adivinanza de frases." << endl;
    cout << "Opciones:" << endl;
    cout << " -h                               Muestra este mensaje de ayuda" << endl;
    cout << " -a / --archivo <ruta archivo>    Archivo con frases (una por línea)" << endl;
    cout << " -c / --cantidad <num>            Intentos por partida (entero positivo)" << endl;
    cout << "Ejemplos:" << endl;
    cout << "  ./servidor -a frases.txt -c 5" << endl;
}

void verificarParametros(int argc, char *argv[]) {
    for(int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "-c" || arg == "--cantidad") {
            if (i + 1 >= argc) throw runtime_error("Falta la cantidad de intentos");
            
            string valor = argv[i+1];
            if (valor.find_first_not_of("0123456789") != string::npos) {
                throw runtime_error("La cantidad de intentos debe ser un número entero positivo");
            }

            if(stoi(valor) < 1) throw runtime_error("La cantidad de intentos debe ser mayor a 0");
            i++;
            
        } else if (arg == "-a" || arg == "--archivo") {
            if (i + 1 >= argc) throw runtime_error("Falta la ruta del archivo");
            
            string rutaArchivo = argv[i+1];
            ifstream archivo(rutaArchivo);
            if (!archivo.good()) throw runtime_error("El archivo no existe o no se puede leer");
            
            archivo.seekg(0, ios::end);
            if (archivo.tellg() == 0) throw runtime_error("El archivo está vacío");
            i++;
            
        } else if (arg == "-h" || arg == "--help") {
            return;
        } else {
            throw runtime_error("Parámetro desconocido: " + arg);
        }
    }
}

vector<string> obtenerFrases(string rutaArchivo){
    try {
        ifstream archivo(rutaArchivo);
        if (!archivo.is_open()) throw runtime_error("No se pudo abrir el archivo");

        string frase;
        vector<string> frasesDisponibles;

        while (getline(archivo, frase)) {
            if (!frase.empty()) frasesDisponibles.push_back(frase);
        }

        archivo.close();

        if (frasesDisponibles.empty()) throw runtime_error("El archivo no contiene frases válidas");
        return frasesDisponibles;

    } catch (const exception &e) {
        cerr << "Error al leer el archivo: " << e.what() << endl;
        return {};
    }
}

bool partida_en_curso = false;
bool finalizar_servidor = false;

void manejador_seniales(int signal) {
    switch(signal) {
        case SIGUSR1:
            if (!partida_en_curso) {
                cout << "SIGUSR1 recibido - Finalizando servidor (no hay partida en curso)" << endl;
                finalizar_servidor = true;
            } else {
                cout << "SIGUSR1 recibido - Esperando finalización de partida..." << endl;
            }
            break;
            
        case SIGUSR2:
            cout << "SIGUSR2 recibido - Finalizando servidor inmediatamente" << endl;
            finalizar_servidor = true;
            break;
    }
}

struct respuestaCliente {
    bool letraCorrecta;
    int intentosRestantes;
    bool partidaTerminada;
};

void ordenarRanking(vector<pair<pair<bool, string>, chrono::microseconds>> &rankingClientes) {
    sort(rankingClientes.begin(), rankingClientes.end(), [](const auto &a, const auto &b) {
        if (a.first.first != b.first.first) return a.first.first < b.first.first;
        return a.second < b.second;
    });
}

void mostrarResultados(const vector<pair<pair<bool, string>, chrono::microseconds>> rankingClientes, 
    string nombreClienteGanador, chrono::microseconds tiempoClienteGanador){
    cout << "\n=== RANKING DE CLIENTES ===" << endl;
    for(const auto &cliente : rankingClientes) {
        cout << "Cliente: " << cliente.first.second 
             << " - Adivinó: " << (cliente.first.first ? "Sí" : "No") 
             << " - Tiempo: " << chrono::duration_cast<chrono::milliseconds>(cliente.second).count() 
             << " ms" << endl;
    }
    cout << "\nCliente ganador: " << nombreClienteGanador 
         << " con un tiempo de " << chrono::duration_cast<chrono::milliseconds>(tiempoClienteGanador).count() 
         << " ms" << endl;  
}

int main(int argc, char *argv[]){
    signal(SIGINT, SIG_IGN);
    signal(SIGUSR1, manejador_seniales);
    signal(SIGUSR2, manejador_seniales);

    if (argc < 5){
        cout << "Parametros incompletos." << endl;
        mostrarAyuda();
        return 1;
    }

    if(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        mostrarAyuda();
        return 0;
    }

    verificarParametros(argc, argv);

    string rutaArchivo = argv[2];
    int cantidadIntentos = stoi(argv[4]);
    vector<string> frasesDisponibles = obtenerFrases(rutaArchivo);

    if (frasesDisponibles.empty()) {
        cerr << "No se pudieron obtener frases del archivo." << endl;
        return 1;
    }

    // Configuración de memoria compartida
    int idMemoria = shm_open(NOMBRE_MEMORIA, O_CREAT | O_RDWR, 0600);
    if (idMemoria == -1) {
        cerr << "Error al crear la memoria compartida." << endl;
        return 1;
    }

    if (ftruncate(idMemoria, sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida." << endl;
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    char *letrAAdivinar = (char *)mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0);
    if (letrAAdivinar == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida." << endl;
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    int idMemoriaNickname = shm_open("miMemoriaNickname", O_CREAT | O_RDWR, 0600);
    if (idMemoriaNickname == -1) {
        cerr << "Error al crear la memoria para nickname." << endl;
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    if (ftruncate(idMemoriaNickname, 20 * sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria para nickname." << endl;
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    char *nicknameCliente = (char *)mmap(NULL, 20 * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaNickname, 0);
    if (nicknameCliente == MAP_FAILED) {
        cerr << "Error al mapear la memoria para nickname." << endl;
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    int idMemoriaRespuesta = shm_open(NOMBRE_MEMORIA_RESPUESTA, O_CREAT | O_RDWR, 0600);
    if (idMemoriaRespuesta == -1) {
        cerr << "Error al crear la memoria para respuesta." << endl;
        munmap(nicknameCliente, 20 * sizeof(char));
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    if (ftruncate(idMemoriaRespuesta, sizeof(respuestaCliente)) == -1) {
        cerr << "Error al definir el tamaño de la memoria para respuesta." << endl;
        close(idMemoriaRespuesta);
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    respuestaCliente *respuesta = (respuestaCliente *)mmap(NULL, sizeof(respuestaCliente), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaRespuesta, 0);
    if (respuesta == MAP_FAILED) {
        cerr << "Error al mapear la memoria para respuesta." << endl;
        close(idMemoriaRespuesta);
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        munmap(nicknameCliente, 20 * sizeof(char));
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    // Configuración de semáforos
    sem_t *semaforoServidor = sem_open(NOMBRE_SEMAFORO_SERVIDOR, O_CREAT, 0600, 0);
    if (semaforoServidor == SEM_FAILED) {
        cerr << "Error al crear semáforo servidor" << endl;
        munmap(respuesta, sizeof(respuestaCliente));
        munmap(nicknameCliente, 20 * sizeof(char));
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoriaRespuesta);
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        return 1;
    }

    sem_t *semaforoCliente = sem_open(NOMBRE_SEMAFORO_CLIENTE, O_CREAT, 0600, 0);
    if (semaforoCliente == SEM_FAILED) {
        cerr << "Error al crear semáforo cliente" << endl;
        sem_close(semaforoServidor);
        munmap(respuesta, sizeof(respuestaCliente));
        munmap(nicknameCliente, 20 * sizeof(char));
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoriaRespuesta);
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        return 1;
    }

    sem_t *semaforoClienteUnico = sem_open(NOMBRE_SEMAFORO_CLIENTE_UNICO, O_CREAT, 0600, 1);
    if (semaforoClienteUnico == SEM_FAILED) {
        cerr << "Error al crear semáforo cliente único" << endl;
        sem_close(semaforoServidor);
        sem_close(semaforoCliente);
        munmap(respuesta, sizeof(respuestaCliente));
        munmap(nicknameCliente, 20 * sizeof(char));
        munmap(letrAAdivinar, sizeof(char));
        close(idMemoriaRespuesta);
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
        close(idMemoriaNickname);
        shm_unlink("miMemoriaNickname");
        close(idMemoria);
        shm_unlink(NOMBRE_MEMORIA);
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);
        return 1;
    }

    // Variables para ranking
    vector<pair<pair<bool, string>, chrono::microseconds>> rankingClientes;
    string nombreClienteGanador = "";
    chrono::microseconds tiempoClienteGanador(0);

    cout << "Servidor iniciado. Esperando cliente..." << endl;

    while(!finalizar_servidor) {
        // Esperar conexión de cliente
        sem_wait(semaforoServidor);
        
        // Verificar cliente único
        if(sem_trywait(semaforoClienteUnico) == -1) {
            cerr << "Ya hay un cliente conectado. Rechazando conexión." << endl;
            continue;
        }

        partida_en_curso = true;
        auto inicioPartida = chrono::high_resolution_clock::now();
        
        // Seleccionar frase aleatoria
        int indiceAleatorio = rand() % frasesDisponibles.size();
        string fraseAdivinar = frasesDisponibles[indiceAleatorio];
        string fraseAuxiliar = fraseAdivinar;
        
        // Eliminar espacios para la comparación
        fraseAuxiliar.erase(remove_if(fraseAuxiliar.begin(), fraseAuxiliar.end(), 
                            [](unsigned char c) { return isspace(c); }), fraseAuxiliar.end());
        
        cout << "Nueva partida iniciada. Frase: " << fraseAdivinar << endl;
        
        int intentosRestantes = cantidadIntentos;
        bool adivinado = false;

        // Bucle principal del juego
        while(intentosRestantes > 0 && !adivinado && !finalizar_servidor) {
            // Permitir que el cliente envíe una letra
            sem_post(semaforoCliente);
            
            // Esperar letra del cliente
            sem_wait(semaforoServidor);
            
            // Procesar letra
            char letra = *letrAAdivinar;
            bool letraCorrecta = false;
            
            // Buscar la letra en la frase
            auto it = find(fraseAuxiliar.begin(), fraseAuxiliar.end(), letra);
            if(it != fraseAuxiliar.end()) {
                letraCorrecta = true;
                fraseAuxiliar.erase(it);
                
                if(fraseAuxiliar.empty()) {
                    adivinado = true;
                    auto finPartida = chrono::high_resolution_clock::now();
                    auto duracion = chrono::duration_cast<chrono::microseconds>(finPartida - inicioPartida);
                    
                    rankingClientes.push_back({{true, string(nicknameCliente)}, duracion});
                    
                    if(duracion < tiempoClienteGanador || tiempoClienteGanador.count() == 0) {
                        tiempoClienteGanador = duracion;
                        nombreClienteGanador = string(nicknameCliente);
                    }
                }
            }
            
            intentosRestantes--;
            
            // Preparar respuesta
            respuesta->letraCorrecta = letraCorrecta;
            respuesta->intentosRestantes = intentosRestantes;
            respuesta->partidaTerminada = (adivinado || intentosRestantes == 0);
            
            // Enviar respuesta
            sem_post(semaforoCliente);
            
            // Si la partida terminó, esperar confirmación del cliente
            if(respuesta->partidaTerminada) {
                sem_wait(semaforoServidor);
                break;
            }
        }
        
        if(!adivinado && intentosRestantes == 0) {
            auto finPartida = chrono::high_resolution_clock::now();
            auto duracion = chrono::duration_cast<chrono::microseconds>(finPartida - inicioPartida);
            rankingClientes.push_back({{false, string(nicknameCliente)}, duracion});
        }
        
        partida_en_curso = false;
        sem_post(semaforoClienteUnico); // Permitir nuevo cliente
    }

    // Mostrar resultados antes de salir
    mostrarResultados(rankingClientes, nombreClienteGanador, tiempoClienteGanador);

    // Liberar recursos
    sem_close(semaforoServidor);
    sem_close(semaforoCliente);
    sem_close(semaforoClienteUnico);
    munmap(respuesta, sizeof(respuestaCliente));
    munmap(nicknameCliente, 20 * sizeof(char));
    munmap(letrAAdivinar, sizeof(char));
    close(idMemoriaRespuesta);
    close(idMemoriaNickname);
    close(idMemoria);
    shm_unlink(NOMBRE_MEMORIA_RESPUESTA);
    shm_unlink("miMemoriaNickname");
    shm_unlink(NOMBRE_MEMORIA);
    sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);

    cout << "Servidor finalizado correctamente." << endl;
    return 0;
}