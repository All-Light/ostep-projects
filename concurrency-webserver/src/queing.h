#ifndef __QUEUE_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// queue based on https://www.geeksforgeeks.org/c/queue-in-c/

typedef struct {
	int connection_fd;
	double task_start_time;
} QueueItem;


typedef struct 
{
	QueueItem* items;
	int buffer_size;
	int front;
	int rear;
} Queue;

void initializeQueue(Queue *q, size_t buffer);

void destroyQueue(Queue *q);

bool isEmpty(Queue *q);

bool isFull(Queue *q);

//static void resetQueue(Queue *q);

void enqueue(Queue *q, QueueItem *item);

void dequeue(Queue *q);

QueueItem *peek(Queue *q);

void printQueue(Queue *q);

#endif // __REQUEST_H__
