#include <stdio.h>

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

const char error_message[30] = "An error has occurred\n";

int main() {

    int running = 1;
    while(running) {

        
    }
}