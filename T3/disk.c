#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "disk.h"
#include "pss.h"

/*****************************************************
 * Agregue aca los tipos, variables globales u otras
 * funciones que necesite
 *****************************************************/

// Creo la estructura Request con su estado ready y su condición
typedef struct {
  int ready;
  pthread_cond_t c;
} Request;

// inicializo el mutex
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

// declaro la variable que dice la posición actual del disco
int pos;
// Declaro una cola de prioridad para los request con pistas mayores al actual
PriQueue *mayores;
// Declaro una cola de prioridad para los request con pistas menores al actual
PriQueue *menores;
// Declaro el estado de la lectura
int busy;

// Inicializo las variables declaradas globalmente
void iniDisk(void) {
  mayores = makePriQueue();
  menores = makePriQueue();
  busy = 0;
  pos = 0;
}

// Limpio las variables inicializadas previamente
void cleanDisk(void) {
  destroyPriQueue(mayores);
  destroyPriQueue(menores);
}

// Función para pedir una pista de lectura
void requestDisk(int track) {
  pthread_mutex_lock(&m);
  if (!busy){ // Si no está ocupada, se le asigna la pista pedida inmediantamente
    busy = 1;
    pos = track; // Se establece la nueva posición
  }
  else{ // Si se está leyendo
    Request req = {0, PTHREAD_COND_INITIALIZER};
    if (pos <= track){ // Si el track pedido es mayor o igual que la posición actual
      priPut(mayores, &req, track);
    }
    else{ // Si el track pedido es menor que la posición actual
      priPut(menores, &req, track);
    }
    while (!req.ready){ // Espera hasta que se desocupe el disco y se le asigne el track
      pthread_cond_wait(&req.c, &m);
    }
  }
  pthread_mutex_unlock(&m);
}

// Función para liberar el disco
void releaseDisk() {
  pthread_mutex_lock(&m);
  if (emptyPriQueue(mayores) && emptyPriQueue(menores)){ // Si no hay nadie esperando, solo se libera
    busy = 0;
  }
  else{
    int valor;
    if (!emptyPriQueue(mayores)){ // Si hay requests esperando con track mayores a la posición actual del disco
      valor = priBest(mayores); // Se obtiene la nueva posición 
      Request *req = priGet(mayores); // Se obtiene el request a asignar
      req -> ready = 1; // Nuevo estado del request
      pos = valor; // Se asigna la nueva posición 
      pthread_cond_signal(&req->c); // Se avisa al request asignado
    }
    else{ // Si sólo hay requests esperando tracks menores a la posición actual de disco
      valor = priBest(menores); // Se obtiene la nueva posición
      Request *req = priGet(menores); // Se obtiene el request a asignar
      while (!emptyPriQueue(menores)){ // Si quedan elementos en la cola de menores, estos deben moverse a la cola de mayores
        int prio = priBest(menores); // Se obtiene el track pedido por el request
        Request *aux = priGet(menores); // Se saca al request de la cola de menores
        priPut(mayores, aux, prio); // Se coloca el request en la cola de mayores
      }
      req -> ready = 1; // Nuevo estado del request
      pos = valor; // Se asigna la nueva posición del request
      pthread_cond_signal(&req->c); // Se le avisa al request asignado
    }
      
  }
  pthread_mutex_unlock(&m);
}
