#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "comun.h"

void R10();
void R12();
void R14();
void visualiza(int cola, int parada, int inout, int pintaborra, int destino);

int llega12 = 0, llega14 = 0, llega10 = 0, pidbus;

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int colagrafica, llega, sale, colaparada;
  int paramfifo;
  struct tipo_parada pasajero;
  char nombrefifo[10];
  int fifosalir, testigo = 1;
  struct ParametrosCliente params;

  srand(getpid());
  signal(10, R10); // me preparo para la senyal 10
  signal(12, R12); // me preparo para la senyal 12
  signal(14, R14); // me preparo para SIGALARM

  // Creamos y abrimos la cola de mensajes
  colagrafica = crea_cola(ftok("./fichcola.txt", 18));
  colaparada = crea_cola(ftok("./fichcola.txt", 20));

  // Abrimos la fifo de parametros de cliente
  paramfifo = open("fifoclienteparam", O_RDONLY);
  if (paramfifo == -1)
    perror("Error al abrir fifo cliente param");
  read(paramfifo, &params, sizeof(params));
  read(paramfifo, &pidbus, sizeof(pidbus));
  close(paramfifo);

  // Generamos aleatoriamente la parada de llegada y la de salida
  srand(getpid());
  llega = rand() % (params.numparadas) + 1;
  sale = rand() % (params.numparadas) + 1;
  // Si la parada de llegada es la misma que la de salida, generamos otra
  while (llega == sale)
    sale = rand() % (params.numparadas) + 1;

  // Abrimos la fifo de la parada de salida
  snprintf(nombrefifo, 10, "fifo%d", sale);
  fifosalir = open(nombrefifo, O_RDONLY);
  if (fifosalir == -1)
    perror("Error al abrir la fifo de parada");

  // Nos pintamos en la parada
  visualiza(colagrafica, llega, IN, PINTAR, sale);
  // Enviamos la peticion a la cola de paradas
  pasajero.tipo = llega;
  pasajero.pid = getpid();
  pasajero.destino = sale;
  if (msgsnd(colaparada, (struct tipo_parada *)&pasajero,
             sizeof(struct tipo_parada) - sizeof(long), 0) == -1)
    perror("Error al escribir en la cola de paradas");

  // Programamos la alarma para decidir cuando nos aburrimos y nos vamos andando
  alarm(rand() % (params.aburrimientomax - params.aburrimientomin + 1) +
        params.aburrimientomin);
  // Espero a que el bus me recoja, cuando me recoja el bus me manda la señal 12
  if (!llega12)
    pause();

  // Comprobamos si realmente ha llegado la señal 12 desde el bus, o la 14 para
  // aburrirme
  if (llega12) {
    // desactivo la alarma, porque el bus me recoge
    alarm(0);

    llega12 = 0;
    visualiza(colagrafica, llega, IN, BORRAR, sale);
    visualiza(colagrafica, 0, 0, PINTAR, sale);
    // Espero a que el bus me deje en la parada de salida, cuando tenga el
    // testigo en la fifo de salida
    if (read(fifosalir, &testigo, sizeof(testigo)) <= 0)
      perror("Error al leer la fifo de salida");
    visualiza(colagrafica, 0, 0, BORRAR, sale);
    visualiza(colagrafica, sale, OUT, PINTAR, sale);
  } else // llego la 14
  {
    visualiza(colagrafica, llega, IN, BORRAR, sale);
    visualiza(colagrafica, 7, OUT, PINTAR,
              sale); // La 7 es la acera

    // Como mis datos stan en la cola, espero que me avise el
    // bus, para decirle que no me voy a montar
    if (!llega12)
      pause();
  }

  // cerramos la fifo de salida
  close(fifosalir);
  return 0;
}

/************************************************************************/
/***********   FUNCION: visualiza     ***********************************/
/************************************************************************/
// Pinta o borra en el servidor gráfico

void visualiza(int cola, int parada, int inout, int pintaborra, int destino) {
  struct tipo_elemento peticion;

  peticion.tipo = 2; // Los clientes son tipo 2, el autobus tipo 1
  peticion.pid = getpid();
  peticion.parada = parada;
  peticion.inout = inout;
  peticion.pintaborra = pintaborra;
  peticion.destino = destino;

  if (msgsnd(cola, (struct tipo_elemento *)&peticion,
             sizeof(struct tipo_elemento) - sizeof(long), 0) == -1)
    perror("Error al enviar a la cola de mensajes del servidor");

  if (pintaborra == PINTAR) {
    if (!llega10)
      pause(); // espero conformidad de que me han pintado, sino me mataran
    llega10 = 0;
  }
}

/************************************************************************/
/***********    FUNCION: R10     ****************************************/
/************************************************************************/

void R10() { llega10 = 1; }

/************************************************************************/
/***********    FUNCION: R14     ****************************************/
/************************************************************************/

void R14() { llega14 = 1; }

/************************************************************************/
/***********    FUNCION: R12     ****************************************/
/************************************************************************/
void R12() {
  if (llega14) {
    // Aviso al bus de que no voy a montarme
    if (kill(pidbus, 5) == -1)
      perror("Error al enviar señal al bus");
  } else {
    // Aviso al bus de que me voy a montar
    if (kill(pidbus, 6) == -1)
      perror("Error al enviar señal al bus");
  }
  llega12 = 1;
}
