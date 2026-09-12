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
 - parallelization &

 */
//     printf("%s (%d)\n",__FILE__,__LINE__);

const char error_message[30] = "An error has occurred\n";

typedef struct
{
    char** paths;
    size_t nr_paths;
    size_t longest_path;
} paths_data;

typedef enum {
    OP_NONE,        // None
    OP_PIPE,        // '|'
    OP_BACKGROUND,  // '&'
} NextOp;

typedef struct 
{
    char** args;
    size_t num_args;
    char* output_file;
    NextOp operation;
} Command;

typedef struct
{
    Command* command_arr; // pointer to command array
    size_t size; // nr slots filled
    size_t capacity; // total allocated slots
} CommandsArr;


int which_path(char** resulting_path_ptr, char* executable, paths_data** paths_struct){
    //if(DEBUG) printf("Number of available paths: %ld\n", (*paths_struct)->nr_paths);
    int max_size = ((*paths_struct)->longest_path + strlen(executable) + 1) * sizeof(char);
    char* candidate_path = malloc(max_size);
    for(int i = 0; i < (*paths_struct)->nr_paths; i++){
        //if(DEBUG) printf("i: %d\n",i);
        //if(DEBUG) printf("path: %s\n",(*paths_struct)->paths[i]);
        //if(DEBUG) printf("executable: %s\n",executable);
        int curr_size = (strlen((*paths_struct)->paths[i]) + strlen(executable) + 1) * sizeof(char);

        // copy in the candidate path into candidate_path
        snprintf(candidate_path, curr_size, "%s%s",(*paths_struct)->paths[i], executable);
        //if(DEBUG) printf("candidate path: %s\n", candidate_path);
        if(access(candidate_path, X_OK) == 0) {
            //if(DEBUG) printf("SUCCESS! candidate path ptr: %p\n", &candidate_path);
            *resulting_path_ptr = candidate_path;
            return 0;
        }
    }
    free(candidate_path); // could not find a valid path
    return 1;
}

void fix_path(char** path){ // pass-by-reference
    //if(DEBUG) printf("path pointer %p\n",path);

    int length = strlen(*path);
    //if(DEBUG) printf("path str length: %d\n", length);
    //if(DEBUG) printf("path str size: %ld\n bytes", sizeof(*path));
    //if(DEBUG) printf("path str: %s\n", *path);
    //if(DEBUG) printf("path str last %c\n", (*path)[length-1]);
    // If path does not end in "/" we append it
    if((*path)[length-1] != '/'){
        // realloc if our buffer is too small
        (*path) = (char*) realloc((*path), (length+2)*sizeof(char));
        (*path)[length] = '/'; // append trailing slash
        (*path)[length+1] = '\0'; // string terminator
        //if(DEBUG) printf("Appended trailing slash\n");
    }
}

// trim pre- and post whitespaces from string
char* trim_string(char* str){
    // https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
    if(str == NULL || strlen(str) < 1) return str;

    char *start = str;
    while(isspace((unsigned char)*start)) start++; // remove all leading whitespaces

    if(start != str){
        memmove(str, start, strlen(start)+1); // we removed some whitespaces, move the original pointer to point at first valid character
    }

    // remove trailing whitespaces
    size_t len = strlen(str);
    while(len > 0 && isspace((unsigned char)str[len-1])){
        str[len-1] = '\0';
        len--;
    }
    return str;
}


// Updates cleaned to be a copy of dirty without any isspace characters
void clean_string(char* cleaned, const char* dirty){

    if(cleaned == NULL) return;
    *cleaned = '\0';
    
    if(dirty == NULL) return;

    while(*dirty != '\0'){
        if(!isspace((unsigned char)*dirty)){
            *cleaned = *dirty;
            cleaned++;
        }
        dirty++;
    }
    *cleaned = '\0'; // end cleaned char*
}

size_t get_total_nr_commands(char** line){
    if(*line == NULL || **line == '\0'){
        return 0;
    }
    size_t nr_commands = 0;
    const char *ptr = *line;
    while(*ptr != '\0'){
        if(*ptr == '|' || *ptr == '&'){
            nr_commands++;
        }
        ptr++;
    }
    return nr_commands+1; // first command is not proceeded by a | or &
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
        commands->command_arr[command_num].args = NULL;
        commands->command_arr[command_num].num_args = 0;
        commands->command_arr[command_num].output_file = NULL;
    }
    return commands;
}

