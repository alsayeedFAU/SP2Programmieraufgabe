#include "atomiccounter.h"
#include "transconde.h"
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nerinet/in.h>
#include <siys/socket.h>

#define MAX_REQUEST_LEN 2048
#define VID_FRAME_SIZE 256

typedef struct VedeoJob {
	FILE*out;
	AtomicCounter processedFrames;
}VideoJob;


typedef struct VideoFrame {
	int id;
	char data[VID_FRAME_SIZE];
	VideoJob *job;
}VideoFrame;

BNDBUF *bbCreate(size_t size);
void bbDestroy(BNDBUF *bb);
void bbPut(BNDBUF *bb, VideoFrame *frame);
VideoFrame *bbGet(BNDBUF *bb);

static int die(const char message[]){
	perror("%s", message);
	exit(EXIT_FAILURE);
}

// Globale Variablen, Funktionsdeklarationen usw.
static void serve(FILE * client);
static in listDir(FILE *client);
static in streamVideo(const char fileName[], FILE *client);
static void *doTranscoding(void*arg);
static BNDBUF *buf;

// Funktion main()
int main(int argc, char **argv){
	// Allgemeine Vorbereitungen
	buf = bbCreate(32);
	if(!buf){
		die("bbCreate");
	}
	pthread_t tid[16];
	for(int i = 0; i < 16; i++){
		errno = pthread__create(&tid[i], NULL, doTranscoding, (void *)NULL);
		if(errno)
			die("pthread_create");
	}
	// Socket erstellen und auf Verbindungsannahme vorbereiten
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if(-1 == listensock)
		die("socket");
	struct sockaddr_in6 name = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(7083),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)&name,sizeof(name)))
		die("bind");
	if(-1 == listen(listensock, SOMAXCONN))
		die("listen");
	
	 // Verbindungen annehmen und in neuem Thread abarbeiten
	for(;;){
		int clientsock = accept(listensock, NULL, NULL);
		if(cliestsock == -1)
			continue;
		FILE *client == fdopen(clientsock, "r+");
		if(!client){
			close(clientsock);
			continue;
		}
		
		pthread_t id;
		errno = pthread_create(&id, NULL, serve, client); // Thread erstellen
		if(errno){
			fprintf(client, "FAILED.");
			fclose(client);
		}
		errno = pthread_detach(&id); // Thread hinzufügen
		if(errno){
			fprintf(client, "FAILED.");
			fclose(client);
			continue;
		}
	}
}
// Thread-Funktion serve()
static void *serve(FILE *client){
	char an[MAX_REQUEST_LEN];
	if(!fgets(an, sizeof(an), client)){
		fprintf(client, "FAILED.");
		fclose(client);
		pthread_exit(-1);
	}
	if(!strcmp("LIST\n", an)){
		if(-1 == listDir(client)){
			fprintf(client, "FAILED.");
			fclose(client);
			pthread_exit(-1);
		}
	}
	char *d = strtok(an, " ");
	if(!d || strcmp(d, "PLAY")){
		fprintf(client, "INVALID.");
		fclose(client);
		pthread_exit(-1);
	}
	d[strlen(d)] = '\0';
	if(-1 == streamVideo(d, client)){
		fprintf(client, "FAILED.");
		fclose(client);
		pthread_exit(-1);
	}
	fclose(client);
	pthread_exit(0);
}

// Funktion listDir()
static int listDir(FILE *client){
	DIR *dir = opendir(".");
	if(!dir)
		return -1;
	struct dirent *d;
	while(errno = 0, (d = readdir(dir)) != NULL){
		if( d->d_name[0] != '.')
			fprintf(client, "%s", d->d_name);
	}
	closedir(dir);
	if(errno)
		return -1;
	return 0;
}
//readdir ist heir trotz threads erlaubt, da opendir erst im thread aufgerufen wird


// Funktion streamVideo()
static int streamVideo(const char fileName[], FILE *client){
	VideoJob jb;
	jb.out = client;
	if(acInit(jb.processedFrames, 0) == -1)
		return -1;
	FILE *file = fopen(fileName, "r");
	if(!file){
		acDestroy(jb.processedFrames);
		return -1;
	}
	int counter = 0;
	while(42){
		int f = 0;
		VideoFrame *frame = malloc(sizeof(VideoFrame));
		if(!frame){
			fclose(file);
			acDestroy(jb.processedFrames);
			return -1;
			//man kann hier auch breaken und die bisher eingehaengten frames rausgeben. 
		}
		frame->id = counter;
		frame->job = &jb;
		for(int i = 0; i <VID_FRAME_SIZE; i++){
			int c = fgetc(file);
			if(c == EOF){
				f =1;
				break;
			}
			frame->data[i] = (char)c;
		}
		if(f == 1){
			if(ferror(file))
				f = 2;
			free(frame);
			break;
		}
		bbPut(buf, frame);
		counter++;
	}
	fclose(file);
	acWait(&jb.processedFrames, counter);
	acDestroy(&vj.processedFrames);
	if(f==2)
		return -1;
	return 0;
}
// Thread-Pool-Funktion doTranscoding()
static void *doTranscoding(void *arg){
	while(42){
		VideoFrame *frame = bbGet(buf);
		char out[VID_FRAME_SIZE];
		size_t size = transcode(out, frame->data, VID_FRAME_SIZE);
		acWait((frame->job)->processedFrames, frame->id);
		fputs(out, (frame->job)->out);
		acIncrement((frame->job)->processedFrames);
		free(frame);
	}
}

