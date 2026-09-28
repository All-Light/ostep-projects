#include "queing.h"
// queue based on https://www.geeksforgeeks.org/c/queue-in-c/

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

bool isEmpty(Queue *q){
    return (q->front == q->rear -1);
}

bool isFull(Queue *q){
    return (q->rear == q->buffer_size);
}

// Add a new value to the end of the queue
void enqueue(Queue *q, int value){
    if(isFull(q)){
        printf("Request queue is full!\n");
        return;
    }
    q->connection_fds[q->rear] = value;
    q->rear++;
}


// remove the first queued element
void dequeue(Queue *q){
    if(isEmpty(q)){
        printf("Queue is empty!\n");
        return;
    }
    q->front++;
}

int peek(Queue *q){
    if(isEmpty(q)){
        printf("Queue is empty!\n");
        return -1;
    }
    return q->connection_fds[q->front+1];
}

void printQueue(Queue *q){
    if(isEmpty(q)){
        printf("Queue is empty!\n");
        return;
    }
    printf("Current Queue: ");
    for (int i = q->front + 1; i < q->rear; i++){
        printf("%d ", q->connection_fds[i]);
    }
    printf("\n");
}
