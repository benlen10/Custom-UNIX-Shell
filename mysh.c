// Project 2 Part A: Shell
//
// Brief Description: Implements a custom UNIX shell that creates a new process
//                    for each command
//
// Semester:         CS 537 (Fall 2017)
//
// Author:           Benjamin Lenington
// Email:            lenington@wisc.edu
// CS Login:         lenington

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <ctype.h>

// Constant values
#define MAXLINELENGTH 128
#define MAXPATHLENGTH 10000
#define MAXARGS 20
const char error_message[30] = "An error has occurred\n";

// An array of arguments (tokens) passed to mysh
char **tokens;

// Token indexes
int redirOutputIndex = -1;
int redirInputIndex = -1;
int backgroundTokenIndex = -1;
int pipeLineTokenPosition = -1;

// The total number of arguments (tokens)
int tokenCount = 0;

// Stores a temp value for STDOUT
int STDOUT_TEMP = 0;

// The current pid
pid_t curPid;

/**
    Prints an error message to stderr
*/
int PrintStdError()
{
    if (write(STDERR_FILENO, error_message, strlen(error_message)) < 0)
    {
        return 1;
    }
    return 1;
}

/**
    Removes a /n newline char from the end of a string

    @returns a char * pointing to the beginning of the trimmed string
*/
char *TrimNewline(char *src)
{
    size_t len = strlen(src);
    if (len > 0 && src[len - 1] == '\n')
    {
        src[--len] = '\0';
    }
    return src;
}

/**
    Removes ' ' space chars 

    @returns a char * pointing to the beginning of the trimmed string
*/
char *TrimSpaces(char *str)
{
    char *endPos;
    while (isspace(*str))
    {
        str++;
    }

    // Check for empty string
    if (*str == 0)
    {
        return str;
    }

    // Set endPos to the last char of the string
    endPos = str + strlen(str) - 1;

    while (endPos > str && isspace(*endPos))
    {
        endPos--;
    }

    // Replace endPos with a null char and return the new string
    *(endPos + 1) = 0;
    return str;
}

/**
    Redirect the standard input

    @returns int result status
*/
int RedirectInput()
{
    // Check for mising filename
    if ((redirInputIndex + 1) > (tokenCount))
    {
        return 1;
    }

    //Check for invalid/extra args
    if (tokens[redirInputIndex + 3] != NULL && ((redirInputIndex + 2) != redirOutputIndex) && ((redirInputIndex + 2) != backgroundTokenIndex))
    {
        return 1;
    }

    // Attempt to open the specfied input file
    int inputFile = open(tokens[redirInputIndex + 1], O_RDONLY, 0);
    if (inputFile < 0)
    {
        return 1;
    }

    // Redirect the standard input source
    if (dup2(inputFile, STDIN_FILENO) < 0)
    {
        return 1;
    }

    // Close the input file and set token positions to null
    close(inputFile);
    tokens[redirInputIndex] = NULL;
    tokens[redirInputIndex + 1] = NULL;
    return 0;
}

