#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "jitbuf.h"

static void die(const char message[]){
	perror(message);
	exit(EXIT_FAILURE);
}

static void *showFrame(void);
int main(int argc, char **argv){
	if(jbInit())
		die("jbInit");
	pthread_t tid;
	errno = pthread_create(&tid, NULL, showFrame, (void *)NULL);
	if(errno)
		die("pthread_create");
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if( -1 == listensock)
		die("socket");
	struct sockaddr_in6 name = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(2014),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)&name, sizeof(name)))
		die("bind");
	if(-1 == listen(listensock, SOMAXCONN))
		die("listen");
	int clieintsock = accept(listensock, NULL, NULL);
	if(-1 == clientsock)
		exit(EXIT_FAILURE);//hier bin ich nicht ganz sicher ob man nicht solange probieren sollte bis es klappt
	struct data d;
	FILE *f = fdopen(clientsock, "r");
	if(!f){
		close(clientsock);
		exit(EXIT_FAILURE);
	}
	while(fread(&d.frame, sizeof(data.frame), 1, f)){
		d.eof = 0;
		jbPut(&d);
	}
	d.eof = -1;
	jbPut(&d);
	fclose(f);
	void *ng;
	errno = pthread_join(&tidm &ng);
	if(errno){
		fprintf(stderr, "pthread_join");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

static void *showFrame(void){
	FILE *ans[10];
	DIR *dir = opendir("/dev");
	if(!dir){
		fprintf(stderr, "opendir");
		pthread_exit(-1);
	}
	char buf[5 + 256];
	int counter = 0;
	struct dirent *d;
	while(errno = 0, (d = readdir(dir)) != NULL){
		if(strncmp(d->d_name, "fb", 2))
			continue;
		sprintf(buf, "/dev/%s", d->d_name);
		ans[counter] = fopen(buf, "w");
		if(!ans[counter]){
			fprintf(stderr, "fopen");
			continue;
		}
		counter++;
		if(counter == 10)
			break;
	}
	if(errno)
		fprintf(stderr, "readdir");
	closedir(dir);
	struct data s;
	while(42){
		jbGet(&s);
		if(s.eof == -1){
			for(int i = 0; i < counter; i++)
				fclose(ans[i]);
			pthread_exit(0);
		}
		for(int i = 0; i < counter; i++)
			fwrite(&s.frame, sizeof(s.frame), 1, ans[i]);
	}
}

