// Plantilla para maleta.c

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>
#include "maleta.h"

#define P 8

// Defina aca las estructuras y funciones adicionales que necesite
typedef struct{
    double *w;
    double *v;
    int *z;
    int n;
    double maxW;
    int k;
    double best;
} Args;


void *thread(void *p){
    Args *args = (Args *)p;
    args->best = llenarMaletaSec(args->w,args->v,args->z,args->n,args->maxW,(args->k)/8);
     
    return NULL;
}

double llenarMaletaPar(double w[], double v[], int z[], int n,
                       double maxW, int k) {
    // ... Modifique esta funcion ...
    pthread_t pid[P];
    Args args[P];

    for (int i = 0; i < P; i++){
      args[i].w = w;
      args[i].v = v;   
      args[i].n = n;
      args[i].maxW = maxW;
      args[i].k = k;
      args[i].z = malloc(n * sizeof(int));
      pthread_create(&pid[i], NULL, thread, &args[i]);
    }



    double respuesta = 0;

    for (int i = 0; i < P; i++){ 
      pthread_join(pid[i], NULL);
      if (args[i].best > respuesta){
        respuesta = args[i].best;
        for (int j = 0; j < n; j++){
          z[j] = args[i].z[j];
        }
      }
      free(args[i].z);
    }

    
    
    return respuesta;
}
