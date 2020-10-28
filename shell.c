#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h> 
#include <fcntl.h> 
#include <signal.h>

#define TRUE          1
#define MAX_COMMANDS 20
#define MAX_ARGS     20

// ********* GLOBAL VARIABLES *********
int numberOfCommands;
char * commands[MAX_COMMANDS] = { NULL };

char * commandList[] = {};
// ************************************

void printCommands()
{
    int i;
    for(i = 0; i < numberOfCommands; i++)
    {
        puts(commands[i]);
    }
}

void type_prompt()
{    
    // Getting username , getlogin() returns null to my machine, so I use getenv()
    char * user = getenv("USER");
    char cwd[512];

    // Getting current working directory
    getcwd(cwd, sizeof(cwd));

    // Print prompt
    printf("\033[0;31m"); // Color RED
    printf("\n[my_shell][%s][%s]", user, cwd);
    printf("\033[0;34m~"); // Color BLUE
    printf("\033[0m$ "); // Reset Color to Default
}

void getCommands()
{
    // Getting whole line
    char * buffer = NULL;
    size_t bufSize = 512;
    getline(&buffer, &bufSize, stdin);

    // Getting string's size
    int stringSize = strlen(buffer);
    // Replace newline character (shortening string by 1 character)
    buffer[stringSize - 1] = '\0';

    // Tokenizing string by spaces with strtok()
    char delim[] = " ";
    char * ptr = strtok(buffer, delim);
    
    int commandIndex = 0;
    while(ptr != NULL)
	{   
        if(commandIndex >= MAX_COMMANDS)
        {
            printf("ERROR: Max Command number exceeded.\n");
            return;
        }

		commands[commandIndex++] = strdup(ptr);
		ptr = strtok(NULL, delim);
	}

    // Storing final number of commands to the global variable
    numberOfCommands = commandIndex;
}

int isRedirection()
{
    // returns 1 for input redirection, 2 for output redirection, 3 for double output redirection and 0 for none 
    if(numberOfCommands <= 1)
        return 0;

    if(strcmp(commands[numberOfCommands - 2], "<")   == 0)
        return 1;
    if(strcmp(commands[numberOfCommands - 2], ">")  == 0)
        return 2;
    if(strcmp(commands[numberOfCommands - 2], ">>") == 0)
        return 3;    

    return 0;
}

void executeSingleCommand()
{
    char * binaryPath = NULL;
    char * args[MAX_ARGS] = { NULL };
    
    // Take main command and append it to '/bin/' path
    binaryPath = malloc(6 + strlen(commands[0]));
    strcpy(binaryPath, "/bin/");
    strcat(binaryPath, commands[0]);

    // Check for arguments in entered commands
    int i;
    for(i = 0; i < numberOfCommands; i++)
    {   
        args[i] = strdup(commands[i]);
    }

    execv(binaryPath, args);
    
    // If it reaches this far, means that execv returned, so the command was not excecuted.
    puts("Error: Unknown command.");
    // The process kills itself, since execv does not get executed.
    kill(getpid(), SIGINT);
}

