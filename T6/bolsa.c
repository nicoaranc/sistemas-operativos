#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "pss.h"
#include "bolsa.h"
#include "spinlocks.h"

// Declare aca sus variables globales


enum{
  ESPERA,
  ADJUDICADO,
  RECHAZADO
};


int mutex = OPEN;

int low_price = 1e9;

char *nom_vendedor;

char *nom_comprador;

int *estado;

int *q;

int vendo(int precio, char *vendedor, char *comprador){
  
  spinLock(&mutex);
  if (precio >= low_price){
    spinUnlock(&mutex);
    return 0;
  }
  else{
    if (low_price != 1e9){
      *estado = RECHAZADO;
      spinUnlock(q);
    }
    low_price = precio;
    nom_comprador = comprador;
    nom_vendedor = vendedor;
    int lk = CLOSED;
    q = &lk;
    int vl = ESPERA;
    estado = &vl;
    spinUnlock(&mutex);
    spinLock(&lk);
    if (vl == RECHAZADO){
      return 0;
    }
    else{
      return 1;
    }
  }
}

int compro(char *comprador, char *vendedor){
  spinLock(&mutex);
  if (low_price == 1e9){
    spinUnlock(&mutex);
    return 0;
  }
  else{
    strcpy(nom_comprador, comprador);
    strcpy(vendedor, nom_vendedor);
    *estado = ADJUDICADO;
    int res = low_price;
    low_price = 1e9;
    spinUnlock(q);
    spinUnlock(&mutex);
    return res;
  }
}
