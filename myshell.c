
#define MAXII 100
#include <stdio.h>
#include <string.h>
#include <stdlib.h>			
#include <unistd.h>			
#include <sys/wait.h>		
#include <signal.h>			
#include <fcntl.h>			
char workingdirectory[1024];
/*Signal was ignored in  the following functions and reregistered for future
The shell prompt was displayed after that fflush was used so that output will be printed at the same time*/
void siginthandler(int sig_num){
	signal(SIGINT,siginthandler);
	printf("\n%s$",getcwd(workingdirectory,sizeof(workingdirectory)));
	fflush(stdout);
}
void sigtstpHandler(int sig_num) {
    signal(SIGTSTP, sigtstpHandler);  
    printf("\n%s$", getcwd(workingdirectory, sizeof(workingdirectory)));
    fflush(stdout);
}
/*Parse input function was used to transfer the line input to an array of tokens
which contain words and " " were trimmed so that each token contains a command word*/
void ParseInput(char* line,char** tokens){
	int i=0;
	char* token;
	while((token=strsep(&line," "))!=NULL){//tokens were acquired on using strsep function
		if(strlen(token)>0){//since to avoid multiple spaces trimming for example
			tokens[i++]=token;
		}
	}
	tokens[i]=NULL;//Marking the end of all tokens as NULL,instead traversing until MAX_SIZE the next time
}
/*The tokens were executed by this execute Command
This function forks a child process and execute it using execvp*/
void executeCommand(char** args) {
    int f = fork();
    if (f < 0) {
        perror("fork failed");
    } 
	else if (f == 0) { // Child process
        // Execute the command
        if (execvp(args[0], args) < 0) {
            printf("Shell: Incorrect command\n");
            exit(EXIT_FAILURE); // Exit with failure if execvp fails
        }
    } 
	else { // Parent process waits until the child process gets completed
        wait(NULL);
    }
}
/*The commands that were separated by && were executed by the below function
At first the line separated by && is converted to cmds array and then they were passed to parse input and executed in parallel*/
void executeParallelCommands(char* line){
	char* cmds[MAXII];
	int i=0;
	while((cmds[i]=strsep(&line,"&&"))!=NULL){
		i++;
	}
	cmds[i]=NULL;   //Now commands[i] doesn't contain && but may contain " "
	for(int j=0;j<i;j++){
		char* tokens[MAXII];
		ParseInput(cmds[j],tokens);
		if (tokens[0] == NULL) {
            continue; // Skip empty commands
        }
		// Handle 'cd' separately in the parent process
        if (strcmp(tokens[0], "cd") == 0) {
            if (tokens[1] == NULL || strcmp(tokens[1], "~") == 0) {
                // Change to the home directory
                if (chdir(getenv("HOME")) != 0) {
                    perror("Shell: Incorrect command\n");
                }
            } 
            else {
                // Change to the specified directory
                if (chdir(tokens[1]) != 0) {
                    perror("Shell: Incorrect command\n");
                }
            }
            continue; // Skip forking for 'cd' as it was handled in the parent
        }
		//For other commands we need forking
		if(fork()==0){//Creating a process for each command and executing only if it is child
			
			if (execvp(tokens[0], tokens) < 0) {
				printf("Shell: Incorrect command\n");
				exit(EXIT_FAILURE); // Exit with failure if execvp fails
    		}
			exit(0);//**This helps to execute only that child process only so that all child process are executed in parallel
		}
	}
	for(int j=0;j<i;j++){
		wait(NULL);//After forking it waits for each child process to complete hence supporting parallel execution
	}
}
/*The commands that were separated by ## were executed by the below function
At first the line separates by ## and is converted to cmds array and then they were passed to parse input and executed sequentially*/
void executeSequentialCommands(char* line){
	char* cmds[MAXII];
	int i=0;
	while((cmds[i]=strsep(&line,"##"))!=NULL){
		i++;
	}
	cmds[i]=NULL;
	for(int j=0;j<i;j++){
		char* tokens[MAXII];
		ParseInput(cmds[j],tokens);
		/*As cd command must be executed in parent process it's execution 
		is different from other commands (cd doesn't need forking as no change will be there if you execute cd in child process)*/
		if (tokens[0] == NULL) {
            continue; // Skip empty commands
        }
		if(strcmp(tokens[0],"cd")==0){
			if(tokens[1]==NULL || strcmp(tokens[1],"~")==0){
				if(chdir(getenv("HOME"))!=0){
					perror("Shell: Incorrect command\n");
				}
			}
			else{
				if(chdir(tokens[1])!=0){
					perror("Shell: Incorrect command\n");
				}
			}
		}
		else{
			executeCommand(tokens);//Executing each command one by one sequentially
		}
	}
}
/*The commands that were separated by > were executed by the below function
At first the line separates by > and is converted to cmds array */
void executeCommandRedirection(char* line) {
    char* com[MAXII]; // Array to hold command and file name
    int a = 0;

    // Split the input line into command and file name
    while ((com[a] = strsep(&line, ">")) != NULL) {
        a++;
    }
    com[a] = NULL; // Null-terminate the array

    // Ensure we have both command and file name
    if (a < 2 || com[0] == NULL || com[1] == NULL) {
        printf("Shell: Incorrect command\n");
        return;
    }

    // Remove leading and trailing whitespaces from command
    char *command = com[0];
    char *file = com[1];
	//Trimming ' ' at start and end
    while (*command == ' ') command++;
    char *end = command + strlen(command) - 1;
    while (end > command && *end == ' ') end--;
    *(end + 1) = '\0'; // Null-terminate

    // Remove leading and trailing whitespaces from file
    while (*file == ' ') file++;
    end = file + strlen(file) - 1;
    while (end > file && *end == ' ') end--;
    *(end + 1) = '\0'; // Null-terminate

    // Fork a new process
    int f = fork();
    if (f < 0) {
        perror("fork failed");
    } 
    else if (f == 0) { // Child process
        // Open file for writing, create if it doesn't exist, truncate if it does
        int fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open failed");
            exit(EXIT_FAILURE);
        }

        // Redirect standard output to the file
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 failed");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Close the original file descriptor
        close(fd);

        // Prepare command for execvp
        char *cmd[] = {command, NULL};

        // Execute the command
        execvp(command, cmd);
        // If execvp fails
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } 
    else { // Parent process
        wait(NULL);
    }
}


