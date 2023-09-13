/* 
*   C Shell Implementation
*   V 0.2
*
*   C implementation of the shell/terminal.
*
*   Authors: Johana Ramirez and John Manalac.
*   Last changed: Dec 11, 2022.
*
*/

/* Header files. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

/* Shell Constants */
#define MAX_PATH 1024
#define MAX_INPUT 4096
#define MAX_ARGS 10
#define COMMAND_SEPARATOR " " 
#define EXEC_SUCCESS 0
#define EXEC_FAILURE -1
#define REG_CMD 0
#define INPUT 1
#define OUTPUT 2
#define OUTPUT_APPEND 3
#define PIPE 4

/* Function Prototypes. */

/* Error Checks (exit on failure) */
static inline void perror_exit(char *cmd);
static inline void execvp_and_handle_error(char *program, char *argv[]);
static inline void close_pipes(int pipe_fds[]);

/* Shell */
void init_shell();
void exit_shell();
void print_prompt();
void get_input(char *buf);
int parse_input_and_exec(char *input, char *dlim);
void save_command(char *dest[], char *command[], int cmd_index);
int exec_command(int mode, char *command[], int io_file_fd);

/* Redirect I/O */
void redirect_io(int mode, char *command[], char *io_file, int exec_redirection);
void exec_redir_bothio(int output_mode, char *command[], char *input_file, char *output_file);

/* Piping */
void input_pipe(int *pipe_fds, char *pipe_cmd[]);
void inter_pipe(int *pipe_fds, char *pipe_cmd[]);
void output_pipe(int *pipe_fds, char *output_cmd[]);
void input_pipe_redirect(int *pipe_fds, char *pipe_cmd[], char *input_file);
void output_pipe_redirect(int *pipe_fds, int output_mode, char *output_cmd[], char *output_file);

/* MAIN */

int main(int argc, char *argv[])
{
    char user_input[MAX_INPUT];

    /* Init shell and loop. */
    init_shell();
    while (1)
    { 
        print_prompt();
        get_input(user_input);
        if (user_input[0] != '\0')
        {
            parse_input_and_exec(user_input, COMMAND_SEPARATOR);
        }
    }
}

/* Function Implementations. */

/* Call perror on failed command and exit
   the current process with EXIT_FAILURE. */

static inline void perror_exit(char *cmd)
{
    perror(cmd);
    exit(EXIT_FAILURE);
}

/* Execute a program with execvp. On failure, 
   an error message will be printed to stderr 
   and the current process will exit. */

static inline void execvp_and_handle_error(char *program, char *argv[])
{
    if (execvp(program, argv) < 0)
    {
        fprintf(stderr, "%s ", program);
        perror_exit("execvp()");
    }
}

/* Close the pipes in the array pointed to by 
   pipe_fds, which must be an int array of 
   size 2. On failure, an error message will 
   be printed to stderr and the current process 
   will exit. */

static inline void close_pipes(int pipe_fds[])
{
    if (close(pipe_fds[0]) < 0)
    {
        perror_exit("close()");
    }
    if (close(pipe_fds[1]) < 0)
    {
        perror_exit("close()");
    }
}

/* Initialize the shell (when shell is executed). */

void init_shell()
{
    printf("\n");
    printf("  |  Shell Implementation  | \n");
    printf("  | Authors: John & Johana |  \n");
    printf("  |         CS 315         |  \n\n");
}

/* Exit the shell. */

void exit_shell()
{
    printf("\n");
    printf("...Exiting shell\n");
    printf("Exited shell!\n\n");
    exit(EXIT_SUCCESS);
}

/* Print the shell prompt. */

void print_prompt()
{
    char *username, *cdirectory;
    char hostname[MAX_PATH + 1], cdir_path[MAX_PATH + 1];
    int print_basic_prompt = 0;

    if ((username = getenv("USER")) == NULL)
    {
        print_basic_prompt = 1;
    }
    if (gethostname(hostname, MAX_PATH + 1) < 0)
    {
        print_basic_prompt = 1;
    }
    if (getcwd(cdir_path, MAX_PATH + 1) == NULL)
    {
        print_basic_prompt = 1;
    }

    /* Print simple prompt if any of the above fails. */
    if (print_basic_prompt)
    {
        printf("SHELL$ ");
    }
    else        
    {
        cdirectory = strrchr(cdir_path, '/') + 1;   
        printf("[%s@%s %s] JSHELL$ ", username, hostname, cdirectory);
    }

}

