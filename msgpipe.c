#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>

static int runFilterApp(const char program[], const char recipent[], FILE *client);
static void handleRequest(FILE *client);
static void pex(const char*err){
	fprintf(stderr, "%s", err);
	exit(EXIT_FAILURE);
}

static void handler(int signal);
static volatile int pcounter = 0;

int main(int argc, char **argv){
	int port_id = 2012;
	if(argc >3)
		pex("usage");
	
	if(argc == 3)
		sscanf(argv[1], "%d", &port_id);
	
	struct sigaction ac = {
		.sa_handler = handler,
		.sa_flags = SA_RESTART
	};
	sigemptyset(&ac.sa_mask);
	if(-1 == sigaction(SIGCHLD, &ac, NULL))
		pex("sigaction");
	
	int listensock = socket(AF_INET6, SOCK_STREAM, 0);
	if( -1 == listensock)
		pex("socket");
	struct sockaddr_in6 name = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port_id),
		.sin6_addr = in6addr_any
	};
	if(-1 == bind(listensock, (struct sockaddr *)&name, sizeof(name)))
		pex("bind");
	if(-1 == listen(listensock, 8))
		pex("listen");
	for(;;){
		int clientsock = accept(listensock, NULL, NULL);
		if(-1 == clientsock)
			continue;
		FILE *c = fdopen(clientsock, "r+");
		if(!c){
			close(clientsock);
			continue;
		}
		sigset_t empty, set;
		sigemptyset(&empty);
		sigemptyset(&set);
		sigaddset(&set, SIGCHLD);
		sigprocmask(SIG_BLOCK, &set, NULL);
		while(pcounter >= 16)
			sigsuspend(&empty);
		pid_t pid = fork();
		if(-1 == pid){
			fclose(c);
			sigprocmask(SIG_UNBLOCK, &set, NULL);
			continue;
		}else if(!pid){
			handleRequest(c);
			fclose(c);
		}else{
			fclose(c);
			pcounter++;
			sigprocmask(SIG_UNBLOCK, &set, NULL);
		}
	}
}

static void handler(int signal){
	int err = errno;
	while(waitpid(-1, NULL, WNOHANG))
		pcounter--;
	errno = err;
}

static void handleRequest(FILE *client){
	char name2[257];
	if(!fgets(name2, 257, client)){
		fprintf(client, "Rejected.");
		return;
	}
	if(chdir("etc/msgpipe")){
		fprintf(client, "Rejected.");
		return;
	}
	FILE *file = fopen(name2, "r");
	if(!file){
		fprintf(client, "Rejected.");
		return;
	}
	
	char prog[257];
	if(!fgets(prog, 257,file)){
		fprintf(client, "Rejected");
		fclose(file);
		return;
	}
	fprintf(client, "Accepted");
	fclose(file);
	int erg = runFilterApp(prog, name2, client);
	if(erg >= 0){
		fprintf(client, "FAILED.");
		return;
	}
	fprintf(client, "OK");
}

static int runFilterApp(const char program[], const char name[], FILE *client){
	pid_t p = fork();
	if(-1 == p){
		return -1;
	}else if(!p){
		int fd = fileno(client);
		if(-1 == dup2(fd, STDIN_FILENO))
			exit(EXIT_FAILURE);
		execlp(program, program, name, NULL);
		exit(EXIT_FAILURE);
	}
	int stat;
	pid_t p2 = waitpid(p, &stat, 0);
	if(WIFEXITED(stat))
		return 0;
	return -1;
}
