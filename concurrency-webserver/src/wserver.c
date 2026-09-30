#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include "request.h"
#include "io_helper.h"
#include "queing.h"


#define MAXBUF (8192) // shouldnt duplicate...
#define DEBUG 1

char default_root[] = ".";


static double get_wall_seconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double seconds = tv.tv_sec + (double)tv.tv_usec / 1000000;
  return seconds;
}


typedef struct{
	int tid;
	int file_d; 
	Queue** q;
	double program_start_time;
} connection_data;


static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

// void append_to_log(int file_d, char* value){
// 	pthread_mutex_lock(&log_lock);
// 	int nbytes = strlen(value)*sizeof(char);
// 	write(file_d, value, nbytes);
// 	pthread_mutex_unlock(&log_lock);
// }



static void* worker(void* arg){
	connection_data* data = (connection_data*) arg;
	Queue* q = *data->q;
	pid_t tid = data->tid; // debug
	double program_start_time = data->program_start_time;
	int file_d = data->file_d;
	//bool logging_enabled = file_d == -1;

	while(1){
		pthread_mutex_lock(&queue_lock);
		while(isEmpty(q)){
			pthread_cond_wait(&queue_cond, &queue_lock);
		}
		if(DEBUG) printf("Woke up as thread %d!\n", tid);
		if(DEBUG) printQueue(q);
		//char text_buffer[100];
		//char* received = sprintf("")
		//append_to_log(file_d, "")
		QueueItem item = *peek(q);
		int conn_fd = item.connection_fd;
		double task_start_time = item.task_start_time;

		if(conn_fd == -1) {
			printf("received invalid conn_fd %d\n",conn_fd);
			return NULL;
		}
		dequeue(q);
		if(DEBUG) printf("thread received conn_fd %d\n", conn_fd);
		pthread_mutex_unlock(&queue_lock);
		pthread_cond_signal(&queue_cond); // signal to other threads to wake and check the queue and for main thread to add new items

		request_handle(conn_fd, file_d, program_start_time, task_start_time, tid, log_lock);
		close_or_die(conn_fd);
	}

	return NULL;
}


//
// ./wserver [-d <basedir>] [-p <portnum>] 
// prompt> ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-l log_file]
// ./wserver -d ./html/ -p 8003 -t 1 -b 5  -l logs.txt
//
const char* usage_str = "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-l logfile]\n";

int main(int argc, char* argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
	size_t num_threads = 1;
	size_t buffer = 1;
	char log_file[50]; // magic numbers...
	bool logging_enabled = false;
	struct stat path_stat;
    char buf[MAXBUF], log_buf[MAXBUF];


    while ((c = getopt(argc, argv, "d:p:t:b:l:")) != -1){
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
				fprintf(stderr, usage_str);
				exit(1);
			}			
			break;
		case 'b':
			buffer = atoi(optarg);
			if(buffer < 1) { // sanity check
				fprintf(stderr, usage_str);
				exit(1);
			}
			break;
		case 'l':
			if(stat(optarg, &path_stat) == 0){ // check that this file is accessible and exists
				if(S_ISREG(path_stat.st_mode)){ // check that its a regular file (not a directory)
					logging_enabled = true;
					strcpy(log_file, optarg);
					break;
				}
				// its not a "file"
				fprintf(stderr, "You specified something other than a file as log file.");
				exit(1);
			}
			if(errno == ENOENT){ // file is not created
				logging_enabled = true;
				strcpy(log_file, optarg);
				break;
			}

			fprintf(stderr, "Could not access log file.");
			exit(1);
			break;
		default:
			fprintf(stderr, usage_str);
			exit(1);
		}
	}
	// sanity check
	if(num_threads < 1 || buffer < 1){
		fprintf(stderr, usage_str);
		exit(1);
	}
	signal(SIGPIPE, SIG_IGN); // ignore sigpipe 

	int file_d = -1;
	if(logging_enabled){
		file_d = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644); // owner: read+write, group: read-only, others: read-only 
		if(file_d == -1){
			perror("open failed");
			exit(1);
		}
		char new_line[] = "\n";
		pthread_mutex_lock(&log_lock);
		write(file_d, new_line, strlen(new_line)*sizeof(char));
		pthread_mutex_unlock(&log_lock);
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
	double start_time = get_wall_seconds();
	connection_data* thread_data = malloc(num_threads*sizeof(thread_data));
    for (int i = 0; i < num_threads; i++) {
		thread_data[i].q = &q;
		thread_data[i].tid = i;
		thread_data[i].file_d = file_d;
		thread_data[i].program_start_time = start_time;
        pthread_create(&threads[i], NULL, worker, (void*)&thread_data[i]);
    }

    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
	char method[MAXBUF], uri[MAXBUF], peak_buf[MAXBUF];//, version[MAXBUF];
    while (1) {
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		pthread_mutex_lock(&queue_lock);
		while(isFull(q)){
			if(DEBUG) printf("main thread waiting...\n");
			pthread_cond_wait(&queue_cond, &queue_lock);
		}
		if(DEBUG) printf("main thread woke up\n");
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
		if(DEBUG) printf("main thread received connection fd=%d\n", conn_fd);
		if(logging_enabled){
			// we need to get method and URI to log it

			ssize_t bytes_peeked = recv(conn_fd, peak_buf, sizeof(peak_buf)-1, MSG_PEEK);
			if(bytes_peeked > 0){
				peak_buf[bytes_peeked] = '\0';
				//printf("Peeked data: %s\n", peak_buf);

				sscanf(peak_buf, "%s %s", method, uri);
				sprintf(log_buf, "%3fs [Thread Main] Arrived - request: %s %s\n", get_wall_seconds() - start_time, method, uri);
				int nbytes = strlen(log_buf)*sizeof(char);
				pthread_mutex_lock(&log_lock);
				write(file_d, log_buf, nbytes);
				pthread_mutex_unlock(&log_lock);
			}
		}
		int task_start_time = get_wall_seconds();
		QueueItem item; // fixme: Should be malloced?
		item.connection_fd = conn_fd;
		item.task_start_time = task_start_time; 

		//pthread_mutex_lock(&queue_lock);
		// enqueue this connection and wake threads
		enqueue(q, &item);
		pthread_mutex_unlock(&queue_lock);
		pthread_cond_signal(&queue_cond);
		if(DEBUG) printf("main thread finished work\n");
		//request_handle(conn_fd);
		//close_or_die(conn_fd);
    }

	pthread_mutex_destroy(&queue_lock);
	destroyQueue(q);
	close(file_d);
	free(thread_data);
	free(q);
    return 0;
}


    


 
