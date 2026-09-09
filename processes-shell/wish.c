#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
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

*/
//     printf("%s (%d)\n",__FILE__,__LINE__);

const char error_message[30] = "An error has occurred\n";
//const char *exit_cmd = "exit\n";

void clean_string(char* cleaned, char* dirty){
    
    for (char c=*dirty; c; c=*++dirty) {
        //printf("dirty char: %c\n", c);
        if(c == ' '  || c == '\t'){
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

        if(strcmp(line, "exit") == 0) return 0; // user typed exit so we stop
        
        char* token;
        char* clean_token = malloc(sizeof(char*));
        char* delim = " ";
        token = strsep(&line, delim); // split line 
        while(token != NULL){
            //printf("token before cleaning: %s\n", token);

            clean_string(clean_token, token);
            printf("token after cleaning: %s\n", token);
            printf("clean_token: %s\n", clean_token);
            token = strsep(&line, delim); 
        }
    }
    return 1; // success
}


int main(int argc, char *argv[]) {

    if(argc == 2){
        // batch shell     
        //printf("%s (%d) BATCH \n",__FILE__,__LINE__);

        char* filename = argv[1];
        printf("%s\n",filename);

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