/* Get up to MAX_SHELL_INPUT bytes from user 
   input into the buffer starting at buf. The 
   trailing new line will be replaced with '\0'.
   If the user shortcuts Cltr-D or inputs "exit",
   the program will exit. If fgets fails, an error 
   diagnostic will be printed, and the program will exit. */

void get_input(char *buf)
{
    if (fgets(buf, MAX_INPUT, stdin) == NULL)
    {
        /* Check end-of-line (CLTR-D). */
        if (feof(stdin))
        {
            exit_shell();
        }
        else
        {
            perror_exit("get_input()");
        }
    }

    /* User inputs exit. */
    if (strcmp(buf, "exit\n") == 0)
    {
        exit_shell();
    }

    /* Replace trailing new line. */
    size_t last_byte = strlen(buf) - 1;
    if (buf[last_byte] == '\n')
    {
        buf[last_byte] = '\0';
    }
}

/* Parse an input string by delimiter dlim. 
   If the input represents a regular command,
   it will execute after fully parsing the string. 
   If not, the function will fork to perform
   either I/O redirection and/or piping.  */

int parse_input_and_exec(char *input, char *dlim)
{
    char *command[MAX_ARGS + 1] = {NULL,};
    char *redirect_cmd[MAX_ARGS + 1] = {NULL,}; 
    char *input_file = NULL;
    char *token = strtok(input, dlim);
    int output_before_pipe = 0;
    int cmd_index = 0;
    int redirect_mode = REG_CMD;
    int pipe_fds[2]; 

    for (int i = 0; i < MAX_INPUT; i++)
    {
        /* User has called too many args. */
        if (cmd_index >= MAX_ARGS + 1)
        {
            printf("Too many args.\n");
            return EXEC_FAILURE;
        }

        /* Reached end of input string. */
        if (token == NULL)
        {
            command[cmd_index] = NULL;
            break;
        }

        /* I/O Redirection. */
        if (strcmp(token, "<") == 0)
        {
            if (redirect_mode != REG_CMD)
            {
                printf("Cannot perform redirection before input.\n");
                return EXEC_FAILURE;
            }
            else
            {
                save_command(redirect_cmd, command, cmd_index);
                redirect_mode = INPUT;
                cmd_index = 0;
            }
        }
        else if (strcmp(token, ">") == 0 || strcmp(token, ">>") == 0)
        {
            int output_mode;
            (strcmp(token, ">>") == 0) ? (output_mode = OUTPUT_APPEND) : (output_mode = OUTPUT); 

            if (redirect_mode == INPUT)
            {
                if (command[0] == NULL)
                {
                    printf("No file for input redirection.");
                    return EXEC_FAILURE;
                }
                input_file = command[0];
            }
            else if (redirect_mode == OUTPUT || redirect_mode == OUTPUT_APPEND)
            {
                if (command[0] == NULL)
                {
                    printf("No file for output redirection.");
                    return EXEC_FAILURE;
                }
                redirect_io(redirect_mode, NULL, command[0], 0);
            }
            else 
            {
                if (redirect_mode == PIPE)
                {
                    output_before_pipe = 1;
                }
                save_command(redirect_cmd, command, cmd_index);
            }
            redirect_mode = output_mode;
            cmd_index = 0;
        }
        else if (strcmp(token, "|") == 0)
        {
            if (redirect_mode == OUTPUT || redirect_mode == OUTPUT_APPEND)
            {
                printf("Cannot perform output operation before piping.");
                return EXEC_FAILURE;
            }
            else if (redirect_mode == PIPE)
            {
                /* Intermediary pipe. */
                if (command[0] == NULL)
                {
                    printf("Missing program to pipe to.\n");
                    return EXEC_FAILURE;
                }
                command[cmd_index] = NULL;
                inter_pipe(pipe_fds, command);
            }
            else
            {
                /* First pipe. */
                if (redirect_mode == INPUT)
                {
                    input_pipe_redirect(pipe_fds, redirect_cmd, command[0]);
                }
                else
                {
                    command[cmd_index] = NULL;
                    input_pipe(pipe_fds, command);
                }
            }
            cmd_index = 0;
            redirect_mode = PIPE;
        }
        else
        {
            /* No special characters found; save current arg. */
            command[cmd_index] = token;
            cmd_index++;
        }
        token = strtok(NULL, dlim);
    }

    /* Execute final command(s) after fully reading input string. */
    if (redirect_mode == REG_CMD)
    {
        exec_command(REG_CMD, command, 0);
    }
    else if (command[0] == NULL)
    {
        printf("No file for I/O redirection.\n");
        return EXEC_FAILURE;
    }
    else if (redirect_mode == INPUT)
    {
        redirect_io(INPUT, redirect_cmd, command[0], 1);
    }
    else if (redirect_mode == OUTPUT || redirect_mode == OUTPUT_APPEND)
    {
        if (input_file != NULL)
        {
            exec_redir_bothio(redirect_mode, redirect_cmd, input_file, command[0]);
        }
        else if (output_before_pipe)
        {
            output_pipe_redirect(pipe_fds, redirect_mode, redirect_cmd, command[0]);
        }
        else
        {
            redirect_io(redirect_mode, redirect_cmd, command[0], 1);
        }
    }
    else if (redirect_mode == PIPE)
    {
        output_pipe(pipe_fds, command);
    }

    return EXEC_SUCCESS;
}

