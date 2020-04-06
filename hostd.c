#include "hostd.h"
#include "queue.h"

#define MAX_MEMORY 1024
#define MAX_USER_MEMORY 960
#define RESERVED_MEMORY 64
#define PRINTERS 2
#define SCANNERS 1
#define MODEMS 1
#define CDDRIVES 2
#define MAX_PROCESSES 1000
#define VERBOSE 1 // toggle this for detailed output
#define SUPERVERBOSE  0 // even more detailed output!

// global vars representing the 5 process queues, resources and time
Queue *dispatchQ, *userQ, *realtimeQ, *p1Q, *p2Q, *p3Q; 
int clock = 0; // represents global time of dispatcher
int numJobs = 0; // total number of jobs from file
volatile int availableMem = MAX_MEMORY; //mb available
volatile int MemArray[1024]= {0};
volatile int availableUserMem = MAX_USER_MEMORY;
volatile int printers = PRINTERS;
volatile int scanner = SCANNERS;
volatile int modem = MODEMS;
volatile int cddrives = CDDRIVES;
char *dispatchName = "DISPATCH QUEUE";
char *userName = "USER PRIORITY JOB QUEUE";
char *rtName =  "REALTIME PRIORITY JOB QUEUE";
char *p1Name = "PRIORITY 1 QUEUE";
char *p2Name = "PRIORITY 2 QUEUE";
char *p3Name = "PRIORITY 3 QUEUE";

//Function prototypes
void createDispatchList(FILE *fd);
void initQueues();
void freeQueues();
bool queuesAreNotEmpty();
bool findMemSpaceReal(Queue *head);
bool checkMemSpaceReal(Queue *head);
int createProcess(Queue *q); 
bool findMemSpaceUser(Queue *head);
bool checkMemSpaceUser(Queue *head);
void freeMemSpace(Queue *head);
bool resourcesAvailable(Queue *head);
void assignResources(Queue *head);
bool userOrReal( Queue *head);
bool checkUserOrReal( Queue *head);
void printJobDetails( Queue *head);

