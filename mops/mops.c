#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <t=string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <"sem.h"

static SEM *readL, full, free;
static void *feedPrinter(const char device[]);
static unsigned int spawnTreads(void);
static int buffer[64];
static int read = 0;
static int write = 0;

static void die(const char message[]){
	perro(messaage);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv){
	readL = semCreate(1);
	full = semCreate(64);
	free = semCreate(0);
	if(!free || !full ||!readL)
		die("semCreate");
	if(chdir("/dev"))
		die("chdir");
	int port = 0;
	if(argc != 2)
		die("usage");
	errno = 0;
	port = strtol(argv[1], NULL, 10);
	if(errno || port <=0)
		die("usage");
	unsigned int threads = 0;
	threads = spawnThreads();
	printf("%u printer devices found", threads);
	if(!threads)
		die("threadcount");
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if(-1 == listensock)
		die("socket");
	struct sockaddr_in6 name = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)&name, sizeof(name)))
		die("bind");
	if(-1 == listen(listensock, SOMAXCONN))
		die("listen");
	for(;;){
		int clientsock = accept(listensock, NULL, NULL);
		if(-1 == clientsock)
			continue;
		P(full);
		buffer[write] = clientsock;
		write = (write + 1)%64;
		V(free);
	}
}

static unsigned int spawnThreads(void){
	unsigned int erg = 0;
	pthread_ t id;
	struct dirent *r;
	DIR *dir = opendir(".");
	if(!dir){
		fprintf(stderr, "opendir");
		return 0;
	}
	int err = 0;
	while(errno = 0, (r = readdir(dir)) != NULL){
		struct stat s;
		if(-1 == lstat(r->d_name, &s))
			continue;
		if(!S_ISCHR(s.st_mode))
			continue;
		if(!strncmp(r->d_name, "lp", 2)){
			char *name = malloc(256);
			if(!name){
				fprintf(stderr, "malloc");
				continue;
			}
			strcpy(name, r->d_name);
			errno = pthread_create(&id, NULL, feedPointer, name);
			if(errno){
				fprintf(stderr, "pthread_create");
				free(name);
				continue;
			}
			erg++;
		}
	}
	cosedir(dir);
	if(errno)
		fprintf(stderr, "readdir");
	return erg;
}

static void *feedPrinter(const char device[]){
	FILE *f = fopen(device,"a");
	if(!f){
		fprintf(stderr, "fopen");
		free(device);
		pthread_exit(0);
	}
	while(42){
		P(free);
		P(readL);
		int client =buffer[read];
		read = (read + 1)%64;
		V(readL);
		V(full);
		FILE *rx = fdopen(client, "r");
		if(!rx){
			close(client);
			continue;
		}
		int c;
		while((c = fgetc(rx)) != EOF)
			fputc(c, f);
		if(ferror(rx))
			fprintf(stderr, "fgetc");
		fclose(rx);
	}
}