void free_commandsArr(CommandsArr* commands){
    if(commands == NULL) return;

    for(size_t command_num = 0; command_num < commands->size; command_num++){
        Command* cmd = &commands->command_arr[command_num];
        if(cmd->args != NULL){
            for(size_t i = 0; i < cmd->num_args; i++){
                free(cmd->args[i]);
            }
        }
        free(cmd->args);
        cmd->args = NULL;

        if(cmd->output_file != NULL){
            free(cmd->output_file);   
            cmd->output_file = NULL;
        }
    }
    if(commands->command_arr != NULL){
        free(commands->command_arr);
    }
    free(commands);
}

void print_args(char** args, size_t num_args){
    for(size_t i = 0; i < num_args+1; i++){
        printf("argument i=%ld: %s\n", i,args[i]);
    }
}

int is_valid_filename(const char* filename){
    if(filename == NULL || *filename == '\0'){
        return 0;
    }
    while (*filename != '\0'){
        if(isspace((unsigned char)*filename)){
            return 0;
        }
        filename++;
    }
    return 1;
}

size_t parse_command(char* command_token, Command* command_obj, int command_nr){
    size_t arg_num = 0;
    command_obj->args = NULL;
    command_obj->num_args = 0;
    command_obj->output_file = NULL;

    // split command token into actual command and redirect part (if any)
    char* cmd_part = NULL;
    char* remaining = command_token;
    char* next_redirect = strchr(remaining, '>'); // find first '>'
    while (next_redirect != NULL){
        // this is the first redirect
        if(cmd_part == NULL){
            size_t cmd_length = next_redirect - remaining;
            cmd_part = strndup(remaining, cmd_length);
            if(cmd_part == NULL){
                write(STDERR_FILENO, error_message, strlen(error_message)); 
                return arg_num;
            }
            if(strlen(cmd_part) == 0){
                write(STDERR_FILENO, error_message, strlen(error_message)); 
                free(cmd_part);
                return arg_num;
            }
            if(DEBUG) printf("cleaned command part: %s\n", cmd_part);
        }

        // TODO: implement append and other ">"-like behaviour???


        char* file_start = next_redirect + 1;
        next_redirect = strchr(file_start, '>');
        if(next_redirect != NULL){
            // we dont allow multiple redirects
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            free(cmd_part);
            return arg_num;
        }

        // file length is size between (file_start - end) OR (file_start to next redirect) ">"
        size_t file_length = strlen(file_start); // next_redirect != NULL ? (size_t)(next_redirect- file_start) : strlen(file_start);
        if(file_length == 0) {
            // invalid filename --> should not continue
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            free(cmd_part);
            return arg_num; 
        
        }
        char* filename = strndup(file_start, file_length);

        filename = trim_string(filename);
        //printf("Redirect file: %s\n",filename);
        if(is_valid_filename(filename) == 0) {
            // invalid filename --> should not continue
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            free(cmd_part);
            free(filename);
            return arg_num;
        } 

        // TODO: Multiple redirects in one command?
        if(command_obj->output_file != NULL){
            free(command_obj->output_file); // free old output file
        }
        command_obj->output_file = strdup(filename);
        free(filename);
        remaining = next_redirect;
        if(!remaining) break;
    }
    char* cmd_part_to_free = NULL;
    if(cmd_part == NULL){ // we had no redirects
        cmd_part = strdup(command_token);
        cmd_part_to_free = cmd_part;
    }
    else{
        cmd_part_to_free = cmd_part;
    }

    char* strsep_tracker = cmd_part;

    const char* delim = " ";
    char* token = strsep(&strsep_tracker, delim); // split command by whitespace 
    while(token != NULL){
        char* clean_token = malloc(strlen(token)+1);
        if(clean_token == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            //fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
            free(cmd_part_to_free);
            return 0;
        }
        clean_string(clean_token, token); // clean string i.e remove whitespaces and \t etc

        size_t length = strlen(clean_token);
        if(length == 0 || isspace(*clean_token)) { // if the entire token is empty
            free(clean_token);
            token = strsep(&strsep_tracker, delim); // skip space-only or empty tokens
            continue;
        }

        // resize our args array
        char** tmp = realloc(command_obj->args, (arg_num+2)*sizeof(char*));
        if(tmp == NULL){
            //fprintf(stderr, "NULL POINTER at line %d\n", __LINE__);
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            free(cmd_part_to_free);
            free(clean_token);
            return 0;
        }
        command_obj->args = tmp;
        command_obj->args[arg_num] = strdup(clean_token);
        command_obj->args[arg_num+1] = NULL; // last argument must be NULL for execvp, otherwise it crashes
        
        arg_num++;

        free(clean_token); 
        token = strsep(&strsep_tracker, delim); 
    }
    free(cmd_part_to_free);
    if(arg_num > 0){
        command_obj->num_args = arg_num;
    }
    else{
        // failed to find any arguments
        free(command_obj->args);
        command_obj->args = NULL;
    }
    return arg_num;
}

