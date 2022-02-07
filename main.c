#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE 2048
#define MAX_ARGS 512

// store the components of a command input
typedef struct command {
    char cmd[MAX_LINE];
    char args[MAX_ARGS][MAX_LINE];
    int numArgs;
    char inFile[MAX_LINE];
    char outFile[MAX_LINE];
    bool foreground;
} command;

// function prototypes
char* expandVariables(char*);
void parseCommand(char*, command*);
void processCommand(command*);
void printCommand(command*);

int main(void) {
    char *input, *expanded, *finalInput;
    size_t len = 0;
    command* currCommand = malloc(sizeof(command));
    bool exit = false;
    while (!exit) {
        printf(":");
        fflush(stdout);
        // store the user input in the command struct
        int read = getline(&input, &len, stdin);
        input[read - 1] = '\0';
        expanded = expandVariables(input);
        finalInput = (expanded != NULL) ? expanded : input;
        parseCommand(finalInput, currCommand);
        if (strcmp(currCommand->cmd, "exit") == 0) {
            // built-in exit
            exit = true;
        } else if (strcmp(currCommand->cmd, "cd") == 0) {
            // built-in cd
            char* dest = (currCommand->numArgs == 0)
                ? getenv("HOME") 
                : currCommand->args[0];
            chdir(dest);
        } else if (strcmp(currCommand->cmd, "status") == 0) {
            // built-in status
            printf("status goes here\n");
        } else if (strncmp(currCommand->cmd, "#", 1) != 0) {
            // exec for other commands
            processCommand(currCommand);
        } 
    }
    // clean up allocated memory
    free(currCommand);
    free(input);
    if (expanded != NULL) {
        free(expanded);
    }
    return 0;
}

char* expandVariables(char* input) {
    const char* var = "$$";
    pid_t pid = getpid();
    int lenVar = 2;
    int lenPid = snprintf(NULL, 0, "%d", pid);
    // find number of expansions
    int count = 0;
    char* cursor = input;
    while ((cursor = strstr(cursor, var))) {
        count++;
        cursor += 2;
    }
    // if no expansions, return null
    if (!count) {
        return NULL;
    }
    // allocate space for expanded string
    char* expanded = malloc(strlen(input) + (lenPid - lenVar) * count + 1);
    char* pidString = malloc(lenPid + 1);
    sprintf(pidString, "%d", pid);
    // replace the variables
    char* tmp = expanded;
    int lenToCopy;
    while (count--) {
        cursor = strstr(input, var);
        lenToCopy = cursor - input;
        // copy and update tmp to next insert point
        tmp = strncpy(tmp, input, lenToCopy) + lenToCopy;
        tmp = strcpy(tmp, pidString) + lenPid;
        input += lenToCopy + lenVar;
    }
    free(pidString);
    // copy the last part of the input string
    strcpy(tmp, input);
    return expanded;
}

void parseCommand(char* input, command* newCommand) {
    // regex vars
    regex_t re;
    size_t nmatch = 9;
    regmatch_t pmatch[9];
    int rc, start, finish;
    const char* pattern = "([^[:space:]]+)([^&<>]+)?"
                          "(<[[:space:]]([^[:space:]]+))?"
                          "([[:space:]]?>[[:space:]]([^[:space:]]+))?"
                          "([[:space:]]?(&$))?";
    // match command
    rc = regcomp(&re, pattern, REG_EXTENDED);
    rc = regexec(&re, input, nmatch, pmatch, 0);
    if (rc != 0) {
        // no match on command, treat as empty comment
        strcpy(newCommand->cmd, "#");
        return;
    } else {
        start = pmatch[1].rm_so;
        finish = pmatch[1].rm_eo;
        strncpy(newCommand->cmd, input + start, finish - start);
        newCommand->cmd[finish - start] = '\0';
    }
    // check args
    if (pmatch[2].rm_so == -1) {
        newCommand->numArgs = 0;
    } else {
        start = pmatch[2].rm_so;
        finish = pmatch[2].rm_eo;
        char* args = calloc(finish - start + 1, sizeof(char));
        strncpy(args, input + start, finish - start);
        args[finish - start] = '\0';
        // parse args with strtok_r
        char* saveptr;
        char* token = strtok_r(args, " ", &saveptr);
        int i = 0;
        while (token != NULL) {
            strcpy(newCommand->args[i], token);
            token = strtok_r(NULL, " ", &saveptr);
            i++;
        }
        // store the number of args for the command
        newCommand->numArgs = i;
        free(args);
    }
    // check infile
    if (pmatch[4].rm_so == -1) {
        strcpy(newCommand->inFile, "");
    } else {
        start = pmatch[4].rm_so;
        finish = pmatch[4].rm_eo;
        strncpy(newCommand->inFile, input + start, finish - start);
        newCommand->inFile[finish - start] = '\0';
    }
    // check outfile
    if (pmatch[6].rm_so == -1) {
        strcpy(newCommand->outFile, "");
    } else {
        start = pmatch[6].rm_so;
        finish = pmatch[6].rm_eo;
        strncpy(newCommand->outFile, input + start, finish - start);
        newCommand->outFile[finish - start] = '\0';
    }
    // check &
    if (pmatch[8].rm_so == -1) {
        newCommand->foreground = true;
    } else {
        newCommand->foreground = false;
    }
    regfree(&re);
}

void processCommand(command* currCommand) {
    // build the arg vector
    char* newargv[currCommand->numArgs + 2];
    newargv[0] = currCommand->cmd;
    for (int i = 0; i < currCommand->numArgs; i++) {
        newargv[i + 1] = currCommand->args[i];
    }
    newargv[currCommand->numArgs + 1] = NULL;
    // create the fork process
    int childStatus;
    pid_t childPid = fork();
    switch (childPid) {
        case -1:
            perror("fork() failed!");
            exit(1);
            break;
        case 0: ;
            // child process
            execvp(newargv[0], newargv);
            perror("shell could not find command to run");
            exit(1);
            break;
        default:
            // parent process
            childPid = waitpid(childPid, &childStatus, 0);
            if (WIFEXITED(childStatus)) {
                printf("Exited normally with status %d\n", WEXITSTATUS(childStatus));
            } else {
                printf("Exited abnormally due to signal %d\n", WTERMSIG(childStatus));
            }
    }
}

void printCommand(command* currCommand) {
    // print out the parsed command
    // useful for debugging
    printf("Cmd: %s\n", currCommand->cmd);
    for (int i = 0; i < currCommand->numArgs; i++) {
        printf("Arg %d: %s\n", i, currCommand->args[i]);
    }
    printf("Input file: %s\n", currCommand->inFile);
    printf("Output file: %s\n", currCommand->outFile);
    if (currCommand->foreground) {
        printf("Process in foreground\n");
    } else {
        printf("Process in background\n");
    }
}

