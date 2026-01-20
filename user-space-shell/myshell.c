#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define LINE 1024
#define NUM_OF_ARGS 100
#define STR_SEP " \t\n"

// redirection: '<' '>' '>>'
static void handle_redirection(char *args[], int allow_in, int allow_out) {
    int in = -1; // index of <
    int out = -1; // index of > or >>
    int append = 0; // bool if there is << for adding to file

    for (int j = 0; args[j] != NULL; ++j) { // looks for signs
        if (allow_in  && strcmp(args[j], "<")  == 0) {
            in  = j;
        }
        if (allow_out && strcmp(args[j], ">>") == 0) {
            out = j;
            append = 1;
        }
        else if (allow_out && strcmp(args[j], ">") == 0) {
            out = j;
        }
    }

    if (in >= 0 && args[in+1]) { // if there is a file to redirect
        int fdin = open(args[in+1], O_RDONLY); // open read
        if (fdin < 0) { perror("open"); _exit(1); } // error and exit
        dup2(fdin, STDIN_FILENO);
        close(fdin);
        args[in] = args[in+1] = NULL;
    }
    if (out >= 0 && args[out+1]) { // if there is > or >>
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC); // checking if to append or not
        int fdout = open(args[out+1], flags, 0); //opening file according to flags
        if (fdout < 0) { perror("open"); _exit(1); } // error and exit
        dup2(fdout, STDOUT_FILENO);
        close(fdout);
        args[out] = args[out+1] = NULL;
    }
}

int main() {
    char line[LINE];
    char *args[NUM_OF_ARGS];

    while (1) {
        write(1, ">> ", 3);
        if (fgets(line, LINE, stdin) == NULL) break;

        // split input to arguments
        args[0] = strtok(line, STR_SEP);
        int i = 0;
        while (args[i] && i < NUM_OF_ARGS - 1) {
            args[++i] = strtok(NULL, STR_SEP);
        }
        if (!args[0]) continue;

        // check pipe
        int pipe_index = -1;
        for (int j = 0; j < i; ++j) {
            if (strcmp(args[j], "|") == 0) { pipe_index = j; break; }
        }

        if (pipe_index >= 0) { // if we found pipe in the command
            // pipe handling: first command may redirect input, second may redirect output
            args[pipe_index] = NULL;
            char **cmd1 = args;
            char **cmd2 = &args[pipe_index + 1];

            int fd[2];
            if (pipe(fd) < 0) {
                perror("pipe");
                continue;
            }

            if (fork() == 0) {
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                handle_redirection(cmd1, 1, 0);
                execvp(cmd1[0], cmd1);
                perror("execvp"); _exit(1);
            }
            if (fork() == 0) {
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                handle_redirection(cmd2, 0, 1);
                execvp(cmd2[0], cmd2);
                perror("execvp"); _exit(1);
            }
            close(fd[0]); close(fd[1]);
            waitpid(-1, NULL, 0);
            waitpid(-1, NULL, 0);

        } else { // no pipe
            // single command: allow both input and output redirection
            if (fork() == 0) {
                handle_redirection(args, 1, 1);
                execvp(args[0], args);
                perror("execvp"); _exit(1);
            }
            waitpid(-1, NULL, 0);
        }
    }

    write(1, "\n", 1);
    return 0;
}
