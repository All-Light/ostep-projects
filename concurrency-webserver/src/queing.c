#include "queing.h"
// queue based on https://www.geeksforgeeks.org/c/queue-in-c/

void initializeQueue(Queue *q){
    q->front = -1;
    q->rear = 0;
}

bool isEmpty(Queue *q){
    return (q->front == q->rear -1);
}

bool isFull(Queue *q){
    return (q->rear == MAX_SIZE);
}

// Add a new value to the end of the queue
void enqueue(Queue *q, int value){
    if(isFull(q)){
        printf("Request queue is full!\n");
        return;
    }
    q->items[q->rear] = value;
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
    return q->items[q->front+1];
}

void printQueue(Queue *q){
    if(isEmpty(q)){
        printf("Queue is empty!\n");
        return;
    }
    printf("Current Queue: ");
    for (int i = q->front + 1; i < q->rear; i++){
        printf("%d ", q->items[i]);
    }
    printf("\n");
}
