#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

/* Universal struct that represents a job/process */
typedef struct Process {
	pid_t pid;
	int arrival_time;
	int priority;
	int cpu_time;
	int time_left;
	int mem_req;
    int mem_start;
	int printers;
	int scanners;
	int modems;
	int cds;
} PCB;

typedef struct processQueue {
   PCB *process;
   struct processQueue* next;
} Queue;

