// Add all queue function prototypes here

Queue* initQueue();
void deleteQueue(Queue *head);
void enqueueJob(Queue *head, PCB *newJob);
PCB* dequeueFront(Queue **headPointer);
void printQueue(char *qName, Queue *head);
bool isEmpty(Queue *head); 
int getLength(Queue *head);
