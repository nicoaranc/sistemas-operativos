#define _XOPEN_SOURCE 500

#include "nthread-impl.h"

#include "rwlock.h"


struct rwlock {
  NthQueue *lectores;
  NthQueue *escritores;
  int readers;
  int writing;
};

nRWLock *nMakeRWLock() {
  nRWLock *controlador = malloc(sizeof(nRWLock));
  controlador -> lectores = nth_makeQueue();
  controlador -> escritores = nth_makeQueue();
  controlador -> readers = 0;
  controlador -> writing = 0;
  return controlador;
}

void function(nThread th){
  nth_delQueue(th->ptr, th);
  th->ptr = NULL;
}

void nDestroyRWLock(nRWLock *rwl) {
  nth_destroyQueue(rwl->lectores);
  nth_destroyQueue(rwl->escritores);
  free(rwl);
}

int nEnterRead(nRWLock *rwl, int timeout) {
  START_CRITICAL
  if (!rwl->writing && nth_emptyQueue(rwl->escritores)){
    rwl->readers++;
  }
  else{
    nThread thisTh = nSelf();
    nth_putBack(rwl->lectores, thisTh);
    suspend(WAIT_RWLOCK);
    schedule();
  }
  END_CRITICAL
  return 1;
}

int nEnterWrite(nRWLock *rwl, int timeout) {
  START_CRITICAL
  if (rwl->readers == 0 && !rwl->writing){
    rwl->writing = 1;
  }
  else{
    nThread thisTh = nSelf();
    nth_putBack(rwl->escritores, thisTh);
    if (timeout > 0){
      thisTh->ptr = rwl->escritores;
      suspend(WAIT_RWLOCK_TIMEOUT);
      nth_programTimer(timeout * 1000000LL, &function);
      schedule();
      if (thisTh->ptr == NULL){
        END_CRITICAL
        return 0;
      }
    }
    else {
      suspend(WAIT_RWLOCK);
      schedule();
    }
    
  }
  END_CRITICAL
  return 1;
}

void nExitRead(nRWLock *rwl) {
  START_CRITICAL
  rwl->readers--;
  if (rwl->readers == 0 && !nth_emptyQueue(rwl->escritores)){
      nThread w = nth_getFront(rwl->escritores);
      if (w->status == WAIT_RWLOCK_TIMEOUT){
        nth_cancelThread(w);
      }
      rwl->writing = 1;
      setReady(w);
      schedule();
  }
  END_CRITICAL
}

void nExitWrite(nRWLock *rwl) {
  START_CRITICAL
  rwl->writing = 0;
  if (!nth_emptyQueue(rwl->lectores)){
    while(!nth_emptyQueue(rwl->lectores)){
      nThread w = nth_getFront(rwl->lectores);
      rwl->readers++;
      setReady(w);
    }
    schedule();
  }
  else if (!nth_emptyQueue(rwl->escritores)){
    nThread w = nth_getFront(rwl->escritores);
    if (w->status == WAIT_RWLOCK_TIMEOUT){
      nth_cancelThread(w);
    }
    rwl->writing = 1;
    setReady(w);
    schedule();
  }

  END_CRITICAL
}