int main(int argc, char **argv) {
	PCB *job; // pointer used to move jobs between queues
	
	int jobPriority;
	int processStatus;
	//open file of jobs
	if(argc < 2) {
		printf("Dispatch list not found!\n");
		return 0;
	}

	FILE *fd;
	fd = fopen(argv[1], "r");
	
	if(fd == NULL) {
		printf("Could not open file %s.\n", argv[1]);
		return 0;
	}

	initQueues(); 
	printf("Queues initialized successfully!\n");
	createDispatchList(fd);
	printf("Read and stored all jobs in dispatch list!\n");
	fclose(fd);
	// print out initial dispatch list
	if (VERBOSE) printQueue(dispatchName, dispatchQ);

	// START DISPATCHER
	while(1) {
		printf("\n-----------------------------------------\n");
		printf("DISPATCHER TIME: %d SECONDS\n", clock);		
		printf("-----------------------------------------\n");

		if (SUPERVERBOSE) printf("DISPATCHER RESOURCE REPORT:\n");
		if (SUPERVERBOSE) printf("Available Memory: %d\n", availableMem);
		if (SUPERVERBOSE) printf("Printers: %d\n", printers);
		if (SUPERVERBOSE) printf("Scanner: %d\n", scanner);
		if (SUPERVERBOSE) printf("Modem: %d\n", modem);
		if (SUPERVERBOSE) printf("CD Drives: %d\n", cddrives);

		// move all jobs with this time from dispatch to the submission queues
		// this happens on EVERY tick 
		if (!isEmpty(dispatchQ)) {
		    //assign all the jobs that are currently in the dispatch Queue 
			while (dispatchQ->process->arrival_time <= clock) {
			  //assign the jobs to the correct submission queue
				jobPriority = dispatchQ->process->priority; 
				if (jobPriority == 0) { // realtimeq priority = 0
					if (VERBOSE) printf("A new realtime job has arrived.\n");
					job = dequeueFront(&dispatchQ);
					enqueueJob(realtimeQ, job);
					if (SUPERVERBOSE) printQueue(rtName, realtimeQ);
				} else if (jobPriority==1 || jobPriority==2 || jobPriority==3) {
					if (VERBOSE) printf("A new user job has arrived.\n");
					job = dequeueFront(&dispatchQ);
					enqueueJob(userQ, job);
					if (SUPERVERBOSE) printQueue(userName, userQ);
				}
				if (isEmpty(dispatchQ)) break;	
			}
		}
		
	    // distribute user jobs into their priority queues bases on resources
	    // happens on EVERY TICK
   	    if(isEmpty(userQ)==false){
   	    	int userQlength = getLength(userQ);	
		    int currJob = 0;
		 
 			// Go through entire queue only once
		    while(currJob < userQlength){
				
		        //checking to make sure all the resources are avalable for the job
		        if(resourcesAvailable(userQ)){
		        	assignResources(userQ);
		        	printf("Successfuly allocated resources to a new user job.\n");
				    //gets the priority and the job off the userQ
				    int userPriority;
					userPriority = userQ->process->priority;
					job = dequeueFront(&userQ);
					if (SUPERVERBOSE) printf("User Priority: %d\n", userPriority);
					//puts the job in the correct priority userQ
					if(userPriority==1){
						enqueueJob(p1Q, job);
					}
					if(userPriority==2){
						enqueueJob(p2Q, job);
					}
					if(userPriority ==3){
						enqueueJob(p3Q, job);
					}

		        // if resources arent avalable then job goes to the end of the queue  
		        } else {
		        	// safety check on if job requires too many resources
				    if(userQ->process->mem_req > MAX_USER_MEMORY ||
				       userQ->process->printers > PRINTERS ||
				       userQ->process->scanners > SCANNERS ||
				       userQ->process->modems > MODEMS ||
				       userQ->process->cds > CDDRIVES) {
				        // simply remove job
				        job = dequeueFront(&userQ);
					    free(job);
					    userQlength--;
				    }else {
				    	// cycle job to back of queue
				    	printf("A user job is waiting on resources...\n");
				        job = dequeueFront(&userQ);
					    enqueueJob(userQ, job);
				    }
		        }
			   
			    currJob++; 
			    if (SUPERVERBOSE) printQueue(p1Name, p1Q);
			    if (SUPERVERBOSE) printQueue(p2Name, p2Q);
			    if (SUPERVERBOSE) printQueue(p3Name, p3Q);
	        } // end while 
	    } // end userQ distributions
		
		/* Now check all the queues for a job to run. */
	    // Check the realtimeQ for a job first!
		if(isEmpty(realtimeQ)==false){
			
			if(realtimeQ->process->pid < 0) {
				// job hasnt started yet so fork and exec
				realtimeQ->process->pid = fork();

		 		if (realtimeQ->process->pid < 0) {
		 			fprintf(stderr, "Dispatcher failed to fork new process.");
					return 0;
		 		}
		 		else if(realtimeQ->process->pid == 0) {
		 			printJobDetails(realtimeQ);
		        	execl("./process", "process", "",NULL);
		 		} else {
		 			sleep(1);
		 		}
		 	}
			else {
				sleep(1); // RT processes never pause so just let it run
		 	}	

		    if (SUPERVERBOSE) printQueue(rtName, realtimeQ);
		    realtimeQ->process->time_left--; //decrement time
		    //check to see if it is zero 
		    if (VERBOSE) printf("Time left in real time process: %d\n", realtimeQ->process->time_left);
		    
		    if(realtimeQ->process->time_left == 0){
		    	kill(realtimeQ->process->pid, SIGINT); // kill the process
			  	waitpid(realtimeQ->process->pid, &processStatus, WUNTRACED);
		      	freeMemSpace(realtimeQ);
		      	job = dequeueFront(&realtimeQ);
		      	free(job);
		    }
		  


		/* Now check lower priority Queues.
		 The code for the lower priority queues will be very symmetrical */
        } else if(isEmpty(p1Q)==false) {

			if(p1Q->process->pid < 0) {
				// job hasnt started yet so fork and exec
				p1Q->process->pid = fork();

		 		if (p1Q->process->pid < 0) {
		 			fprintf(stderr, "Dispatcher failed to fork new process.");
					return 0;
		 		}
		 		else if(p1Q->process->pid == 0) {
		 			printJobDetails(p1Q);
		        	execl("./process", "process", "",NULL);
		 		} else {
		 			sleep(1);
		 		}
		 	}
			else {
				// it was previously paused, so resume it
		 		if (SUPERVERBOSE) printf("Attempting to resume process...\n");
				kill(p1Q->process->pid, SIGCONT);
				sleep(1); // let it run for 1 s
		 	}	
		 	
		    if (SUPERVERBOSE) printQueue(p1Name, p1Q);
			//decrement time
		    p1Q->process->time_left--;
			//check to see if it is zero 
			if (VERBOSE) printf("Time left in p1Q process: %d\n", p1Q->process->time_left);

		    if(p1Q->process->time_left == 0){
		    	kill(p1Q->process->pid, SIGINT); // kill the process
			  	waitpid(p1Q->process->pid, &processStatus, WUNTRACED);
		        freeMemSpace(p1Q); // free its memory
				
		        // free all resources
				printers += p1Q->process->printers;
				scanner += p1Q->process->scanners;
				modem += p1Q->process->modems;
				cddrives += p1Q->process->cds;
				// remove the PCB from the queue
				job = dequeueFront(&p1Q);
				free(job);
       		}else { // pause it and decrease its priority
       			kill(p1Q->process->pid, SIGTSTP);
				waitpid(p1Q->process->pid, &processStatus, WUNTRACED);	
			    job = dequeueFront(&p1Q);
				enqueueJob(p2Q, job);	
			}



		//check second user priority queue
		}else if(isEmpty(p2Q)==false){

			if(p2Q->process->pid < 0) {
				// job hasnt started yet so fork and exec
				p2Q->process->pid = fork();

		 		if (p2Q->process->pid < 0) {
		 			fprintf(stderr, "Dispatcher failed to fork new process.");
					return 0;
		 		}
		 		else if(p2Q->process->pid == 0) {
		 			printJobDetails(p2Q);
		        	execl("./process", "process", "",NULL);
		 		} else {
		 			sleep(1);
		 		}
		 	}
			else {
				// it was previously paused, so resume it
		 		if (SUPERVERBOSE) printf("Attempting to resume process...\n");
				kill(p2Q->process->pid, SIGCONT);
				sleep(1); // let it run for 1 s
		 	}	
		 	
		    if (SUPERVERBOSE) printQueue(p2Name, p2Q);
			//decrement time
		    p2Q->process->time_left--;
			//check to see if it is zero 
			if (VERBOSE) printf("Time left in p2Q process: %d\n", p2Q->process->time_left);

		    if(p2Q->process->time_left == 0){
		    	kill(p2Q->process->pid, SIGINT); // kill the process
			  	waitpid(p2Q->process->pid, &processStatus, WUNTRACED);
		        freeMemSpace(p2Q); // free its memory
				
		        // free all resources
				printers += p2Q->process->printers;
				scanner += p2Q->process->scanners;
				modem += p2Q->process->modems;
				cddrives += p2Q->process->cds;
				// remove the PCB from the queue
				job = dequeueFront(&p2Q);
				free(job);
       		}else { // pause it and decrease its priority
       			kill(p2Q->process->pid, SIGTSTP);
				waitpid(p2Q->process->pid, &processStatus, WUNTRACED);	
			    job = dequeueFront(&p2Q);
				enqueueJob(p3Q, job);	
			}



		//check third priority queue
		}else if(isEmpty(p3Q)==false){

			if(p3Q->process->pid < 0) {
				// job hasnt started yet so fork and exec
				p3Q->process->pid = fork();

		 		if (p3Q->process->pid < 0) {
		 			fprintf(stderr, "Dispatcher failed to fork new process.");
					return 0;
		 		}
		 		else if(p3Q->process->pid == 0) {
		 			printJobDetails(p3Q);
		        	execl("./process", "process", "",NULL);
		 		}
		 		else {
		 			sleep(1);
		 		}
		 	}
			else {
				// it was previously paused, so resume it
		 		if (SUPERVERBOSE) printf("Attempting to resume process...\n");
				kill(p3Q->process->pid, SIGCONT);
				sleep(1); // let it run for 1 s
		 	}
		 		
		 	
		    if (SUPERVERBOSE) printQueue(p3Name, p3Q);
			//decrement time
		    p3Q->process->time_left--;
			//check to see if it is zero 
			if (VERBOSE) printf("Time left in p3Q process: %d\n", p3Q->process->time_left);

		    if(p3Q->process->time_left == 0){
		    	kill(p3Q->process->pid, SIGINT); // kill the process
			  	waitpid(p3Q->process->pid, &processStatus, WUNTRACED);
		        freeMemSpace(p3Q); // free its memory
				
		        // free all resources
				printers += p3Q->process->printers;
				scanner += p3Q->process->scanners;
				modem += p3Q->process->modems;
				cddrives += p3Q->process->cds;
				// remove the PCB from the queue
				job = dequeueFront(&p3Q);
				free(job);
       		} else { // pause it and cycle the queue b/c its now Round Robin
       			kill(p3Q->process->pid, SIGTSTP);
				waitpid(p3Q->process->pid, &processStatus, WUNTRACED);	
			    job = dequeueFront(&p3Q);
				enqueueJob(p3Q, job);	
			}

		} else {
			sleep(1); // if nothing new happens, sleep anyway
		} 
		
		// increment clock
	    clock++;

	    // exit the dispatcher only once all queues are empty
		if(isEmpty(dispatchQ) && isEmpty(userQ) && isEmpty(realtimeQ) && 
			isEmpty(p1Q) && isEmpty(p2Q) && isEmpty(p3Q)){
		  break;
		}
	}

	printf("All jobs ran to completion. Terminating dispatcher...\n");
	// free all allocated mem before exiting
	freeQueues();
	return 0;
}


