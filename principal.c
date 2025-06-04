#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "comun.h"

void leeparametros(struct ParametrosBus *parambus,
                   struct ParametrosCliente *paramclientes, int *maxclientes,
                   int *creamin, int *creamax);
int creaproceso(const char *);
int creaservigraf(int);
void R10();
void R12();

int llega10 = 0;

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int pservidorgraf, i, pidbus;
  char nombrefifo[10];
  int fifos[7];
  int fifoparbus, fifoparcli;

  struct ParametrosBus parambus;
  struct ParametrosCliente paramclientes;
  int maxclientes, creamin, creamax;

  signal(10, R10);
  signal(12, R12);
  srand(getpid());

  // Leemos los parámetros desde el teclado
  leeparametros(&parambus, &paramclientes, &maxclientes, &creamin, &creamax);

  // Lanzamos el servidor gráfico
  // El servidor gráfico se lanza con la última parada como argumento
  pservidorgraf = creaservigraf(
      parambus.numparadas); // El argumento es la ultima parada. Maximo 6
  if (!llega10)
    pause(); // Espero a que el servidor gráfico de el OK

  // creamos las fifos. Las abrimos para que luego no haya que esperar en
  // aperturas síncronas
  for (i = 1; i <= parambus.numparadas; i++) {
    snprintf(nombrefifo, 10, "fifo%d", i);
    unlink(nombrefifo); // por si se quedo de una ejecución previa
    if (mkfifo(nombrefifo, 0600) == -1)
      perror("Error al crear la fifo");
    fifos[i] = open(nombrefifo, O_RDWR);
    if (fifos[i] == -1)
      perror("Errro al abrir la fifo");
  }

  // Creamos las fifos para los parametros del bus y los clientes
  unlink("fifobusparam");
  unlink("fifoclienteparam");
  if (mkfifo("fifobusparam", 0600) == -1)
    perror("Error al crear fifo bus param");
  if (mkfifo("fifoclienteparam", 0600) == -1)
    perror("Error al crear fifo cliente param");
  fifoparbus = open("fifobusparam", O_RDWR);
  if (fifoparbus == -1)
    perror("Error al abrir fifo bus param");
  fifoparcli = open("fifoclienteparam", O_RDWR);
  if (fifoparcli == -1)
    perror("Error al abrir fifo cliente param");

  // Creamos el proceso bus y le pasamos los parametros por la fifo
  pidbus = creaproceso("bus");
  if (write(fifoparbus, &parambus, sizeof(parambus)) == -1)
    perror("error al escribir parametros al bus");

  for (i = 1; i <= maxclientes; i++) {
    // Creamos el proceso cliente y le escribimos los parametros en la fifo
    creaproceso("cliente");
    if (write(fifoparcli, &paramclientes, sizeof(paramclientes)) == -1)
      perror("error al escribir parametros al cliente");
    if (write(fifoparcli, &pidbus, sizeof(pidbus)) == -1)
      perror("error al escribir pid del bus al cliente");
    sleep(rand() % (creamax - creamin + 1) + creamin);
  }

  // Esperamos que todos los clientes finalicen
  for (i = 1; i <= maxclientes; i++)
    wait(NULL);
  sleep(2);

  // Avisamos al bus para que termine
  if (kill(pidbus, 12) == -1) {
    perror("Error al enviar 12 al bus");
    exit(-1);
  }

  // Avisamos al servidor grafico para que termine
  if (kill(pservidorgraf, 12) == -1) {
    perror("Error al enviar 12 al servidor grafico");
    exit(-1);
  }

  // Cerramos y borramos las fifos
  for (i = 1; i <= parambus.numparadas; i++) {
    snprintf(nombrefifo, 10, "fifo%d", i);
    close(fifos[i]);
    unlink(nombrefifo);
  }
  close(fifoparbus);
  close(fifoparcli);
  unlink("fifobusparam");
  unlink("fifoclienteparam");

  //Esperar la finalizacion del servidor grafico y el bus	
  wait(NULL);
  wait(NULL);


  // Limpiamos la terminal de restos graficos
  system("reset");
  return 0;
}

/************************************************************************/
/***********    FUNCION: leeparametros     ******************************/
/************************************************************************/

