#ifndef __QUEUE_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// queue based on https://www.geeksforgeeks.org/c/queue-in-c/

typedef struct 
{
	int* connection_fds;
	int buffer_size;
	int front;
	int rear;
} Queue;

void initializeQueue(Queue *q, size_t buffer);

bool isEmpty(Queue *q);

bool isFull(Queue *q);

void enqueue(Queue *q, int value);

void dequeue(Queue *q);

int peek(Queue *q);

void printQueue(Queue *q);

#endif // __REQUEST_H__
