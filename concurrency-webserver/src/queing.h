#ifndef __QUEUE_H__

#include <stdio.h>
#include <stdbool.h>

// queue based on https://www.geeksforgeeks.org/c/queue-in-c/
#define MAX_SIZE 100

typedef struct 
{
	int items[MAX_SIZE];
	int front;
	int rear;
} Queue;

void initializeQueue(Queue *q);

bool isEmpty(Queue *q);

bool isFull(Queue *q);

void enqueue(Queue *q, int value);

void dequeue(Queue *q);

int peek(Queue *q);

void printQueue(Queue *q);

#endif // __REQUEST_H__
