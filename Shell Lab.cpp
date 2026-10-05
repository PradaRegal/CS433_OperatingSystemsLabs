/**
 * Lab #1: Basic Shell
 * @file shell.cpp
 * @brief A simple UNIX shell: reads command lines, runs external commands
 *        in a child process (fork + execvp + wait), supports background
 *        execution ('&') and input/output redirection (<, >, >>), and
 *        implements the built-ins cd, pwd, echo, exit, history, help.
 *
 * Build:  make
 * Run:    ./shell
 *
 * Complete every function marked with a TODO. You may add helper
 * functions as needed. Do not rename this file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

using namespace std;

#define MAX_LINE 80               // The maximum length of a command line
#define MAX_ARGS (MAX_LINE / 2 + 1) // Max number of parsed arguments
#define NEW_FILE_MODE 0644        // Permissions for files created by redirection:
                                  // owner read/write, group and others read

/**
 * PROVIDED — do not modify.
 *
 * @brief Split a command line into an argument array on whitespace.
 *        Strips the trailing newline and a trailing '&' (which sets
 *        *background to true). Recognizes redirection operators <, >,
 *        and >> (each followed by a file name, separated by spaces).
 *
 *        Note: the tokens in `args` are not copies — `strtok` splits
 *        `command` in place, so each entry points into the `command`
 *        buffer. Keep `command` alive until you are done with `args`.
 *
 * @param command    the raw line read from the user
 * @param args       output array of argument strings
 * @param background set to true if the line ended in '&'
 * @param redir_in   input file for '<' (NULL if none)
 * @param redir_out  output file for '>' or '>>' (NULL if none)
 * @param redir_append set to true if the operator was '>>'
 * @return           number of arguments (0 for an empty line)
 */
int parse_command(char command[], char *args[], bool *background,
                 const char **redir_in, const char **redir_out,
                 bool *redir_append)
{
    *background = false;
    *redir_in = NULL;
    *redir_out = NULL;
    *redir_append = false;
    int n = 0;

    // Strip the trailing newline
    size_t len = strlen(command);
    while (len > 0 && (command[len - 1] == '\n' || command[len - 1] == '\r'))
        command[--len] = '\0';

    // Strip a trailing '&' (background execution)
    len = strlen(command);
    while (len > 0 && command[len - 1] == ' ')
        command[--len] = '\0';
    if (len > 0 && command[len - 1] == '&') {
        command[len - 1] = '\0';
        *background = true;
    }

    // Tokenize on whitespace; skip redirection operators and record their files
    char *token = strtok(command, " \t");
    while (token != NULL && n < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            char *file = strtok(NULL, " \t");
            if (file == NULL)
                break; // missing file name; ignore the operator
            *redir_in = file;
        } else if (strcmp(token, ">") == 0) {
            char *file = strtok(NULL, " \t");
            if (file == NULL)
                break;
            *redir_out = file;
            *redir_append = false;
        } else if (strcmp(token, ">>") == 0) {
            char *file = strtok(NULL, " \t");
            if (file == NULL)
                break;
            *redir_out = file;
            *redir_append = true;
        } else {
            args[n++] = token;
        }
        token = strtok(NULL, " \t");
    }
    args[n] = NULL; // execvp requires a NULL-terminated argument list
    return n;
}

/**
 * @brief Open a redirection file and connect it to stdin or stdout in the
 *        current process, saving the original descriptor so it can be
 *        restored later (used for built-ins; a child that is about to
 *        execvp does not need the saved descriptor).
 *
 * @param file     the file name
 * @param append   true for '>>'
 * @param to_stdout true to redirect stdout, false for stdin
 * @param saved_fd [out] the saved original descriptor (-1 on failure)
 */
void apply_redirection(const char *file, bool append, bool to_stdout,
                       int *saved_fd)
{
    // TODO:
    // 1. open() the file: O_WRONLY | O_CREAT (plus O_APPEND if append)
    //    for output, O_RDONLY for input. On failure, print an error
    //    with strerror(errno) and set *saved_fd = -1.
    // 2. Save the original descriptor: *saved_fd = dup(target), where
    //    target is STDOUT_FILENO or STDIN_FILENO.
    // 3. Swap: dup2(fd, target), then close(fd).
    *saved_fd = -1;
    int fd = -1;
    if (to_stdout) {
        // TODO: compute flags for open with "|" operator:
        // (O_WRONLY | O_CREAT, plus O_APPEND or O_TRUNC),
        // then open the file with NEW_FILE_MODE. (below is our addition)
        int flags; 
        if (append){
            flags = O_WRONLY | O_CREAT | O_APPEND; //preserves files and writes at end
        } else{
            flags = O_WRONLY | O_CREAT | O_TRUNC; //erases files existing contents
        }
        fd = open(file, flags, NEW_FILE_MODE);
    } else {
        // Open for reading (O_RDONLY)
        fd = open(file, O_RDONLY);
    }
    if (fd < 0) {
        fprintf(stderr, "osh: %s: %s\n", file, strerror(errno));
        return;
    }
    // TODO (steps 2 and 3 above): compute target (STDOUT_FILENO or
    // STDIN_FILENO), save it with *saved_fd = dup(target), swap with
    // dup2(fd, target), then close(fd).

    // LINES OF CODE THAT WE ADDED TO NOTE BACK ON!!!!!
    int target;
    if (to_stdout) {
        target = STDOUT_FILENO;
    } else {
        target = STDIN_FILENO;
    }
    *saved_fd = dup(target);
    dup2(fd,target);
    close(fd);
    /////
}

