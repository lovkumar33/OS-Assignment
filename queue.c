#include "hostd.h"


/* Create a new queue */
Queue* initQueue() {
  Queue* newQueue = malloc(sizeof(Queue));
  assert(newQueue != NULL); //ensure malloc worked

  newQueue->process = NULL;
  newQueue->next = NULL;

  return newQueue;
} 

/* Walks through the queue freeing each queue element
as well as the process pointed to by that element */
void deleteQueue(Queue *head) {
    while(head != NULL) {
        // store the next item before freeing current one
        Queue *temp = head->next;
        free(head->process);
        free(head);
        head = temp; //move to next element
    }
}

/* Adds an element to the back of the queue */
void enqueueJob(Queue *head, PCB *newJob) {
  // if this is the first element
  if (head->process == NULL) {
      head->process = newJob;
  }
  else {
      Queue *newQueueEntry = initQueue(); // make new queue entry first
      newQueueEntry->process = newJob; // set the job
      // walk through queue and insert the job at the end.
      while (head->next != NULL) {
          head = head->next;
      }

      head->next = newQueueEntry;
  }
}

/* Removes and returns the first job in the queue. 
  From there it can either be used or freed by the caller
  NOTE: If only 1 queue element left, it should not be freed.*/
PCB* dequeueFront(Queue **headPointer) {
    Queue *head = *headPointer;
    assert(head != NULL);
    // Create a pointer to the process so its reference can be returned.
    PCB *job = head->process;

    // if there is a process stored here return it
    // otherwise we don't want to delete the only element in the list
    if (head->next != NULL) {
        *headPointer = head->next;
        free(head);
       
    }
    else {
      // this is the last element in the queue so just make the job null
        head->process = NULL;
    }

    return job;
}

/* Prints out the given queue with select data*/
void printQueue(char *qName, Queue *head) {

    printf("\n%s CONTENTS =========================\n", qName);
    printf("PID ARV_TIME TIME_LEFT  MEM   RESOURCES(P,S,M,C)\n");
    
   while (head->process != NULL) {
      pid_t pid = head->process->pid;
      int arrTime = head->process->arrival_time;
      int timeRem = head->process->time_left;
      int mem = head->process->mem_req;
      int p = head->process->printers;
      int s = head->process->scanners;
      int m = head->process->modems;
      int c = head->process->cds;

      printf("%d     %d         %d      %d      (%d,%d,%d,%d)\n",
          pid, arrTime, timeRem, mem, p,s,m,c);
      if (head->next == NULL)
          break;
      else
          head = head->next;
   } 
   printf("==================================================\n\n");
}

bool isEmpty(Queue *head) {
  if (head->process == NULL)
    return true;
  else
    return false;
}

/* Returns length of queue */
int getLength(Queue *head){
  if(head==NULL) return 0;
  int length = 1;

  while (head->process != NULL) {
    if (head->next == NULL)
	     return length;
    else {
        head = head->next;
	     length++;
    }
  } 
  return length;
}