/* Builds the list of jobs by parsing the input file.
Jobs are already sorted by ascending start time 
file contains 8 pieces of job info: Arrival time, priority, cpu time,
memory, printers, scanners, modems, CDs*/
void createDispatchList(FILE *fd) {
	char linebuf[100];
	char *processInfo;
	// read the file line by line
	while (fgets(linebuf, 100, fd) != NULL) {
		// create a struct for this job and insert it into the dispatch list
		PCB *newJob = malloc(sizeof(PCB));
		assert(newJob != NULL);

		newJob->pid = -1; //process is not 'live' yet
		// break the line up by the , delims and store job info 
		processInfo = strtok(linebuf, ",");
		newJob->arrival_time = atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->priority = atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->cpu_time = atoi(processInfo); 
		newJob->time_left = atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->mem_req = atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->printers = atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->scanners = atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->modems= atoi(processInfo); 
		processInfo = strtok(NULL, ",");
		newJob->cds = atoi(processInfo); 
		numJobs ++;

		// add job to the dispatch list
		enqueueJob(dispatchQ, newJob);
		if (numJobs == MAX_PROCESSES) break; // safety check on job num
	}
}

/* Creates a new process */
int createProcess(Queue *q) {

	q->process->pid = fork();
	if (q->process->pid < 0) {
		return 0;
	}
	else if(q->process->pid == 0) {
		printf("\nA new process was started with parameters:\n");
		printf("PID: %d\n", (int)getpid());
		printf("Priority: %d\n", q->process->priority);
		printf("CPU time remaining: %d\n", q->process->time_left);
		printf("Memory location: 0x%d\n", q->process->mem_start);
		printf("Block size: %dMb\n", q->process->mem_req);
		printf("Resources requested (printer, scanner, modem, cd): (%d,%d,%d,%d)\n\n", 
		q->process->printers,q->process->scanners,q->process->modems, q->process->cds);
		
		execl("./process", "process", "",NULL);
	}	
	return 1;
}

