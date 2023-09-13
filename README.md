/*  C Shell Implementation
*   V 0.2
*
*   C implementation of the shell/terminal.
*
*   Author: John Manalac
*   Collaborators: Johana Ramirez
*   Last changed: Dec 11, 2022.
*
*   Implementation Notes: 
*   
*   This shell's input limit depends on the MAX_SHELL_INPUT and 
*   MAX_SHELL_ARGS constants, with the max input including 
*   the trailing new line and NULL terminating bytes. 
*
*   This shell supports input redirection operations to be perfomed 
*   once per command, but it must be the first command if piping or 
*   redirecting output. 
*
*   The shell also supports an arbitrary number of pipes in 
*   a single command, as well as pipes intermixed with 
*   I/O redirection. However, piping can only be performed 
*   after input redirection, and must be completed before 
*   any output redirections. An arbritary number of pipes
*   is supported.
*
*   When performing output redirection, all files specified
*   will be created with permission 0666 if they do not
*   yet exist. Input files must be preexisting.
*
*   There are three static inline functions that will always 
*   exit upon failure. All other error checking is done within 
*   functions, where either perror or perror_exit is called,
*   depending on whether to return to caller or exit the 
*   the current process. Executing a program must be done in a child
*   process, so execvp_and_handle_error will print an error message
*   to stderr and exit upon failure. Although close_pipes are called
*   in both child and the parent (the shell itself) processes, this 
*   implementation will automatically terminate the shell if piping fails, 
*   as stdin or stdout might have been replaced by an unclosed pipe. 
*
*   Supported I/O Redirection and Piping: 
*
*       Input redirection: 
*
*           program < input-file 
*
*       Output redirection (file truncated): 
*
*           program > output-file 
*
*       Output redirection (output appended to file): 
*            
*           program >> output-file 
*
*       Input, then output redirection (redirection must be performed in this exact order):
*       
*           program < input-file > output-file
*
*       Multiple output redirection (all files will be created and truncated, but only the last will be written to.): 
*       
*           program > output-file1 > output-file2 > output-file3 ...
*
*       The above, with output being appended (can intermix output symbols, but only the last output type will be counted). 
*       Files not redirected with append mode will be truncated. 
*
*           program >> output-file1 >> output-file2 >> output-file3 ...
*
*       Single pipe: 
*
*           program1 | program2
*
*       Multiple pipes (in arbritrary order)): 
*
*           program1 | program2 | program3 ...
*
*       Input redirection before pipe: 
*           
*           program1 < input_file | program2 
*
*       Output redirection after pipe: 
*           
*           program1 | program2 > output_file.txt
*
*       Both redirection with pipe: 
*           
*           program 1 < input_file | program2 > output_file.txt
*
*/
