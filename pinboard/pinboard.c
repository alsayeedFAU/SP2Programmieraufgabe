#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define LISTEN_PORT 1952
#define MAX_CLIENTS 32
#define MAX_LINE_LEN 1024
#define MESSAGE_DIR "voar/lib/pinboard"

static void die(const char message[]){
	perror(message);
	exit(EXIT_FAILURE);
}

static void serve(int clientsock);
static int show(FILE *tx);
static int pin(FILE *rx, const char msgTitel[]);
static volatile int pcounter = 0;
static void handler(int signal);

int main(int argc, char **argv){
	if(chdir(MESSAGE_DIR))
		die("chdir");
	struct sigaction act = {
		.sa_handler = handler,
		.sa_flags = SA_RESTART
	};
	sigemptyset(&act.sa_mask);
	if(-1 == sigaction(SIGCHLD, &act, NULL))
		die("sigaction");
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if(-1 == listensock)
		die("socket");
	struct sockkaddr name = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(LISTEN_PORT),
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
		sigset_t set, empty;
		sigemptyset(&set);
		sigemptyset(&empty);
		sigaddset(&set, SIGCHLD);
		sigprocmask(SIG_BLOCK, &set, NULL);
		while(pcounter == MAX_CLIENTS)
			sigsuspend(&empty);
		pid_t pid= fork();
		if(-1 == pid){
			perror("fork");
			close(clientsock);
			sigprocmask(SIG_UNBLOCK, &set, NULL);
			continue;
		}else if(!pid){
			close(listensock);
			serve(clientsock);
		}else{
			pcounter++;
		}
		sigprocmask(SIG_UNBLOCK, &set, NULL);
		close(clientsock);
	}
}

static void handler(int signal){
	int err = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0)
		pcounter--;
	errno = err;
}

static void serve(int clientsock){
	int err = 0;
	int d = dup(clientsock);
	if(d ==-1){
		close(clientsock);
		exit(EXIT_FAILURE);
	}
	FILE *rx = fdopen(clientsock, "r");
	if(!rx){
		close(clientsock);
		close(d);
		exit(EXIT_FAILURE);
	}
	FILE *tx = fdopen(d, "w");
	if(!tx){
		fclose(rx);
		close(clientsock);
		exit(EXIT_FAILURE);
	}
	char buffer[MAX_LINE_LEN + 1];
	if(!fgets(buffer, sizeof(buffer), rx) || buffer[0] == '\n'){
		fclose(tx);
		fclose(rx);
		exit(EXIT_FAILURE);
	}
	char *c = strtok(buffer, "\n");
	if(!strcmp("show", c)){
		err = show(tx);
	}else if(!strncmp(c, "pin", 4)){
		err = pin(rx, c+4);
	}else{
		err = -1;
	}
	if(-1 == err){
		fprintf(tx, "Failed.");
	}else{
		fprintf(tx, "OK");
	}
	fclose(tx);
	fclose(rx);
	exit(EXIT_SUCCESS);
}

static int show(FILE *tx){
	int erg = 0;
	DIR *dir = opendir(".");
	if(!dir)
		return -1;
	struct dirent *d;
	while(errno = 0, (d = readdir(dir)) != NULL){
		if(d->d_name[0] == '.')
			continue;
		stuct stat s;
		if(-1 == lstat(d->d_name, &s))
			continue;
		if(!S_ISREG(s.st_mode))
			continue;
		char title[256 + 8];
		sprintf(title, "== %s ==\n", d->d_name);
		fputs(tilte, tx);
		int a;
		FILE *f = fopen(d->d_name, "r");
		if(!f)
			continue;
		while((a = fgetc(f)) != EOF)
			fputc(a, tx);
		if(ferror(f))
			erg = -1;
	}
	if(errno)
		erg = -1;
	closedir(dir);
	return erg;
}

static int pin(FILE *rx, const char msgTitle[]){
	if(!msgTitle)
		return -1;
	if(msgTitle[0] == '.' || strchr(msgTitle, '/'))
		return -1;
	FILE *f = fopen(msgTitle, "w");;
	if(!f)
		return -1;
	int a;
	while((a = fgetc(rx)) != EOF)
		fputc(a, f);
	if(ferror(rx)){
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}



		



