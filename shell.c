/*
Author: Chan Tsz Ho
Development platform: developed on MacOs
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 1025 // Support max 1024 characters long command
#define MAX_NUM_CMD 6 // Support max 5 commands
#define MAX_NUM_ARG 30 // Support max 30 strings

int receivedUSR1 = 0; // 1 if received SIGUSR1 message from parent

// Function: Print standard shell message
void prompt() {
    printf("$$ shellSim ## ");
}

// Function: To handle SIGINT signal
void sigintParent(int sig) {
    return;
}

// Function: To handle SIGUSR1 signal
void sigusr1(int sig) {
    receivedUSR1 = 1;
}

// Function: Discard first argument, shift remaning arguments forward by 1 position
void shiftListForward(int argNum, char* argList[]) {
    for (int i = 0; i < argNum - 1; i++) {
        argList[i] = argList[i+1];
    }
    argList[argNum - 1] = NULL;
}

/*
Function: Count the number of arguments in a command
or number of commands in an input
*/
int argCount(char* command, char* separator) {
    // Prevent modifying the command
    char commandCpy[MAX_CMD_LEN];
    strcpy(commandCpy, command);

    char* piece = strtok(commandCpy, separator);
    int argNum = (piece == NULL) ? 0 : 1;

    while (piece != NULL) {
        piece = strtok(NULL, separator);
        argNum = (piece == NULL) ? argNum : argNum + 1;
    }
    return argNum;
}

/* 
Function: Check whether the command uses "exit" correctly
Return:
 1: Exit used correctly
 0: Exit not used (Normal)
-1: Exit used incorrectly
*/
int checkExit(char* argList[]) {
    // Check only has 1 argument "exit"
    if (!strcmp(argList[0], "exit") && argList[1] == NULL) {
        printf("shellSim: Terminated\n");
        return 1;
    // Prompt when exit has other arguments
    } else if (!strcmp(argList[0], "exit") && argList[1] != NULL) {
        printf("shellSim: \"exit\" with other arguments!!!\n");
        return -1;
    }
    return 0;  
}

/* 
Function: Check whether the command uses "timeX" correctly and modify argList
Return:
 1: timeX used correctly
 0: timeX not used (Normal)
-1: timeX used incorrectly
*/
int checkTimeX(int argNum, char* argList[]) {
    // Check more 1 argument with "timeX"
    if (!strcmp(argList[0], "timeX") && argList[1] != (char*) NULL) {
        shiftListForward(argNum, argList); // discard first element, move the others forward
        return 1;
    // Prompt when timeX has no other arguments
    } else if (!strcmp(argList[0], "timeX") && argList[1] == NULL) {
        printf("shellSim: \"timeX\" cannot be a standalone command\n");
        return -1;
    }
    return 0;  
}

/* 
Function: Check whether the command uses "|" correctly
Return:
 1: Used correctly
-1: Used incorrectly
*/
int checkPipe(char* input) {
    // check first or last character as "|"
    if (input[0] == '|' || input[strlen(input) - 1] == '|') {
        printf("shellSim: should not have | as first or last character\n");
        return -1;
    }

    // check two consecutive "|"
    for (int i = 0; i < strlen(input) - 1; i++) {
        if (input[i] == '|' && input[i + 1] == '|') {
            printf("shellSim: should not have two consecutive | without in-between command\n");
            return -1;
        }
    }
    return 1;
}

// Return 1 if all character of a string is empty, 0 otherwise
int isEmpty(char* input) {
    for (int i = 0; i < strlen(input); i++) {
        if (input[i] != ' ') {
            return 0;
        }
    }
    return 1;
}

/*
Function: Break a command by separator into pieces which are stored in argList.
The length of argList is given by max_idx
*/
void fillArgList(char* argList[], char* command, char* separator, int max_idx) {
    int argIdx = 0;
    char* piece = strtok(command, separator);
    argList[argIdx] = piece;
    argIdx++;
    while (piece != NULL) {
        piece = strtok(NULL, separator);
        argList[argIdx] = piece;
        argIdx++;
    }

    // Reset unused argList
    for (int i = argIdx; i < max_idx; i++) {
        argList[i] = NULL;
    }
}

// Function: Execute a command given by the argList
void execute(char* argList[]) {
    // Execute program
    // Program name is given by first element of argList
    execvp((char*) argList[0], argList);

    // Error handling
    printf("shellSim: '%s': %s\n", argList[0], strerror(errno));
}

// Function: Extract program name from path. Similar to fillArgList
void pathToName(char* path) {
    char name[MAX_CMD_LEN];
    char* piece = strtok(path, "/");

    if (piece != NULL)
        strcpy(name, piece);

    while (piece != NULL) {
        piece = strtok(NULL, "/");
        if (piece != NULL)
            strcpy(name, piece);
    }
    strcpy(path, name);
}

