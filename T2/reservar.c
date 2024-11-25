#define _XOPEN_SOURCE 500

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "reservar.h"

// Defina aca las variables globales y funciones auxiliares que necesite

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;
int ticket_dist = 0, display = 0;
int estacionamientos[10] = {0,0,0,0,0,0,0,0,0,0};

void initReservar() {

}

void cleanReservar() {

}

int checkear(int k){
  int res = -1;
  int i = 0;
  while(i < 10){
    if (10-i < k){
      break;
    }
    if (estacionamientos[i] == 1){
      i++;
    }
    else{
      int j = 0;
      res = i;
      while (j < k){
        if (estacionamientos[i] == 0){
          if (j == k - 1){
            break;
          }
          i++;
          j++;
        }
        else{
          i++;
          res = -1;
          break;
        }
      }
      if (res != -1){
        break;
      }
    }
  }
  return res;
}

void asignar(int e, int k){
  for (int i = e; i < e+k; i++){
    estacionamientos[i] = 1;
  }
  
}

int reservar(int k) {
  pthread_mutex_lock(&m);
  int num = ticket_dist++;


  while(num != display || checkear(k) == -1){
    pthread_cond_wait(&c, &m);
  }
  int res=checkear(k);
  asignar(res, k);
  display++;
  pthread_cond_broadcast(&c);

  pthread_mutex_unlock(&m);
  
  return res;
}

void liberar(int e, int k) {

  pthread_mutex_lock(&m);
  
  for (int i = e; i < e + k; i++){
    estacionamientos[i] = 0;
  }

  pthread_cond_broadcast(&c);
  
  pthread_mutex_unlock(&m);
  
} 
