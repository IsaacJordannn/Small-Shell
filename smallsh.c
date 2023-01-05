#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#define MAX_ARGS 	    512
#define MAX_CHARS 	    2048
#define MAX_PROCESSES	200 	// Arbitrarily set amount of processes to account for

// Global Variables
int   num_args = 0;				// Number of arguments
char* arg_list[MAX_ARGS];	    // Array of arguments
int   bg_flag = 1;		        // Flag created for background
int   bg_process = 0;			// Value used to check if process is run in background
char  dir[100];			        // CWD
int   processes[MAX_PROCESSES];	// Array of PIDs
int   process_count = 0;		// Number of processes
int   process_status;			// Status of a particular process
struct sigaction SIGINT_action;	// SIGINT handler
struct sigaction SIGTSTP_action; // SIGTSTP handler

int  user_input(char* args);
void signal_handler();
void run_commands();
void exit_call();
void cd_call();
void status_call(int* exit_value);
void non_builtin(int* exit_value);
void child_process();
void parent_process(pid_t pid);

// main function sets signal handlers for SIGINT and SIGTSTP and runs a loop
// to get input from the user and then parses through to run commands
int main() {
	char input_string[2048];						// initialize string value that will be parsed through
	
	SIGTSTP_action.sa_handler = signal_handler;
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigfillset(&SIGTSTP_action.sa_mask);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    SIGINT_action.sa_handler=SIG_IGN;               // starts by ignoring CTRL-C
    sigfillset(&SIGINT_action.sa_mask);
    sigaction(SIGINT, &SIGINT_action, NULL);
	
	while(1) {
		num_args = user_input(input_string);        // input is put on input_string and num_args will reflect correct number of args found
		arg_list[num_args] = NULL; 				    // NULLify the last argument of the array
		run_commands();							    // last step is to run the commands in the list of arguments
	}
	return 0;
}

// signal_handler function is used to set the flag value and give feedback to the user
void signal_handler() {
	char* status_message;
	int status_messageSize = -1;
	char* prompt_message = ": ";
	switch(bg_flag) {
		case 0:
			status_message = "\nExiting foreground-only mode\n";
			status_messageSize = 30;
			bg_flag = 1;
			break;
		case 1:
			status_message = "\nEntering foreground-only mode (& is now ignored)\n";
			status_messageSize = 50;
			bg_flag = 0;
			break;
		default:
			status_message = "\nError: bg_flag is not 0 or 1\n";
			status_messageSize = 38;
			bg_flag = 1;
	}
	write(STDOUT_FILENO, status_message, status_messageSize);
	write(STDOUT_FILENO, prompt_message, 2);
}

// takes the input string as an argument and fills it with user input and uses tokens with spaces as
// delimiters to separate the arguments into the global list
int user_input(char* args) {
	int i, arg_index = 0;
	char temp [MAX_CHARS];
	printf(": ");
	fflush(stdout);
	fgets(args, MAX_CHARS, stdin);
	strtok(args, "\n");

	char* token = strtok(args, " ");
	while(token != NULL) {

		arg_list[arg_index] = token;
        // when an instance of $$ is found the characters are NULLified and replaced with the PID
		for(i = 1; i < strlen(arg_list[arg_index]); i++) {
			if(arg_list[arg_index][i] == '$' && arg_list[arg_index][i-1] == '$') {
				arg_list[arg_index][i] = '\0';
				arg_list[arg_index][i-1] = '\0';
				snprintf(temp, MAX_CHARS, "%s%d", arg_list[arg_index], getpid());
				arg_list[arg_index] = temp;
			}
		}
		token = strtok(NULL, " ");
		arg_index++;
	}
	return arg_index;
}

// run_commands handles both the calling of built-in and non-builtin commands
void run_commands() {
	int exit_value = 0;
    // comments and blank lines are ignored
	if(arg_list[0][0] == '#' || arg_list[0][0] == '\n') {
		
	}
	else if(strcmp(arg_list[0], "exit") == 0) {
		exit_call();
	}
	else if(strcmp(arg_list[0], "cd") == 0) {
		cd_call();
	}
	else if(strcmp(arg_list[0], "status") == 0) {
		status_call(&exit_value);
	}
	else {
		non_builtin(&exit_value);

		if(WIFSIGNALED(process_status) && exit_value == 0){ 
	        status_call(&exit_value); 
	    }
	}
}