/* Save the current command by copying cmd_index strings 
   from the array pointed to by command into the dest array. 
   The new destination will be NULL-terminated, and the
   first index of the original array will be set to NULL.  */

void save_command(char *dest[], char *command[], int cmd_index)
{
    command[cmd_index] = NULL;
    for (int i = 0; i <= cmd_index; i ++)
    {
        dest[i] = command[i];
    }
    command[0] = NULL;
}

/* Execute a command. The array representing the 
   command and its arguments must be a NULL-terminated 
   array of strings. I/O redirection is supported, and
   mode must be set to one of the 4 following
   values: REG, INPUT, OUTPUT, OUTPUT_APPEND.
   If mode is REG, the command is a regular command, 
   and io_file may be replaced with NULL.
   If mode is INPUT, OUTPUT or OUTPUT_APPEND, io_file_fd
   must be an open file descriptor. Piping is not supported. */

int exec_command(int mode, char *command[], int io_file_fd)
{
    pid_t child_pid;
    int redirect_fd;

    if ((child_pid = fork()) == -1)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* I/O Redirection (Single). */
        if (mode != REG_CMD)
        {
            if (mode == INPUT)
            {
                redirect_fd = STDIN_FILENO;
            }
            else if (mode == OUTPUT || mode == OUTPUT_APPEND)
            {
                redirect_fd = STDOUT_FILENO;
            }
            else
            {
                printf("Unsupported mode.\n");
                exit(EXIT_FAILURE);
            }
            if (dup2(io_file_fd, redirect_fd) < 0)
            {
                perror_exit("dup2()");
            }
            if (close(io_file_fd) < 0)
            {
                perror_exit("close()");
            }
        }

        /* Child process executes the program. */
        execvp_and_handle_error(command[0], command);
    }
    else
    {
        wait(NULL);
    }

    return EXEC_SUCCESS;
}

/* Redirect input or output for a command depending 
   on the given mode. If exec_redirection is 0, command
   may point to NULL. Else, the file for I/O redirection
   will be opened, the command run with redirection, and 
   the file will be closed. If exec_redirection is 0 and
   mode is any output mode, the corresponding io_file will
   be opened, created if it does not yet exist, and closed,  */

void redirect_io(int mode, char *command[], char *io_file, int exec_redirection)
{
    int flags = O_WRONLY | O_CREAT; 
    int io_file_fd;
    
    if (mode == INPUT)
    {
        if (!exec_redirection)
        {
            return;
        }
        flags = O_RDONLY;
    }
    else if (mode == OUTPUT)
    {
        flags = flags | O_TRUNC;
    }
    else if (mode == OUTPUT_APPEND)
    {
        flags = flags | O_APPEND;
    }

    /* Open, execute (if exec_redirection), and close.*/
    if ((io_file_fd = open(io_file, flags, 0666)) < 0)
    {
        perror("open()");
        return;
    }
    if (exec_redirection)
    {
        exec_command(mode, command, io_file_fd);
    }
    if (close(io_file_fd) < 0)
    {
        perror("close()");
    }
}

/* Redirect input, then output, assuming no pipes.
   Will accept commands in the form of:
   program < input_file > output_file 
   Input redirection may only occur once, but
   output redirection may be of either type and
   can take in as many output files as needed, with
   the only file being written to being the final one. */

