#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/wait.h>

#define DEBUG 0


/*
Things to include: 
- Interactive mode (loop until user types exit)
- Batch mode (input a t.xt file --> run commands line-by-line)
- Create a child processes with args for each new command
- Read lines with getline(), separate the input with strsep()
- Implement Path searching for libraries try access("/bin/ls", X_OK)

Built-in commands:
- exit (exits the shell) no arguments
- cd (one argument only) use the chdir() system call with the argument supplied by the user; if chdir fails, that is also an error.
- path The path command takes 0 or more arguments, with each argument separated by whitespace from the others. A typical usage would be like this:
wish> path /bin /usr/bin, which would add /bin and /usr/bin to the search path of the shell. If the user sets path to be empty, then the shell
should not be able to run any programs (except built-in commands). The path command always overwrites the old path with the newly specified path.

Redirection:
 - The > character redirects stdout to the specified file (ls -la /tmp > output) if it already exists we truncate and overwrite. 
 - We should also reroute the standard error of the program to the file

Parallel commands
 - Using the "&" character we should be able to run commands in parallel   cmd1 & cmd2 args1 args2 & cmd3 args1
 - Wait for all of them to finish before issuing the next prompt

Program Error
 - Only one program error 
    write(STDERR_FILENO, error_message, strlen(error_message)); 

// BUGS:
 - The echo command is buggy: echo "hi" works but echo "hi this is a test" gives invalid realloc size
 - Magic numbers for string buffers!
 - Redirection not implemented
 */
//     printf("%s (%d)\n",__FILE__,__LINE__);

const char error_message[30] = "An error has occurred\n";

typedef struct
{
    char** paths;
    unsigned char nr_paths;
    unsigned char longest_path;
} paths_data;

int which_path(char** restrict resulting_path_ptr, char* restrict command, paths_data* restrict paths_struct){
    if(DEBUG) printf("Number of available paths: %d\n", paths_struct->nr_paths);
    int i;
    for(i = 0; i < paths_struct->nr_paths; i++){
        int max_size = (strlen(paths_struct->paths[i]) + strlen(command) + 1) * sizeof(char);
        char* candidate_path = (char*) malloc(max_size);

        if(DEBUG) printf("path: %s\n",paths_struct->paths[i]);
        if(DEBUG) printf("command: %s\n",command);
        // copy in the candidate path into candidate_path
        snprintf(candidate_path, max_size, "%s%s",paths_struct->paths[i], command);
        if(DEBUG) printf("candidate path: %s\n", candidate_path);
        if(access(candidate_path, X_OK) == 0) {
            if(DEBUG) printf("SUCCESS! candidate path ptr: %p\n", &candidate_path);
            *resulting_path_ptr = candidate_path;
            return 0;
        }
        free(candidate_path);
    }
    return 1;
}

void fix_path(char** path){ // pass-by-reference
    //if(DEBUG) printf("path pointer %p\n",path);

    int length = strlen(*path);
    //if(DEBUG) printf("path str length: %d\n", length);
    //if(DEBUG) printf("path str size: %ld\n bytes", sizeof(*path));
    //if(DEBUG) printf("path str: %s\n", *path);
    //if(DEBUG) printf("path str last %c\n", (*path)[length-1]);
    if((*path)[length-1] != '/'){
        // realloc if our buffer is too small
        (*path) = (char*) realloc((*path), (length+1)*sizeof(char));
        (*path)[length] = '/'; // append trailing slash
        (*path)[length+1] = '\0'; // string terminator
    }
}

// Updates cleaned to be a copy of dirty without any isspace characters
void clean_string(char* cleaned, char* dirty){
    
    for (char c=*dirty; c; c=*++dirty) {
        //printf("dirty char: %c\n", c);
        if(isspace(c)){
            //printf("skipping dirty char: %d\n", c);
        }
        else{
            *cleaned = c;
            //printf("clean char: %c\n",  *cleaned);
            ++cleaned;
        }
    }
    *cleaned = '\0'; // end cleaned char*

}

void run_command(char** args, paths_data* paths_struct, int should_wait){
    // allocate stack pointer for longest possible path
    char* command_path = (char*) malloc(1000*sizeof(char)); //FIXME: We shouldnt need to use this large of a buffer 
    command_path = NULL;
    int path_found = which_path(&command_path, args[0], paths_struct);
    //printf("is path found? %d\n", path_found);
    if (command_path == NULL){ 
        if(DEBUG) printf("POINTER IS STILL NULL LINE=%d\n", __LINE__); 
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        free(command_path);
        return;
    }
    if(path_found != 0){
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        free(command_path);
        return; // not found in path
    }
    if(DEBUG) printf("command_path: %p\n",command_path);


    int rc = fork();
    if(rc < 0){
        // fork was unsuccessful
        write(STDERR_FILENO, error_message, strlen(error_message)); 
    }
    else if (rc == 0){
        execvp(command_path, args);
    }
    else if (should_wait == 1){
        int wc = wait(NULL);
        assert(wc >= 0);
    }
    free(command_path);
}