// exits instantly if no processes have been ran, otherwise loops through the list of
// processes and calls the kill command on their PIDs
void exit_call() {
	if(process_count == 0)
		exit(0);
	else{
		int i;
		for(i = 0; i < process_count; i++) 
			kill(processes[i], SIGTERM);
		exit(1);
	}
}

// if cd is not given an argument the environment home value is used as the directory
// whereas when a argument is given then that argument is made the new directory
void cd_call() {
	int error = 0;

	if(num_args == 1) 
		error = chdir(getenv("HOME"));
	else 
		error = chdir(arg_list[1]);
	
	if(error == 0)
		printf("%s\n", getcwd(dir, 100));
	else
		printf("chdir() failed\n");
	fflush(stdout);
}

// gives either the exit status of the given process or the terminating signal
void status_call(int* exit_value) {
	int error_value = 0, signal_value = 0, exitvalue;

	waitpid(getpid(), &process_status, 0);		        // process status is given

	if(WIFEXITED(process_status)) 
        error_value = WEXITSTATUS(process_status);	    // exit status if exited normally

    if(WIFSIGNALED(process_status)) 
        signal_value = WTERMSIG(process_status);		// term signal if exited abnormally

    exitvalue = error_value + signal_value == 0 ? 0 : 1;

    if(signal_value == 0) 
    	printf("exit value %d\n", exitvalue);
    else {
    	*exit_value = 1;
    	printf("terminated by signal %d\n", signal_value);
    }
    fflush(stdout);
}

// used to perform all other commands
void non_builtin(int* exit_value) {
	pid_t pid;
	bg_process = 0;

    if(strcmp(arg_list[num_args-1], "&") == 0) {    // check if able to be made a bg process, if so make necessary changes
    	if(bg_flag == 1) 
    		bg_process = 1;
    	arg_list[num_args - 1] = NULL;
    }

	pid = fork();
	processes[process_count] = pid;					// adds PID to array
	process_count++;
	switch(pid) {
		case -1:
			perror("fork() failed\n");
			exit(1);
			break;

		case 0:
			child_process();
			break;

		default:
			parent_process(pid);
	}

	// Wait for background processes
	while ((pid = waitpid(-1, &process_status, WNOHANG)) > 0) {
        printf("background pid %d is done: ", pid);
        fflush(stdout);
        status_call(exit_value); 
	}
}

// handles file redirection and alters the signal handlers for the child processes and executes the process
void child_process() {
	int i, input_file = 0, output_file = 0;
	char input_filename[MAX_CHARS], output_filename[MAX_CHARS];
	
	for(i = 0; arg_list[i] != NULL; i++) {
		if(strcmp(arg_list[i], "<") == 0) {									// input file found, character nullified and file name copied
			input_file = 1;
			arg_list[i] = NULL;
			strcpy(input_filename, arg_list[i+1]);
			i++;
		}

		else if(strcmp(arg_list[i], ">") == 0) {							// output file found, character nullified and file name copied
			output_file = 1;
			arg_list[i] = NULL;
			strcpy(output_filename, arg_list[i+1]);
			i++;
		}
	}

	if(input_file) {
		int input_filenameDes = 0;
		if ((input_filenameDes = open(input_filename, O_RDONLY)) < 0) { 
            fprintf(stderr, "cannot open %s for input\n", input_filename);
            fflush(stdout); 
            exit(1); 
        }  
        dup2(input_filenameDes, 0);											// duplicates file descriptor if successfully opened input file
        close(input_filenameDes);
	}

	if(output_file) {
		int output_filenameDes = 0;
		if((output_filenameDes = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
			fprintf(stderr, "cannot open %s for output\n", output_filename);
			fflush(stdout); 
			exit(1); 
		}
		dup2(output_filenameDes, 1);										// duplicates file descriptor if successfully opened output file
        close(output_filenameDes);
	}

	// SIGINT handler altered to be able to terminate
	if(!bg_process) 
		SIGINT_action.sa_handler = SIG_DFL;
	    sigaction(SIGINT, &SIGINT_action, NULL);

	if(execvp(arg_list[0], arg_list) == -1 ) {
        perror(arg_list[0]);
        exit(1); 
    }
}

// returns control to user and doesn't wait if run in bg, waits otherwise
void parent_process(pid_t pid) {
	if(bg_process == 1) {
		waitpid(pid, &process_status, WNOHANG);
		printf("background pid is %d\n", pid);
		fflush(stdout); 
	}
	else {
		waitpid(pid, &process_status, 0);
	}
}