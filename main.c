#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <regex.h>
#include "command.h"

// flag for signal handler
int fgOnlyMode = 0;

// function prototypes
char* expandVariables(char*);
void ignoreSIGINT(void);
void handleSIGTSTP(int);
void setSIGTSTP(void);

int main(void) {
    // parent process ignores SIGINT
    ignoreSIGINT();
    // set the handler for SIGTSTP
    setSIGTSTP();
    // declare variables
    int childPid, status = 0;
    char *input = NULL;
    char *expanded, *finalInput;
    size_t len = 0;
    regex_t re;
    compile_regex(&re);
    command* currCommand = createCommand();
    bool exit = false;
    // run the shell loop
    while (!exit) {
        printf(":");
        fflush(stdout);
        // store the user input in the command struct
        int read = getline(&input, &len, stdin);
        input[read - 1] = '\0';
        expanded = expandVariables(input);
        finalInput = (expanded != NULL) ? expanded : input;
        parseCommand(finalInput, currCommand, &re);
        if (strcmp(currCommand->argList->argText, "exit") == 0) {
            // built-in exit
            exit = true;
        } else if (strcmp(currCommand->argList->argText, "cd") == 0) {
            // built-in cd
            char* dest = (currCommand->numArgs == 1)
                ? getenv("HOME") 
                : currCommand->argList->next->argText;
            chdir(dest);
        } else if (strcmp(currCommand->argList->argText, "status") == 0) {
            // built-in status
            printStatus(status);
        } else if (strncmp(currCommand->argList->argText, "#", 1) != 0) {
            // exec for other commands
            processCommand(currCommand, &status);
        } 
        // check for completed background processes
        while ((childPid = waitpid(-1, &status, WNOHANG)) > 0) {
            printf("background pid %d is done: ", childPid);
            printStatus(status);
        }
    }
    // clean up allocated memory
    destroyCommand(currCommand);
    free(input);
    regfree(&re);
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
        cursor += lenVar;
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

void ignoreSIGINT(void) {
    struct sigaction SIGINT_action = {{0}};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);
}

void handleSIGTSTP(int sig) {
    if (!fgOnlyMode) {
        write(2, "\nEntering foreground-only mode (& is now ignored)\n:", 51);
        fgOnlyMode = 1;
    } else {
        write(2, "\nExiting foreground-only mode\n:", 31);
        fgOnlyMode = 0;
    }
}

void setSIGTSTP(void) {
    struct sigaction SIGTSTP_action = {{0}};
    SIGTSTP_action.sa_handler = handleSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

