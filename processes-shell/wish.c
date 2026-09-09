#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/wait.h>

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

*/
//     printf("%s (%d)\n",__FILE__,__LINE__);

const char error_message[30] = "An error has occurred\n";

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

void run_command(char** args, int should_wait){
    int rc = fork();
    if(rc < 0){
        // fork was unsuccessful
        write(STDERR_FILENO, error_message, strlen(error_message)); 
    }
    else if (rc == 0){
        execvp(args[0], args);
    }
    else if (should_wait == 1){
        int wc = wait(NULL);
        assert(wc >= 0);
    }
}


int handle_command(char* line, size_t len, ssize_t read, FILE* input){
    errno = 0;
    read = getline(&line, &len, input);
    if (read == -1){ 
        if (errno == ENOMEM){
            // OUT OF MEMORY
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            
        }
        else if (feof(input)){
            // end of file EOF reached
            return 0; // exit
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
        char* clean_token = malloc(sizeof(char*));
        char* delim = " ";        

        unsigned int arg_num = 0;
        unsigned int max_args = 2; // assume max 1 arguments (+1 for executable)
        char** args = calloc(max_args, sizeof(char*)); 
        token = strsep(&line, delim); // split line 
        while(token != NULL){
            clean_string(clean_token, token); // clean string 
            if(strcmp(clean_token, "exit") == 0) return 0;// user typed exit so we stop

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
                }
                token = strsep(&line, delim); 
            }
        }
        if(strcmp(args[0], "cd") == 0){
            if(arg_num != 2){ // we want exactly one argument after cd
                write(STDERR_FILENO, error_message, strlen(error_message)); 
            }
            else{
                // we have exactly one argument, the path to move to
                chdir(args[1]);
            }
        }
        else{
            // Our command is assumed to be a binary 
            run_command(args, 1);
        }
        free(args);
        args = NULL;
    }
    return 1; // success
}


int main(int argc, char *argv[]) {

    if(argc == 2){
        // batch shell     
        //printf("%s (%d) BATCH \n",__FILE__,__LINE__);

        char* filename = argv[1];
        FILE* input_file = fopen(filename, "r");
        
        char *line = NULL;
        size_t len = 0;
        ssize_t read = 0;

        int running = 1;
        while(running) {
            running = handle_command(line, len, read, input_file);
        }
        free(line);
        return 0;
    }
    else if (argc != 1){
        // invalid number of arguments
        exit(1);
    }
    //printf("%s (%d) INT \n",__FILE__,__LINE__);


    char *line = NULL;
    size_t len = 0;
    ssize_t read = 0;

    int running = 1;
    while(running) {
        printf("wish> ");
        running = handle_command(line, len, read, stdin);
    }
    free(line);
    return 0;
}