/**
    Redirect the standard output

    @returns int result status
*/
int RedirectOutput()
{
    // Check for mising filename
    if (redirOutputIndex + 1 > (tokenCount))
    {
        return 1;
    }

    //Check for invalid/extra args
    if (tokens[redirOutputIndex + 2] != NULL && (redirOutputIndex + 2) != redirInputIndex && ((redirOutputIndex + 2) != backgroundTokenIndex))
    {
        return 1;
    }

    // Attempt to open the spe
    int outputFile = open(tokens[redirOutputIndex + 1], O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (outputFile < 0)
    {
        return 1;
    }

    // Redirect the stdout to the specified output file
    if (dup2(outputFile, STDOUT_FILENO) < 0)
    {
        return 1;
    }

    // Close the output file and set the specfied token indexes to NuLL
    close(outputFile);
    tokens[redirOutputIndex] = NULL;
    tokens[redirOutputIndex + 1] = NULL;
    return 0;
}

/**
    Implement a pipeline I/O redirect according to the arguments provided to mysh

    @returns int result status
*/
int RedirectPipeline()
{
    // Confirm that the second pipeline argument is present
    if(tokens[pipeLineTokenPosition + 1] == NULL){
        return 1;
    }

    // Set pipeline token argument positions to null
    char *firstArg[pipeLineTokenPosition + 1];
    char *secondArg[tokenCount - pipeLineTokenPosition];
    firstArg[pipeLineTokenPosition] = NULL;
    secondArg[tokenCount - pipeLineTokenPosition + 1] = NULL;

    // Populate arguments
    int i;
    for (i = 0; i < tokenCount - pipeLineTokenPosition; i++)
    {
        secondArg[i] = tokens[i + pipeLineTokenPosition + 1];
    }

    int pipeFd[2];

    for (i = 0; i < pipeLineTokenPosition; i++)
    {
        firstArg[i] = tokens[i];
    }

    // Attempt to create a new pipline and exit upon failure
    if (pipe(pipeFd) < 0)
    {
        return 1;
    }

    // Fork the current process and save the pid
    int newPid = fork();
    if (newPid > 0)
    {
        close(pipeFd[0]);
        dup2(pipeFd[1], 1);
        if (execvp(firstArg[0], firstArg) < 0)
        {
            return 1;
        }
        close(pipeFd[1]);
        exit(0);
    }
    else
    {
        close(pipeFd[1]);
        dup2(pipeFd[0], 0);
        if (execvp(secondArg[0], secondArg) < 0)
        {
            return 1;
        }
        close(pipeFd[0]);
        exit(0);
    }
    return 0;
}

/**
    Entry point for the program

    @returns int result status
*/
int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        return PrintStdError();
    }

    int counter = 1;
    while (1)
    {
        // Print the shell message and counter value
        char *shellOutput = malloc(80);
        sprintf(shellOutput, "mysh (%d)> ", counter);
        if (write(STDOUT_FILENO, shellOutput, strlen(shellOutput)) < 1)
        {
            return PrintStdError();
        }

        // Attempt to fetch user input
        char *buffer = (char *)malloc(MAXLINELENGTH + 1000);
        if (fgets(buffer, (MAXLINELENGTH + 1000), stdin) == NULL)
        {
            return PrintStdError();
        }

        // Check if the user input exceeds the max allowed input length
        if (strlen(buffer) > (MAXLINELENGTH + 1))
        {
            counter++;
            PrintStdError();
        }
        else
        {
            // Allocate heap memory to store each argument
            tokens = malloc(MAXLINELENGTH + 1000);

            // Reset all index values
            redirOutputIndex = -1;
            redirInputIndex = -1;
            pipeLineTokenPosition = -1;
            backgroundTokenIndex = -1;

            // Reset the current token count and create a char * variable to store the current argument
            tokenCount = 0;
            char *curToken;

            //Trim whitespce from the line and verify that the line is not all whitespace before incrementing the counter value
            buffer = TrimSpaces(buffer);
            if (strlen(buffer) < 2)
            {
                continue;
            }
            else
            {
                counter++;
            }

            while ((curToken = strtok_r(buffer, " ", &buffer)))
            {
                char *temp = TrimNewline(curToken);
                if (strcmp(temp, ">") == 0)
                {
                    redirOutputIndex = tokenCount;
                }
                if (strcmp(temp, "<") == 0)
                {
                    redirInputIndex = tokenCount;
                }
                if (strcmp(temp, "|") == 0)
                {
                    pipeLineTokenPosition = tokenCount;
                }
                if (strcmp(temp, "&") == 0)
                {
                    backgroundTokenIndex = tokenCount;
                }
                tokens[tokenCount++] = temp;
            }

            if (strcmp(buffer, "\n") != 0)
            {
                if (strcmp(tokens[0], "exit") == 0)
                {
                    if (tokenCount != 1)
                    {
                        PrintStdError();
                    }
                    else
                    {
                        exit(0);
                    }
                }
                else if (strcmp(tokens[0], "pwd") == 0)
                {
                    char *cwdPath = malloc(MAXPATHLENGTH);
                    if (getcwd(cwdPath, MAXPATHLENGTH) == NULL)
                    {
                        return PrintStdError();
                    }
                    if (tokens[1] != NULL)
                    {
                        PrintStdError();
                    }
                    else
                    {
                        if (write(STDOUT_FILENO, cwdPath, strlen(cwdPath)) < 1)
                        {
                            return PrintStdError();
                        }
                        if (write(STDOUT_FILENO, "\n", strlen("\n")) < 1)
                        {
                            return PrintStdError();
                        }
                    }
                }
                else if (strcmp(tokens[0], "cd") == 0)
                {
                    // If no additional arguments provided for cd, change the current directory to the home directory
                    if (tokens[1] == NULL)
                    {
                        if (chdir(getenv("HOME")) < 0)
                        {
                            return PrintStdError();
                        }
                    }
                    else
                    {
                        // Attempt to move to the specified directory
                        if (chdir(tokens[1]) < 0)
                        {
                            PrintStdError();
                        }
                    }
                }
                else
                {
                    curPid = fork();
                    if (curPid == 0)
                    {
                        // Redirect input
                        if (redirInputIndex > 0)
                        {
                            if (RedirectInput() != 0)
                            {
                                return PrintStdError();
                            }
                        }

                        // Redirect Output
                        if (redirOutputIndex > 0)
                        {
                            if (RedirectOutput() != 0)
                            {
                                return PrintStdError();
                            }
                        }

                        // Pipeline redirect
                        if (pipeLineTokenPosition > 0)
                        {
                            if (RedirectPipeline() != 0)
                            {
                                return PrintStdError();
                            }
                        }
                        else
                        {
                            // Remove background '&' token if present
                            if (backgroundTokenIndex > 0)
                            {
                                // TODO: Uncommenting this causes timeout
                                //tokens[backgroundTokenIndex] = NULL;
                            }

                            // Attempt to execute the specified command
                            if (execvp(tokens[0], tokens) < 0)
                            {
                                PrintStdError();
                            }
                        }
                        exit(1);
                    }
                    else
                    {
                        // Run process in the background if '&' was included as an argument to mysh
                        if (backgroundTokenIndex < 0)
                        {
                            int status;
                            wait(&status);
                        }
                    }
                }
            }
        }
    }
    // Exit with a sucuess status
    exit(0);
}