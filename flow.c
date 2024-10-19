
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_NODES 100
#define MAX_LINE_LENGTH 1000

// Structure to represent a node
struct Node {
    char name[50];
    char command[100];
};

// Structure to represent a pipe
struct Pipe {
    char from[50];
    char to[50];
};

// Global variables to store nodes and pipes
struct Node nodes[MAX_NODES];
struct Pipe pipes[MAX_NODES];
int node_count = 0;
int pipe_count = 0;

// Function to parse the flow file
void parse_flow_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        char key[50], value[100];
        sscanf(line, "%[^=]=%s", key, value);

        if (strcmp(key, "node") == 0) {
            strcpy(nodes[node_count].name, value);
        } else if (strcmp(key, "command") == 0) {
            strcpy(nodes[node_count].command, value);
            node_count++;
        } else if (strcmp(key, "pipe") == 0) {

        } else if (strcmp(key, "from") == 0) {
            strcpy(pipes[pipe_count].from, value);
        } else if (strcmp(key, "to") == 0) {
            strcpy(pipes[pipe_count].to, value);
            pipe_count++;
        }
    }

    fclose(file);
}

// Function to find a node by name
int find_node(const char* name) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to print error message and exit
void error_exit(const char* message) {
    perror(message);
    exit(1);
}

int execute_flow(const char* action) {
    int pipe_fds[MAX_NODES][2];
    pid_t pids[MAX_NODES];

    // Create pipes
    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            error_exit("Failed to create pipe");
        }
    }

    // Execute nodes
    for (int i = 0; i < node_count; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            error_exit("Fork failed");
        } else if (pids[i] == 0) {  // Child process
            // Set up input redirection
            for (int j = 0; j < pipe_count; j++) {
                if (strcmp(pipes[j].to, nodes[i].name) == 0) {
                    if (dup2(pipe_fds[j][0], STDIN_FILENO) == -1) {
                        error_exit("Failed to redirect input");
                    }
                    close(pipe_fds[j][0]);
                    close(pipe_fds[j][1]);
                }
            }

            // Set up output redirection
            for (int j = 0; j < pipe_count; j++) {
                if (strcmp(pipes[j].from, nodes[i].name) == 0) {
                    if (dup2(pipe_fds[j][1], STDOUT_FILENO) == -1) {
                        error_exit("Failed to redirect output");
                    }
                    close(pipe_fds[j][0]);
                    close(pipe_fds[j][1]);
                }
            }

            // Close all unused pipe ends
            for (int j = 0; j < pipe_count; j++) {
                if (strcmp(pipes[j].from, nodes[i].name) != 0 && strcmp(pipes[j].to, nodes[i].name) != 0) {
                    close(pipe_fds[j][0]);
                    close(pipe_fds[j][1]);
                }
            }

            // Execute the command
            char* args[] = {"/bin/sh", "-c", nodes[i].command, NULL};
            execv("/bin/sh", args);
            error_exit("Exec failed");
        }
    }

    // Close all pipe ends in the parent
    for (int i = 0; i < pipe_count; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < node_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child process exited with non-zero status\n");
            return 1;
        }
    }

    return 0;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <flow_file> <action>\n", argv[0]);
        return 1;
    }

    parse_flow_file(argv[1]);
    int result = execute_flow(argv[2]);
    
    if (result != 0) {
        fprintf(stderr, "Error executing flow\n");
        return 1;
    }

    return 0;
}