void exec_redir_bothio(int output_mode, char *command[], char *input_file, char *output_file)
{
    pid_t child_pid;
    int input_fd, output_fd;
    int output_flags = O_WRONLY | O_CREAT;

    if ((child_pid = fork()) == -1)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* Get correct flags for output. */
        (output_mode == OUTPUT_APPEND) ? (output_flags = output_flags | O_APPEND) : (output_flags = output_flags | O_TRUNC); 

        /* Open files, redirect input and output, then execute. */
        if ((input_fd = open(input_file, O_RDONLY, 0666)) < 0)
        {
            perror_exit("open()");
        }
        if ((output_fd = open(output_file, output_flags, 0666)) < 0)
        {
            perror_exit("open()");
        }
        if (dup2(input_fd, STDIN_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        if (dup2(output_fd, STDOUT_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        if (close(input_fd) < 0)
        {
            perror_exit("close()");
        }
        if (close(output_fd) < 0)
        {
            perror_exit("close()");
        }
        execvp_and_handle_error(command[0], command);
    }
    else
    {
        wait(NULL);
    }
}

/* Execute the first piped command. */

void input_pipe(int *pipe_fds, char *pipe_cmd[])
{
    pid_t child_pid; 

    /* Create first pipe. */
    if (pipe(pipe_fds) < 0)
    {
        perror_exit("pipe()");
    }

    /* Fork. */
    if ((child_pid = fork()) < 0)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* Child process dups stdout, closes, and executes. */
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        close_pipes(pipe_fds);
        execvp_and_handle_error(pipe_cmd[0], pipe_cmd);
    }
    else
    {
        wait(NULL);
    }
}

/* Execute any intermediary piped commands. */

void inter_pipe(int *pipe_fds, char *pipe_cmd[])
{
    pid_t child_pid; 
    int input_pipe_fds[2];

    /* Save old pipe to input_pipe_fds. */
    input_pipe_fds[0] = pipe_fds[0];
    input_pipe_fds[1] = pipe_fds[1];

    /* Create new pipe. */
    if (pipe(pipe_fds) < 0)
    {
        perror_exit("pipe()");
    }

    /* Fork. */
    if ((child_pid = fork()) < 0)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* Child process dups stdin and stdout, closes, then executes. */
        if (dup2(input_pipe_fds[0], STDIN_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        close_pipes(input_pipe_fds);
        close_pipes(pipe_fds);
        execvp_and_handle_error(pipe_cmd[0], pipe_cmd);
    }
    else
    {
        /* Parent closes input pipe and waits once. */
        close_pipes(input_pipe_fds);
        wait(NULL);
    }
}

/* Execute the final piped command. */

void output_pipe(int *pipe_fds, char *pipe_cmd[])
{
    pid_t child_pid;

    /* Fork. */
    if ((child_pid = fork()) < 0)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* Child process dups stdin, closes, then executes to stdout. */
        if (dup2(pipe_fds[0], STDIN_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        close_pipes(pipe_fds);
        execvp_and_handle_error(pipe_cmd[0], pipe_cmd);
    }
    else
    {
        /* Parent closes final pipe and waits once. */
        close_pipes(pipe_fds);
        wait(NULL);
    }
}

/* Execute the first piped command with 
   input redirection. */

void input_pipe_redirect(int *pipe_fds, char *input_cmd[], char *input_file)
{
    pid_t child_pid; 
    int input_fd;

    /* Pipe fds. */
    if (pipe(pipe_fds) < 0)
    {
        perror_exit("pipe()");
    }

    /* Fork. */
    if ((child_pid = fork()) < 0)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* Child process dups stdout, closes, and executes. */
        if ((input_fd = open(input_file, O_RDONLY, 0666)) < 0)
        {
            perror_exit("open()");
        }
        if (dup2(input_fd, STDIN_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        close_pipes(pipe_fds);
        execvp_and_handle_error(input_cmd[0], input_cmd);
    }
    else
    {
        wait(NULL);
    }
}

/* Execute the final piped command
   with output redirection. */

void output_pipe_redirect(int *pipe_fds, int output_mode, char *output_cmd[], char *output_file)
{
    pid_t child_pid;
    int flags = O_CREAT | O_WRONLY;
    int output_fd; 

    /* Fork. */
    if ((child_pid = fork()) < 0)
    {
        perror("fork()");
    }
    else if (child_pid == 0)
    {
        /* Open file with corresponding flags. */
        (output_mode == OUTPUT_APPEND) ? (flags = flags | O_APPEND) : (flags = flags | O_TRUNC);
        if ((output_fd = open(output_file, flags, 0666)) < 0)
        {
            perror_exit("open()");
        }

        /* Dup, close pipes, execute, then close output fd. */
        if (dup2(pipe_fds[0], STDIN_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        if (dup2(output_fd, STDOUT_FILENO) < 0)
        {
            perror_exit("dup2()");
        }
        close_pipes(pipe_fds);
        execvp_and_handle_error(output_cmd[0], output_cmd);
        if (close(output_fd) < 0)
        {
            perror_exit("close()");
        }
    }
    else
    {
        /* Parent closes pipes and waits once. */
        close_pipes(pipe_fds);
        wait(NULL);
    }
}
