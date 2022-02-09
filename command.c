#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <regex.h>
#include "command.h"

extern int fgOnlyMode;

// constructor
command* createCommand(void) {
    // allocate space and set values to null
    command* currCommand = malloc(sizeof(command));
    currCommand->argList = NULL;
    currCommand->numArgs = 0;
    currCommand->inFile = NULL;
    currCommand->outFile = NULL;
    currCommand->foreground = false;
    return currCommand;
}

// clears out allocated space
void clearCommand(command* currCommand) {
    // free the individual args
    arg* tmp;
    while (currCommand->argList != NULL) {
        tmp = currCommand->argList;
        currCommand->argList = currCommand->argList->next;
        free(tmp->argText);
        free(tmp);
    }
    currCommand->argList = NULL;
    // free input/output files if used
    if (currCommand->inFile != NULL) {
        free(currCommand->inFile);
        currCommand->inFile = NULL;
    }
    if (currCommand->outFile != NULL) {
        free(currCommand->outFile);
        currCommand->outFile = NULL;
    }
}

// destructor
void destroyCommand(command* currCommand) {
    // clear out the data
    clearCommand(currCommand);
    // free the whole command
    free(currCommand);
}

void compile_regex(regex_t* re) {
    int rc;
    const char* pattern = "([^&<>]+)"
                          "(<[[:space:]]([^[:space:]]+))?"
                          "([[:space:]]?>[[:space:]]([^[:space:]]+))?"
                          "([[:space:]]?(&$))?";
    rc = regcomp(re, pattern, REG_EXTENDED);
    if (rc != 0) {
        printf("regex error\n");
        exit(1);
    }
}

void parseCommand(char* input, command* newCommand, regex_t* re) {
    // regex vars
    size_t nmatch = 8;
    regmatch_t pmatch[8];
    int rc, start, finish;
    arg* newArg;
    // clear out previous command
    clearCommand(newCommand);
    // match input text
    rc = regexec(re, input, nmatch, pmatch, 0);
    if (rc != 0) {
        // no match on command, treat as empty comment
        newArg = malloc(sizeof(arg));
        newArg->argText = calloc(2, sizeof(char));
        strcpy(newArg->argText, "#");
        newArg->next = NULL;
        newCommand->argList = newArg;
        newCommand->numArgs = 1;
        return;
    } else {
        // get full args text
        start = pmatch[1].rm_so;
        finish = pmatch[1].rm_eo;
        char* args = calloc(finish - start + 1, sizeof(char));
        strncpy(args, input + start, finish - start);
        args[finish - start] = '\0';
        // parse args with strtok_r
        char* saveptr;
        char* token = strtok_r(args, " ", &saveptr);
        int i = 0;
        arg *head = NULL, *prev;
        while (token != NULL) {
            // copy data to new arg
            newArg = malloc(sizeof(arg));
            newArg->argText = calloc(strlen(token) + 1, sizeof(char));
            strcpy(newArg->argText, token);
            newArg->next = NULL;
            // add new arg to the list
            if (head == NULL) {
                head = newArg;
            } else {
                prev->next = newArg;
            }
            prev = newArg;
            // move to next token
            token = strtok_r(NULL, " ", &saveptr);
            i++;
        }
        // store the number of args for the command
        newCommand->argList = head;
        newCommand->numArgs = i;
        free(args);
        // check infile
        if (pmatch[3].rm_so != -1) {
            start = pmatch[3].rm_so;
            finish = pmatch[3].rm_eo;
            newCommand->inFile = calloc(finish - start + 1, sizeof(char));
            strncpy(newCommand->inFile, input + start, finish - start);
            newCommand->inFile[finish - start] = '\0';
        }
        // check outfile
        if (pmatch[5].rm_so != -1) {
            start = pmatch[5].rm_so;
            finish = pmatch[5].rm_eo;
            newCommand->outFile = calloc(finish - start + 1, sizeof(char));
            strncpy(newCommand->outFile, input + start, finish - start);
            newCommand->outFile[finish - start] = '\0';
        }
        // check &
        newCommand->foreground = (pmatch[7].rm_so == -1);
    }
}

void processCommand(command* currCommand, int* childStatus) {
    // build the arg vector
    char* newargv[currCommand->numArgs + 1];
    arg* head = currCommand->argList;
    int i = 0;
    while (head != NULL) {
        newargv[i] = head->argText;
        head = head->next;
        i++;
    }
    newargv[i] = NULL;
    // check for background redirects
    fixRedirects(currCommand);
    // create the fork process
    int source, target, result;
    pid_t childPid = fork();
    switch (childPid) {
        case -1:
            perror("fork() failed!");
            exit(1);
            break;
        case 0:
            // child process
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_IGN);
            // check for redirection
            if (currCommand->inFile != NULL) {
                // open file for input redirection
                source = open(currCommand->inFile, O_RDONLY);
                if (source == -1) {
                    printf("cannot open %s for input\n", currCommand->inFile);
                    fflush(stdout);
                    exit(1);
                }
                result = dup2(source, 0);
                if (result == -1) {
                    perror("source file dup2()");
                    exit(1);
                }
                fcntl(source, F_SETFD, FD_CLOEXEC);
            }
            if (currCommand->outFile != NULL) {
                // open file for output redirection
                target = open(currCommand->outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (target == -1) {
                    printf("cannot open %s for output\n", currCommand->outFile);
                    fflush(stdout);
                    exit(1);
                }
                result = dup2(target, 1);
                if (result == -1) {
                    perror("target file dup2()");
                    exit(1);
                }
                fcntl(target, F_SETFD, FD_CLOEXEC);
            }
            // execute the command
            execvp(newargv[0], newargv);
            printf("%s: no such file or directory\n", newargv[0]);
            fflush(stdout);
            exit(1);
            break;
        default:
            if (!currCommand->foreground && !fgOnlyMode) {
                // process in background
                printf("background id is %d\n", childPid);
                childPid = waitpid(childPid, childStatus, WNOHANG);
                fflush(stdout);
            } else {
                // process in foreground
                childPid = waitpid(childPid, childStatus, 0);
                // output status message on sigint
                if (WIFSIGNALED(*childStatus)) {
                    printStatus(*childStatus);
                }
            }
    }
}

void printStatus(int status) {
    if (WIFEXITED(status)) {
        printf("exit value %d\n", WEXITSTATUS(status));
    } else {
        printf("terminated by signal %d\n", WTERMSIG(status));
    }
    fflush(stdout);
}

void fixRedirects(command* currCommand) {
    // only used for background processes
    if (!currCommand->foreground) {
        if (currCommand->inFile == NULL) {
            currCommand->inFile = calloc(10, sizeof(char));
            strcpy(currCommand->inFile, "/dev/null");
        
        }
        if (currCommand->outFile == NULL) {
            currCommand->outFile = calloc(10, sizeof(char));
            strcpy(currCommand->outFile, "/dev/null");
        }
    }
}

void printCommand(command* currCommand) {
    int i = 0;
    arg* head = currCommand->argList;
    while (head != NULL) {
        printf("Arg%d: %s\n", i, head->argText);
        head = head->next;
        i++;
    }
    if (currCommand->inFile != NULL) {
        printf("Input file: %s\n", currCommand->inFile);
    }
    if (currCommand->outFile != NULL) {
        printf("Output file: %s\n", currCommand->outFile);
    }
    if (currCommand->foreground) {
        printf("Process in foreground\n");
    } else {
        printf("Process in background\n");
    }
}

