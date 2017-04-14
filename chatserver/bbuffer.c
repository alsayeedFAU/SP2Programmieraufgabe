#include "bbuffer.h"
#include <stdlib.h>
#include "sem.h"

typedef struct BNDBUF {
	void **data;
	int size;
	volatile int readPos;
	volatile int writePos;
	SEM *full;
	SEM *free;
	SEM *readLock;
	SEM *writeLock;
} BNDBUF;

BNDBUF *bb_init(size_t size){
	BNDBUF *erg = malloc(sizeof(BNDBUF));
	if(!erg)
		return NULL;
	erg->full = sem_init(0);
	erg->free = sem_init(size);
	erg->size = size;
	erg->readPos = 0;
	erg->writePos = 0;
	erg->readLock = sem_init(1);
	erg-> data = malloc (size * sizeof(void *));
	if(!erg->data){
		free(erg);
		return NULL;
	}
	return erg;
}

void bb_add(BNDBUF *bb, void *value){
	P(bb->free);	
	P(bb->writeLock);
	bb->data[bb->writePos]= value;
	bb->writePos = (bb->writePos +1) % bb->size;
	V(bb->writeLock);
	V(bb->full);
}

void *bb_get(BNDBUF *bb){
	P(bb->full);
	P(bb->readLock);
	void *erg = bb->data[bb->readPos];
	bb->readPos = (bb->readPos+1) % bb->size;
	V(bb->readLock);
	V(bb->free);
	return erg;
}
