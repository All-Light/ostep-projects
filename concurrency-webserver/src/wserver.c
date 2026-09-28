#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "request.h"
#include "io_helper.h"
#include "queing.h"

char default_root[] = ".";

//
// ./wserver [-d <basedir>] [-p <portnum>] 
// prompt> ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
// ./wserver -d ./html/ -p 8003 -t 1 -b 5 
//
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
	size_t threads = 0;
	size_t buffer = 0;
    
    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1){
		printf("optarg: %s\n",optarg);
		switch (c) {
		case 'd':
			root_dir = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			threads = atoi(optarg);
			if(threads < 1) { // sanity check
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
	if(threads == 0 || buffer == 0){
		fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t threads] [-b buffers]\n");
		exit(1);
	}


    // run out of this directory
    chdir_or_die(root_dir);
	printf("Buffer size %ld\n", buffer);
	printf("We have %ld threads\n", threads);

	// create queue

	Queue q;
	initializeQueue(&q, buffer);

	// create threads


    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);


		request_handle(conn_fd);
		close_or_die(conn_fd);
    }
	destroyQueue(&q);
    return 0;
}


    


 
