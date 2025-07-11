/*                GRUPO 2
 INTEGRANTES
   AGUIRRE, SEBASTIAN HERNAN
   DE LA CRUZ, LEANDRO ARIEL
   JUCHANI CALLAMULLO, JAVIER ANDRES
   LOIOTILE, JUAN CRUZ
   RIVAS, NAHUEL ALBERTO
*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <string.h>
#include <filesystem>
#include <signal.h>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <set>
#include <vector>
#include <chrono>

using namespace std;

#define NombreMemoria "memoriaAhorcado"
#define NombreCrono "memoriaCrono"
#define SemServidor "semServidor"
#define SemCliente "semCliente"
#define SemResponder "semResponder"
#define SemRespuesta "semRespuesta"
#define SemServerUnico "semServerUnico"
#define SemQuieroJugar "semQuieroJugar"
#define SemPartida "semPartida"

struct Jugador {
    std::string nombre;
    std::chrono::duration<double> tiempoTotal;
    unsigned cantidadAciertos;
    unsigned cantidadIntentos;
};

std::ifstream archFrases;
atomic<bool> partidaEnProgreso(false);
int* memoria = nullptr;
double* crono = nullptr;
size_t tam = 1024;
pid_t pidClienteActual;
std::vector<Jugador> jugadores;
sem_t* semServidor = sem_open(SemServidor, O_CREAT, 0600, 0);
sem_t* semCliente = sem_open(SemCliente, O_CREAT, 0600, 0);
sem_t* semRespuesta = sem_open(SemRespuesta, O_CREAT, 0600, 0); 
sem_t* semResponder = sem_open(SemResponder, O_CREAT, 0600, 0); 
sem_t* semServerUnico = sem_open(SemServerUnico, O_CREAT, 0600, 1);
sem_t* semQuieroJugar = sem_open(SemQuieroJugar, O_CREAT, 0600, 0);

void procesarParametros(int argc, char* argv[], int& cantIntentos, string& pathArch)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-a | --archivo ./pepe.txt o /path1/path2/pepe.txt / -c | --cantidad 5" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if(argc < 5)
    {
        cout << "La cantidad de parametros es insuficiente, revise la ayuda con -h | --help si lo necesita." << endl;
        exit(1);
    }

    for(int i = 1; i < argc; i++)
    {
        if(string(argv[i]) == "-a" || string(argv[i]) == "--archivo")
        {
            if(i + 1 < argc)
            {
                pathArch = argv[++i];
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -a | --archivo." << endl;
                exit(1);
            }
        }

        if(string(argv[i]) == "-c" || string(argv[i]) == "--cantidad")
        {
            if(i + 1 < argc)
            {
                cantIntentos = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -c | --cantidad." << endl;
                exit(1);
            }
        }

    }
}

bool comparar(const Jugador& a, const Jugador& b)
{
    if (a.cantidadAciertos != b.cantidadAciertos) {
        return a.cantidadAciertos > b.cantidadAciertos; // Más aciertos primer criterio
    }
    if (a.tiempoTotal != b.tiempoTotal) {
        return a.tiempoTotal < b.tiempoTotal; // Menor tiempo segundo criterio
    }
    return a.cantidadIntentos < b.cantidadIntentos; // Menos intentos en caso de empate
}

void mostrarResultados()
{
    // ordenar jugadores
    std::sort(jugadores.begin(), jugadores.end(), comparar);
    unsigned i = 1;

    cout << "\n\n------------ RESULTADOS ------------\n\n" << endl;
    
    for (const auto& jugador : jugadores)
    {
        std::cout << i++ << ") " << jugador.nombre << " - Cantidad de aciertos: " << jugador.cantidadAciertos << " - Tiempo: " << jugador.tiempoTotal.count() << " - Intentos: " << jugador.cantidadIntentos << std::endl;
    }

}

void finalizarUsoRecursos()
{
    // Cerrar y unlinkear semáforos
    sem_close(semCliente);
    sem_close(semServidor);
    sem_close(semResponder);
    sem_close(semRespuesta);
    sem_close(semServerUnico);
    sem_close(semQuieroJugar);
    
    sem_unlink(SemServidor);
    sem_unlink(SemCliente);
    sem_unlink(SemResponder);
    sem_unlink(SemRespuesta);
    sem_unlink(SemServerUnico);
    sem_unlink(SemQuieroJugar);
    sem_unlink(SemPartida);

    // Desmapear y cerrar la memoria compartida
    munmap(memoria, tam);
    munmap(crono, sizeof(double));
    shm_unlink(NombreMemoria);
    shm_unlink(NombreCrono);

    kill(pidClienteActual, SIGUSR2);
}

void manejadorSenial(int senial)
{
    //! Cerrar cosas (UNLINKEAR TODOS LOS SEMAFOROS, CERRAR ARCHIVOS Y UNLINKEAR MEMORIA).
    if (senial == SIGUSR1)
    {
        if (!partidaEnProgreso.load())  // Si no hay partida en progreso
        {
            cout << "\n\nRecibida señal SIGUSR1. No hay partidas en progreso, finalizando servidor." << endl;
            mostrarResultados();
            finalizarUsoRecursos();
            exit(0);  // Finalizar el proceso servidor
        }
        else
        {
            cout << "\n\nRecibida señal SIGUSR1, pero hay una partida en progreso. No se puede finalizar." << endl;
        }
    }

    if (senial == SIGUSR2)
    {
        cout << "\n\nRecibida señal SIGUSR2, finalizando servidor." << endl;
        sleep(2);
        mostrarResultados();
        finalizarUsoRecursos();
        exit(0);  // Finalizar el proceso servidor
    }
}

int contarLetras(const std::string& frase) {
    int cantidad = 0;

    for (size_t i = 0; i < frase.length(); ++i) {
        if (std::isalpha(static_cast<unsigned char>(frase[i])))
            cantidad++;
    }

    return cantidad;
}

void inicializarResCLIConEspacios(const std::string& resOK, std::string& resCLI) {
    for (size_t i = 0; i < resOK.length(); ++i) {
        if(isalpha(resOK[i]) && (resOK[i] != ' '))
            resCLI += "_ ";
        else if(resOK[i] == ' ')
        {
            resCLI += "  ";
        }
        else
        {
            resCLI += resOK[i];
            resCLI += " ";
        }
    }
}

int actualizarResCLI(std::string& resCLI, const std::string& resOK, char caracterRecibido) {
    int cambio = 0;

    for (size_t i = 0; i < resOK.length(); ++i) {
        if (std::toupper(resOK[i]) == std::toupper(caracterRecibido)) {
            if(i != 0)
                resCLI[i*2] = resOK[i];
            else
                resCLI[i] = resOK[i];
            
            cambio++;
        }
    }

    return cambio;
}

string obtenerFraseRandom(std::ifstream& archivo, std::string& buffer)
{
    // Primera lectura general contando cuántas líneas tiene el archivo
    size_t totalLineas = 0;
    std::string linea;
    while (getline(archivo, linea))
    {
        ++totalLineas;
    }

    if (totalLineas == 0) {
        cout << "El archivo de frases está vacío." << endl;
        return "";
    }

    // Generamos un índice aleatorio
    std::srand(std::time(nullptr));
    size_t lineaObjetivo = std::rand() % totalLineas;

    // Segunda pasada para obtener una línea al azar
    archivo.clear(); // limpiar flags de EOF
    archivo.seekg(0, std::ios::beg); // volver al inicio

    buffer.clear();
    size_t actual = 0;
    while (getline(archivo, buffer) && (actual != lineaObjetivo))
    {
        ++actual;
    }

    return buffer;
}

int main(int argc, char* argv[])
{   
    signal(SIGUSR1, manejadorSenial);
    signal(SIGUSR2, manejadorSenial);
    signal(SIGINT, SIG_IGN);
    
    string buffer;
    string resOK;
    string resCLI;
    size_t longitud;
    string pathArch;
    int cantIntentos;
    int cantMaxIntentos;
    int pos;
    int cantAciertosRestantes;
    std::set<char> letrasUsadas;
    Jugador jugActual;

    procesarParametros(argc, argv, cantIntentos, pathArch);

    cantMaxIntentos = cantIntentos;

    filesystem::path absPathArch = filesystem::absolute(pathArch);

    if(!filesystem::exists(absPathArch))
    {
        cout << "El directorio del archivo de preguntas no existe." << endl;
        finalizarUsoRecursos();
    }

    int idMemoria = shm_open(NombreMemoria, O_CREAT | O_RDWR, 0600); // Creo o abro una referencia al espacio de memoria que quiero usar.
    int idCrono = shm_open(NombreCrono, O_CREAT | O_RDWR, 0600);
 
    ftruncate(idMemoria, tam); // Le asigno su tamaño.
    ftruncate(idCrono, sizeof(double));

    memoria = (int*)mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0); // Mapeamos la memoria a nuestro proceso.
    crono = (double*)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, idCrono, 0);

    close(idMemoria);
    close(idCrono);

    if(sem_trywait(semServerUnico) != 0)
    {
        cout << "Ya existe un proceso servidor corriendo en la computadora." << endl;
        finalizarUsoRecursos();
    }
    // Hasta aca tengo solamente 1 sevidor.

    archFrases.open(absPathArch);

    if (!archFrases.is_open()) {
        cout << "No se pudo abrir el archivo de frases." << endl;
        finalizarUsoRecursos();
    }

    if((obtenerFraseRandom(archFrases, buffer)).empty())
    {
        cout << "Error inesperado al obtener una frase." << endl;
        finalizarUsoRecursos();
    }

    if((pos = buffer.find('\n')) != std::string::npos)
        buffer[pos] = '\0';

    archFrases.close();

    resOK = buffer;

    while(true)
    {
        letrasUsadas.clear();
        sem_wait(semQuieroJugar); //! Me quedo trabado en quiero jugar.
        sem_post(semCliente); // informo que estoy listo para recibir el pid
        sem_wait(semServidor); // espero el pid del cliente ante eventualidades
        pidClienteActual = *memoria;
        cout << "\n\nPID del cliente actual: " << pidClienteActual << endl;
        sem_post(semCliente); // confirmo que lo recibí
        *memoria = cantMaxIntentos;
        sem_post(semCliente); //! Devuelvo un cliente V(CLIENTE).
        sem_wait(semServidor); //! Me quedo trabado aca. Como tengo 0, se traba. | Me devuelven 2 pero consumo 1.

        partidaEnProgreso.store(true);

        sleep(2);
        cantAciertosRestantes = contarLetras(resOK);
        *memoria = cantAciertosRestantes;
        cantIntentos = cantMaxIntentos;
        resCLI.clear();
        inicializarResCLIConEspacios(resOK, resCLI);
        sem_post(semCliente); // Habilito a que el cliente consuma la cantidad de aciertos que conllevará la frase
        
        while((cantIntentos > 0) && cantAciertosRestantes)
        {
            cout << "resOK: " << resOK << endl;
            cout << "resCLI: " << resCLI << endl;
            sem_wait(semServidor); // Pido el servidor. -> 1 a 0. || Se espera que el cliente solicite info

            longitud = resCLI.length();
            memcpy(memoria, &longitud, sizeof(longitud)); 
            memcpy(memoria + sizeof(longitud), resCLI.c_str(), longitud + 1);
            sleep(1);
            sem_post(semCliente); //! Devuelvo el cliente -> 0 a 1.
            sem_post(semResponder);

            sem_wait(semRespuesta); // Va a bloquearse porque quien responde es el cliente.
            
            char* res = new char[tam];
            memcpy(&longitud, memoria, sizeof(longitud));
            res[longitud] = '\0';
            memcpy(res, memoria + sizeof(longitud), longitud);

            cout << "Respuesta: " << resOK << " Res: " << (string)res << endl;

            if (!((string)res).empty())
                *res = std::toupper(static_cast<unsigned char>(*res));
            
            if((letrasUsadas.insert(*res)).second)
            {
                *memoria = actualizarResCLI(resCLI, resOK, *res); // guardamos la cantidad de aciertos que tuvo con la letra que respondió
            }
            else
            {
                *memoria = 0; // repitió un caracter, no acertó nada nuevo
            }

            sem_post(semCliente); // Habilito a que el cliente reciba si fue un acierto o no  (1)
            cantAciertosRestantes -= *memoria;

            cout << "\nAciertos para la respuesta actual: " << *memoria << endl;

            sem_wait(semServidor); // espero a que actualice su cantidad de intentos restantes  (2)

            if(!(*memoria)) // si no tuvo aciertos
                cantIntentos--;

            sleep(2);
            *memoria = cantAciertosRestantes; // Comunicamos al proceso cliente la cantidad restante de letras a adivinar
            sem_post(semCliente);
            sem_wait(semServidor); // Esperamos a que lo reciba correctamente, así no ' ensuciar ' la memoria compartida con otros valores

            cout << "resCLI: " << resCLI << endl;
        }
        
        sem_wait(semServidor); // espero a que el cliente esté listo para recibir los resultados

        // informo cómo terminó todo
        longitud = resOK.length();
        memcpy(memoria, &longitud, sizeof(longitud)); 
        memcpy(memoria + sizeof(longitud), resOK.c_str(), longitud + 1);
        sem_post(semCliente);
        sleep(1);
        sem_wait(semServidor);

        buffer.clear();

        longitud = resCLI.length();
        memcpy(memoria, &longitud, sizeof(longitud)); 
        memcpy(memoria + sizeof(longitud), resCLI.c_str(), longitud + 1);
        sem_post(semCliente);
        sleep(1);
        sem_wait(semServidor);

        buffer.clear();

        cout << "\nFinalizando la partida actual..." << endl;

        // recibo los datos del jugador
        jugActual.nombre.clear();
        sem_wait(semServidor); // espero el nombre (1)
        memcpy(&longitud, memoria, sizeof(longitud));
        char* nombre = new char[longitud];
        // memcpy(jugActual.nombre, memoria + sizeof(longitud), longitud);
        memcpy(nombre, memoria + sizeof(longitud), longitud);
        nombre[longitud] = '\0';
        jugActual.nombre = nombre;
        cout << "\nNombre del jugador: " << jugActual.nombre << endl;
        sem_post(semCliente); // confirmo lectura nombre (2)
        sleep(2);
        sem_wait(semServidor); // espero el tiempo (3)
        double tiempo = *crono;
        // memcpy(&tiempo, memoria, sizeof(tiempo));
        jugActual.tiempoTotal = std::chrono::duration<double>(tiempo);
        cout << "\nTiempo total en responder: " << jugActual.tiempoTotal.count() << endl;
        sem_post(semCliente); // confirmo lectura tiempo (4)
        sleep(2);
        sem_wait(semServidor); // espero la cant de aciertos (5)
        jugActual.cantidadAciertos = *memoria;
        cout << "\nCantidad de aciertos: " << jugActual.cantidadAciertos << endl;
        sem_post(semCliente); // confirmo lectura cant de aciertos (6)
        sleep(2);
        sem_wait(semServidor); // espero la cant de intentos (7)
        jugActual.cantidadIntentos = *memoria;
        cout << "\nCantidad de intentos: " << jugActual.cantidadIntentos << endl;
        sem_post(semCliente); // confirmo lectura cant de intentos (8)
        sleep(2);

        jugadores.push_back(jugActual);

        sem_wait(semServidor);
        sem_wait(semServidor);
        
        memset(memoria, 0, tam);
        memset(crono, 0, sizeof(double));

        partidaEnProgreso.store(false);
    }

    //! Hacer en el signalHandler.

    sem_close(semCliente);
    sem_close(semServidor);
    sem_close(semResponder);
    sem_close(semRespuesta);
    sem_close(semServerUnico);
    sem_close(semQuieroJugar);
    munmap(memoria, tam);
    munmap(crono, sizeof(double));

    return 0;

}
