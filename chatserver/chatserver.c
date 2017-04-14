#include "bbuffer.h"
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_CLIENTS 100
#define MAX_USER_LEN 8
#define MAX_MSG_LEN 1024
#define CONN_BUF_SIZE 10
#define LINE_BUF_SIZE 128

static void *receive(void *arg);
static void *broadcast(void *arg);
static void pex(char *msg){
	fprintf(stderr, "%s", msg);
	exit(EXIT_FAILURE);
}

BNDBUF *buf;
BNDBUF *msgs;
FILE *msgto[MAX_CLIENTS];

int main(int argc, char **argv){
	buf = bb_init(CONN_BUF_SIZE);
	if(!buf)
		pex("bbinit");
	msgs = bb_init(LINE_BUF_SIZE);
	if(!msgs)
		pex("bbinit");
	pthread_t sende;
	errno = pthread_create(&sende, NULL, broadcast, NULL);
	if(errno)
		pex("pthread_create");
	pthread_t tids[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; i++){
		errno = pthread_create(&tids[i], NULL, receive, (void *)i);
		if(errno)
			pex("pthread_create");
	}
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if(-1== listensock)
		pex("socket");
	struct sockaddr_in6 name = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(6670),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)&name, sizeof(name)))
		pex("bind");
	if(-1 == listen(listensock, SOMAXCONN))
		pex("listen");
	for(;;){
		int clientsock = accept(listensock, NULL, NULL);
		if(-1 == clientsock)
			continue;
		FILE *fd = fdopen(clientsock, "r+");
		if(!fd){
			close(clientsock);
			continue;
		}
		bb_put(buf, fd);
	}
}

static void *receive(void *arg){
	while(42){
		FILE *conn = bb_get(buf);
		int a = (int) arg;
		msgto[a] = conn;
		char name[MAX_USER_LEN +1];
		if(!fgets(name, sizeof(name), conn))
			continue;
		strtok(name, "\n"); //strtok erlaubt, da keine weitere zerlegung notwendig
		char msg[4 + MAX_MSG_LEN + MAX_USER_LEN];
		sprintf(msg, "<%s>\t", name);
		char msg2[MAX_MSG_LEN +1];
		while(fgets(msg2, sizeof(msg2), conn)){
			strcat(msg, msg2);
			char *mmsg = malloc(sizeof(msg));
			if(!mmsg)
				continue;
			strcpy(mmsg, msg);
			bb_put(msgs, mmsg);
		}
		msgto[a] = NULL;
	}
}

static void *broadcast(void *arg){
	while(42){
		char *msg = bb_get(msgs);
		for(int i = 0; i < MAX_CLIENTS; i++){
			if(msgto[i])
				fprintf(msgto[i], "%s", msg);
		}
		free(msg);
	}
}
