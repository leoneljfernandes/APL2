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

using namespace std;

#define NOMBRE_SEMAFORO_SERVIDOR "semaforoServidor"
#define NOMBRE_SEMAFORO_CLIENTE "semaforoCliente"
#define NOMBRE_SEMAFORO_CLIENTE_UNICO "semaforoClienteUnico"
#define NOMBRE_MEMORIA "miMemoria"
#define NOMBRE_MEMORIA_RESPUESTA "miMemoriaRespuesta"

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

            if(stoi(valor) < 1) {
                throw runtime_error("La cantidad de intentos debe ser mayor a 0");
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
};



//ordenar ranking de clientes por bool true primero y luego por tiempo
void ordenarRanking(vector<pair<pair<bool, string>, chrono::microseconds>> &rankingClientes) {
    sort(rankingClientes.begin(), rankingClientes.end(), [](const auto &a, const auto &b) {
        if (a.first.first != b.first.first) {
            return a.first.first < b.first.first; // Ordenar por si adivinó o no
        }
        return a.second < b.second; // Si ambos adivinaron, ordenar por tiempo
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

    // Configurar manejo de señales
    signal(SIGINT, SIG_IGN);  // Ignorar SIGINT
    signal(SIGUSR1, manejador_seniales);
    signal(SIGUSR2, manejador_seniales);

    if (argc < 5){
        cout << "Parametros incompletos." << endl;
        mostrarAyuda();
        return 1;
    }

    if(argv[1] == "-h" || argv[1] == "--help") {
        mostrarAyuda();
        return 0;
    }

    verificarParametros(argc, argv);

    string rutaArchivo = argv[2];
    int cantidadIntentos = stoi(argv[4]);

    // leo archiv y obtengo frases
    vector<string> frasesDisponibles = obtenerFrases(rutaArchivo);

    if (frasesDisponibles.empty()) {
        cerr << "No se pudieron obtener frases del archivo." << endl;
        return 1;
    }

    // CREO LAS MEMORIAS COMPARTIDAS
    int idMemoria = shm_open(NOMBRE_MEMORIA, O_CREAT | O_RDWR, 0600);
    if (idMemoria == -1) {
        cerr << "Error al crear la memoria compartida." << endl;
        return 1;
    }

    if (ftruncate(idMemoria, sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida." << endl;
        close(idMemoria);  // Cerrar descriptor antes de shm_unlink
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    char *letrAAdivinar = (char *)mmap(NULL, sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0);
    if (letrAAdivinar == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida." << endl;
        close(idMemoria);  // Cerrar descriptor antes de shm_unlink
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    int idMemoriaNickname = shm_open("miMemoriaNickname", O_CREAT | O_RDWR, 0600);
    if (idMemoriaNickname == -1) {
        cerr << "Error al crear la memoria compartida para el nickname." << endl;
        munmap(letrAAdivinar, sizeof(char));  // Desmapear primero
        close(idMemoria);  // Luego cerrar descriptor
        shm_unlink(NOMBRE_MEMORIA);
        return 1;
    }

    if (ftruncate(idMemoriaNickname, 20 * sizeof(char)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida para el nickname." << endl;
        close(idMemoriaNickname);  // Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");  // Eliminar memoria de nickname
        munmap(letrAAdivinar, sizeof(char));  // Desmapear letra
        close(idMemoria);  // Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);  // Eliminar memoria de letra
        return 1;
    }

    char *nicknameCliente = (char *)mmap(NULL, 20 * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaNickname, 0);
    if (nicknameCliente == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida para el nickname." << endl;
        close(idMemoriaNickname);  // Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");  // Eliminar memoria de nickname
        munmap(letrAAdivinar, sizeof(char));  // Desmapear letra
        close(idMemoria);  // Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);  // Eliminar memoria de letra
        return 1;
    }

    int idMemoriaRespuesta = shm_open(NOMBRE_MEMORIA_RESPUESTA, O_CREAT | O_RDWR, 0600);
    if (idMemoriaRespuesta == -1) {
        cerr << "Error al crear la memoria compartida para la respuesta." << endl;
        munmap(nicknameCliente, 20 * sizeof(char));  // Desmapear nickname
        close(idMemoriaNickname);  // Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");  // Eliminar memoria de nickname
        munmap(letrAAdivinar, sizeof(char));  // Desmapear letra
        close(idMemoria);  // Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);  // Eliminar memoria de letra
        return 1;
    }

    if (ftruncate(idMemoriaRespuesta, sizeof(respuestaCliente)) == -1) {
        cerr << "Error al definir el tamaño de la memoria compartida para la respuesta." << endl;
        close(idMemoriaRespuesta);  // Cerrar descriptor de respuesta
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);  // Eliminar memoria de respuesta
        munmap(nicknameCliente, 20 * sizeof(char));  // Desmapear nickname
        close(idMemoriaNickname);  // Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");  // Eliminar memoria de nickname
        munmap(letrAAdivinar, sizeof(char));  // Desmapear letra
        close(idMemoria);  // Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);  // Eliminar memoria de letra
        return 1;
    }

    respuestaCliente *respuesta = (respuestaCliente *)mmap(NULL, sizeof(respuestaCliente), PROT_READ | PROT_WRITE, MAP_SHARED, idMemoriaRespuesta, 0);
    if (respuesta == MAP_FAILED) {
        cerr << "Error al mapear la memoria compartida para la respuesta." << endl;
        close(idMemoriaRespuesta);  // Cerrar descriptor de respuesta
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);  // Eliminar memoria de respuesta
        munmap(nicknameCliente, 20 * sizeof(char));  // Desmapear nickname
        close(idMemoriaNickname);  // Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");  // Eliminar memoria de nickname
        munmap(letrAAdivinar, sizeof(char));  // Desmapear letra
        close(idMemoria);  // Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);  // Eliminar memoria de letra
        return 1;
    }

    // creo semaforos
    sem_t *semaforoServidor = sem_open(NOMBRE_SEMAFORO_SERVIDOR, O_CREAT, 0600, 1);
    if (semaforoServidor == SEM_FAILED) {
        cerr << "Error al crear el semaforo servidor" << endl;
        
        // Liberar recursos en orden inverso a su creación
        munmap(respuesta, sizeof(respuestaCliente));          // 1. Desmapear respuesta
        munmap(nicknameCliente, 20 * sizeof(char));           // 2. Desmapear nickname
        munmap(letrAAdivinar, sizeof(char));                  // 3. Desmapear letra
        close(idMemoriaRespuesta);                            // 4. Cerrar descriptor de respuesta
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);                // 5. Eliminar memoria de respuesta
        close(idMemoriaNickname);                            // 6. Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");                     // 7. Eliminar memoria de nickname
        close(idMemoria);                                    // 8. Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);                          // 9. Eliminar memoria de letra
        // Si el sem_open falló, no necesitamos sem_close, pero sí sem_unlink si O_CREAT estaba activo
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);               // 10. Eliminar semáforo (por si quedó algún remanente)
        return 1;
    }

    sem_t *semaforoCliente = sem_open(NOMBRE_SEMAFORO_CLIENTE, O_CREAT, 0600, 1);
    if (semaforoCliente == SEM_FAILED) {
        cerr << "Error al crear el semáforo del cliente." << endl;
        
        // Liberar recursos en orden inverso a su creación
        sem_close(semaforoServidor);  // 1. Cerrar semáforo del servidor

        munmap(respuesta, sizeof(respuestaCliente));          // 1. Desmapear respuesta
        munmap(nicknameCliente, 20 * sizeof(char));           // 2. Desmapear nickname
        munmap(letrAAdivinar, sizeof(char));                  // 3. Desmapear letra
        close(idMemoriaRespuesta);                            // 4. Cerrar descriptor de respuesta
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);                // 5. Eliminar memoria de respuesta
        close(idMemoriaNickname);                            // 6. Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");                     // 7. Eliminar memoria de nickname
        close(idMemoria);                                    // 8. Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);                          // 9. Eliminar memoria de letra
        
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);                 // 10. Eliminar semáforo del cliente
        return 1;
    }

    sem_t *semaforoClienteUnico = sem_open(NOMBRE_SEMAFORO_CLIENTE_UNICO, O_CREAT, 0600, 0);
    if (semaforoClienteUnico == SEM_FAILED) {
        cerr << "Error al crear el semáforo del cliente." << endl;
        
        // Liberar recursos en orden inverso a su creación
        sem_close(semaforoServidor);  // 1. Cerrar semáforo del servidor
        sem_close(semaforoCliente);   // 2. Cerrar semáforo del cliente

        munmap(respuesta, sizeof(respuestaCliente));          // 1. Desmapear respuesta
        munmap(nicknameCliente, 20 * sizeof(char));           // 2. Desmapear nickname
        munmap(letrAAdivinar, sizeof(char));                  // 3. Desmapear letra
        close(idMemoriaRespuesta);                            // 4. Cerrar descriptor de respuesta
        shm_unlink(NOMBRE_MEMORIA_RESPUESTA);                // 5. Eliminar memoria de respuesta
        close(idMemoriaNickname);                            // 6. Cerrar descriptor de nickname
        shm_unlink("miMemoriaNickname");                     // 7. Eliminar memoria de nickname
        close(idMemoria);                                    // 8. Cerrar descriptor de letra
        shm_unlink(NOMBRE_MEMORIA);                          // 9. Eliminar memoria de letra
        
        sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
        sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO);                 // 10. Eliminar semáforo del cliente
        return 1;
    }

   

    // Ranking cliente
    vector<pair<pair<bool, string>, chrono::microseconds>> rankingClientes;
    string nombreClienteGanador = "";
    chrono::microseconds tiempoClienteGanador(0);

    // bucle principal del servidor
    cout << "Servidor iniciado. Esperando cliente." << endl;
    while(!finalizar_servidor){
   

        sem_wait(semaforoServidor); // Espero a que un cliente se conecte

        partida_en_curso = true;

        if(finalizar_servidor){
            cout << "Servidor finalizado por señal." << endl;
            break;
        }

        // verificar que haya solo un cliente conectado
        if(sem_trywait(semaforoClienteUnico) == -1){
            cerr << "Ya hay un cliente conectado." << endl;
            sem_post(semaforoServidor); // Libero el semáforo del servidor
            continue; // Vuelvo al inicio del bucle
        }

        int indiceAleatorio = rand() % frasesDisponibles.size();
        string fraseAdivinar = frasesDisponibles[indiceAleatorio];
        string fraseAdivinarAUsar = fraseAdivinar;
        int intentosUtilizados = 0;
        bool adivinoFrase = false;
        auto inicioPartida = chrono::high_resolution_clock::now();
        
        // remuevo espacios en blanco
        fraseAdivinarAUsar.erase(std::remove_if(fraseAdivinarAUsar.begin(), 
            fraseAdivinarAUsar.end(), [](unsigned char c) { return std::isspace(c); }), fraseAdivinarAUsar.end());
        
        while((intentosUtilizados < cantidadIntentos) && !adivinoFrase && !finalizar_servidor){

            char letraRecibida = *letrAAdivinar;
            bool letraCorrecta = false;

            if(fraseAdivinarAUsar.find(letraRecibida)!= string::npos){
                letraCorrecta = true;
                cout << "El cliente adivino una letra: " << letraRecibida << endl;
                fraseAdivinarAUsar.erase(std::remove(fraseAdivinarAUsar.begin(), fraseAdivinarAUsar.end(), letraRecibida), fraseAdivinarAUsar.end());

                respuesta->letraCorrecta = letraCorrecta;
                respuesta->intentosRestantes = cantidadIntentos - (intentosUtilizados + 1);

                if(fraseAdivinarAUsar.empty()){
                    auto finPartida = chrono::high_resolution_clock::now();
                    auto duracionPartida = chrono::duration_cast<chrono::microseconds>(finPartida - inicioPartida);
                    cout << "El cliente adivino la frase: " << fraseAdivinar << endl;
                    if(duracionPartida < tiempoClienteGanador || tiempoClienteGanador == chrono::microseconds(0)){
                        nombreClienteGanador = string(nicknameCliente);
                        tiempoClienteGanador = duracionPartida;
                    }
                    adivinoFrase = true;
                    rankingClientes.push_back(make_pair(make_pair(adivinoFrase,string(nicknameCliente)), duracionPartida));
                }
                
                intentosUtilizados++;

            }else{
                cout << "El cliente no adivino la letra: " << letraRecibida << endl;
                respuesta->letraCorrecta = letraCorrecta;
                respuesta->intentosRestantes = cantidadIntentos - (intentosUtilizados + 1);
                intentosUtilizados++;
            }
        }

        partida_en_curso = false; // La partida ha terminado

        if(!adivinoFrase){
            auto finPartida = chrono::high_resolution_clock::now();
            auto duracionPartida = chrono::duration_cast<chrono::microseconds>(finPartida - inicioPartida);
            rankingClientes.push_back(make_pair(make_pair(adivinoFrase,string(nicknameCliente)), duracionPartida));
        }

        if(finalizar_servidor){ // termina por el sigusr2
            cout << "Servidor finalizado por señal." << endl;
            mostrarResultados(rankingClientes, nombreClienteGanador, tiempoClienteGanador);
            break;
        }
        
        sem_post(semaforoClienteUnico); // Libero el semáforo del cliente único
        sem_post(semaforoCliente);
    }

    // limpiar todos los recursos de memoria y semaforos
    sem_close(semaforoServidor);
    sem_close(semaforoCliente);
    sem_close(semaforoClienteUnico);
    munmap(respuesta, sizeof(respuestaCliente));          // Desmapear respuesta
    munmap(nicknameCliente, 20 * sizeof(char));           // Desmapear nickname
    munmap(letrAAdivinar, sizeof(char));                  // Desmapear letra
    close(idMemoriaRespuesta);                            // Cerrar descriptor de respuesta
    shm_unlink(NOMBRE_MEMORIA_RESPUESTA);                // Eliminar memoria de respuesta
    close(idMemoriaNickname);                            // Cerrar descriptor de nickname
    shm_unlink("miMemoriaNickname");                     // Eliminar memoria de nickname
    close(idMemoria);                                    // Cerrar descriptor de letra
    shm_unlink(NOMBRE_MEMORIA);                          // Eliminar memoria de letra
    sem_unlink(NOMBRE_SEMAFORO_SERVIDOR);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE);
    sem_unlink(NOMBRE_SEMAFORO_CLIENTE_UNICO); // Eliminar semáforo del cliente único
    cout << "Servidor finalizado correctamente." << endl;

   return 0; 
}