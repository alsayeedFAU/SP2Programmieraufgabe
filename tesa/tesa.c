#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_LINE_LEN 102
#define MAX_PROGNAME 256
#define MAX_TESTCASES 100
#define TESTCASE_TIMEOUT 3

struct testcase {
	char progName[MAX_PROGNAME];
	int status;
	pid_t pid;
};

struct testsuite {
	int numberOfTestcases;
	struct testcase testcases[MAX_TESTCASES];
};

static int die(const char message[]){
	perror(message);
	exit(EXIT_FAILURE);
}

#define S_ISEXEC(mode) (((S_IXUSR|S_IXGRP|S_IXOTH) &mode) > 0)

static void printSummary(const struct testsuite ts[]);

static void readTestsuite(void);
static void runTestsuite(void);
static void handler(int);
static volatile int pcounter = 0;
static void handlerCHLD(int);
static void handlerAL(int);
static struct testsuite test;

int main(int argc, char **argv){
	struct sigaction act = {
		.sa_handler = handler,
		.sa_flags = SA_RESTART
	};
	sigemptyset(&act.sa_mask);
	if(-1 == sigaction(SIGINT, &act, NULL))
		die("sigaction");
	struct sigaction act2 = {
		.sa_handler = handlerCHLD,
		.sa_flags = SA_RESTART
	};
	sigemptyset(&act.sa_mask);
	if( -1 == sigaction(SIGCHLD, &act2, NULL))
		die("sigaction");
	struct sigaction act3 = {
		.sa_handler = handlerAL,
		.sa_flags = SA_RESTART
	};
	sigemptyset(&act3.sa_mask);
	if(-1 == sigaction(SIGALRM, &act3, NULL))
		die("sigaction");
	
	while(42){
		char buffer[MAX_LINE_LEN];
		if(!fgets(buffer, sizeof(buffer), stdin)){
			break;
		}else if(!strcmp("readTestsuite\n", buffer)){
			readTestsuite();
		}else if(!strcmp("runTestsuite\n", buffer)){
			runTestsuite();
		}else if(!strcmp(buffer, "\n")){
			continue;
		}else{
			fprintf(stderr, "Unknown Command");
		}
	}
	if(ferror(stdin))
		die("fgets");
}

static void readTestsuite(void){
	DIR *dir = opendir(".");
	if(!dir){
		fprintf(stderr, "opendir");
		return;
	}
	int counter = 0;
	struct dirent *d;
	while(errno = 0, (d = readdir(dir)) != NULL){
		struct stat s;
		if(-1 == lstat(d->d_name, &s))
			continue;
		if(S_ISREG(s.st_mode) && S_ISEXEC(s.st_mode)){
			test.numberOfTestcases++;
			test.testcases[counter].status = 0;
			strcpy(test.testcases[counter].progName, d->d_name);
			test.testcases[counter].pid = 0;
			counter++;
		}
		if(counter == 100)
			break;
	}
	closedir(dir);
	if(errno)
		fprintf(stderr, "readdir");
}

static void runTestsuite(void){
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	for(int i = 0; i < test.numberOfTestcases; i++){
		sigprocmask(SIG_BLOCK, &set, NULL);
		pid_t p = fork();
		if(p == -1){
			fprintf(stderr, "fork");
			continue;
		}else if(!p){
			sigprocmask(SIG_UNBLOCK, &set, NULL);
			execl(test.testcases[i].progName, test.testcases[i].progName, NULL);
			die("exec");
		}else{
			pcounter++;
			test.testcases[i].pid = p;
			sigprocmask(SIG_UNBLOCK, &set, NULL);
		}
	}
	sigset_t set2 ,empty;
	sigemptyset(&set2);
	sigaddset(&set2, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set2, NULL);
	while(pcounter != 0)
		sigsuspend(&empty);
	sigprocmask(SIG_UNBLOCK, &set2, NULL);
	printSummary(&test);
}

static void handlerCHLD(int signal){
	int err = errno;
	int s;
	pid_t p;
	while((p = waitpid(-1 , &s, WNOHANG) > 0)){
		pcounter--;
		for(int i = 0; i < test.numberOfTestcases; i++){
			if(test.testcases[i].pid == 0){
				test.testcases[i].status = s;
				break;
			}
		}
	}
	errno = err;
}

static void handler(int s){
	alarm(3);
}

static void handlerAL(int n){
	int err = errno;
	for(int i = 0; i < test.numberOfTestcases; i++)
		kill(test.testcases[i].pid, SIGKILL);
	errno = err;
}