void executePipeCommands(int pipeNumber)
{

    if(pipeNumber == 1)
    {
        int fd[2];

        // Make the pipe for communication
        pipe(fd);

        // Fork a child process
        pid_t pid = fork();

        if (pid == -1) 
        {
            perror("fork");
            exit(1);
        }

        if (pid == 0) // Child process
        {    
            dup2(fd[0], 0);
            close(fd[1]);
            
            char * binaryPath = NULL;
            char * args[MAX_ARGS] = { NULL };    

            // Check for arguments in entered commands
            int i;
            for(i = 0; i < numberOfCommands; i++)
            {   
                if(strcmp(commands[i], "|") == 0)
                {                       
                    // Take main command and append it to '/bin/' path
                    binaryPath = malloc(6 + strlen(commands[i + 1]));
                    strcpy(binaryPath, "/bin/");
                    strcat(binaryPath, commands[i + 1]);

                    // Find arguments
                    int index = 0;
                    int j;
                    for(j = i + 1; j < numberOfCommands; j++)
                        args[index++] = strdup(commands[j]);                    
                    break;
                }
                
            }
            
            execv(binaryPath, args); 
        } 
        else 
        { // Parent process

            char * binaryPath = NULL;
            char * args[MAX_ARGS] = { NULL };

            dup2(fd[1], 1);
            close(fd[0]);
    
            // Take main command and append it to '/bin/' path
            binaryPath = malloc(6 + strlen(commands[0]));
            strcpy(binaryPath, "/bin/");
            strcat(binaryPath, commands[0]);

            // Check for arguments in entered commands
            int i;
            for(i = 0; i < numberOfCommands; i++)
            {   
                if(strcmp(commands[i], "|") == 0)                
                    break;

                args[i] = strdup(commands[i]);
            }

            //execlp("ls", "ls", NULL);
            execv(binaryPath, args);
        }
    }
    else if(pipeNumber > 1)
    {
        printf("Pipeline with more than 2 commands not yet implemented.\n");
    }
    
    puts("We reach here if error....End of PipeCommands");
    kill(getpid(), SIGINT);
}

int isPipe()
{
    int numberOfPipes = 0;
    int i;
    for(i = 0; i < numberOfCommands; i++)
    {
        if(strcmp(commands[i], "|") == 0)
            numberOfPipes++;
    }
    
    return numberOfPipes;
}

void executeInputRedirection()
{
    char * fileName = strdup(commands[numberOfCommands - 1]);
    int in;
    in = open(fileName, O_RDONLY);

    // replace standard output with output file
    dup2(in, 0);

    close(in);

    commands[numberOfCommands - 1] = NULL;
    commands[numberOfCommands - 2] = NULL;
    numberOfCommands -= 2; // Removing '<' and filename from commands

    executeSingleCommand();
}

void executeOutputRedirection()
{
    char * fileName = strdup(commands[numberOfCommands - 1]);
    int out;
    out = open(fileName, O_RDWR | O_TRUNC | O_CREAT, S_IRWXU); // CREATE OR REPLACE. FILE PERMISSIONS: read write execute

    // replace standard output with output file
    dup2(out, 1);

    close(out);

    commands[numberOfCommands - 1] = NULL;
    commands[numberOfCommands - 2] = NULL;
    numberOfCommands -= 2; // Removing '>' and filename from commands

    executeSingleCommand();
}

void executeDoubleOutputRedirection()
{
    char * fileName = strdup(commands[numberOfCommands - 1]);
    int out;
    out = open(fileName, O_RDWR | O_APPEND | O_CREAT, S_IRWXU); // Append or create, FILE PERMISSIONS: read write execute

    // replace standard output with output file
    dup2(out, 1);

    close(out);

    commands[numberOfCommands - 1] = NULL;
    commands[numberOfCommands - 2] = NULL;
    numberOfCommands -= 2; // Removing '>>' and filename from commands

    executeSingleCommand();
}

void executeCommands()
{
    int pipeNumber = isPipe();
    if(pipeNumber > 0)
    {
        executePipeCommands(pipeNumber);
        return;
    }

    int redirection = isRedirection();

    switch (redirection)
    {
 
        case 0: // No redirection
            executeSingleCommand();
            break;
        case 1: // input redirection
            executeInputRedirection();
            break;
        case 2: // output redirection
            executeOutputRedirection();
            break;
        case 3: // double output redirection (append)
            executeDoubleOutputRedirection();
            break;

    }
}

int main()
{
    pid_t pid;
    int status;

    while(TRUE)
    {
        type_prompt();
        getCommands();

        // Check for special command 'exit'
        if(strcmp(commands[0], "exit") == 0)         
            exit(0);   

        // Check for  special command 'cd'
        if(strcmp(commands[0], "cd") == 0)
        {
            int failure = chdir(commands[1]);

            if(failure)
                puts("Error: could not find directory.");
        
            continue;
        }     

        pid = fork();
        
        if(pid != 0) // Parent code
        { 
            waitpid(-1, &status, 0); // Wait for child to finish
        } 
        else
        {
            executeCommands(); // Child code
        }
    }    
    
    return 0;
}