// takes in a line and returns an array of commands 
CommandsArr* parse_line(char** line, unsigned int line_size){
    size_t max_commands = get_total_nr_commands(line);     
    CommandsArr* commands = allocate_commands_arr(max_commands);
    if(commands == NULL){
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        return NULL;
    }

    size_t command_num = 0; // number of current command    

    // split line into commands by pipe symbol | and &
    char* remaining = *line;
    char* next_delim = NULL;

    int keep_running = 1;

    do {

        // defend against white spaces and empty commands
        while(*remaining != '\0' && isspace((unsigned char)*remaining)){
            remaining++;
        }

        if(*remaining == '\0'){
            break;
        }

        // locate occurences of | or & and to split our line into commands
        next_delim = strpbrk(remaining, "|&");
        NextOp curr_op = OP_NONE;
        size_t cmd_segment_length;
        if(next_delim != NULL){
            if(*next_delim == '|'){
                curr_op = OP_PIPE;
            }
            else if (*next_delim == '&'){
                curr_op = OP_BACKGROUND;
            }
            *next_delim = '\0'; // remove delimiter from command
            cmd_segment_length = next_delim - remaining;
        }
        else{
            // no delimiter found --> final command
            curr_op = OP_NONE;
            cmd_segment_length = strlen(remaining);
            keep_running = 0;
        }
        
        // copy the master command token (parse_command may mangle it)
        char* command_token_cpy = strndup(remaining, cmd_segment_length);
        if(command_token_cpy == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            free_commandsArr(commands);
            return NULL;
        }
        if(DEBUG) printf("command_token_cpy %s\n", command_token_cpy);
        size_t arg_num = parse_command(command_token_cpy, &commands->command_arr[command_num], command_num);

        free(command_token_cpy); // remove copy
        if(arg_num == 0) {
            // we hit an invalid command
            if(!keep_running || next_delim == NULL){
                break;
            }
            remaining = next_delim + 1;
            continue;
        }
        commands->command_arr[command_num].operation = curr_op;
        commands->size++;
        command_num++;

        if(keep_running){
            remaining = next_delim+1;
        }
    } while(keep_running);

    
    return commands;
}

