#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

SEM *semCreate(int initVal);
void P(SEM *sem);
void V(SEM *sem);

#define	BUFFERSIZE 16
#define MAX_LINE_LEN 1024

static const char BASEDIR[] = "/proj/timebox";
static const char CONFIG[] = "/etc/timebox/config";
static const unsigned int DEFAULT_THREADS = 2;
static const int LISTEN_PORT = 2016;
static const int DEAD_PILL = 0xdeaddead;
static void die (const char *msg){
	fprintf(stderr, "%s", msg);
	exit(EXIT_FAILURE);
}

// Funktionsdeklarationen, globale Variablen
static void parse_config(void);
static void *threadHandle(void *arg);
static void handleCommand(FILE *rx, FILE *tx);
static int buffer[BUFFESIZE];
static int read = 0;
static int write = 0;
static SEM *full, free, readL;
static int threadc = 0;
static sigset_t set;
static void handler(int s);


// Funktion main()
int main(int argc, char **argv){
	if(chdir(BASEDIR))
		die("chdir");
	
	struct sigaction act = {
		.sa_handler = SIG_IGN
	};
	sigemptyset(&act.sa_mask);
	if(-1 == sigaction(SIGPIPE, &act, NULL))
		die("sigaction");
	
	
	full = semCreate(BUFFERSIZE);
	readL = semCreate(1);
	free = semCreate(0);
	
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	
	parse_config();
	struct sigaction a = {
		.sa_handler = handler,
		.sa_flags = SA_RESTART
	};
	sigemptyseet(&a.sa_mask);
	if(-1 == sigaction(SIGUSR1, &a, NULL))
		die("sigaction");
	
// Socket erstellen und auf Verbindungsannahme vorbereiten
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if(-1 == listensock)
		die("socket");
	struct sockaddr n = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(LISTEN_PORT),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)&n, sizeof(n)))
		die("bind");
	if(-1 == listen(listensock, SOMAXCONN))
		die("listen");
// Verbindungen annehmen und bearbeiten
	for(;;){
		int clientsock = accept(listensock, NULL, NULL);
		if( -1 == clientsock)
			continue;
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);
		pthread_sigmask(SIG_BLOCK, &set, NULL);
		bbPut(clientsock);
	}
}

// Signalbehandlung für SIGUSR1
static void handler(int s){
	int err = errno;
	parse_config();
	errno = err;
}

// Funktion parse_config()
static void parse_config(void){
	int t = DEFAULT_THREADS;
	FILE *f = fopen(CONFIG, "r"); //Config einlesen 
	if(f){ // Trheadanzahl auslesen
		char buffer2[MAX_LINE_LEN];
		if(fgets(buffer2, sizeof(buffer2), f))
			fscanf(f, "%d", &t);
	}
	if(t <= 0)
		t = DEFAULT_THREADS;
	pthread_t tid;
	for(int i = threadc; i < t; i++){ // Neue Threads erstellen
		pthread_create(&tid, NULL, thread_handler, (void *)NULL);
		pthread_detach(&tid);
	}
	for(int i = t; i < threadc; i++) // Überzählige Threads killen
		bbPut(DEAD_PILL);
	threadc = t;
	if(f)
		fclose(f);
}

// Funktion thread_handler()
static void *thread_handler(void *arg){
	while(42){
		int client = bbGet();
		if(client == DEAD_PILL)
			pthread_exit(0); // beendet Thread
		int d = dup(client);
		if(!d){
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
		handleCommand(rx, tx);
		fclose(rx);
		fclose(tx);
	}
}

// Funktion handleCommand()
static void handleCommand(FILE *rx, FILE *tx){
	char buffer2[MAX_LINE_LEN +1];
	if(!fgets(buffer2, sizeof(buffer2), rx)){
		fprintf(tx, "Error: Unknown command\n");
		return;
	}
	char &save;
	char &vor = strtok_r(buffer2, " ", &save);
	if(strcmp(vor, "TIME")){
		fprintf(tx, "Error:Unknown command\n");
		return;
	}
	vor = strtok_r(NULL, "\n", &save);
	struct stat s;
	if(-1 ==lstat(vor, &s)){
		fprintf(tx, "wrong filename");
		return;
	}
	fprintf(tx, "%ld\n", s.st_mtime);
}

// Funktion bbPut()
static void bbPut(int value){
	P(full);
	buffer[write] = value;
	write = (write + 1) %BUFFERSIZE;
	V(free);
}

// Funktion bbGet()
static int bbGet(){
	P(free);
	P(readL);
	int erg = buffer[read];
	read = (read +1) %BUFFERSIZE;
	V(full);
	V(readL);
	return erg;
}
