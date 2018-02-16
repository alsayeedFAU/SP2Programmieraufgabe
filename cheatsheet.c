//SIGNALBEHANDLUNG

static sigset_t block;
struct sigaction action = {
    .sa_handler = sighandler | SIG_IGN | SIG_DFL,
        /*  sighandler = selbst geschriebene Signalbehandlung
            SIGN_IGN: Signal ignorieren
            SIG_DFL: Standard-Signalbehandlung einstellen */
    .sa_mask = block,
    .sa_flags = SA_RESTART | SA_NOCLDSTOP
        /*  SA_NOCLDSTOP: SIGCHLD wird nur zugestellt, wenn ein Kindprozess terminiert, nicht wenn er gestoppt wird
            SA_RESTART: durch das Signal unterbrochene Systemaufrufe werden automatisch neu aufgesetzt */
};
sigemptyset(&action.sa_mask);
if (sigaction(SIGCHLD, &action, NULL) == -1) die("action");
sigemptyset(&block);
sigaddset(&block,SIGCHLD);

//SERVER: SOCKET ERSTELLEN, VERBINDUNGSANNAHME VORBEREITEN UND VERBINDUNGSANNAHME

int listenSock = socket(AF_INET6, SOCK_STREAM, 0);
if(listenSock==-1) { die("socket"); }
struct sockaddr_in6 name = {
    .sin6_family = AF_INET6,
    .sin6_port   = htons(port),
    .sin6_addr   = in6addr_any,
};
 
if (0 != setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))) die("setsockopt");
if(-1 == bind(listenSock, (struct sockaddr *) &name, sizeof(name))) die("bind");
if(-1 == listen(listenSock, SOMAXCONN)) die("listen");
 
for (;;) {
    int clientSock = accept(listenSock, NULL, NULL);
    if (-1 == client_sock) {
        die("accept");
    }
    //...
    close(clientSock);
}

// FUNKTION ZUR ERSTELLUNG EINES ZWEI-ELEMENTIGEN FILE*-ARRAYS ZUM LESEN UND SCHREIBEN

static FILE** sock2fds(int sock) {
    FILE *rx = fdopen(sock, "r"); if (!rx) {
        close(sock);
        return NULL;
    }
    int sock2 = dup(sock); if (sock2 < 0) {
        close(sock);
        return NULL;
    }
    FILE *tx = fdopen(sock2, "w");
    if(!tx) {
        fclose(rx); close(sock2); return NULL;
        }
        FILE **farray = calloc(2, sizeof(FILE *)); if (farray) {
            farray[0] = rx;
            farray[1] = tx; }
    return farray;
}

//CLIENT: DNS-ANFRAGE (werden wir wohl nicht brauchen)

char *host, *port;
struct addrinfo hints = {
     .ai_socktype = SOCK_STREAM,
     .ai_family = AF_UNSPEC,
     .ai_flags = AI_ADDRCONFIG,
};
struct addrinfo *head;
int err = getaddrinfo("xkcd.com", "80", &hints, &head);
if (err != 0) {
    if (err = EAI_System) {
        fperror("getaddrinfo");
    }
    else {
                fprintf(stderr, getaddrinfo: "%s", gai_strerror(err))
    }
}
 
int sock;
for (curr = head; curr != NULL; curr = curr->ai next) {
    sock = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
    if (sock == -1) continue;
    if (connect(sock, curr->ai addr, curr->ai addrlen) == 0) break;
    close(sock);
}
if (curr == NULL) {
    exit(EXIT_FAILURE);
}

// SEMAPHOR

#include<errno.h>
#include<pthread.h>
#include<stdlib.h>
/**Opaque type of a semaphore.*/
 
 
typedef struct SEM{
    volatile int a;
    pthread_mutex_t m;
    pthread_cond_t c;
} SEM;
 
SEM* semCreate(int initVal){
    SEM* sem = (SEM*) malloc(sizeof(SEM));
    if(NULL==sem) return NULL;
    if((errno=pthread_mutex_init(&sem->m,NULL))) {
        free(sem);
        return NULL;
    }
    if((errno=pthread_cond_init(&sem->c,NULL))) {
        pthread_mutex_destroy(&sem->m);
        free(sem);
        return NULL;
    }
    sem->a = initVal;
    return sem;
}
void semDestroy(SEM*sem){
    if((errno=pthread_mutex_destroy(&sem->m))) {
        pthread_cond_destroy(&sem->c);
        free(sem);
        return;
    }
    errno=pthread_cond_destroy(&sem->c);
    free(sem);
}
void P(SEM*sem){
    pthread_mutex_lock(&sem->m);
    while(0==sem->a) pthread_cond_wait(&sem->c,&sem->m);
    --sem->a;
    pthread_mutex_unlock(&sem->m);
}
void V(SEM*sem){
    pthread_mutex_lock(&sem->m);
    ++sem->a;
    pthread_cond_broadcast(&sem->c);
    pthread_mutex_unlock(&sem->m);
}

//JBUFFER

#include "bbuffer.h"
#include "sem.h"
#include <stdlib.h>
 
typedef struct BNDBUF {
    void **data;
    int size;
    volatile int readPos, writePos;
    SEM* readLock, writeLock;
} BNDBUF;
 
BNDBUF* bb_init(size_t size) {
    if(size >= INT_MAX)
        return NULL;
    BNDBUF* bb = (BNDBUF*) malloc(sizeof(BNDBUF));
    if(!bb)
        return NULL;
 
    bb->data = (void**) malloc(sizeof(*bb->data));
    if(!bb->data)
        return NULL;
 
     bb->size = size;
    bb->readPos = buf->writePose = 0;
    bb->readLock = sem_init(0);
    bb->writeLock = sem_init(size);
 
    return bb;
}
 
void bb_add(BNDBUF* bb, void* value) {
    P(bb->writeLock);
    bb->data[bb->write%bb->size] = value;
    bb->write ++;
    V(bb-readLock);
}
 
void* bb_get(BNDBUF* bb) {
    P(bb->readLock);
    void* get;
    int old = 0;
    int new = 1;
    do {
        old = bb->readPos;
        new = old + 1;
        get = bb->data[old];
    } while(__sync_bool_compare_and_swap(&bb->readPos, old, new%bb->size));
    V(bb->writeLock);
    return get;
}
