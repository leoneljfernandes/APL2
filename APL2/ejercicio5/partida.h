/*
Integrantes del grupo:
- Berti Rodrigo
- Burnowicz Alejo
- Fernandes Leonel
- Federico Agustin
*/
#ifndef PARTIDA_H
#define PARTIDA_H
// Declare the thread function
void devolverPalabraJuego(char *destino, char *original);
int partida(char *palabra,char* recv_buffer,char* send_buffer,const int client_fd);
void devolverPalabraJuego(char *destino, char *original);
#endif