// Function: Close all pipes constructed
void closePipe(int fd[4][2]) {
    for (int i = 0; i < 4; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
} 

int main(int argc, char *argv[])
{
    // Define signal handler for interrupt signal in parent process
    signal(SIGINT, sigintParent);
    // Handle SIGUSR1 signal
    signal(SIGUSR1, sigusr1);

    int fd[4][2] = {0}; // Define pipe

    while (1) {
        // Create pipes
        for (int i = 0; i < 4; i++) {
            pipe(fd[i]);
        }

        // Give Terminal prompt
        prompt();

        // Get user input and format input
        char input[MAX_CMD_LEN];
        fgets(input, MAX_CMD_LEN, stdin);
        input[strlen(input) - 1] = '\0'; // Remove the ending '\n'

        // If no input or first or last character = '|', ignore command
        if (!strcmp(input, "")) {
            printf("shellSim: should not empty command\n");
            continue;
        }
        // Skip if "|" is used incorrectly
        if (checkPipe(input) == -1) continue;

        // Turn input into commandList
        int cmdNum = argCount(input, "|");
        char* cmdList[MAX_NUM_CMD] = {0};
        if (cmdNum != 0) // Prevent from extracting empty command
            fillArgList(cmdList, input, "|", MAX_NUM_CMD);

        // Check for consecutive | with space in between (result in empty command)
        int emptyCmd = 0;
        for (int i = 0; i < cmdNum; i++) {
            if (isEmpty(cmdList[i]) == 1) {
                emptyCmd = 1;
                printf("shellSim: should not have two consecutive | without in-between command\n");
                break;
            }
        }
        if (emptyCmd)
            continue;
            
        // Turn command into argList (for each command)
        // ArgList of first child is constructed first to check usage of "exit" and "timeX"
        int argNum = argCount(cmdList[0], " ");
        char* argList[MAX_NUM_ARG] = {0};
        if (argNum != 0) // Prevent from extracting empty command
            fillArgList(argList, cmdList[0], " ", MAX_NUM_ARG);

        // Check exit and timeX
        int exitVal = checkExit(argList);
        int timeXVal = checkTimeX(argNum, argList);

        if (exitVal == 1) {
            return 0; // exit shell
        } else if (exitVal == -1 || timeXVal == -1 || argNum == 0) { // Invalid input
            continue;
        }
        
        // Create process(es)
        int pid, pidIdx; // pidIdx denotes the index of process. First has value 0 and so on
        int pidList[5] = {0}; // Store pid of child in parent process
        for (int i = 0; i < cmdNum; i++) {
            pid = fork();
            if (pid == 0) {
                pidIdx = i;
                break; // child process leave
            }
            pidList[i] = pid;
        }

        // Child process
        if (pid == 0) {
            // Connect input of second or behind processes to output of previous process
            if (pidIdx != 0) {
                dup2(fd[pidIdx - 1][0], STDIN_FILENO);
            }
            // Connect output of first to second-last processes to input of next process
            if (pidIdx != cmdNum - 1) {
                dup2(fd[pidIdx][1], STDOUT_FILENO);
            }
                
            // Close unused pipes
            closePipe(fd);

            // Wait for SIGUSR1 from parent before running
            while (!receivedUSR1);

            // Construct argument List
            // First child already partially processed -> no need to fill argList again
            if (pidIdx == 0) {
                execute(argList);
                return 1; // Return error if cannot execute
            }

            // Other child processes
            argNum = argCount(cmdList[pidIdx], " ");
            // Prevent from extracting empty command
            if (argNum != 0) {
                fillArgList(argList, cmdList[pidIdx], " ", MAX_NUM_ARG);
                execute(argList);
            }
            return 1; // Return failed child process
        }

        // Parent process
        int status;
        struct rusage rusage;
        int excIdx = 0; // Index of process is it executing

        // Close unused pipe
        closePipe(fd);

        // Send SIGUSR1 to child
        kill(pidList[0], SIGUSR1);

        // Create buffers to store process status
        int bufferPid[5];
        float bufferUsr[5];
        float bufferSys[5];
        char* bufferCmd[5];

        // Wait for child process to complete then start next child
        while(wait4(pidList[excIdx], &status, 0, &rusage) > 0) {
            excIdx++; // Index of next process to execute
            pid = pidList[excIdx]; // pid of next process to execute
            if (pid != 0) { // If still have process to execute
                kill(pid, SIGUSR1);
            }

            // If child process is terminated by signal, prints signal message
            if (WIFSIGNALED(status)) {
                printf("%s\n", strsignal(WTERMSIG(status)));
            }

            // If timeX command is used and child process is terminated normally
            if (timeXVal && WIFEXITED(status)) {
                pid = pidList[excIdx - 1]; // curr pid
                
                if (excIdx - 1 != 0) {
                    fillArgList(argList, cmdList[excIdx - 1], " ", MAX_NUM_ARG);
                }

                pathToName(argList[0]);
                bufferPid[excIdx - 1] = pid;
                bufferUsr[excIdx - 1] = rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec / 1000000.0;
                bufferSys[excIdx - 1] = rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec / 1000000.0;
                bufferCmd[excIdx - 1] = argList[0];
            }
        }
        // Print process status stored in buffer
        if (timeXVal && WIFEXITED(status)) {
            for (int i = 0; i < cmdNum; i++) {
                printf("(PID)%d  (CMD)%s    (user)%.3f s  (sys)%.3f s\n", bufferPid[i], bufferCmd[i], bufferUsr[i], bufferSys[i]);
            }
        }
    }
    return 0;
}