size_t spawn_commands(CommandsArr* commands, size_t start, size_t end, paths_data** paths_struct, pid_t* pids){
    size_t nr_cmds_started = 0;
    int in_fd = -1;
    //if(DEBUG) printf("start: %ld\n", start);
    //if(DEBUG) printf("end: %ld\n", end);
    for (size_t k = start; k <= end; k++){
        Command* command = &commands->command_arr[k];

        int fds[2] = {-1, -1};
        if(k < end && pipe(fds) < 0){
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            break;
        }
        fflush(stdout);

        pid_t pid = fork();
        if(pid < 0){
            // fork was unsuccessful
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            if(fds[0] != -1){
                close(fds[0]);
                close(fds[1]);
            }
            break;
        }
        else if (pid == 0){

            if(in_fd != -1){ // We open stdin from pipe
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if(fds[1] != -1){ // We open stdout to pipe
                close(fds[0]);
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
            }
            if(command->output_file != NULL){
                // we have a redirect!
                int fd = open(command->output_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
                if(fd < 0){ // could not open file
                    write(STDERR_FILENO, error_message, strlen(error_message)); 
                    exit(1); // kill child process
                }
                dup2(fd, STDOUT_FILENO); // redirect stdout
                dup2(fd, STDERR_FILENO); // redirect stderr
                close(fd); // no need to keep open
            }

            //if(DEBUG) print_args(args, nr_args);
            char* path = NULL;
            if(which_path(&path, command->args[0], paths_struct) != 0){
                // could not find path
                write(STDERR_FILENO, error_message, strlen(error_message)); 
                exit(1); // kill child process
            }

            execv(path, command->args);
            // if execvp fails this process must die
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            exit(1);
        }

        if(in_fd != -1){
            close(in_fd);
        }
        if(fds[1] != -1){
            close(fds[1]);
        }
        in_fd = fds[0];
        pids[nr_cmds_started] = pid;
        nr_cmds_started++;
    }
    if(in_fd != -1) close(in_fd);


    return nr_cmds_started;
}


int is_builtin(char* executable){ // FIXME duplicate "str"comparisons here and in run_builtin
    if(strcmp(executable, "exit") == 0){
        return 1;
    }else if(strcmp(executable, "cd") == 0){
        return 1;
    }else if(strcmp(executable, "path") == 0){
        return 1;
    }
    return 0;
}

int run_builtin(char* executable, char**args, size_t num_args, paths_data** paths_struct, CommandsArr* commands){
    if(strcmp(executable, "exit") == 0){
        if(num_args != 1){ // invalid number of arguments
            write(STDERR_FILENO, error_message, strlen(error_message)); 
        }
        else{
            free_commandsArr(commands);
            return 0;
        }
    }
    else if(strcmp(executable, "cd") == 0){
        if(num_args != 2){ // we want exactly one argument after cd
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

        if(num_args == 1) return 1; // we dont want to realloc to 0 so we just skip.
        // update our paths struct to hold arg_num-1 pointers

        char** tmp = realloc((*paths_struct)->paths, (num_args-1)*sizeof(char*));
        if(tmp == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            free_commandsArr(commands);
            return 1;
        }
        (*paths_struct)->paths = tmp;

        int i;
        for(i = 1; i < num_args; i++){
            fix_path(&args[i]);
            (*paths_struct)->paths[i-1] = strdup(args[i]);
            (*paths_struct)->nr_paths++;

            //if(DEBUG) printf("adding path: %s at index %d\n", (*paths_struct)->paths[i-1], i-1);
            if(strlen(args[i]) > (*paths_struct)->longest_path){
                (*paths_struct)->longest_path = strlen(args[i]);
            }
        }
    }
    return 1;
}


int handle_command(char** line, size_t len, ssize_t read, FILE* input, paths_data** paths_struct){
    errno = 0;
    read = getline(line, &len, input);
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
        if((*line)[read - 1] == '\n'){ // remove trailing new line
            (*line)[read - 1] = '\0';
            read--;
        }
        
        CommandsArr* commands = parse_line(line, len);
        
        
        pid_t* pids = calloc(1,commands->size*sizeof(pid_t));
        size_t nr_children = 0;

        if(DEBUG) printf("nr of commands to run: %ld\n", commands->size);
        size_t command_num = 0;
        while(command_num < commands->size){
            //printf("currently starting command nr: %ld\n", command_num);
            // calculate how many pipelines 
            size_t end = command_num;
            while(end + 1 < commands->size && commands->command_arr[end].operation == OP_PIPE){
                end++;
            }


            char* executable = commands->command_arr[command_num].args[0];
            char** args = commands->command_arr[command_num].args;
            size_t num_args = commands->command_arr[command_num].num_args;
            // if (DEBUG)printf("executable: %s\n", executable);
            // if (DEBUG)printf("num_args: %ld\n", num_args);
            // if (DEBUG)printf("command_num: %ld\n", command_num);
            // if (DEBUG)printf("end: %ld\n", end);

            // a builtin 
            if(command_num == end && is_builtin(executable)){
                int success = run_builtin(executable, args, num_args, paths_struct, commands);
                if(success == 0){
                    // EXIT!
                    return 0;
                }
            }
            else{
                // note that pids is offset by the number of already running children
                nr_children += spawn_commands(commands, command_num, end, paths_struct, pids+nr_children);
            }
            command_num = end +1;
        }
        
        if(DEBUG){
            for(size_t k = 0; k < nr_children; k++){
                printf("waiting for pid=%d with id=%ld\n", pids[k], k);
            }
        }
        //pid_t child_pid;
        //while((waitpid(-1, NULL, 0) != -1));
        for(size_t k = 0; k < nr_children; k++){
            waitpid(pids[k], NULL, 0);
            // if(waitpid(pids[k], NULL, 0) < 0){
            //     fprintf(stderr, "waitpid(%d) failed: %s\n", (int)pids[k], strerror(errno));
            // }
        }
        free(pids);
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
        if(access(filename, F_OK) == 0){
            input_file = fopen(filename, "r"); 
        }
        else{ // input file does not exist
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            exit(1);
        }
    }
    else if (argc != 1){
        // invalid number of arguments
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        exit(1);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read = 0;

    // set up default path
    paths_data* paths_struct = malloc(sizeof(paths_data));
    if(paths_struct == NULL){
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        exit(1);
    }
    paths_struct->paths = malloc(1000*sizeof(char*)); // fixme
    if(paths_struct->paths == NULL){
        write(STDERR_FILENO, error_message, strlen(error_message)); 
        exit(1);
    }
    paths_struct->paths[0] = "/bin/";
    paths_struct->longest_path = strlen(paths_struct->paths[0]);
    paths_struct->nr_paths = 1;

    int running = 1;
    while(running) {
        if(argc!=2) printf("wish> ");
        running = handle_command(&line, len, read, input_file, &paths_struct);
    }
    free(line);
    free(paths_struct->paths);
    free(paths_struct);
    if(argc == 2){
        fclose(input_file);
    }
    return 0;
}