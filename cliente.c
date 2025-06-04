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

int main(int argc, char *argv[]) { // Añadido argc y argv

  int colagrafica, llega, sale, colaparada, tuberia;
  int fd_params_fifo; // Descriptor para la FIFO de parámetros
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

  // Verificar argumentos y abrir FIFO de parámetros
  if (argc < 2) {
    fprintf(stderr, "Uso: %s <nombre_fifo_parametros>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  fd_params_fifo = open(argv[1], O_RDONLY);
  if (fd_params_fifo == -1) {
    perror("Error al abrir FIFO de parámetros en el cliente");
    exit(EXIT_FAILURE);
  }

  // Redirigir stderr (descriptor 2) a la FIFO para leer los parámetros
  if (dup2(fd_params_fifo, 2) == -1) {
    perror("Error al redirigir stderr a la FIFO en el cliente");
    close(fd_params_fifo);
    exit(EXIT_FAILURE);
  }
  // Ya no necesitamos el descriptor original de la FIFO, pues ahora es accesible vía el descriptor 2
  close(fd_params_fifo);

  // Ahora el descriptor 2 (stderr) apunta a la FIFO.
  // Guardamos este descriptor (que es el de la FIFO) en 'tuberia'.
  tuberia = dup(2);
  if (tuberia == -1) {
    perror("Error al duplicar el descriptor de la FIFO en el cliente");
    exit(EXIT_FAILURE);
  }

  // Restauramos stderr para que apunte a la terminal para mensajes de error normales.
  // Esto se hace DESPUÉS de que 'tuberia' haya capturado el descriptor de la FIFO.
  close(2);
  if (open("/dev/tty", O_WRONLY) == -1) {
    perror("Error al restaurar stderr a /dev/tty en el cliente");
    // Aunque esto falle, intentamos continuar, pero los errores futuros no irán a la tty
  }

  // Leemos los parametros desde 'tuberia' (que es la FIFO)
  if (read(tuberia, &params, sizeof(params)) != sizeof(params)) {
    perror("Error al leer parámetros (params) desde la FIFO en el cliente");
    close(tuberia);
    exit(EXIT_FAILURE);
  }
  // Leemos el pid del bus desde 'tuberia' (que es la FIFO)
  if (read(tuberia, &pidbus, sizeof(pidbus)) != sizeof(pidbus)) {
    perror("Error al leer parámetros (pidbus) desde la FIFO en el cliente");
    close(tuberia);
    exit(EXIT_FAILURE);
  }
  // Cerramos el descriptor de la FIFO que estaba en 'tuberia' ya que hemos terminado de leer.
  close(tuberia);

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