void leeparametros(struct ParametrosBus *parambus,
                   struct ParametrosCliente *paramclientes, int *maxclientes,
                   int *creamin, int *creamax) {
  int ok = 0;

  *maxclientes = 60; // Numero de clientes que se crearan
  *creamin = 1;      // Intervalo de tiempo para crear nuevos clientes MIN
  *creamax = 2;      // Intervalo de tiempo para crear nuevos clientes MAX
  parambus->numparadas = paramclientes->numparadas = 6; // Cantidad de paradas
  parambus->capacidadbus = 4;                           // Capacidad del bus
  parambus->tiempotrayecto = 2;       // Tiempo del trayecto entre paradas
  paramclientes->aburrimientomax = 8; // Intervalo de tiempo en aburrirse MAX
  paramclientes->aburrimientomin = 4; // Intervalo de tiempo en aburrirse MIN

  while (ok == 0) {
    system("clear");
    printf("Valores de los parámetros...\n\n");
    printf("Numero de pasajeros que se crearan: %d\n", *maxclientes);
    printf("Intervalo de tiempo para crear nuevos pasajeros: [%d-%d] \n",
           *creamin, *creamax);
    printf("Número de paradas: %d\n", parambus->numparadas);
    printf("Capacidad del Bus: %d\n", parambus->capacidadbus);
    printf("Tiempo en el trayecto entre paradas: %d\n",
           parambus->tiempotrayecto);
    printf("Intervalo de tiempo de aburrimiento: [%d-%d]\n",
           paramclientes->aburrimientomin, paramclientes->aburrimientomax);
    printf("Pulse 0 si desea introducir nuevos valores, cualquier otro valor "
           "si desea continuar.\n");
    scanf("%d", &ok);

    if (ok == 0) {
      do {
        printf("Numero de pasajeros que se crearan [maximo 50]:\n");
        scanf("%d", maxclientes);
      } while (*maxclientes <= 0 || *maxclientes > 50);

      do {
        printf("Intervalo de tiempo para crear nuevos pasajeros MIN [entre 1 y "
               "8]: \n");
        scanf("%d", creamin);
      } while (*creamin < 1 || *creamin > 8);

      do {
        printf("Intervalo de tiempo para crear nuevos pasajeros MAX [entre 2 y "
               "20]: \n");
        scanf("%d", creamax);
      } while (*creamax < 2 || *creamax > 20 || *creamax <= *creamin);

      do {
        printf("Número de paradas: \n");
        scanf("%d", &parambus->numparadas);
      } while (parambus->numparadas < 2 || parambus->numparadas > 6);
      paramclientes->numparadas = parambus->numparadas;

      do {
        printf("Capacidad del bus [maximo 10]: \n");
        scanf("%d", &parambus->capacidadbus);
      } while (parambus->capacidadbus <= 0 || parambus->capacidadbus > 10);

      do {
        printf("Tiempo en el trayecto entre paradas [maximo 10]:\n");
        scanf("%d", &parambus->tiempotrayecto);
      } while (parambus->tiempotrayecto < 1 || parambus->tiempotrayecto > 10);

      do {
        printf("Intervalo de tiempo en esperar para aburrirse MIN [entre 1 y "
               "10]:\n");
        scanf("%d", &paramclientes->aburrimientomin);
      } while (paramclientes->aburrimientomin < 1 ||
               paramclientes->aburrimientomin > 10);

      do {
        printf("Intervalo de tiempo en esperar para aburrirse MAX [entre 5 y "
               "200]:\n");
        scanf("%d", &paramclientes->aburrimientomax);
      } while (paramclientes->aburrimientomax < 5 ||
               paramclientes->aburrimientomax > 20 ||
               paramclientes->aburrimientomin > paramclientes->aburrimientomax);
    }
  }
}

/************************************************************************/
/***********     FUNCION: creaproceso      ******************************/
/************************************************************************/

int creaproceso(const char nombre[]) {

  int vpid;

  vpid = fork();
  if (vpid == 0) {
    execl(nombre, nombre, NULL);
    perror("error de execl");
    exit(-1);
  } else if (vpid == -1) {
    perror("error de fork");
    exit(-1);
  }
  return vpid;
}

/************************************************************************/
/***********    FUNCION: creaservigraf     ******************************/
/************************************************************************/

// Lanza el servidor gráfico
int creaservigraf(int ultimaparada) {

  int vpid;
  char cadparada[10];

  snprintf(cadparada,10, "%d", ultimaparada);
  
  vpid = fork();
  if (vpid == 0) {
    execl("servidor_ncurses", "servidor_ncurses", cadparada, NULL);
    perror("error de execl");
    exit(-1);
  } else if (vpid == -1) {
    perror("error de fork");
    exit(-1);
  }
  return vpid;
}

/************************************************************************/
/***********    FUNCION: R10     ****************************************/
/************************************************************************/

void R10() { llega10 = 1; }

/************************************************************************/
/***********    FUNCION: R12     ****************************************/
/************************************************************************/

void R12() {
  printf("No es posible arrancar el servidor gráfico\n");
  exit(-1);
}
