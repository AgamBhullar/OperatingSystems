#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_BACKGROUND_PROCESSES 50 
#define MAX_LINE 512   
#define MAX_ARGS 10    

pid_t background_pids[MAX_BACKGROUND_PROCESSES];
int background_pid_count = 0;


char* path = "/bin";
int global_last_command_status = 0; 

// Function Declarations
void execute_command(char *args[], int redirect, char *output_file, int background, int batch_mode);
void builtin_cd(char *args[]);
void builtin_path(char *args[]);
int is_builtin_command(char *cmd);
void parse_and_execute(char *line, int batch_mode);
void wait_for_background_processes();

int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    
    int batch_mode = (argc == 2) ? 1 : 0;  // 1 for batch mode, 0 for interactive

    if (argc == 1) {  
        while (1) {
            printf("wish> ");
            if ((nread = getline(&line, &len, stdin)) == -1)
                break;  
            parse_and_execute(line, batch_mode);
        }
    } else if (argc == 2) {  
        //printf("Running in batch mode\n"); // Debug print
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }
        line = NULL;
        len = 0;
        while ((nread = getline(&line, &len, file)) != -1) {
            //printf("Batch mode: 1, Command: %s", line);  // Debug print (including line break in 'line')
            parse_and_execute(line, batch_mode);
            free(line); 
            line = NULL; 
        }
        free(line);
        fclose(file);
        wait_for_background_processes();
        exit(global_last_command_status);
    } else {
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }

    free(line);
    wait_for_background_processes(); 
    return 0;
}

void trim(char *str) {
    char *end;

    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) {
        return;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;


    end[1] = '\0';
}

void parse_and_execute(char *line, int batch_mode) {
    //printf("Batch mode: %d, Command: %s\n", batch_mode, line); // Debug print
    char *args[MAX_ARGS];
    int argc = 0;
    int redirect = 0;
    char *output_file = NULL;
    int background = 0;

    trim(line);

    if (strcmp(line, "&") == 0) {
        return;
    }

    char *redirect_pos = strchr(line, '>');
    if (redirect_pos != NULL) {
        *redirect_pos = '\0';   
        redirect = 1;
        output_file = strtok(redirect_pos + 1, " \t\n");  
        if (output_file == NULL || strtok(NULL, " \t\n") != NULL) {
            write(STDERR_FILENO, "An error has occurred\n", strlen("An error has occurred\n"));
            return;
        }
    }

    char *token = strtok(line, " \t\n");
    while (token != NULL) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    // Check for background command
    if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
        background = 1;
        args[--argc] = NULL; 
    }

    args[argc] = NULL;

  
    if (argc == 1 && strcmp(args[0], "&") == 0) {
        return;
    }

  
    if (argc == 0) {
        if (redirect) {
            write(STDERR_FILENO, "An error has occurred\n", strlen("An error has occurred\n"));
        }
        return;  
    } 

    if (is_builtin_command(args[0])) {
        if (strcmp(args[0], "exit") == 0) {
            if (args[1] != NULL) {
                write(STDERR_FILENO, "An error has occurred\n", strlen("An error has occurred\n"));
            } else {
                wait_for_background_processes();
                exit(0);
            }
        } else if (strcmp(args[0], "cd") == 0) {
            builtin_cd(args);
        } else if (strcmp(args[0], "path") == 0) {
            builtin_path(args);
        }
    } else {
        execute_command(args, redirect, output_file, background, batch_mode);
    }
}

void execute_command(char *args[], int redirect, char *output_file, int background, int batch_mode) {
    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    } else if (rc == 0) {  
        if (redirect) {
            int fd = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                exit(1);
            }
            
            close(fd);  
        }

        char command[256];
        if (path != NULL) {
            sprintf(command, "%s/%s", path, args[0]); 
        } else {
            strcpy(command, args[0]);  
        }

        if (access(command, X_OK) == -1) {
            fprintf(stderr, "An error has occurred\n");
            global_last_command_status = 1;
            exit(1);  
        }

        execv(command, args);
        fprintf(stderr, "An error has occurred\n");
        global_last_command_status = 1;
        exit(1);  

    } else {  
        if (!background) {
            int status;
            waitpid(rc, &status, 0);  

            if (WIFEXITED(status)) {
                global_last_command_status = WEXITSTATUS(status);
            }
        } else {
            if (background_pid_count < MAX_BACKGROUND_PROCESSES) {
                background_pids[background_pid_count++] = rc; 
            } else {
                // Handle error
            }
            if (!batch_mode) {
                printf("Background process %d started.\n", rc);
            }
        }
    }
}


void builtin_cd(char *args[]) {
    if (args[1] == NULL || args[2] != NULL) {
        write(STDERR_FILENO, "An error has occurred\n", strlen("An error has occurred\n"));
        return;
    }
    if (chdir(args[1]) != 0) {
        write(STDERR_FILENO, "An error has occurred\n", strlen("An error has occurred\n"));
    }
}

void builtin_path(char *args[]) {
    if (args[1] == NULL) {
        //free(path);
        path = NULL;
    } else {
        //free(path);
        path = strdup(args[1]);
    }
}

int is_builtin_command(char *cmd) {
    return (strcmp(cmd, "exit") == 0 || strcmp(cmd, "cd") == 0 || strcmp(cmd, "path") == 0);
}

void wait_for_background_processes() {
    for (int i = 0; i < background_pid_count; i++) {
        int status;
        waitpid(background_pids[i], &status, 0);
    }
    background_pid_count = 0; 
}

