// store the args in a linked list
typedef struct arg {
    char* argText;
    struct arg* next;
} arg;

// store the components of a command input
typedef struct command {
    arg* argList;
    int numArgs;
    char* inFile;
    char* outFile;
    bool foreground;
} command;

command* createCommand(void);
void clearCommand(command*);
void destroyCommand(command*);
void compile_regex(regex_t*);
void parseCommand(char*, command*, regex_t*);
void processCommand(command*, int*);
void printStatus(int);
void fixRedirects(command*);
void printCommand(command*);

