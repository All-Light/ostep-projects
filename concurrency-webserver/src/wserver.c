#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include "request.h"
#include "io_helper.h"
#include "queing.h"
#include <time.h>

#define MAXBUF (8192) // shouldnt duplicate...
#define DEBUG 0

char default_root[] = ".";


static double get_wall_seconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double seconds = tv.tv_sec + (double)tv.tv_usec / 1000000;
  return seconds;
}


typedef struct{
	int tid;
	Queue** q;
	double program_start_time;
	char* filename; 
} connection_data;


static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_producer_wake = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_consumer_wake = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void* worker(void* arg){
	connection_data* data = (connection_data*) arg;
	Queue* q = *data->q;
	pid_t tid = data->tid; // debug
	double program_start_time = data->program_start_time;

	char* filename = data->filename;
	int file_d = -1;
	bool logging_enabled = filename != NULL;
	if(logging_enabled){
		file_d = open(filename, O_WRONLY | O_APPEND, 0644);
		if(file_d == -1){
			// failed to open log file, kill
			perror("Could not open log file");
			exit(1);
		} 
	}

	const unsigned int buffer_size = 10;
	char log_buffer[buffer_size][MAX_STR_LEN]; 
	unsigned int buffer_count = 0;

	if(!logging_enabled){
		// we wont write to log file
		buffer_count = -1;
	}

	//bool logging_enabled = file_d == -1;
	while(1){
		pthread_mutex_lock(&queue_lock);
		while(isEmpty(q)){
			// we are going to sleep this thread, let it flush the log buffer before
			if(logging_enabled && buffer_count > 0){
				// flush buffer to file
				pthread_mutex_lock(&log_lock);
				for(unsigned int i=0; i < buffer_count; i++){
					printf("wrote: %s", log_buffer[i]);
					int nbytes = strlen(log_buffer[i])*sizeof(char);
					write(file_d, log_buffer[i], nbytes);
				}
				//fsync(file_d); // force actual write to disk
				pthread_mutex_unlock(&log_lock);
				buffer_count = 0;
			}
			pthread_cond_wait(&queue_consumer_wake, &queue_lock);
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
		pthread_cond_signal(&queue_producer_wake); // signal to main threads to wake 
		pthread_cond_signal(&queue_consumer_wake); // signal to consumer thread to wake so we empty the queue

		request_handle(conn_fd, log_buffer, &buffer_count, logging_enabled, program_start_time, task_start_time, tid);
		close_or_die(conn_fd);


		if(logging_enabled && buffer_count + 3 > buffer_size){
			// flush buffer to file
			pthread_mutex_lock(&log_lock);
			for(unsigned int i=0; i < buffer_count; i++){
				printf("wrote: %s", log_buffer[i]);
				int nbytes = strlen(log_buffer[i])*sizeof(char);
				if(nbytes > 0) write(file_d, log_buffer[i], nbytes);
			}
			//fsync(file_d); // force actual write to disk
			pthread_mutex_unlock(&log_lock);
			buffer_count = 0;
		}
		// write to log file on condition
	}
	if(logging_enabled){
		close(file_d);
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
	char log_file[MAXBUF]; // magic numbers...
	bool logging_enabled = false;
	struct stat path_stat;
    //char buf[MAXBUF],
	//char log_buf[MAXBUF];


    while ((c = getopt(argc, argv, "d:p:t:b:s:l:")) != -1){
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
					
					strcpy(log_file, realpath(optarg, NULL));
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
		case 's': // ignore s
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

	int main_log_file_d = -1;
	if(logging_enabled){
		main_log_file_d = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644); // owner: read+write, group: read-only, others: read-only 
		fsync(main_log_file_d); // Force this file to exist on disk if it was just created, we must be able to reference it in workers
		if(main_log_file_d == -1){
			perror("Could not open the log file from main thread.");
			exit(1);
		}
		strcpy(log_file, realpath(log_file, NULL)); // if we just created this file we need the absolute path for consumers  

		// just add a new line between ./wserver runs. cosmetic
		char new_line[] = "\n";
		pthread_mutex_lock(&log_lock);
		write(main_log_file_d, new_line, strlen(new_line)*sizeof(char));
		pthread_mutex_unlock(&log_lock);
	}

	// create queue
	Queue* q = malloc(sizeof(Queue));
	initializeQueue(q, buffer);

	// create threads
	pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
	if(threads == NULL){
		perror("malloc failed");
		exit(1);
	}
	double start_time = get_wall_seconds();
	connection_data* thread_data = malloc(num_threads*sizeof(connection_data));
    for (size_t i = 0; i < num_threads; i++) {
		thread_data[i].q = &q;
		thread_data[i].tid = i;
		if(logging_enabled){
			thread_data[i].filename = malloc((strlen(log_file)+1)*sizeof(char));
			strcpy(thread_data[i].filename, log_file);
		}
		else{
			thread_data[i].filename = NULL; // indicate that we have no file descriptor
		}
		thread_data[i].program_start_time = start_time;
        pthread_create(&threads[i], NULL, worker, (void*)&thread_data[i]);
    }

    // run out of this directory
    chdir_or_die(root_dir);
	

    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
	//char method[MAXBUF], uri[MAXBUF], peak_buf[MAXBUF];//, version[MAXBUF];
    while (1) {
		pthread_mutex_lock(&queue_lock);
		while(isFull(q)){
			//if(DEBUG) printf("main thread waiting...\n");
			pthread_cond_wait(&queue_producer_wake, &queue_lock);
			//if(DEBUG) printf("main thread woke up\n");
		}
		// the queue is not full so we will listen for new connections

		pthread_mutex_unlock(&queue_lock); // unlock mutex so threads can work while we wait

		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
		// if(logging_enabled){
		// 	// we need to peek at method and URI to log it
		// 	ssize_t bytes_peeked = recv(conn_fd, peak_buf, sizeof(peak_buf)-1, MSG_PEEK);
		// 	if(bytes_peeked > 0){
		// 		peak_buf[bytes_peeked] = '\0';

		// 		sscanf(peak_buf, "%s %s", method, uri);
		// 		int result = snprintf(log_buf, MAXBUF, "%3fs [Thread Main] Arrived - request: %s %s\n", get_wall_seconds() - start_time, method, uri);
		// 		if(result < 0){
		// 			continue;
		// 		}
		// 		if(DEBUG) printf(log_buf);
		// 		int nbytes = strlen(log_buf)*sizeof(char);

		// 		pthread_mutex_lock(&log_lock);
		// 		write(main_log_file_d, log_buf, nbytes);
		// 		pthread_mutex_unlock(&log_lock);
		// 	}
		// }
		double task_start_time = get_wall_seconds();
		QueueItem item; // fixme: Should be malloced?
		item.connection_fd = conn_fd;
		item.task_start_time = task_start_time; 

		struct timeval timeout = { // we need a timeout for the connection. 1 seconds to get request data should be enough
			.tv_sec = 1,
			.tv_usec = 0
		};

		if(setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO,&timeout, sizeof(timeout)) == -1){
			perror("setsockopt");
			close(conn_fd);
			continue;
		}

		pthread_mutex_lock(&queue_lock);
		// enqueue this connection and wake threads
		enqueue(q, &item);
		pthread_mutex_unlock(&queue_lock);
		pthread_cond_signal(&queue_consumer_wake); // wake a consumer
		if(DEBUG) printf("main thread finished work\n");
		//request_handle(conn_fd);
		//close_or_die(conn_fd);
    }

	pthread_mutex_destroy(&queue_lock);
	destroyQueue(q);
	close(main_log_file_d);
	for (size_t i = 0; i < num_threads; i++) {
		free(thread_data[i].filename);
	}

	free(thread_data);
	free(q);
    return 0;
}

