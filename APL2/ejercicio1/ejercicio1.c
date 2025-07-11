#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]){

if(argc > 2) {
        printf("parametros invalidos\n");
        return 1;
}

if(argv[1] != NULL) {
        if(strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-h") == 0){
        printf("El proceso ejercicio1 crea una jerarquia de procesos\n");
        printf("ademas pide un caracter de entrada y hace que cada hijo necesario tambien lo haga\n");
        printf("visualizar la jerarquia mediante pstree, ademas corroborar que al terminar la ejecución de todos los procesos\n");
        printf("existe un proceso que quedo en modo Demonio viviendo por su cuenta, este debe ser terminado con kill pid\n");
        return 0;
        } else {
        printf("Parametros ingresados invalidos\n");
        return 1;
        }
}

pid_t pid_h1;

//creo el hijo 1
pid_h1 = fork();

//si soy el hijo 1
if(pid_h1 == 0){
        printf("soy el hijo 1, %d y mi padre es %d\n", getpid(),getppid());
        pid_t pid_n1;
        pid_t pid_n3;
        pid_t pid_zombie;

        //creo al nieto 1
        pid_n1 = fork();

        //si soy el nieto 1
        if(pid_n1 == 0) {
                printf("soy el nieto 1, %d y mi padre es %d\n", getpid(), getppid());
                getchar();
                return 0;
        }

        //creo al zombie
        pid_zombie = fork();

        //si soy el zombie
        if(pid_zombie == 0){
                printf("soy el hijo zombie, %d y mi padre es %d\n", getpid(), getppid());
                exit(0);
        }

        //creo al nieto 3
        pid_n3 = fork();

        if(pid_n3 == 0){
                printf("soy el nieto 3, %d y mi padre es %d\n",getpid(),getppid());
                getchar();
                return 0;
        }

        getchar();
        return 0;
}

pid_t pid_h2;

//creo al hijo 2
pid_h2 = fork();

//si soy el hijo 2
if(pid_h2 == 0) {
        pid_t pid_demon;

        //creo al demonio
        pid_demon = fork();

        //si soy el demonio
        if(pid_demon == 0) {
                printf("soy el demonio, %d y mi padre es %d\n", getpid(), getppid());
                while(1)
                {
			sleep(1);
                }

        }

        printf("soy el hijo 2, %d y mi padre es %d\n", getpid(),getppid());
        getchar();
	return 0;

}

printf("soy el padre, %d y mi padre es %d\n", getpid(),getppid());
getchar();

return 0;

}
