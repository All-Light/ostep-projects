//
// client.c: A very, very primitive HTTP client.
// 
// To run, try: 
//      client hostname portnumber filename
//
// Sends one HTTP request to the specified HTTP server.
// Prints out the HTTP response.
//
// For testing your server, you will want to modify this client.  
// For example:
// You may want to make this multi-threaded so that you can 
// send many requests simultaneously to the server.
//
// You may also want to be able to request different URIs; 
// you may want to get more URIs from the command line 
// or read the list from a file. 
//
// When we test your server, we will be using modifications to this client.
//

#include "io_helper.h"
#include <pthread.h>

#define MAXBUF (8192)

//
// Send an HTTP request for the specified file 
//
void client_send(int fd, char *filename) {
    char buf[MAXBUF];
    char hostname[MAXBUF];
    
    gethostname_or_die(hostname, MAXBUF);
    
    /* Build the request without reading and writing the same buffer at once. */
    int len = snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
                       filename, hostname);
    if (len < 0 || (size_t)len >= sizeof(buf)) {
        fprintf(stderr, "Request is too long for %s\n", filename);
        return;
    }
    write_or_die(fd, buf, strlen(buf));
}

//
// Read the HTTP response and print it out
//
void client_print(int fd) {
    char buf[MAXBUF];  
    int n;
    
    // Read and display the HTTP Header 
    n = readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n") && (n > 0)) {
	printf("Header: %s", buf);
	n = readline_or_die(fd, buf, MAXBUF);
	
	// If you want to look for certain HTTP tags... 
	// int length = 0;
	//if (sscanf(buf, "Content-Length: %d ", &length) == 1) {
	//    printf("Length = %d\n", length);
	//}
    }
    
    // Read and display the body as bytes; images may contain zero bytes and
    // need not contain newline characters.
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        if (fwrite(buf, 1, (size_t)bytes_read, stdout) != (size_t)bytes_read) {
            perror("fwrite");
            break;
        }
    }
    if (bytes_read < 0) {
        perror("read");
    }
}

typedef struct{
    int port;
    char host[MAXBUF];
    char filename[MAXBUF];
} worker_data;

void* worker(void* arg){
    worker_data* data = (worker_data*) arg;

    int clientfd = open_client_fd_or_die(data->host, data->port);
    
    client_send(clientfd, data->filename);
    client_print(clientfd);
    
    close_or_die(clientfd);
    return NULL;
}


// ./wclient localhost 8003 /images/image5.jpg /cgi/spin?1 /images/image4.jpg
// ./wclient localhost 8003 /index.html

// cgi tests:
// time ./wclient localhost 8003 /cgi/spin?3 /cgi/spin?3 /cgi/spin?3 /cgi/spin?3 > /dev/null 
//      -> 12 seconds on 1 server threads
//      -> 6 seconds on 2 server threads
//      -> 6 seconds on 3 server threads
//      -> 3 seconds on 6 server threads
int main(int argc, char *argv[]) {
    char *host;
    int port;
    int nr_threads;
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <filename>\n", argv[0]);
        exit(1);
    }
    
    host = argv[1];
    port = atoi(argv[2]);
    //filename = argv[3];
    nr_threads = argc-3;
    pthread_t threads[nr_threads]; // nr of filenames
    worker_data** threads_data = calloc(nr_threads, sizeof(*threads_data));
    for(int i = 0; i < nr_threads; i++){
        worker_data* data = malloc(sizeof(worker_data));

        if(data == NULL){
            printf("Could not create thread data");
            return 1;
        }
        strncpy(data->host, host, MAXBUF-1);
        data->host[sizeof(data->host)-1] = '\0';
        strncpy(data->filename, argv[3+i], MAXBUF-1);
        data->filename[sizeof(data->filename)-1] = '\0';
        data->port = port;
        threads_data[i] = data;
        pthread_create(&threads[i], NULL, worker, (void*)data);
    }
    
    for(int i = 0; i < nr_threads; i++){
        pthread_join(threads[i], NULL);
        free(threads_data[i]);
    }

    free(threads_data);
    exit(0);
}
