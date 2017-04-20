#include "sem.h"

SEM *semCreate(int initial_alue){
	SEM *sem = malloc(sizeof(SEM));
	if(!sem)
		return NULL;
	if(errno = pthread_mutex_init(sem->m, NULL)){
		free(sem);
		return NULL;
	}
	if(errno = pthread_cond_init(sem->c, NULL)){
		pthread_mutex_destroy(sem->m);
		free(sem);
		return NULL;
	}
	sem->v = initial_value;
	return sem;
}

void P(SEM *sem){
	pthread_mutex_lock(sem->m);
	while(sem->v <= 0)
		pthread_cond_wait(sem->c, sem->m);
	sem->v--;
	pthread_mutex_unlock(sem->m);
}

void V(SEM *sem){
	pthread_mutex_lock(sem->m);
	sem->v++;
	pthread_cond_broadcast(sem->c);
	pthread_mutex_unlock(sem->m);
}

	
