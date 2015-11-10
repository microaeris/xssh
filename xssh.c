#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xssh.h"

// 16 + 1 for null character
#define MAX_ARGS 17
#define MAX_VAR_SIZE 256
#define MAX_LINE_SIZE 256


int foregroundPID = -1;     // PID of foreground child process
int displayCommand = 0;     // Command line arg set on start of xssh
int numLocalVars = 8;       // Total local variables the current array can hold
int localVarIndex = 0;      // Count of the local varables, used to index array


// Array of local variables
struct variableHashStruct **localVars;


char *argBuffer[MAX_ARGS + 1];      // Array of command that was read in, split by word
char *line = NULL;                  // Command string read in
int argCount = 1;              	    // Number of args found in command



/*
 * Counts the number of digits in a integer
 */
int lengthOfInt(int num) {
    return (num == 0 ? 1 : (int)(log10(num)+1));
}



/*
 * Loops through the argument buffer and deletes
 * all of the variables. Should be called each time
 * a command is processed
 */
void freeArgBuffer() {
    int k;

    for(k = 0; k < argCount; ++k) {
        free(argBuffer[k]);
    }
}


/*
 * Called right before exiting the program.
 * It frees each local var struct and then it frees
 * the array holding the local vars.
 */
void freeLocalVar() {
    int j;

    // Free all local variables
    for(j = 0; j < localVarIndex; ++j) {
        free(localVars[j]);
    }

    free(localVars);
}



/*
 * Find the struct in the localVars array that
 * matches the id that is passed in
 */
struct variableHashStruct * findLocalVar(char * id) {
    int i;

    // Find local var
    for(i = 0; i < localVarIndex; ++i) {
        if(strcmp(id, localVars[i]->id) == 0) {
            return localVars[i];
        }
    }

    return NULL;
}



/*
 * Mallocs space to hold the struct for the
 * new local variable. The localVar array is then
 * given a pointer to this new variable to hold.
 */
void setLocalVar(char* id, char* value) {
    int i;
    struct variableHashStruct *var;

    // If the item is already in the array
    var = findLocalVar(id);
    if(var != NULL) {
        // edit the existing struct
        strncpy(var->value, value, MAX_VAR_SIZE);
        return;
    } else {
        // Malloc space for the new var
        var = (struct variableHashStruct *)
                malloc(sizeof(struct variableHashStruct));

        strncpy(var->id, id, MAX_VAR_SIZE);
        strncpy(var->value, value, MAX_VAR_SIZE);
    }

    // If we're out of space in the local var array
    if(localVarIndex >= numLocalVars) {
        // Resize (double) the local var array
        struct variableHashStruct **tmp = localVars;

        numLocalVars *= 2;
        localVars = (struct variableHashStruct **) malloc(sizeof(struct
                variableHashStruct *) * numLocalVars);

        // Copy over the elements
        for(i = 0; i < localVarIndex; ++i) {
            localVars[i] = tmp[i];
        }

        free(tmp);
    }

    localVars[localVarIndex] = var;
    ++localVarIndex;
}



/*
 * Calls external commands with fork and exec.
 * Also handles background processes and I/O redirection
 */