/* checks to see if there are enough resources to run a process */
bool resourcesAvailable(Queue *q) {
	
	return( checkUserOrReal(q) &&
	   	   q->process->printers <= printers &&
		   q->process->scanners <= scanner &&
		   q->process->modems <= modem &&
		   q->process->cds <= cddrives);
}

/* assign resources and memory to a process */
void assignResources(Queue *q) {
    // assign by subtracting from the global amounts
    // perform memory allocation here
    userOrReal(q);
    printers -= q->process->printers;
	scanner -= q->process->scanners;
	modem -= q->process->modems;
	cddrives -= q->process->cds;

	if (SUPERVERBOSE) printf("Memory block used: %d - %d\n", q->process->mem_start,(q->process->mem_start+q->process->mem_req));
	if (SUPERVERBOSE) printf("Available printers: %d\n", printers);
	if (SUPERVERBOSE) printf("Available scanners: %d\n", scanner);
	if (SUPERVERBOSE) printf("Available modems: %d\n", modem);
	if (SUPERVERBOSE) printf("Available cd drives: %d\n\n", cddrives);
}

/* initialize memory for queues */
void initQueues() {
	dispatchQ = initQueue();
  	realtimeQ = initQueue();
  	userQ = initQueue();
 	 p3Q = initQueue();
 	 p2Q = initQueue();
 	 p1Q = initQueue();
}

/* free queue memory after dispatcher quits*/
void freeQueues() {
	deleteQueue(dispatchQ);
  	deleteQueue(realtimeQ);
 	deleteQueue(userQ);
 	deleteQueue(p1Q);
	deleteQueue(p2Q);
 	deleteQueue(p3Q);
}

