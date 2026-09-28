#include "queing.h"
// queue based on https://www.geeksforgeeks.org/c/queue-in-c/

#define DEBUG 1

void initializeQueue(Queue *q, size_t buffer){
    if(buffer < 1){
        printf("Could not allocate buffer of size 0\n");
        return;
    }
    q->connection_fds = malloc(buffer*sizeof(int));
    if(q->connection_fds == NULL){
        printf("Could not allocate buffer\n");
        exit(1);
    }
    q->buffer_size = buffer;
    q->front = -1;
    q->rear = 0;
}

void destroyQueue(Queue *q){
    free(q->connection_fds);
}


// move front to 0 and rear to queue length
// FIXME: performance issue running this on every enqueue?
static void resetQueue(Queue *q){
    int nr_items = q->rear - q->front;
    if(nr_items >= q->buffer_size){
        if(DEBUG) printf("queue is genuinely full\n");
        return; // queue is genuinely full
    }
    // nr items is less than buffer size
    printf("reset queue\n");
    // shift entire array to start
    memmove(&q->connection_fds[0], &q->connection_fds[q->front], nr_items*sizeof(int));
    q->front = -1;
    q->rear = nr_items-1;
}



bool isEmpty(Queue *q){
    return (q->front == q->rear -1);
}

bool isFull(Queue *q){
    // try to move memory to unused slots to free up the buffer
    //printf("isFull buffer size: %d\n", q->buffer_size);
    //printf("isFull q->front: %d\n", q->front);
    //printf("isFull q->rear: %d\n", q->rear);
    if(q->rear == q->buffer_size){
        resetQueue(q);
    }
    // if its still equal we are genuinely full
    return (q->rear == q->buffer_size);
}

// Add a new value to the end of the queue
void enqueue(Queue *q, int value){
    if(isFull(q)){
        if(DEBUG) printf("Request queue is full!\n");
        return;
    }
    q->connection_fds[q->rear] = value;
    q->rear++;
}


// remove the first queued element
void dequeue(Queue *q){
    if(isEmpty(q)){
        if(DEBUG) printf("Queue is empty!\n");
        return;
    }
    q->front++;
}

int peek(Queue *q){
    if(isEmpty(q)){
        if(DEBUG) printf("Queue is empty!\n");
        return -1;
    }
    return q->connection_fds[q->front+1];
}

void printQueue(Queue *q){
    if(isEmpty(q)){
        if(DEBUG) printf("Queue is empty!\n");
        printf("Current Queue: \n");
        return;
    }
    printf("Current Queue: ");
    for (int i = q->front + 1; i < q->rear; i++){
        printf(" %d", q->connection_fds[i]);
    }
    printf("\n");
}
