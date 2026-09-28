#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "request.h"
#include "io_helper.h"
#include "queing.h"

char default_root[] = ".";

typedef struct{
	int tid;
	Queue** q;
} connection_data;


static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

static void* worker(void* arg){
	connection_data* data = (connection_data*) arg;
	Queue* q = *data->q;
	pid_t tid = data->tid; // debug
	while(1){
		pthread_mutex_lock(&queue_lock);
		while(isEmpty(q)){
			pthread_cond_wait(&queue_cond, &queue_lock);
		}
		printf("Woke up as thread %d!\n", tid);
		int conn_fd = peek(q);
		if(conn_fd == -1) {
			printf("received invalid conn_fd %d\n",conn_fd);
			return NULL;
		}
		dequeue(q);
		printf("thread received conn_fd %d\n", conn_fd);
		pthread_mutex_unlock(&queue_lock);
		pthread_cond_signal(&queue_cond); // signal to other threads to wake and check the queue 

		request_handle(conn_fd);
		close_or_die(conn_fd);
	}

	return NULL;
}


//
// ./wserver [-d <basedir>] [-p <portnum>] 
// prompt> ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
// ./wserver -d ./html/ -p 8003 -t 1 -b 5 
//
int main(int argc, char* argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
	size_t num_threads = 0;
	size_t buffer = 0;
    
    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1){
		switch (c) {
		case 'd':
			root_dir = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			num_threads = atoi(optarg);
			if(num_threads < 1) { // sanity check
				fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
				exit(1);
			}			
			break;
		case 'b':
			buffer = atoi(optarg);
			if(buffer < 1) { // sanity check
				fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
			exit(1);
		}
	}
	// sanity check
	if(num_threads == 0 || buffer == 0){
		fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
		exit(1);
	}


    // run out of this directory
    chdir_or_die(root_dir);
	//printf("Buffer size %ld\n", buffer);
	//printf("We have %ld threads\n", num_threads);

	// create queue

	Queue* q = malloc(sizeof(Queue));
	initializeQueue(q, buffer);

	// create threads
	pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
	if(threads == NULL){
		perror("malloc");
		exit(1);
	}
	connection_data* thread_data = malloc(num_threads*sizeof(thread_data));
    for (int i = 0; i < num_threads; i++) {
		thread_data[i].q = &q;
		thread_data[i].tid = i;
        pthread_create(&threads[i], NULL, worker, (void*)&thread_data[i]);
    }

    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
		// enqueue this connection and wake threads
		//printf("enqueing conn_fd = %d\n", conn_fd);
		pthread_mutex_lock(&queue_lock);
		enqueue(q, conn_fd);
		pthread_mutex_unlock(&queue_lock);

		pthread_cond_signal(&queue_cond);

		//request_handle(conn_fd);
		//close_or_die(conn_fd);
    }

	pthread_mutex_destroy(&queue_lock);
	destroyQueue(q);
	free(thread_data);
	free(q);
    return 0;
}


    


 
