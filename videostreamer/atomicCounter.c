#include "atmiccounter.h"
#include <pthread.h>

typedef struct AtomicCounter {
	volatile int value;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
}AtomicCounter;

int acInit(AtomicCounter *c, int i){
	c = malloc(sizeof(AtomicCounter));
	if(!c){
		return -1;
	c->value = i;
	if((errno = pthread_mutex_init(&c->mutex, NULL)))
		free(c);
		return -1;
	if((errno = pthread_cond_init(&c->cond, NULL)))
		pthread_mutex_deestroy(&c->mutex);
		free(c);
		return -1;
	return 0;
}

void acDestroy(AtomicCounter *c){
	pthread_mutex_destroy(&c->mutex);
	pthread_cond_destroy(&c->cond);
	free(c);
}

void acIncrement(AtomicCounter *c){
	pthread_mutex_lock(&c->mutex);
	c->value++;
	pthread_cond_broadcast(&c->cond);
	pthread_mutex_unlock(&c->mutex);
}

void acWait(AtomicCounter *c, int v){
	pthread_mutex_lock(&c->mutex);
	while(c->value < v){
		pthread_cond_wait(&c->cond, &c->mutex);
	}
	pthread_mutex_unlock(&c->mutex);
}



