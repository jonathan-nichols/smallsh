#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>

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

int main(void) {
    runShell();
    return 0;
}

void runShell(void) {
    char* input;
    size_t len = 0;
    command* currCommand = malloc(sizeof(command));
    bool exit = false;
    // read input from user
    while (!exit) {
        printf(":");
        // store the user input in the command struct
        int read = getline(&input, &len, stdin);
        input[read - 1] = '\0';
        parseCommand(input, currCommand);
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
}

void parseCommand(char* input, command* newCommand) {
    // regex vars
    regex_t re;
    size_t nmatch = 2;
    regmatch_t pmatch[2];
    int rc, start, finish;
    // match command
    rc = regcomp(&re, "[^[:space:]]+", REG_EXTENDED);
    rc = regexec(&re, input, nmatch, pmatch, 0);
    if (rc != 0) {
        // no match on command, treat as empty comment
        strcpy(newCommand->cmd, "#");
    } else {
        start = pmatch[0].rm_so;
        finish = pmatch[0].rm_eo;
        strncpy(newCommand->cmd, input + start, finish - start);
        newCommand->cmd[finish - start] = '\0';
    }
    // match args
    rc = regcomp(&re, "[^&<>][[:space:]]([^&<>]+)", REG_EXTENDED);
    rc = regexec(&re, input, nmatch, pmatch, 0);
    if (rc != 0) {
        newCommand->numArgs = 0;
    } else {
        start = pmatch[1].rm_so;
        finish = pmatch[1].rm_eo;
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
    // match infile
    rc = regcomp(&re, "<[[:space:]]([^[:space:]]+)", REG_EXTENDED);
    rc = regexec(&re, input, nmatch, pmatch, 0);
    if (rc != 0) {
        strcpy(newCommand->inFile, "");
    } else {
        start = pmatch[1].rm_so;
        finish = pmatch[1].rm_eo;
        strncpy(newCommand->inFile, input + start, finish - start);
        newCommand->inFile[finish - start] = '\0';
    }
    // match outfile
    rc = regcomp(&re, ">[[:space:]]([^[:space:]]+)", REG_EXTENDED);
    rc = regexec(&re, input, nmatch, pmatch, 0);
    if (rc != 0) {
        strcpy(newCommand->outFile, "");
    } else {
        start = pmatch[1].rm_so;
        finish = pmatch[1].rm_eo;
        strncpy(newCommand->outFile, input + start, finish - start);
        newCommand->outFile[finish - start] = '\0';
    }
    // match &
    rc = regcomp(&re, "&$", REG_EXTENDED);
    rc = regexec(&re, input, nmatch, pmatch, 0);
    if (rc != 0) {
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
        printf("Process in foreground\n\n");
    } else {
        printf("Process in background\n\n");
    }
}

