#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
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
 - Redirection not kinda working (> is not parsed from none-spaced words)


How to implement redirection (and later pipes):
    1. We read each line normally,
    2. Go token by token 
    3. if we encounter ">" we set redirect_flag = 1;
    4. when handling command and redirect_flag == 1
        - Read argument-by-argument until we find ">"
        - get the next argument as output file 
        - remove output file and ">" from arguments
        - run command as normal
 */
//     printf("%s (%d)\n",__FILE__,__LINE__);

const char error_message[30] = "An error has occurred\n";

typedef struct
{
    char** paths;
    size_t nr_paths;
    size_t longest_path;
} paths_data;

typedef struct 
{
    char** args;
    size_t num_args;
    char* output_file;
} Command;

typedef struct
{
    Command* command_arr; // pointer to command array
    size_t size; // nr slots filled
    size_t capacity; // total allocated slots
} CommandsArr;


int which_path(char** restrict resulting_path_ptr, char* restrict executable, paths_data** restrict paths_struct){
    if(DEBUG) printf("Number of available paths: %ld\n", (*paths_struct)->nr_paths);
    for(int i = 0; i < (*paths_struct)->nr_paths; i++){
        if(DEBUG) printf("i: %d\n",i);
        if(DEBUG) printf("path: %s\n",(*paths_struct)->paths[i]);

        int max_size = (strlen((*paths_struct)->paths[i]) + strlen(executable) + 1) * sizeof(char);
        char* candidate_path = (char*) malloc(max_size);

        if(DEBUG) printf("executable: %s\n",executable);
        // copy in the candidate path into candidate_path
        snprintf(candidate_path, max_size, "%s%s",(*paths_struct)->paths[i], executable);
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
    if(DEBUG) printf("path pointer %p\n",path);

    int length = strlen(*path);
    if(DEBUG) printf("path str length: %d\n", length);
    if(DEBUG) printf("path str size: %ld\n bytes", sizeof(*path));
    if(DEBUG) printf("path str: %s\n", *path);
    if(DEBUG) printf("path str last %c\n", (*path)[length-1]);
    // If path does not end in "/" we append it
    if((*path)[length-1] != '/'){
        // realloc if our buffer is too small
        (*path) = (char*) realloc((*path), (length+1)*sizeof(char));
        (*path)[length] = '/'; // append trailing slash
        (*path)[length+1] = '\0'; // string terminator
        if(DEBUG) printf("Appended trailing slash\n");
    }
}

// Updates cleaned to be a copy of dirty without any isspace characters
void clean_string(char* cleaned, char* dirty){
    for (char c=*dirty; c; c=*++dirty) {
        //printf("dirty char: %c\n", c);
        if(!isspace(c)){
            *cleaned = c;
            //printf("clean char: %c\n",  *cleaned);
            ++cleaned;
        }
    }
    *cleaned = '\0'; // end cleaned char*
}

size_t get_total_nr_commands(const char* line){
    if(line == NULL || *line == '\0'){
        return 0;
    }
    size_t nr_pipes = 0;
    const char *ptr = line;
    while(*ptr != '\0'){
        if(*ptr == '|'){
            nr_pipes++;
        }
        ptr++;
    }
    return nr_pipes+1; // first command is not proceeded by a pipe
}


CommandsArr* allocate_commands_arr(size_t nr_commands){
    CommandsArr* commands = calloc(1, sizeof(CommandsArr)); // allocate the commands array
    if(commands == NULL){
        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        return NULL;
    }


    commands->command_arr = calloc(nr_commands, sizeof(Command)); // allocate commands
    if(commands->command_arr == NULL){
        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        return NULL;
    }
    commands->size = 0; // current nr of commands 
    commands->capacity = nr_commands;

    // allocate each commands argument array
    for(size_t command_num = 0; command_num < nr_commands; command_num++){
        commands->command_arr[command_num].args = calloc(1, sizeof(char*)); // assume 1 argument
        if(commands->command_arr[command_num].args == NULL){
            fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
            return NULL;
        }
    }
    return commands;
}

size_t parse_command(char** command_token, Command* command_obj,  int command_nr){
    size_t arg_num = 0;
    char* clean_token = malloc((strlen(*command_token)+1)*sizeof(char));
    if(clean_token == NULL){
        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        return 0;
    }
    const char* delim = " ";
    char* token = strsep(command_token, delim); // split command by whitespace 
    while(token != NULL){
        clean_string(clean_token, token); // clean string i.e remove whitespaces and \t etc

        size_t length = strlen(clean_token);
        if(isspace(*clean_token) || length == 0 || clean_token == NULL) { // if the entire token is empty
            token = strsep(command_token, delim); // skip space-only or empty tokens
            continue;
        }
        //printf("TOKEN: %s\n",clean_token);

        // we need to parse the token for redirects
        
        // for (size_t i = 0;  i < length;  i++) {
        //     //printf("char: %c\n",clean_token[i]);
    
        //     // Redirect!
        //     if(clean_token[i] == '>'){
        //         // We need to cut off the first part of the token as argument 1 and second part as outputfile   
        //         commands->command_arr[command_num].args[arg_num] = strndup(clean_token, i);
        //         commands->command_arr[command_num].output_file = strndup(clean_token+i,length-i);
        //         printf("Argument: %s\n", commands->command_arr[command_num].args[arg_num]);
        //         printf("output_file: %s\n", commands->command_arr[command_num].output_file);

        //         arg_num++;
        //         command_num++;
        //         special_op = 1;
        //     }

        // }
        (*command_obj).args[arg_num] = strdup(clean_token);
        arg_num++;

        // TODO: implement piping and redirects here
        token = strsep(command_token, delim); 
    }
    return arg_num;
}


// takes in a line and returns an array of commands 
CommandsArr* parse_line(char* line, unsigned int line_size){
    // a line is a long string
    //  we split by whitespace and set them either as executable, argument, redirect, or pipe.

    size_t max_commands = get_total_nr_commands(line);     
    CommandsArr* commands = allocate_commands_arr(max_commands);
    if(commands == NULL){
        fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
        return NULL;
    }

    size_t command_num = 0; // number of current command
    size_t arg_num = 0; // number of current argument
    
    // char* redirect_ptr = strchr(&line, '>');
    // if(redirect_ptr != NULL){

    // }

    // split line into commands by pipe symbol |
    char* savedptr1;
    char* command_delim = "|";
    char* command_token = strtok_r(line, command_delim, &savedptr1);
    while (command_token != NULL){
        arg_num = parse_command(&command_token, &commands->command_arr[command_num], command_num);
        
        commands->command_arr->num_args = arg_num-1; // remove executable from num args
        commands->size++;
        command_token = strtok_r(NULL, command_delim, &savedptr1);
    }
    return commands;
}

void free_commandsArr(CommandsArr* commands){
    for(size_t command_num = 0; command_num < commands->size; command_num++){
        for(int i = 0; i < commands->command_arr[command_num].num_args; i++){
            free(commands->command_arr[command_num].args[i]);
        }
        free(commands->command_arr[command_num].args);
    }
    free(commands->command_arr);
    free(commands);
}


void run_command(char* executable, char** args, paths_data** paths_struct, int nr_args, int should_wait){
    // allocate stack pointer for longest possible path
    char* command_path = (char*) malloc(1000*sizeof(char)); //FIXME: We shouldnt need to use this large of a buffer 
    command_path = NULL;
    int path_found = which_path(&command_path, executable, paths_struct);
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
    if(DEBUG) printf("command_path pointer: %p\n",command_path);


    int rc = fork();
    if(rc < 0){
        // fork was unsuccessful
        write(STDERR_FILENO, error_message, strlen(error_message)); 
    }
    else if (rc == 0){
        // check for redirection:
        for(int arg_nr = 0; arg_nr < nr_args; arg_nr++){
            if(DEBUG) printf("arg:%s\n", args[arg_nr]);
            if(DEBUG) printf("arg+1:%s\n", args[arg_nr+1]);
            if (strlen(args[arg_nr]) == 1 && args[arg_nr][0] == '>'){
                if(DEBUG) printf("redirection caught!\n");
                if(DEBUG) printf("nr_args: %d\n", nr_args);
                if(DEBUG) printf("arg_nr: %d\n", arg_nr);
                if (nr_args - arg_nr != 1){ 
                    // multiple output files or no output file
                    write(STDERR_FILENO, error_message, strlen(error_message)); 
                    free(command_path);
                    return;
                }
                // args[arg_nr+1] must be file nmae 
                int fd = open(args[arg_nr+1], O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
                dup2(fd, 1); // redirect stdout
                dup2(fd, 2); // redirect stderr
                close(fd); // no need to keep open
                // remove these args from actual executable
                args[arg_nr] = NULL;
                args[arg_nr+1] = NULL;
                break;
            }
        }

        execvp(command_path, args);
    }
    else if (should_wait == 1){
        int wc = wait(NULL);
        assert(wc >= 0);
    }
    free(command_path);
}

int handle_command(char* line, size_t len, ssize_t read, FILE* input, paths_data** paths_struct){
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
        
        CommandsArr* commands = parse_line(line, len);
        if(DEBUG) printf("nr of commands to run: %ld\n", commands->size);
        for(size_t command_num = 0; command_num < commands->size; command_num++){
            char* executable = commands->command_arr[command_num].args[0];
            char** args = commands->command_arr[command_num].args;
            size_t num_args = commands->command_arr[command_num].num_args;
            if (DEBUG)printf("executable: %s\n", executable);
            if (DEBUG)printf("num_args: %ld\n", num_args);

            if(strcmp(executable, "exit") == 0){
                if(num_args != 0){ // invalid number of arguments
                    write(STDERR_FILENO, error_message, strlen(error_message)); 
                }
                else{
                    free_commandsArr(commands);
                    return 0;
                }
            }
            else if(strcmp(executable, "cd") == 0){
                if(num_args != 1){ // we want exactly one argument after cd
                    write(STDERR_FILENO, error_message, strlen(error_message)); 
                }
                else{
                    // we have exactly one argument, the path to move to
                    chdir(args[1]);
                }
            }
            else if(strcmp(executable, "path") == 0){// add args to path
                if(DEBUG) printf("adding paths!\n");
                // Clear old paths
                
                int prev_nr_paths = (*paths_struct)->nr_paths;
                if(DEBUG) printf("prev nr paths: %ld\n",(*paths_struct)->nr_paths);
                for(int j = 0; j < prev_nr_paths; j++){
                    (*paths_struct)->paths[j] = NULL;
                }
                (*paths_struct)->nr_paths = 0;
                (*paths_struct)->longest_path = 0;

                if(num_args == 0) return 1; // we dont want to realloc to 0 so we just skip.
                // update our paths struct to hold arg_num-1 pointers
                if(DEBUG) printf("num_args: %ld\n",num_args);

                char** tmp = realloc((*paths_struct)->paths, num_args*sizeof(char*));
                if(tmp == NULL){
                    write(STDERR_FILENO, error_message, strlen(error_message)); 
                    return 1;
                }
                (*paths_struct)->paths = tmp;

                int i;
                for(i = 1; i < num_args+1; i++){
                    fix_path(&args[i]);
                    (*paths_struct)->paths[i-1] = strdup(args[i]);
                    (*paths_struct)->nr_paths++;

                    if(DEBUG) printf("adding path: %s at index %d\n", (*paths_struct)->paths[i-1], i-1);
                    if(strlen(args[i]) > (*paths_struct)->longest_path){
                        (*paths_struct)->longest_path = strlen(args[i]);
                    }
                }
            }
            else{
                // Our command is assumed to be a binary 
                run_command(executable, args, paths_struct, num_args , 1);
            }
        }    
        free_commandsArr(commands);
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
        running = handle_command(line, len, read, input_file, &paths_struct);
    }
    free(line);
    free(paths_struct->paths);
    free(paths_struct);
    if(argc == 2){
        fclose(input_file);
    }
    return 0;
}