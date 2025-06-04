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
void R15();
void visualiza(int cola, int parada, int inout, int pintaborra, int destino);

int llega12 = 0, llega10 = 0, baja = 0, pidbus;

int main() {
  int colagrafica, llega, sale, colaparada, tuberia;
  struct tipo_parada pasajero;
  char nombrefifo[10];
  int fifosalir, testigo = 1;
  struct ParametrosCliente params;

  srand(getpid());
  signal(10, R10);
  signal(12, R12);
  signal(15, R15);

  colagrafica = crea_cola(ftok("./fichcola.txt", 18));
  colaparada = crea_cola(ftok("./fichcola.txt", 20));

  tuberia = dup(2);
  close(2);
  open("/dev/tty", O_WRONLY);
  read(tuberia, &params, sizeof(params));
  read(tuberia, &pidbus, sizeof(pidbus));

  llega = rand() % (params.numparadas) + 1;
  sale = rand() % (params.numparadas) + 1;
  while (llega == sale)
    sale = rand() % (params.numparadas) + 1;

  snprintf(nombrefifo, 10, "fifo%d", sale);
  fifosalir = open(nombrefifo, O_RDONLY);
  if (fifosalir == -1)
    perror("Error al abrir la fifo de parada");

  visualiza(colagrafica, llega, IN, PINTAR, sale);
  pasajero.tipo = llega;
  pasajero.pid = getpid();
  pasajero.destino = sale;
  pasajero.maleducado = 1;
  if (msgsnd(colaparada, (struct tipo_parada *)&pasajero,
             sizeof(struct tipo_parada) - sizeof(long), 0) == -1)
    perror("Error al escribir en la cola de paradas");

  if (!llega12)
    pause();

  if (llega12) {
    llega12 = 0;
    if (kill(pidbus, 6) == -1)
      perror("Error al enviar señal al bus");
    sleep(1);
    if (baja) {
      visualiza(colagrafica, llega, IN, BORRAR, sale);
      visualiza(colagrafica, 7, OUT, PINTAR, sale);
      close(fifosalir);
      return 0;
    }
    visualiza(colagrafica, llega, IN, BORRAR, sale);
    visualiza(colagrafica, 0, 0, PINTAR, sale);
    if (read(fifosalir, &testigo, sizeof(testigo)) <= 0)
      perror("Error al leer la fifo de salida");
    visualiza(colagrafica, 0, 0, BORRAR, sale);
    visualiza(colagrafica, sale, OUT, PINTAR, sale);
  }
  close(fifosalir);
  return 0;
}

void visualiza(int cola, int parada, int inout, int pintaborra, int destino) {
  struct tipo_elemento peticion;

  peticion.tipo = 3;
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
      pause();
    llega10 = 0;
  }
}

void R10() { llega10 = 1; }
void R12() { llega12 = 1; }
void R15() { baja = 1; }