/* Prints out jobs details */
void printJobDetails( Queue *q) {
	printf("\nA new process was started with parameters:\n");
	printf("PID: %d\n", (int)getpid());
	printf("Priority: %d\n", q->process->priority);
	printf("CPU time remaining: %d\n", q->process->time_left);
	printf("Memory location: 0x%d\n", q->process->mem_start);
	printf("Block size: %dMb\n", q->process->mem_req);
	printf("Resources requested (printer, scanner, modem, cd): (%d,%d,%d,%d)\n\n", 
	q->process->printers,q->process->scanners,q->process->modems, q->process->cds);
}

bool findMemSpaceReal(Queue *head){
  //real time memory allocation
	int i =0;
   bool avalable= false;
  int mem = head->process->mem_req;
  int slot;
  int count;
    for(slot=0;slot <  MAX_MEMORY; slot++ ){
            if(MemArray[slot]==0){
                   for(count =0; count < mem; count++){
		        if((MemArray[slot+count]==1)||(slot+count >= MAX_MEMORY)){
	 	               break;
		         }
			if(count == (mem-1)){
	                       head->process->mem_start = slot;
			       avalable = true;
			       break;
			}
		   }
            }
	    if(avalable == true){
	      break;
	    }
    }
    for(i=0; i < mem; i++){
      MemArray[slot+i]=1;
    }
    return avalable;
}
  
bool checkMemSpaceReal(Queue *head){
 //real time memory allocation

   bool avalable= false;
  int mem = head->process->mem_req;
  int slot;
  int count;
    for(slot=0;slot <  MAX_MEMORY; slot++ ){
            if(MemArray[slot]==0){
                   for(count =0; count < mem; count++){
		        if((MemArray[slot+count]==1)||(slot+count >= MAX_MEMORY)){
	 	               break;
		         }
			if(count == (mem-1)){
	                       head->process->mem_start = slot;
			       avalable = true;
			       break;
			}
		   }
            }
	    if(avalable == true){
	      break;
	    }
    }
    
    return avalable;
}




bool findMemSpaceUser(Queue *head){
  //user memory allocation
  //look in the array and try to find a space
  //head->process->mem_req
  //head->process->mem-start
  // if space is found mark it as used
  int i = 0;
  bool avalable= false;
  int mem = head->process->mem_req;
  int slot;
  int count;
    for(slot=0;slot <  MAX_USER_MEMORY; slot++ ){
            if(MemArray[slot]==0){
                   for(count =0; count < mem; count++){
		        if((MemArray[slot+count]==1)||(slot+count >= MAX_USER_MEMORY)){
	 	               break;
		         }
			if(count == (mem-1)){
	                       head->process->mem_start = slot;
			       avalable = true;
			       break;
			}
		   }
            }
	    if(avalable == true){
	      break;
	    }
    }
    for(i=0; i < mem; i++){
      MemArray[slot+i]=1;
    }
    return avalable;
}

bool checkMemSpaceUser(Queue *head){
  //user memory allocation
  //look in the array and try to find a space
  //head->process->mem_req
  //head->process->mem-start
  // if space is found mark it as used

  bool avalable= false;
  int mem = head->process->mem_req;
  int slot;
  int count;
    for(slot=0;slot <  MAX_USER_MEMORY; slot++ ){
            if(MemArray[slot]==0){
                   for(count =0; count < mem; count++){
		        if((MemArray[slot+count]==1)||(slot+count >= MAX_USER_MEMORY)){
	 	               break;
		         }
			if(count == (mem-1)){
	                       head->process->mem_start = slot;
			       avalable = true;
			       break;
			}
		   }
            }
	    if(avalable == true){
	      break;
	    }
    }
   
    return avalable;
}

void freeMemSpace(Queue *head){
  //find the space that was allocated to that process and free it.
 //head->process->mem_req
  int i = head->process->mem_start;
  int j;
  for(j=0 ; j < head->process->mem_req; j++){
       MemArray[i+j] = 0;
  }
  //head->process->mem-start

}


bool userOrReal( Queue *head){
  bool good;
  int priority;
  priority = head->process->priority;
  if(priority == 0){
   good=  findMemSpaceReal(head);
  }else{
  good =  findMemSpaceUser(head);
  }
  return good; 
}


bool checkUserOrReal( Queue *head){
  bool good;
  int priority;
  priority = head->process->priority;
  if(priority == 0){
   good=  checkMemSpaceReal(head);
  }else{
  good =  checkMemSpaceUser(head);
  }
  return good; 
}
