#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#define PROCESS_LIFE 20

void suspend_handler(int signum)
{
    printf("Process %d was suspended.\n", getpid());
    signal(SIGTSTP, SIG_DFL); // set to default handler and suspend
    raise(SIGTSTP);
}

void continue_handler(int signum) {
    printf("Process %d was resumed.\n", getpid());
    signal(SIGCONT, SIG_DFL); // set to default handler and resume
    raise(SIGCONT);
}

void stop_handler(int signum) {
    printf("Process %d has finished executing and was terminated.\n", getpid());
    signal(SIGINT, SIG_DFL); // set to default handler and suspend
    raise(SIGINT);
}

void set_signal_handlers() {
	signal(SIGTSTP, suspend_handler);
    signal(SIGCONT, continue_handler);
    signal(SIGINT, stop_handler);
}

int main(int argc, char **argv) {
	srand(time(NULL));
	int life = PROCESS_LIFE;
	//int r = rand() % 10000; // between 0 and 10000
	printf("Process with PID %d has started executing!\n", getpid());

	while (life > 0) {
		set_signal_handlers();
    	printf("Process %d is doing some processing...\n", getpid());
    	sleep(1); 
    	life --;
	}


	printf("Process %d terminated itself.\n", getpid());	
	return 0;
}