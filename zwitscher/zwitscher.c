#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "sem.h"
 
#define LISTEN_PORT 2017
#define MAX_MSG_LEN 140
#define MAX_NAME_LEN 15
#define STORAGE_DIR "/etc/zwitscher/"
#define UINT_LEN (sizeof(unsigned int) * 8)
 
static unsigned int ID = 0;
static int maxT;
static void* handleConnection(void *argument);
static int parseCommand(FILE *rx, FILE *tx);
static int getMessages(const char *username, FILE *tx);
static int storeMessage(const char *username, const char *message);
SEM* m; //nur maxthreads
SEM* d; //id holen
 
SEM *semCreate(int initVal);
void P(SEM *sem);
void V(SEM *sem);
static void die(const char msg[]) {
perror(msg);
exit(EXIT_FAILURE);
}
// Erzeugt zum uebergegebenen Socket ein dynamisch allokiertes, //rip free points
// zwei-elementiges FILE*-Array (Index 0: Lesen, Index 1: Schreiben).
// Im Fehlerfall wird NULL zurueckgegeben.
 
static FILE** sock2fds(int sock) {
FILE *rx = fdopen(sock, "r");
if (!rx) {
close(sock);
return NULL;
}
int sock2 = dup(sock);
if (sock2 < 0) {
close(sock);
return NULL;
}
FILE *tx = fdopen(sock2, "w");
if(!tx) {
fclose(rx);
close(sock2);
return NULL;
}
FILE **farray = calloc(2, sizeof(FILE *));
if (farray) {
farray[0] = rx;
farray[1] = tx;
}
return farray;
}
 
 
 
void main(int argc, char const *argv[]) {
  if (argc != 2) {
    die("USAGE: zwitscher <# of max threads>")
  }
  maxT = atoi(argv[1]);
 
  m = semCreate(maxT);
  d = semCreate(1);
 
  int sock = socket(PF_INET6, SOCKSTREAM, 0);
  if(sock == -1) die("socket");
  struct sockaddr_sin6 addr = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(LISTEN_PORT),
    .sin6_addr = in6addr_any
  }
  if(bind(sock, (const struct sockaddr*) &addr, sizeof(addr)) == -1) {
    die("bind");
  }
  if(listen(sock, SOMAXCONN) == -1) {
    die("listen");
  }
  while(1) {
 
    P(m);
 
    int ac;
    ac = accept(socket, NULL, NULL);
    if (ac == -1) {
      continue;
    }
    pthread_t t;
    errno = pthread_create(&t, NULL, handleConnection(&ac));
    if(errno != 0) {
      die("pthread_create");
    }
    V(m);
  }
  return 0;
}
 
static void* handleConnection(void *argument) {
  if((FILE** kanal = sock2fds(&argument)) == NULL) {
    printf("%s\n", "FAILED");;
  }
  if(parseCommand(kanal[0], kanal[1]) == -1) {
    printf(kanal[1], "FAILED");
  }
 
  pthread_detach(pthread_self());
}
static int parseCommand(FILE *rx, FILE *tx) {
  char bef[4+ MAX_NAME_LEN];
  char nach[MAX_MSG_LEN + 1];
  if(fgets(bef, sizeof(bef), rx) == NULL) {
    return -1;
  }
  char* nach;
  char* vor;
  if((vor  = strtok(bef, " ")) == NULL) {
    return -1;
  }
  if((nach = strtok(NULL, "\n") == NULL) {
    return -1;
  }
  if((0 == strcmp(vor, "GET")) {
    return getMessages(nach, tx);
  }
  if (0 == strcmp(vor, "PUT")) {
  char bef2[MAX_MSG_LEN+1];
    if(fgets(bef2, sizeof(bef), rx) == NULL) {
      return -1;
    }
    return storeMessage(nach, bef2);
  }
  return -1;
}
 
 
static int getMessages(const char *username, FILE *tx) {
  char add[] = STORAGE_DIR+username;
  DIR* dir = opendir(add);
  if(dir == NULL) {
    return -1;
  }
  struct dirent* d;
  while ((errno = 0; d = readdir(dir)) != NULL) {
 
    if(strncmp("t", d->name) == 0) {
      FILE* f;
      if ((f = fdopen(d->name, "r")) == NULL) {
       continue;
      }
      char b[MAX_MSG_LEN];
      if(fgets(b, sizeof(b), f) == NULL) {
        return -1;
      }
      fprintf(tx, "%s\n", f);
    }
 
  }
  return 0;
}
 
static int storeMessage(const char *username, const char *message) {
        P(d);
        char buf[25]; //UINT_LEN
        snprintf(buf, sizeof( buf), "%d", id++);
        V(d);
 
         FILE *fp = fopen(buf, "w");
        if (!fp)
                return -1;
                
        fputs(message, fp);
        fflush(fp);

        char buf2[100];
        sprintf(buf2, "%s/%s", STORAGE_DIR, username);
        struct stat st = {0};
        if (stat(buf2, &st) == -1) { //wahrscheinlich nicht benötigt, da stat, mkdir nicht in ManPage
                if (mkdir(buf2, 0700) == -1)
                        return -1;
        }
        sprintf(buf2, "%s/%s/t%s", STORAGE_DIR, username, buf);
        if (rename(buf, buf2) == -1)
                return -1;
        fclose(fp);
        return 0;
}
