#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH 4096
#define MAX_LINE 2048
#define MAX_ARGS 512

// store the components of a command input
typedef struct command {
    char cmd[MAX_LINE];
    char args[MAX_ARGS][MAX_LINE];
    int numArgs;
    char inFile[MAX_PATH];
    char outFile[MAX_PATH];
    bool foreground;
} command;

// function prototypes
void runShell(void);
void parseCommand(char*, command*);
void printCommand(command*);
char* expandVariables(char*);

int main(void) {
    runShell();
    return 0;
}

void runShell(void) {
    char *input, *expanded, *finalInput;
    size_t len = 0;
    command* currCommand = malloc(sizeof(command));
    bool exit = false;
    while (!exit) {
        printf(":");
        // store the user input in the command struct
        int read = getline(&input, &len, stdin);
        input[read - 1] = '\0';
        // perform variable expansion
        expanded = expandVariables(input);
        finalInput = expanded != NULL ? expanded : input;
        // parse the command into a struct
        parseCommand(finalInput, currCommand);
        // process the given command
        if (strcmp(currCommand->cmd, "exit") == 0) {
            exit = true;
        } else if (strcmp(currCommand->cmd, "#") != 0) {
            printCommand(currCommand);
        }
    }
    // clean up allocated memory
    free(currCommand);
    free(input);
    if (expanded != NULL) {
        free(expanded);
    }
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

