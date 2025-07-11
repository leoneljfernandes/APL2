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
#include <signal.h>
#include <chrono>

using namespace std;

#define NombreMemoria "memoriaAhorcado"
#define NombreCrono "memoriaCrono"
#define SemCliente "semCliente"
#define SemServidor "semServidor"
#define SemResponder "semResponder"
#define SemRespuesta "semRespuesta"
#define SemPartida "semPartida"
#define SemQuieroJugar "semQuieroJugar"

struct Jugador {
    std::string nombre;
    std::chrono::duration<double> tiempoTotal;
    unsigned cantidadAciertos;
    unsigned cantidadIntentos;
};

int* memoria = nullptr;
double* crono = nullptr;
size_t tam = 1024;
sem_t* semCliente = sem_open(SemCliente, O_CREAT, 0600, 0);
sem_t* semServidor = sem_open(SemServidor, O_CREAT, 0600, 0);
sem_t* semResponder = sem_open(SemResponder, O_CREAT, 0600, 0);
sem_t* semRespuesta = sem_open(SemRespuesta, O_CREAT, 0600, 0);
sem_t* semPartida = sem_open(SemPartida, O_CREAT, 0600, 1);
sem_t* semQuieroJugar = sem_open(SemQuieroJugar, O_CREAT, 0600, 0);

void procesarParametros(int argc, char* argv[], string& nickname)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-n | -nickname Pepe" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if(argc < 3)
    {
        cout << "La cantidad de parametros es insuficiente, revise la ayuda con -h | --help si lo necesita." << endl;
        exit(1);
    }

    for(int i = 1; i < argc; i++)
    {
        if(string(argv[i]) == "-n" || string(argv[i]) == "--nickname")
        {
            if(i + 1 < argc)
            {
                std::string nick(argv[++i]);
                nickname = nick;
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -n | --nickname." << endl;
                exit(1);
            }
        }
    }
}

void finalizarUsoRecursos()
{
    sem_close(semCliente);
    sem_close(semServidor);
    sem_close(semResponder);
    sem_close(semRespuesta);
    sem_close(semQuieroJugar);
    sem_close(semPartida);
    sem_unlink(SemPartida);
    munmap(memoria, tam);
    munmap(crono, sizeof(double));
}

void manejadorSenial(int senial)
{
    //! Cerrar cosas (UNLINKEAR TODOS LOS SEMAFOROS, CERRAR ARCHIVOS Y UNLINKEAR MEMORIA).
    if (senial == SIGUSR1)
    {
        cout << "\n\nRecibida señal SIGUSR1. Finalizando cliente." << endl;
        finalizarUsoRecursos();
    }
    if (senial == SIGUSR2)
    {
        cout << "\n\nRecibida señal SIGUSR2. Finalizando cliente." << endl;
        finalizarUsoRecursos();
    }

    exit(1);
}

