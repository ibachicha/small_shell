#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>


int isForeground = 0;
int exitStatus = 0;
int isValid = 0;
int backgroundStatus = 0;
int inRunCommand = 0;

struct Command
{
	char* argument_array[512];
	char* inputFile;
    int setInput;
	char* outputFile;
    int setOutput;
	int isBackground;
};

void initCommand(struct Command *c)
// Initialize *some* commands. others had to be made global. 
{
    for (int i=0; i < 512; i++) 
    {
        c->argument_array[i] = NULL;
    }
    // Set input & output to NULL
    c->inputFile = NULL;
    c->outputFile = NULL;

}

void printExitStatus()
// Print Status. 
{
	if (WIFEXITED(exitStatus)) {
		// If exited by status
		printf("exit value %d\n", WEXITSTATUS(exitStatus));
	} else {
		// If terminated by signal
		printf("terminated by signal %d\n", WTERMSIG(exitStatus));

	}
    fflush(stdout);
}

int redirection(struct Command* r)
// Handle redirection. 
{
    if (r->setInput == 1) 
    {
        int input = open(r->inputFile, O_RDONLY);
        if (input == -1) 
        {
            printf("cannot open %s for input\n", r->inputFile);
            fflush(stdout);
            exitStatus = 1;
            close(input);
            exit(1);
        }
        
        else
        {
            if (dup2(input, 0) == -1) 
            {
                printf("Unable to assign %s as input file\n", r->inputFile);
                exitStatus = 1;
                r->inputFile = NULL;
                fflush(stdout);
            } 
            dup2(input, 0);
        }
    fcntl(input, F_SETFD, FD_CLOEXEC);
    }

    // Output
    if (r->setOutput == 1) 			
    {
        int output = open(r->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (output == -1) 
        {
            printf("cannot open %s for output\n", r->outputFile);
            exitStatus = 1;
            exit(1);
        }
       
        else
        {
            if (dup2(output, 1) == -1) 
            {
                printf("Unable to assign %s as output file\n", r->outputFile);
                exitStatus = 1;
            }
            dup2(output,1);
        }
    fcntl(output, F_SETFD, FD_CLOEXEC);
    }

return 0;
}


void background_output()
// If background set to dev/Null. Created in large part with help from 
// the explorations. 
{
    int input = open("/dev/null", O_RDONLY);
    if (input == -1) 
    {
        printf("cannot open trash\n");
        fflush(stdout);
        close(input);
        exit(1);
    }
    
    else
    {
        if (dup2(input, 0) == -1) 
        {
            printf("STDIN is not able to redirect to trash.\n");
        } 
    dup2(input, 0);
    }
    fcntl(input, F_SETFD, FD_CLOEXEC);

    int output = open("/dev/null", O_WRONLY);
    if (output == -1) 
    {
        printf("cannot open throw to trash\n");
        fflush(stdout);
        exit(1);
    }
    
    else
    {
        if (dup2(output, 1) == -1) 
        {
            printf("unable to redirect to trash\n");
            fflush(stdout);
        }
        dup2(output,1);
    }
    fcntl(output, F_SETFD, FD_CLOEXEC);
}

void runCommand(struct Command *c, struct sigaction sa)
{
    pid_t spawnPid = -100;
    inRunCommand = 1; //Set flag to know that we are in sun Commaand Function
    spawnPid = fork(); 
    switch(spawnPid)
    {
        case -1: // fork was not successful
            perror("FATAL ERROR: fork() failed!");
            fflush(stdout);
            exit(1);
            break;
        
        case 0: // fork was successful and we are in the child process
            // execvp runs our command

            if (c->isBackground == 0 || (isForeground = 1))
            {
                sa.sa_handler = SIG_DFL;
                sigaction(SIGINT, &sa, NULL);

                // If there's an input or output file, call redirection
                if(c->inputFile || c->outputFile)
                {
                    redirection(c);
                }

                // If background if true dev/null it
                if(c->isBackground)
                {
                    background_output(c);
                }

                if (execvp(c->argument_array[0], c->argument_array))
                {
                    // if we returned from execvp, it means an error occurred 
                    // probably the command wasn't found
                    printf("%s: no such file or directory\n", c->argument_array[0]);
                    fflush(stdout);
                    exitStatus = 1;        
                    exit(1); 
                }
                break;
            }
            
            
        default: 
            // fork was successful and we are in the "parent" (smallsh) process
            // in our simple command runner, we just wait for the command to finish
            if (c->isBackground == 1 && !isForeground)
            {
                waitpid(spawnPid, &exitStatus, WNOHANG); //exit status of child
                printf("background pid is %d\n", spawnPid);
                fflush(stdout); 
            }
            else
            {
                // waitpid will update our exitStatus variable that we passed in
                waitpid(spawnPid, &exitStatus, 0);
                if (!WIFEXITED(exitStatus))
                {
                    // If exited by status
                    printf("terminated by signal %d\n", WTERMSIG(exitStatus));
                    fflush(stdout);  
                }
            
            }
    } 
    inRunCommand = 0; // reset inRunCommand on exit     
}

void checkBackground()
// Check the background processes
{
    int status;
    int pid;
    
    while((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {   
        printf("background pid %d is done: ", pid);
        exitStatus = status;
        printExitStatus(); 
        fflush(stdout);          
    }
}


int checkBuiltIn(struct Command *c, int argument_count, struct sigaction sa)
// Check the builting functions cd, status and exit as well as checking for 
// # and NULL. Then pass everything to an actual run commmand for exececution. 
{    
    if (!c)
    {
        return 0;
    }
    else if (c->argument_array[0] == NULL)
    {  // Blank Line Entered if c->argument_array[0] is !NULL
        return 0;
    }
    else if (c->argument_array[0][0] == '\0')
    {
        return 0;
    }
    else if (c->argument_array[0][0] == '\n')
    {
        return 0;
    }
    // Check for Comment. First argument, first character. 
    else if (c->argument_array[0][0] == '#')
    {
        return 0;
    }
    else if (strcmp(c->argument_array[0], "cd") == 0)
    { 
        // Change to the directory specified
        if (c->argument_array[1])
        {
            if (chdir(c->argument_array[1]) != 0)
            {
                printf("Directory not found: %s\n", c->argument_array[1]);
                fflush(stdout);					
            }
        }
        else 
        {
            // If no directory given, go to home dir
            printf("Changing directory to home directory\n");
            chdir(getenv("HOME"));
            fflush(stdout);
        }
    }
    else if (strcmp(c->argument_array[0], "status") == 0)
    { 

        printExitStatus(c);
        backgroundStatus = 1;
        
    }
    else if (strcmp(c->argument_array[0], "exit") == 0)
    {  
        exit(0);
    }
    else    // Run the command 
    {
        runCommand(c, sa);
    }
    
return(0);
}

void parseCommand(char* userInput,struct Command* command_obj, struct sigaction sa)
// Begins by checking for invalid input as well the symbols <, >, &, and $$.
// Builds the array arguments and then passes everything over to checkBuilin(). 
{
    int argument_count = 0;
    int length = strlen(userInput);

    // Flag & and delete it once flagged
    if(userInput[length-1] == '&')
    {
        command_obj->isBackground = 1;
        userInput[length - 1] = '\0';
    }
    if(userInput[length] != '\0')
    {
        printf("Not NULL\n");
        fflush(stdout);
    }

    // Set the initial flags and start parsing the input
    char* token = strtok(userInput, " ");
    command_obj->setInput = -1;
    command_obj->setOutput = -1;


    for (int i = 0; token; i++)
    { 
        if (strcmp(token, "<") == 0) // Flag Input
        {
            token = strtok(NULL, " ");
            command_obj->inputFile = strdup(token);
            command_obj->setInput = 1;
    
        }
        else if (strcmp(token, ">") == 0) // Flag Output
        {
            token = strtok(NULL, " ");
            command_obj->outputFile = strdup(token);
            command_obj->setOutput = 1;
            break;
        }
        // Replace "$$" with the pid
        else if(strstr(token, "$$") != NULL)
        {
            while(strstr(token, "$$") != NULL)
            {
                // Create a new token
                char * dollar = strstr(token, "$$");
                char * newToken = (char *)(malloc(sizeof(char)*(strlen(token)+15)));
                char * iter = token;
                int i=0;
                while(iter!=dollar)
                {
                    newToken[i] = token[i];
                    iter++;
                    i++;
                }
                // Get the PID
                char pid_string[15];
                int pid = getpid();
                sprintf(pid_string, "%d", pid);

                int j;
                for(j = 0; j<strlen(pid_string); j++)
                {
                    newToken[j+i] = pid_string[j];
                }

                i = i + 2;
                for( ; i<strlen(token); i++)
                {
                    newToken[i+j-2] = token[i];
                }
                newToken[i+j-2] = '\0';
                token = newToken;
            }
        }

        else
        {   // Add tokens to Arguments Struct Array
            command_obj->argument_array[argument_count] = strdup(token); 
            token = strtok(NULL, " "); // Next
            argument_count++;
        }
    }
    checkBuiltIn(command_obj, argument_count, sa);
}


void handle_SIGTSTP(int signo) //control z handler
{
	// If foreground == 0, set it to 1 and display a message
	if (isForeground == 0) 
    {
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		fflush(stdout);
		isForeground = 1;
	}

	// If foreground == 1, set it to 0 and display a message
	else 
    {
		char* message = "\nExiting foreground-only mode\n";
		write (STDOUT_FILENO, message, 30);
		fflush(stdout);
		isForeground = 0;
	}
}


void handle_SIGINT(int signo)
// Get Exit Status
{ 
    if (!WIFEXITED(exitStatus) && inRunCommand == 1)
    {
		// If exited by status
		printf("terminated by signal %d\n", WTERMSIG(exitStatus));
        fflush(stdout); 
	}
    else
    {
        // Format a new line if cntrl C so " : " is on new line. 
        printf("\n");
        fflush(stdout);
    } 
}

int main()
{
    char userInput[2048];
    
    struct sigaction SIGINT_action = {{0}};
    struct sigaction SIGSTP_action = {{0}};

    // Ignore ^C
    // Taken largely from homework modules. 
	// SIGINT_action.sa_handler = handle_SIGINT;
    SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Redirect ^Z 
	SIGSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGSTP_action.sa_mask);
	SIGSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGSTP_action, NULL);
    
    while (1)
    {
        checkBackground();
        printf(": ");
        
        if(fgets(userInput, 2048, stdin) != NULL)
        {
            if(userInput[0] != '\n')
            {
                userInput[strcspn(userInput, "\n")] = 0; // take off trailing \n
                struct Command* command_obj = malloc(sizeof(struct Command));
                if(command_obj == NULL)
                {
                    // exit the code
                    printf("Memory allocation failed for command. Try Again!");
                    exit(1);
                }
                initCommand(command_obj);
                parseCommand(userInput, command_obj, SIGINT_action);
            }
            
            fflush(stdin);
        }
    
    }
    return 0;

}