void executeCommandPipelines(char* line) {
    char* cmds[MAXII];
    int numCommands = 0;

    // Split the input line by the pipe symbol "|"
    while ((cmds[numCommands] = strsep(&line, "|")) != NULL) {
        if(strlen(cmds[numCommands])==0)continue;//Empty command no need to execute
        numCommands++;
    }
    if(numCommands==0)return;
    cmds[numCommands] = NULL; // Mark the end of the array

    int pipefiledescrip[2];   // File descriptors for the pipe
    int prevPipe = -1; // To store the previous pipe's read end
    int i;

    for (i = 0; i < numCommands; i++) {
        char* tokens[MAXII];
        ParseInput(cmds[i], tokens); // Parse each command to get arguments

        // Create a pipe, except for the last command
        // Handle built-in commands like "cd" before forking
        if (strcmp(tokens[0], "cd") == 0) {
            if (tokens[1] == NULL || chdir(tokens[1]) != 0) {
                perror("Shell: Incorrect command");
            }
            else{
				if(chdir(tokens[1])!=0){
					perror("Shell: Incorrect command\n");
				}
			}
            continue; // Skip the rest of the loop for built-in commands
        }
        if (i < numCommands - 1) {
            if (pipe(pipefiledescrip) < 0) {
                perror("pipe failed");
                return;
            }
        }

        // Fork a new process
        int rc = fork();
        if (rc < 0) {
            perror("fork failed");
            return;
        }

        if (rc == 0) { // Child process
            // If not the first command, set input to previous pipe's read end
            if (prevPipe != -1) {
                if (dup2(prevPipe, STDIN_FILENO) < 0) {//If it fails
                    perror("dup2 prevPipe failed");
                    exit(EXIT_FAILURE);
                }
                close(prevPipe); // Close the read end after dup
            }

            // If not the last command, set output to current pipe's write end
            if (i < numCommands - 1) {
                close(pipefiledescrip[0]); // Close read end of the current pipe
                if (dup2(pipefiledescrip[1], STDOUT_FILENO) < 0) {
                    perror("dup2 pipefd[1] failed");
                    exit(EXIT_FAILURE);
                }
                close(pipefiledescrip[1]); // Close the write end after dup
            }

            // Execute the command
            if (execvp(tokens[0], tokens) < 0) {
                perror("Shell: Incorrect command");
                exit(EXIT_FAILURE);
            }
        } 
		else { // Parent process
            // Close the previous pipe's read end, if any
            if (prevPipe != -1) {
                close(prevPipe);
            }

            // Close the current pipe's write end
            if (i < numCommands - 1) {
                close(pipefiledescrip[1]);
            }

            // Store the read end of the current pipe for the next iteration
            prevPipe = pipefiledescrip[0];
        }
    }

    // Wait for all child processes to finish
    for (i = 0; i < numCommands; i++) {
        wait(NULL);
    }
}



int main()
{
	//Registering signal handlers SIGINT for ctrl c and SIGSTP for ctrl z
	signal(SIGINT,siginthandler);//for ctrl+c
	signal(SIGTSTP,sigtstpHandler);//for ctrl+z
	// Initial declarations
    char *line = NULL;
    size_t len = 0;//Since length is always non-negtive
    ssize_t nread;
	char* tokens[MAXII];//Tokens array to store each command
	
	while(1)	// This loop will keep your shell running until user exits.
	{

		printf("%s$",getcwd(workingdirectory,sizeof(workingdirectory)));// Print the prompt in format - currentWorkingDirectory$
		
		nread = getline(&line, &len, stdin);// accept input with 'getline()'
		if (nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }//Removing the newline characters
		if(line[0]=='\0'){
			continue;//Show the prompt if command is empty
		}
		if(strstr(line,"&&")!=NULL){
			executeParallelCommands(line);
		}
		else if(strstr(line,"##")!=NULL){
			executeSequentialCommands(line);
		}
		else if(strstr(line,">")!=NULL){
			executeCommandRedirection(line);
		}
		else if(strstr(line,"|")!=NULL){
			executeCommandPipelines(line);
		}
		else{
			ParseInput(line,tokens);
			if(strcmp(tokens[0],"exit")==0){
				printf("Exiting shell...\n");
				break;
			}
			else if(strcmp(tokens[0],"cd")==0){//cd cmd
				if (tokens[1] == NULL || strcmp(tokens[1], "~") == 0) {
					// Changing to home directory
					if (chdir(getenv("HOME")) != 0) {
						perror("Shell: Incorrect command\n");//Indicating failure
					}
				}
				else {//It might be cd .. or cd path
					if(chdir(tokens[1])!=0){
						perror("Shell: Incorrect command\n");
					}
				}
			}
			else{
				executeCommand(tokens);
			}
		}
		
		
				
	}
	
	return 0;
}