int main(int argc, char* argv[])
{
    signal(SIGUSR1, manejadorSenial);
    signal(SIGUSR2, manejadorSenial);
    signal(SIGINT, SIG_IGN);
    
    pid_t pid = getpid();
    size_t longitud;
    string pregunta;
    string respuesta;
    unsigned cantIntentosRestantes;
    unsigned cantAciertosRestantes;
    Jugador jugActual;
    std::chrono::duration<double> duracion;
    std::chrono::high_resolution_clock::time_point inicio;
    std::chrono::high_resolution_clock::time_point fin;

    procesarParametros(argc, argv, jugActual.nombre);

    int idMemoria = shm_open(NombreMemoria, O_CREAT | O_RDWR, 0600);
    int idCrono = shm_open(NombreCrono, O_CREAT | O_RDWR, 0600);

    ftruncate(idMemoria, tam);
    ftruncate(idCrono, sizeof(double));

    memoria = (int*) mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0);
    crono = (double*) mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, idCrono, 0);

    close(idMemoria);
    close(idCrono);

    if(sem_trywait(semPartida) != 0)
    {
        cout << "Ya hay una partida en curso de otro jugador." << endl;
        exit(1);
    }

    cout << "¡Bievenida/o " << jugActual.nombre << "!" << endl;

    jugActual.tiempoTotal = std::chrono::duration<double>(0.0);
    jugActual.cantidadAciertos = 0;
    jugActual.cantidadIntentos = 0;
    sem_post(semQuieroJugar); //*Posteo quiero jugar. -> Destrabo al servidor.
    sem_wait(semCliente); // espero a que se me solicite el pid
    *memoria = pid;
    sem_post(semServidor); // informo que ya se puede leer pid
    sleep(1);
    sem_wait(semCliente); // espero a que se confirme lectura de pid
    sem_wait(semCliente); // espero recibir la cantidad de intentos totales permitidos
    cantIntentosRestantes = *memoria;
    cout << "\n\nTenés " << cantIntentosRestantes << " intentos para adivinar la frase, ¡Éxitos!" << endl;
    sem_post(semServidor);
    sem_post(semServidor); //* Destrabo el servidor.

    cantAciertosRestantes = *memoria;
    sem_wait(semCliente); // espero a recibir la cantidad de aciertos que conllevará la frase a adivinar

    while((cantIntentosRestantes > 0) && cantAciertosRestantes)
    {
        if(sem_trywait(semCliente) == 0)
        {
            cout << "\n\n----- Frase a adivinar  -----" << endl;

            memcpy(&longitud, memoria, sizeof(size_t));
            //cout << "Long: " << longitud << endl;
            char* buffer = new char[longitud + 1];
            memcpy(buffer, memoria + sizeof(longitud), longitud);
            buffer[longitud] = '\0';
            cout << buffer << endl;
            sem_post(semServidor);
            delete[] buffer;
        }
        else if(sem_trywait(semResponder) == 0)
        {
            cout << "\nIngresá una letra: " << endl;
            inicio = std::chrono::high_resolution_clock::now();
            getline(cin, respuesta);
            fin = std::chrono::high_resolution_clock::now();
            duracion = fin - inicio;
            jugActual.tiempoTotal += duracion;
            cout << "\nLetra ingresada: " << respuesta << endl;
            
            longitud = respuesta.length();
            memcpy(memoria, &longitud, sizeof(longitud));
            memcpy(memoria + sizeof(longitud), respuesta.c_str(), longitud);
            sem_post(semRespuesta); // desbloquea y sigue desde donde se almacena res en servidor
            sem_wait(semCliente); //! Espero a que se informe si fue un acierto o no  (1)
            if(!(*memoria))
                cantIntentosRestantes--;
            jugActual.cantidadAciertos += *memoria;
            cout << "\nCantidad de aciertos para tu respuesta actual: " << *memoria << endl;
            sem_post(semServidor); // confirmo que actualicé la cantidad de intentos  (2)
            sem_wait(semCliente); // Espero a que se informe la nueva cantidad de aciertos restantes  (3)
            cantAciertosRestantes = *memoria;
            cout << "\nCantidad de aciertos restantes: " << cantAciertosRestantes << endl;
            cout << "\nCantidad de intentos restantes: " << cantIntentosRestantes << endl;
            jugActual.cantidadIntentos++;
            sem_post(semServidor); // confirmo la lectura de la nueva cantidad de aciertos restantes
        }
    }

    sem_post(semServidor); // aviso que estoy listo para recibir resultados

    sem_wait(semCliente); // espero el informe de cómo terminó todo
    cout << "\n\n----- Respuesta esperada -----" << endl;

    memcpy(&longitud, memoria, sizeof(size_t));
    char* resEsp = new char[longitud + 1];
    memcpy(resEsp, memoria + sizeof(longitud), longitud);
    resEsp[longitud] = '\0';
    cout << resEsp << endl;
    sem_post(semServidor);
    delete[] resEsp;

    sem_wait(semCliente);
    cout << "\n\n----- Tu respuesta -----" << endl;

    memcpy(&longitud, memoria, sizeof(size_t));
    char* resFinal = new char[longitud + 1];
    memcpy(resFinal, memoria + sizeof(longitud), longitud);
    resFinal[longitud] = '\0';
    cout << resFinal << endl;
    sem_post(semServidor);
    delete[] resFinal;

    cout << "\n------ Tus estadísticas, " << jugActual.nombre << " ------" << endl;

    longitud = jugActual.nombre.length();
    memcpy(memoria, &longitud, sizeof(longitud));
    memcpy(memoria + sizeof(longitud), jugActual.nombre.c_str(), longitud);
    sem_post(semServidor); // informo el nombre (1)
    //sleep(1);
    sem_wait(semCliente); // espero a que confirme lectura nombre (2)
    double tiempo = jugActual.tiempoTotal.count();
    // memcpy(memoria, &tiempo, sizeof(tiempo));
    *crono = tiempo;
    cout << "\nTiempo total: " << jugActual.tiempoTotal.count() << endl;
    sem_post(semServidor); // informo tiempo (3)
    sleep(1);
    sem_wait(semCliente); // espero a que confirme lectura tiempo (4)
    *memoria = jugActual.cantidadAciertos;
    cout << "\nCantidad de aciertos: " << jugActual.cantidadAciertos << endl;
    sem_post(semServidor); // informo la cant de aciertos (5)
    sleep(1);
    sem_wait(semCliente); // espero a que confirme lectura cant de intentos (6)
    *memoria = jugActual.cantidadIntentos;
    cout << "\nCantidad de intentos: " << jugActual.cantidadIntentos << endl;
    sem_post(semServidor); // informo la cant de intentos (7)
    sleep(1);
    sem_wait(semCliente); // espero a que confirme lectura cant de intentos (8)

    // sem_post(semServidor);
    sem_post(semServidor); //* Destrabo el servidor.

    cout << "\nFin de la partida, ¡Muchas gracias por jugar!" << endl;

    finalizarUsoRecursos();

    return 0;
}