/**
 * @brief Restore the original stdin/stdout descriptors saved by
 *        apply_redirection (used after running a built-in).
 */
void restore_redirection(int saved_in, int saved_out)
{
    // TODO: for each saved descriptor that is >= 0, dup2 it back onto
    //       STDIN_FILENO / STDOUT_FILENO, then close it.
    if (saved_in >= 0) {
        // Fill in
        dup2(saved_in,STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out >= 0) {
        // Fill in
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
}

/**
 * @brief Change the shell's working directory (built-in, so the change
 *        survives in the shell process itself).
 * @param args  argument vector; args[1] is the directory to change to
 */
void builtin_cd(char *args[])
{
    // TODO:
    // - No argument: change to the home directory (getenv("HOME")).
    // - Otherwise: chdir(args[1]).
    // - On failure, print an error message.
    const char *directory;
    if(args[1] == NULL){
        directory = getenv("HOME");
    } else{
        directory = args[1];
    }

    if(directory == NULL) {
        fprintf(stderr, "osh: cd: HOME not set\n");
        return;
    }

    if(chdir(directory)<0) {
        fprintf(stderr, "osh: cd: %s: %s\n", directory, strerror(errno));
    }
}

/**
 * @brief Print the current working directory (built-in).
 */
void builtin_pwd(void)
{
    // TODO: use getcwd() and print the result.
    char *cwd = getcwd(NULL, 0); // allocates enough mem for directory path

    if (cwd != NULL) {
        printf("%s\n", cwd);
        free(cwd); // releases allocated memory
    } else {
        fprintf(stderr, "osh: pwd: %s\n", strerror(errno)); // prints error
    }
    
}

/**
 * @brief Print the arguments of echo separated by spaces (built-in).
 */
void builtin_echo(char *args[])
{
    // TODO: print args[1..n] separated by single spaces, then a newline.
    for (int i = 1; args[i] != NULL; i++) {
        if (i > 1) {
            printf(" ");
        }
        printf("%s", args[i]);
    }
    printf("\n");
}


/**
 * @brief Print all previously entered command lines, numbered (built-in).
 */
void builtin_history(const vector<string> &history)
{
    // TODO: print each stored line numbered from 1.
    for (size_t i = 0; i < history.size(); i++) {
        printf("%zu %s\n", i + 1, history[i].c_str());
    }
}

/**
 * PROVIDED — do not modify.
 *
 * @brief Print the list of built-in commands and usage (built-in).
 */
void builtin_help(void)
{
    printf("Built-in commands (run in the shell itself):\n");
    printf("  cd [dir]     change the shell's working directory\n");
    printf("  pwd          print the current working directory\n");
    printf("  echo <text>  print text\n");
    printf("  exit         leave the shell\n");
    printf("  history      show previously entered command lines\n");
    printf("  help         show this message\n");
    printf("External commands (run in a child process):\n");
    printf("  ls           list directory contents\n");
    printf("  cat          print file contents\n");
    printf("  ...          any other program in your PATH\n");
    printf("Append '&' to run a command in the background.\n");
    printf("Use '<', '>', '>>' (followed by a file name) to redirect\n");
    printf("input and output of external commands.\n");
}

/**
 * @brief Main loop of the simple UNIX shell.
 * @return exit status of the program
 */
int main(int argc, char *argv[])
{
    char command[MAX_LINE];            // the command that was entered
    char *args[MAX_ARGS];             // hold parsed out command line arguments
    vector<string> history;           // previously entered command lines
    int job_count = 0;                // counter for background jobs

    while (true)
    {
        printf("osh>");
        fflush(stdout);

        // Read the input command; NULL means EOF (Ctrl+D) -> exit
        if (fgets(command, MAX_LINE, stdin) == NULL)
            break;

        // 1. Remember only non-empty lines in `history` (for the history
        //    built-in). Strip the trailing newline first.
        if (command[0] != '\0' && command[0] != '\n') {
            // TODO: strip the trailing newline and carriage return, then push
            //      the line into history.
            size_t len = strlen(command); // finds command length
            while (len > 0 && (command[len -1] == '\n' || command[len -1]== 'r')) {
                command[--len] = '\0'; // loop removes \n or \r
            }
            history.push_back(command); //adds cleaned command to history vector
        }
        
        // 2. Parse the line with parse_command(). If it returned 0
        //    arguments, just re-prompt.
        bool background = false, redir_append = false;
        const char *redir_in = NULL, *redir_out = NULL;
        int num_args = parse_command(command, args, &background,
                                    &redir_in, &redir_out, &redir_append);
        if (num_args == 0)
            continue; // empty line, just re-prompt

        // 3. Built-ins (cd, pwd, echo, exit, history, help) run in the
        //    shell process itself, with no fork. If the line includes
        //    redirection, apply it in the shell process with
        //    apply_redirection() (saving the original descriptors),
        //    run the built-in, then call restore_redirection() so the
        //    shell's own stdin/stdout survive. 'exit' leaves the loop.

        bool is_builtin = false;
        // a. use strcmp() to check if args[0] is one of the built-ins
        is_builtin = strcmp(args[0], "cd") == 0 ||
                     strcmp(args[0], "pwd") == 0 ||
                     strcmp(args[0], "echo") == 0 ||
                     strcmp(args[0], "exit") == 0 ||
                     strcmp(args[0], "history") == 0 ||
                     strcmp(args[0], "help") == 0;
        // b. Branching structure for each built-in command
        if (is_builtin) { // runs block only for the arguments above
            int saved_in = -1;
            int saved_out = -1;
            bool redirection_ok = true; // tracks whether redirection has been successfully set up
            if (redir_in != NULL) {
                apply_redirection(redir_in, false, false, &saved_in);
                if (saved_in < 0) {
                    redirection_ok = false;
                }
            }
            if (redirection_ok && redir_out != NULL) {
                apply_redirection(redir_out, redir_append, true, &saved_out);
                if (saved_out < 0) {
                    redirection_ok = false;
                }
            }

            bool should_exit = false;
            if (redirection_ok) {
                if (strcmp(args[0], "cd") == 0){
                    builtin_cd(args);
                } else if (strcmp(args[0], "pwd") == 0) {
                    builtin_pwd();
                } else if (strcmp(args[0], "echo") == 0) {
                    builtin_echo(args);
                } else if (strcmp(args[0], "exit")== 0) {
                    should_exit = true;
                } else if (strcmp(args[0], "history") == 0) {
                    builtin_history(history);
                } else if (strcmp(args[0], "help")==0) {
                    builtin_help();
                }

                fflush(stdout); // finished writing directed output
                restore_redirection(saved_in,saved_out); // reconnects input/output to terminal

                if(should_exit){
                    break; // leaves shell loop if command was exit
                }
                continue; // returns to next prompt and prevents built ins from reaching external command section

        }

        // 4. External commands run in a child process:
        //    - The parent calls fork().
        //    - In the child: set up any redirection (<, >, >>) with
        //      open() + dup2(), then call execvp(args[0], args).
        //      If execvp() returns, the command was not found: print
        //      "osh: <command>: command not found" and _exit(127).
        //    - In the parent: if the line ended in '&', print
        //      "[<job#>] <pid>" (job# starts at 1) and keep reading
        //      commands. Otherwise wait for the child with waitpid().
        pid_t pid = fork();

        // check if fork failed
        if (pid < 0)
        {
            fprintf(stderr, "osh: fork: %s\n", strerror(errno));
            continue;
        }

        if (pid == 0) {
            // in child, redirect standard input if command uses < 
            if (redir_in != NULL) {
                int saved_in = -1;
            int saved_in = -1;

            apply_redirection(redir_in, false, false, &saved_in);
            // end child if input file could not be opened
            if (saved_in < 0){
                _exit(1);
                }
        
            // child will not restore its original input
            close (saved_in);
            }

            if (redir_out != NULL) { // redirects standard output for > or >>
                int saved_out = -1;

                apply_redirection(redir_out, redir_append, true, &saved_out);

                // end child if failed
                if (saved_out < 0 ){
                    _exit(1);
                }
                close(saved_out); // will not restore its original output

                //. replace child proceses w requested command
                execvp(args[0], args);
                fprintf(stderr, "osh: %s: command not found\n", args[0]); // returns when failed
                _exit(127);
            }
        }

        // parent process
        if(background){
            job_count++;
            printf("[%d] %ld", job_count, (long)pid);
            fflush(stdout);
        } else {
            waitpid(pid, NULL, 0);
        }





    }
    return 0;
}


}