int forkCommand(char* program, char** args, int argCount) {
    pid_t childPID;
    int i, status;
    int parentWait = 1;
    char fileIn[MAX_VAR_SIZE];
    char fileOut[MAX_VAR_SIZE];
    fileIn[0] = 0;
    fileOut[0] = 0;

    // Check to see if parent should wait
    for(i = 0; i < argCount; ++i) {
        if(args[i][0] == '&') {
            parentWait = 0;
            free(args[i]);
            args[i] = NULL;
        }
    }

    // Check for I/O redirection: < and >
    for(i = 0; i < argCount; ++i) {
        // Found an output file
        if(args[i] && strcmp(args[i], ">") == 0 ) {
            if(i+1 < argCount && args[i+1]) {
                // there are more args, should be the file name
                strncpy(fileOut, args[i+1], strlen(args[i+1]) + 1);
                free(args[i+1]);
                args[i+1] = NULL;
            }
            free(args[i]);
            args[i] = NULL;
        }

        // Found an input file
        if(args[i] && strcmp(args[i], "<") == 0) {
            if(i+1 < argCount && args[i+1]) {
                // there are more args, should be the file name
                strncpy(fileIn, args[i+1], strlen(args[i+1]) + 1);
                free(args[i+1]);
                args[i+1] = NULL;
            }
            free(args[i]);
            args[i] = NULL;
        }
    }

    childPID = fork();

    if(childPID >= 0) {
        if(childPID == 0) {

            // child process
            if(!parentWait) {
                // Put the child into the background (into a diff process group)
                setpgid(0, 0);
            }

            // Check if string is not null, you got a file: <
            if(fileIn != NULL && fileIn[0] != '\0') {
                int fd = open(fileIn, O_RDONLY);

                if(fd == -1) {
                    printf("Error: %s\n", strerror(errno));
                    return 1;
                } else {
                    dup2(fd, 0);   // make stdin come from file
                    close(fd);
                }
            }


            // Check if string is not null, you got a file: >
            if(fileOut != NULL && fileOut[0] != '\0') {
                int fd = open(fileOut, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

                if(fd == -1) {
                    printf("Error: %s\n", strerror(errno));
                    freeArgBuffer();
                    return 1;
                } else {
                    dup2(fd, 1);    // make stdout go to file
                    close(fd);
                }
            }

            if(execvp(program, args) == -1) {
                // Exec couldn't execute the commands
                printf("Error: %s\n", strerror(errno));
                exit(0);
            }

            //freeArgBuffer();

            // Gotta stop these naughty children...
            exit(0);

        } else {
            // parent process
            if(parentWait) {
                foregroundPID = childPID;
                waitpid(childPID, &status, 0);
                fprintf(stderr, "Child is done. Status: %d\n", status);

                // Getting the status as a string
                int lengthOfStatus = lengthOfInt(status);
                char statusBuffer[lengthOfStatus + 1];
                sprintf(statusBuffer, "%d", status);
                setLocalVar("?", statusBuffer);

                foregroundPID = -1;

            } else {
                // Getting the pid as a string
                int lengthOfPID = lengthOfInt(childPID);
                char pidBuffer[lengthOfPID + 1];
                sprintf(pidBuffer, "%d", childPID);
                setLocalVar("!", pidBuffer);
            }
        }
    } else {
        // Fork failed
        printf("Fork failed\n");
        return 1;
    }
    return 0;
}



/*
 * Processes the string read in from the command line.
 * Splits the string into an array of pointers to strings.
 * Each word in teh command becomes an individual element.
 *
 * Comments (#) are ignored.
 */
void splitCommand(char* line, int *argCount) {
    // Tokenize the input
    const char* delim = " \t\x09\xA";
    char *program;
    char *arguments;
    char* arg;
    int j;

    // Check if the line is commented out
    if(line[0] == '#') {
        *argCount = 0;
        argBuffer[0] = 0;
        return;
    }

    program = strtok(line, delim);

    if(program == NULL || program[0] == '\0' || strcmp(program, "") == 0) {
        *argCount = 0;
        argBuffer[0] = 0;
        return;
    }

    // Remove new line characters
    char *newline = strchr(program, '\n');
    if (newline) {
        *newline = 0;
    }

    // Cut off line after a "#"
    char *commented = strchr(program, '#');
    if (commented) {
        *commented = 0;
    }

    // NOTE max number of 16 args + 1 (null char) + 1 (program name)
    // char **argBuffer = (char **) malloc(sizeof(char) * MAX_ARGS + 1);
    arg = (char* ) malloc(sizeof(char) * strlen(program) + 1);
    strncpy(arg, program, strlen(program) + 1);
    argBuffer[0] = (char*) arg;


    arguments = strtok(NULL, delim);
    // walk through other tokens
    while(arguments != NULL && *argCount < MAX_ARGS) {
        fprintf(stderr, "strtok found an arg: %s\n", arguments);

        // Check if rest of line is commented out
        if(arguments[0] == '#') {
            break;
        }

        // # in the middle of a word, Cut off line after a "#"
        char *commented = strchr(arguments, '#');
        if (commented) {
            *commented = 0;
        }

        // Erasing the new line character
        char *newline = strchr(arguments, '\n');
        if (newline) {
            *newline = 0;

            // if the only character was a new line
            // Now have an empty string
            if(arguments[0] == 0) {
                break;
            }
        }

        // Save the next argument
        arg = (char* ) malloc(sizeof(char) * strlen(arguments) + 1);
        strncpy(arg, arguments, strlen(arguments) + 1);
        argBuffer[*argCount] = (char*) arg;

        arguments = strtok(NULL, delim);
        *argCount += 1;

        if (commented) {
            break;
        }
    }

    // terminates args with null char
    argBuffer[*argCount] = 0;

    for(j = 0; j < *argCount+1; ++j) {
        fprintf(stderr, "args: %s\n", argBuffer[j]);
    }

    return;
}




/*
 * Finds and displayed the command.
 * If the user asks for a variable ($) to be displayed,
 * search and then print that variable.
 */
void showVar(char ** argBuffer, int argCount) {
    int i;

    for(i = 1; i < argCount; ++i) {
        if(argBuffer[i][0] == '$') {
            // Found a var to replace
            // Removing the $
            char * searchId = malloc(sizeof(char) * strlen
                    (argBuffer[i]));
            strncpy(searchId, argBuffer[i] + 1, strlen(argBuffer[i]));

            // Find the string
            struct variableHashStruct *localResult = findLocalVar(searchId);
            if (localResult != NULL) {
                if (displayCommand) {
                    printf("show %s\n", argBuffer[i]);
                }

                printf("%s ", localResult->value);
            } else {
                // Check for global variables
                char *result = getenv(searchId);

                if (result != NULL) {
                    if (displayCommand) {
                        printf("show %s\n", argBuffer[i]);
                    }

                    printf("%s ", result);

                } else {
                    // Variable not found
                    printf("%s not found\n", argBuffer[i]);
                }
            }

            free(searchId);
        } else {
            // Just print out the word
            printf("%s ", argBuffer[i]);
        }
    }

    printf("\n");
}




/*
 * Removed the data of the variable from the
 * local variable array. Doesn't free the space.
 * Freeing happens at the end of the program.
 */
void unsetVar(char ** argBuffer) {
    // Find the struct
    struct variableHashStruct *var;
    var = findLocalVar(argBuffer[1]);

    if(var == NULL) {
        printf("%s not found\n", argBuffer[1]);
    } else {
        if(displayCommand) {
            printf("unset %s\n", argBuffer[1]);
        }

        //struct variableHashStruct *copy = var;
        fprintf(stderr, "var val: %s\n", var->value);

        // Delete/clear the struct
        var->id[0] = 0;
        var->value[0] = 0;

        fprintf(stderr, "var val: %s\n", var->value);
    }
}



/*
 * Called once at the start of the program. This
 * sets the default values for $$, $!, and $?.
 */
void setBasicEnvVar() {
    // $ - PID of shell
    // Getting the pid as a string
    int mainPID = getpid();
    int lengthOfPID = lengthOfInt(mainPID);
    char pidBuffer[lengthOfPID + 1];
    sprintf(pidBuffer, "%d", mainPID);
    setLocalVar("$", pidBuffer);

    // ? - Decimal value returned by last foreground process
    setLocalVar("?", "-1");

    // ! - PID of last background process
    setLocalVar("!", "-1");
}



/*
 * Catches Ctrl-C to only kill the foreground process.
 *
 * Tutorial:
 * http://www.geeksforgeeks.org/write-a-c-program-that-doesnt-terminate-when
 * -ctrlc-is-pressed/
 */
void signalTrap(){
    signal(SIGINT, signalTrap);

    if(foregroundPID != -1) {
        // Terminate the foreground process
        kill(foregroundPID, SIGKILL);
    }

    fprintf(stderr, "Got ctrl-c\n");

    if(displayCommand) {
        printf("Ctr-C");
    }

    printf("\n>> ");
    fflush(stdout);
}



/*
 * Replaces any variables in a command with its value.
 */
void subVar() {
    int i;

    for(i = 0; i < argCount; ++i) {
        if(argBuffer[i][0] == '$') {
            // Found a replaceable variable

            // Removing the $
            char * searchId = malloc(sizeof(char) * strlen
                    (argBuffer[i]));
            strncpy(searchId, argBuffer[i] + 1, strlen(argBuffer[i]));

            struct variableHashStruct *var;
            var = findLocalVar(searchId);

            if(var != NULL) {
                char * value = malloc(sizeof(char) * strlen(var->value)
                        + 1);
                strncpy(value, var->value, strlen(var->value) + 1);

                free(argBuffer[i]);
                argBuffer[i] = value;
            } else{
                // Check for global variables
                char * result = getenv(searchId);

                if(result != NULL) {
                    char * value = malloc(sizeof(char) * strlen
                            (result) + 1);
                    strncpy(value, result, strlen(result) + 1);

                    free(argBuffer[i]);
                    argBuffer[i] = value;

                } else {
                    // Variable not found
                    fprintf(stderr, "%s not found\n", argBuffer[i]);
                }
            }
            free(searchId);
        }
    }
}




/*
 * Reads in the different commands and processes them accordingly.
 * Internal and external commands are handled here.
 */
void processCommands() {
    // No input
    if(strcmp(line, "\n") == 0 || line[0] == 0) {
        return;
    }

    // Process the command
    argCount = 1;
    splitCommand(line, &argCount);

    fprintf(stderr, "arg count: %d\n", argCount);

    // if argBuffer is empty, continue
    if(argBuffer == NULL || argBuffer == 0 || argCount == 0) {
        freeArgBuffer();
        return;
    }


    // Run any internal commands
    if(strcmp(argBuffer[0], "show") == 0) {
        fprintf(stderr, "got show as input arg\n");

        if(argCount < 2) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        showVar(argBuffer, argCount);
    } else if(strcmp(argBuffer[0], "set") == 0) {
        fprintf(stderr, "got set as input arg\n");
        //printf("got set as input arg\n");

        if(argCount != 3) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        if(displayCommand) {
            printf("set %s %s\n", argBuffer[1], argBuffer[2]);
        }

        setLocalVar(argBuffer[1], argBuffer[2]);
    } else if(strcmp(argBuffer[0], "unset") == 0) {
        fprintf(stderr, "got unset as input arg\n");

        if(argCount != 2) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        unsetVar(argBuffer);
    } else if(strcmp(argBuffer[0], "export") == 0) {
        fprintf(stderr, "got export as input arg\n");

        if(argCount != 3) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        if(displayCommand) {
            printf("export %s %s\n", argBuffer[1], argBuffer[2]);
        }

        // Create the evn variable string: name=value
        char evnStr[2 * MAX_VAR_SIZE];
        strcpy (evnStr, argBuffer[1]);
        strcat (evnStr, "=");
        strcat (evnStr, argBuffer[2]);

        if(putenv(evnStr) != 0) {
            // Error has occurred
            printf("Error: %s\n", strerror(errno));
        }

    } else if(strcmp(argBuffer[0], "unexport") == 0) {
        fprintf(stderr, "got unexport as input arg\n");

        if(argCount != 2) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        if(displayCommand) {
            printf("unexport %s\n", argBuffer[1]);
        }

        if(unsetenv(argBuffer[1]) == -1) {
            // Error has occurred
            printf("Error: %s\n", strerror(errno));
        }

    } else if(strcmp(argBuffer[0], "chdir") == 0) {
        fprintf(stderr, "got chdir as input arg\n");

        if(argCount != 2) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        if(displayCommand) {
            printf("chdir %s\n", argBuffer[1]);
        }

        if(chdir(argBuffer[1]) == -1) {
            // Error has occurred
            printf("Error: %s\n", strerror(errno));
        }

    } else if(strcmp(argBuffer[0], "exit") == 0) {
        fprintf(stderr, "got exit as input arg\n");

        if(argCount != 2) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        if(displayCommand) {
            printf("exit %s\n", argBuffer[1]);
        }

        int exitCode = atoi(argBuffer[1]);

        freeArgBuffer();
        freeLocalVar();
        free(line);
        line = NULL;
        exit(exitCode);

    } else if(strcmp(argBuffer[0], "wait") == 0) {
        fprintf(stderr, "got wait as input arg\n");

        if(argCount != 2) {
            printf("Incorrect number of arguments.\n");
            freeArgBuffer();
            return;
        }

        // Variable substitution
        subVar();

        if(displayCommand) {
            printf("wait %s\n", argBuffer[1]);
        }

        int pid = atoi(argBuffer[1]);
        int status = 0;

        if(pid == -1) {
            // Wait for any children
            waitpid(-1, &status, 0);
        } else {
            // Wait for pid
            waitpid(pid, &status, 0);
        }

    } else {
        // Variable substitution
        subVar();

        // Process an external command
        forkCommand(argBuffer[0], argBuffer, argCount);
    }

    freeArgBuffer();
}




int main(int argc, char *argv[]) {
    int opt;                    // Command line arguments for xssh
    int debugLevel = 1;         // 1 = print debug, 0 = don't print
    size_t size;
    int i;

    // For file reading
    char *commandFile = "";
    int numFileArgs = 0;        // number of command line args for the file
    char *fileArgs[MAX_ARGS];   // $Vars to be set for the file to use

    // Set up Local variable array
    localVars = (struct variableHashStruct **)
            malloc(sizeof(struct variableHashStruct *) * numLocalVars);

    // Catching Ctrl-C
    signal(SIGINT, &signalTrap);

    // Set $$, $!, and $?
    setBasicEnvVar();


    // Read in the options from the command line
    while ((opt = getopt(argc, argv, "xd:f:")) != -1) {
        switch (opt) {

            case 'x':           // Display the command to be run
                displayCommand = 1;
                break;

            case 'd':           // Output debug messages
                debugLevel = atoi(optarg);
                break;

            case 'f':           // Option to input file
                commandFile = optarg;

                int index = optind;                     // Index of opt
                int fileArgIndex = 0;
                while(index < argc){
                    char * next = strdup(argv[index]);  // Get arg
                    index++;

                    if(next[0] != '-'){     // check if optarg is next switch
                        fileArgs[fileArgIndex++] = next;
                        ++numFileArgs;
                    } else {
                        optind = index - 1;
                        break;
                    }
                }
                optind = index - 1;
                break;

            default: /* '?' */
                printf("Usage: \n"
                        "\t\"-x\" Used to see the command to be run\n"
                        "\t\"-d <DebugLevel>\" Debug level 0 for no "
                        "messages\n \t\t\tDebug level = 1 to see messages\n"
                        "\t\"-f <file> <args>\" Input is from a file "
                        "instead of stdin.");
                return 0;
        }
    }


    if(debugLevel == 0) {
        // Don't output debug messages
        freopen("/dev/null", "w", stderr);
    }

    if(displayCommand) {
        fprintf(stderr, "got x\n");
    }

    fprintf(stderr, "debug level: %d \n", debugLevel);


    // Set the file args
    for(i = 0; i < numFileArgs; ++i) {
        int varId = i + 1;
        int lengthOfVarId = lengthOfInt(varId);
        char varIdBuffer[lengthOfVarId + 1];
        sprintf(varIdBuffer, "%d", varId);
        setLocalVar(varIdBuffer, fileArgs[i]);
    }


    // Run the commands in the given file
    if(commandFile[0] != '\0') {
        fprintf(stderr, "got file\n");

        FILE* fr = fopen(commandFile, "rt");

        if(fr == NULL) {
            perror("Error opening file.");
            freeLocalVar();
            return(-1);
        }

        line = (char*) malloc(MAX_LINE_SIZE * sizeof(char));

        while(fgets(line, MAX_LINE_SIZE, fr) != NULL) {
            // do the rest of the parsing and shell things!
            processCommands();
        }

        free(line);
        line = NULL;
    }

    // Command line prompt
    printf(">> ");

    // Run the commands from command line
    int readLineResult = 0;
    while((readLineResult = getline(&line, &size, stdin)) >= -1) {

        if(readLineResult == -1) {
            printf("Error: %s\n", strerror(errno));

            // Got a bad input, so you should just quit now
            freeLocalVar();

            if(line != NULL) {
                free(line);
                line = NULL;
            }

           return -1;
        } else {
            processCommands();
        }

        if(line != NULL) {
            free(line);
            line = NULL;
        }

        // Command line prompt
        printf(">> ");
    }

    freeLocalVar();
    return 0;
}
