#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "sem.h"

SEM *semCreate(int initVal);
void P(SEM *sem);
void V(SEM *sem);

#define LISTEN_PORT 2016
#define BUFFERSZE 16
#define MAX_LINE 256
#define MAX_NAME 127

static void die(const char msg[]){
	perror(msg);
	exit(EXIT_FAILURE);
}

// Funktions- / Strukturdekleration, globale Varialen ect
static void bbPut(int value);
static int bbGet();
static int doWork(FILE *rx, FILE *tx);
static int namecmp(const void *p1, const void *p2);
static void *threadHandler(void *);
static int data[16];
static SEM *full,free,readL;
static volatile int read,write;
typedef struct name{
	char vorname[MAX_NAME + 1];
	char nachname[MAX_NAME + 1];
}name;

　
　
　
//Funktion main()
int main(int argc, char **argv){

//Argumente prüfen, Initialsierungen, ect
	if(argc != 2)
		die("usage");
	errno = 0;
	int threads = strtol(argv[1], NULL, 0);
	if(errno)
		die("usage");
	read = 0;
	write = 0;
	full = semCreate(BUFFERSIZE);
	free = semCreaet(0);
	readL = semCreate(1);
	struct sigaction act = {
		.sa_handler = SIG_IGN, //sig-ignore
	};
	sigemptyset(&act.sa_mask);
	if(-1 == sigaction(SIGPIPE, &act, NULL)) // Ignoriert SIGPIPE
		die("sigaction");
		
//Threads starten
	pthread_t tid;
	for(int i = 0; i < threads; i++){
		errno = pthread_create(&tid, NULL, threadHandler, (void *)NULL);
		if(errno)
			die("pthread_create");
	}
	
// Socket erstellen und für Verbindungsannehme vorbereiten 
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if(-1 == listensock)
		die("socket");
	struct sockaddr_in6 n = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(LISTEN_PORT),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)n, sizeof(n)))
		die("bind");
	if(-1 == listen(listensock, SOMAXCONN))
		die("listen");
//Verbindung annehmen und in den Buffer legen
	for(;;){
		int clientsock = accept(listensock, NULL, NULL);
		if(-1 == clientsock)
			continue;
		bbPut(clientsock);
	}
}

//Funktion bbPut()
static void bbPut(int value){
	P(full);
	data[write] = value;
	write = (write + 1)%BUFFERSIZE;
	V(free);
}

//Funktion bbGet()
static int bbGet(){
	P(free);
	P(readL);
	int erg = data[read];
	read = (read +1)%BUFFERSIZE;
	V(full);
	V(readL);
	return erg;
}

//Funktion threadHandler
static void *threadHandler(void *arg){
	while(42){
		int cliet = bbGet();
		int d = dup(client);
		if(d == -1){
			close(client);
			continue;
		}
		FILE *rx = fdopen(client, "r");
		if(!rx){
			close(client);
			close(d);
			continue;
		}
		FILE *tx = fdopen(d, "w");
		if(!tx){
			fclose(rx);
			close(d);
			continue;
		}
		int e = doWork(rx, tx);
		if( e ==  -1)
			fprintf(tx, "Fehler beim Bearbeiten");
		fclose(tx);
		fclose(rx);
	}
}

//Funktion doWork
static int doWork(FILE *rx, FILE *tx){
	name *n = malloc(sizeof(name));
	if(!n)
		return -1;
	char buffer[MAX_LINE + 1];
	int counter = 0; 
	int err = 0;
//Zeilen vom Client einlesen, Array aufbauen
// char * strchr ( const char *, int );  Locate last occurrence of character in string 
// char *strtok(char *str, const char *delim); extracts token from string

	while(fgets(buffer, sizeof(buffer), rx)){
		if(!strchr(buffer, '\n'))
			continue;
		char *save;
		char *vor = strtok_r(buffer, "\t", &save);
		if(strlen(vor) > MAX_NAME)
			continue;
		strcpy(n[counter].vorname, vor);
		vor = strtok_r(NULL, "\n", &save);
		if(strchr(vor, ' ') || strchr(vor, '\t') || strlen(vor) > MAX_NAME)
			continue;
		strcpy(n[counter].nachname, vor);
		if(!realloc(n, ((++counter) + 1) * sizeof(name))){
			err = 1;
			break;
		}
		if(err || ferror(rx)){
			free(n);
			return -1;
		}
//Eingelesene Daten sortieren und an den CLienten senden
		qsort(n, counter, sizeof(name), namecmp);
		for(int i = 0; i < counter ; i++)
			fprintf(tx, "%s, %s\n", n[i].vorname, n[i].nachname);
		free(n);
		return 0;
	}
}

//Funktion namecmp
static int namecmp(const void *p1, const void *p2){
	name *n1 = (name *) p1;
	name *n2 = (name *) p2;
	int a = strcmp(n1->nachname, n2->nachname);
	if(!a)
		return strcmp(n1->vorname, n2->vorname);
	return a;
}