int handle_command(char* line, size_t len, ssize_t read, FILE* input, paths_data* paths_struct){
    errno = 0;
    read = getline(&line, &len, input);
    if (read == -1){ 
        if (errno == ENOMEM){
            // OUT OF MEMORY
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            
        }
        else if (feof(input)){
            // end of file EOF reached
            return 0;
        }
        else{
            // Could not read input
            write(STDERR_FILENO, error_message, strlen(error_message)); 
        }
    }
    else{
        // Successfully read line
        if(line[read - 1] == '\n'){ // remove trailing new line
            line[read - 1] = '\0';
            read--;
        }

        char* token;
        char* clean_token = malloc(100*sizeof(char*)+1);
        if(clean_token == NULL){
            fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        }
        char* delim = " ";        

        unsigned int arg_num = 0;
        unsigned int max_args = 2; // assume max 1 arguments (+1 for executable)
        char** args = calloc(max_args, sizeof(char*)); 
        if(args == NULL){
            fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        }
        token = strsep(&line, delim); // split line 
        while(token != NULL){
            clean_string(clean_token, token); // clean string 

            if(isspace(*token) || strlen(token) == 0) {
                token = strsep(&line, delim); // skip space-only or empty tokens
            }
            else{
                // Update our arguments with the new clean token
                args[arg_num] = strdup(clean_token);
                arg_num++;
                // If our number of arguments exceeds max_args we realloc the argument container
                if(arg_num > max_args){
                    max_args += 1; // add 1 to our container size
                    args = realloc(args, max_args*sizeof(char*));
                    if(args == NULL){
                        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
                    }
                }
                token = strsep(&line, delim); 
            }
        }
        free(clean_token);
        if(args[0] == NULL){
            if(DEBUG) fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        }
        else if(strcmp(args[0], "exit") == 0){
            if(arg_num != 1){ // invalid number of arguments
                write(STDERR_FILENO, error_message, strlen(error_message)); 
            }
            else{
                free(args);
                return 0;
            }
        }
        else if(strcmp(args[0], "cd") == 0){
            if(arg_num != 2){ // we want exactly one argument after cd
                write(STDERR_FILENO, error_message, strlen(error_message)); 
            }
            else{
                // we have exactly one argument, the path to move to
                chdir(args[1]);
            }
        }
        else if(strcmp(args[0], "path") == 0){// add args to path
            if(DEBUG) printf("adding paths!\n");
            // Clear old paths
            
            int prev_nr_paths = paths_struct->nr_paths;
            for(int j=0; j < prev_nr_paths;j++){
                paths_struct->paths[j] = NULL;
            }
            paths_struct->nr_paths = 0;
            paths_struct->longest_path = 0;

            // update our paths struct to hold arg_num-1 pointers
            paths_struct->paths = realloc(paths_struct->paths, sizeof((arg_num-1)*sizeof(char*)));
            if(paths_struct->paths == NULL){
                fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
            }
            
            paths_struct->nr_paths += arg_num-1; // -1 to remove the actual "path" argument (args[0])
            

            int i;
            for(i = 1; i < arg_num; i++){
                fix_path(&args[i]);
                paths_struct->paths[i-1] = args[i];
                if(DEBUG) printf("adding path: %s\n", args[i]);
                if(strlen(args[i])> paths_struct->longest_path){
                    paths_struct->longest_path = strlen(args[i]);
                }
            }
        }
        else{
            // Our command is assumed to be a binary 
            run_command(args, paths_struct, 1);
        }
        free(args);
        args = NULL;
    }
    return 1; // success
}

//printf("%s (%d) INT \n",__FILE__,__LINE__);
int main(int argc, char *argv[]) {
    FILE* input_file = stdin; // assume interactive mode
    char* filename = NULL;
    if(argc == 2){
        // batch shell - overwrite input_file
        filename = argv[1];
        input_file = fopen(filename, "r"); 
    }
    else if (argc != 1){
        // invalid number of arguments
        exit(1);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read = 0;

    // set up default path
    paths_data* paths_struct = malloc(sizeof(paths_data));
    if(paths_struct == NULL){
        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
    }
    paths_struct->paths = malloc(1000*sizeof(char*));
    if(paths_struct->paths == NULL){
        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
    }
    paths_struct->paths[0] = "/bin/";
    paths_struct->longest_path = strlen(paths_struct->paths[0]);
    paths_struct->nr_paths = 1;

    int running = 1;
    while(running) {
        if(argc!=2) printf("wish> ");
        running = handle_command(line, len, read, input_file, paths_struct);
    }
    free(line);
    free(paths_struct->paths);
    free(paths_struct);
    if(argc == 2){
        fclose(input_file);
    }
    return 0;
}