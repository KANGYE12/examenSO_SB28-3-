/**
 * Linux Job Control Shell Project
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "OS Concepts Essentials", Silberschatz et al.
 *
 * To compile and run the program:
 *   $ gcc shell.c job_control.c -o shell
 *   $ ./shell
 *	(then type ^D to exit program)
 **/

#include "job_control.h"   /* Remember to compile with module job_control.c */

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough */

job * my_job_list;

void sigchld_handler(int num_sig) {//It manage SIGCHLD signals that happens when a child process change its state
	int status, info;
	pid_t pid_wait;
	enum status status_res;
	job_iterator iter = get_iterator(my_job_list);
	while(has_next(iter)) {//Iter for all the jobs availables
		job *the_job = next(iter);
		
		pid_wait = waitpid(the_job->pgid, &status, WUNTRACED | WNOHANG | WCONTINUED);//Revise the state of a process in background without blocking it
		//WUNTRACED-->It allows us to detect if a child enter in a STOPPED process
		//WNOHANG-->It allows the waitpit not to block if there is no child that has change its state. Return 0 if there is no change(most important since it avoid the block)
		//WCONTINUED-->It allows waitpid to return if a child has continued its execution after been stopped

		if (pid_wait == the_job->pgid) {//Revise the process that was return by the waitpit(process group ID)
			status_res = analyze_status(status, &info);
			printf("Background pid: %d, command: %s, %s, info: %d\n",
						the_job->pgid,the_job->command,status_strings[status_res],info);
			//Update the state of the job or delete it if it has finished
			if(status_res == SUSPENDED) {
				the_job->state = STOPPED;
			} else if (status_res == CONTINUED) {
				the_job->state = BACKGROUND;
			} else {
				/* status_res == SIGNALED || status_res == EXITED */
				delete_job(my_job_list,the_job);
			}
		} else if (pid_wait == -1) {
			perror("Wait error from sigchld_handler");
		}
	}
}

void handle_sighup(int signum) {
    FILE *fp = fopen("hup.txt", "a");
    if (fp == NULL) {
        perror("Error opening hup.txt");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "SIGHUP received.\n");
    fclose(fp);
}
/**
 * MAIN
 **/
int main(void)
{
	char inputBuffer[MAX_LINE]; /* Buffer to hold the command entered */
	int background;             /* Equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* Command line (of 256) has max of 128 arguments */
	/* Probably useful variables: */
	int pid_fork, pid_wait; /* PIDs for created and waited processes */
	int status;             /* Status returned by wait */
	enum status status_res; /* Status processed by analyze_status() */
	int info;				/* Info processed by analyze_status() */

	ignore_terminal_signals(); //Ignore all the signals related to terminal job control
	my_job_list = new_list("Job List");//Create a new job list

	signal(SIGCHLD,sigchld_handler);//Associate the SIGCHLD signals with the sigchld_handler
	signal(SIGHUP, handle_sighup);//The new line signal to handle it

	while (1)   /* Program terminates normally inside get_command() after ^D is typed */
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* split the command into an array of strings */

		//Task 5--> detect the redirections
		char *file_in = NULL, *file_out = NULL;
		parse_redirections(args, &file_in, &file_out);//Detect the redirections and split it in out and in files

		if(args[0] == NULL) continue;   /* If command is empty or has syntax error in redirection */

		if(!strcmp(args[0],"cd")) {
			if(args[1] == NULL) {
				fprintf(stderr, "missing argument for cd\n");
			} else if(chdir(args[1]) != 0) {//call the chdir that search in path for the new direction.
				//If the path is found, it doesn't return anything, otherwise it returns -1
				perror("cd");
			}
		}else if (!strcmp(args[0], "fg")) {//fg command: Move a job to the foreground part
    		block_SIGCHLD();//Like a mutex in scala to avoid race conditions
    		int pos = 1;  // default
    		if (args[1]) pos = atoi(args[1]);
    		job *j = get_item_bypos(my_job_list, pos);//get the specific job in base in its position. It starts in 1 since position 0 is the title
    		if (j == NULL) {
        		printf("No such job at position %d\n", pos);
        		unblock_SIGCHLD();
        		continue;
   		 	}

    		pid_t pgid = j->pgid;
    		printf("Bringing job to foreground: [%d] %s\n", pos, j->command);
    		set_terminal(pgid);//give the terminal control to the job since we are in foreground
    		killpg(pgid, SIGCONT);  // continue if suspended
    		j->state = FOREGROUND;
			//Wait until the group finish or is suspended. After that, the terminal is recover by the shell
    		pid_wait = waitpid(-pgid, &status, WUNTRACED);//We use -pgid since we want the shell wait for all the group processes finish. In case there is not '-', it will wait only for the child with pid==pgid
    		set_terminal(getpid());//Recover the terminal for the shell, since the job has finished or suspended beacuse WUNTRACED detect it

    		if (pid_wait == -1)
			{
				perror("wait error");
				unblock_SIGCHLD();
				continue;
			}
    		enum status s = analyze_status(status, &info);
    		printf("Foreground pid: %d, command: %s, %s, info: %d\n",
    		       pgid, j->command, status_strings[s], info);
    		if (s == SUSPENDED) {//If it was suspended by Ctrl+Z, then it will keep in the job list
    		    j->state = STOPPED;
    		} else {
    		    delete_job(my_job_list, j); //if the job has finished
    		}
    		unblock_SIGCHLD();

		} else if(!strcmp(args[0], "bg")) {//bg command: move a job to the background part
			block_SIGCHLD();
			int pos = 1;
			if(args[1] != NULL)
				pos = atoi(args[1]);
			job *j = get_item_bypos(my_job_list, pos);
			if(j == NULL)
			{
				printf("No such job at position %d\n", pos);
				unblock_SIGCHLD();
				continue;
			}
			if(j->state == STOPPED)
			{
				killpg(j->pgid, SIGCONT); //resume the jobs that were stopped
				j->state = BACKGROUND;
				printf("Bacground pid: %d, command: %s, Continued, info: 0\n", j->pgid, j->command);
			}else{
				printf("Job [%d] is not stopped. Nothing to do.\n", pos);
			}
			unblock_SIGCHLD();
			
		} else if(!strcmp(args[0],"exit")) {
			printf("Bye\n");
			exit(EXIT_SUCCESS);
		} else if(!strcmp(args[0], "jobs")) {//Print the job list in the current my_job_list
			block_SIGCHLD();
			if(empty_list(my_job_list)) {
				printf("There is no jobs in the list\n");
			} else {
				print_job_list(my_job_list);
			}
			unblock_SIGCHLD();
		} else {
			/* External commands */
			pid_fork = fork(); //duplicate the process into parent and child
			if(pid_fork > 0) { //Parent process
				new_process_group(pid_fork);//Create a new process group for the the child process id(ensure child becomes leader of its own group)
				if(!background) { /* Foreground-->it must to wait for the process to finish before it can write new commands */
					set_terminal(pid_fork);//give the terminal control to the child process id
					pid_wait = waitpid(pid_fork, &status, WUNTRACED);
					set_terminal(getpid());//return the terminal control to the shell(parent process) once it is done
					if (pid_wait == pid_fork) {//If the child has either terminated or stopped and the process parent has successfully waited for it
						status_res = analyze_status(status, &info);
						printf("Foreground pid: %d, command: %s, %s, info: %d\n",
							pid_fork, args[0], status_strings[status_res], info);
						//Add a new job is the process was suspended
						if(status_res == SUSPENDED) {
							block_SIGCHLD();
							add_job(my_job_list, new_job(pid_fork, args[0], STOPPED));
							unblock_SIGCHLD();
						}
					} else if(pid_wait == -1) {
						perror("Wait error");
					}

				} else { /* Background--> I needn`t wait for the process to finish and can still put new process in background */
					printf("Background job running... pid: %d, command: %s\n", pid_fork, args[0]);
					block_SIGCHLD();
					add_job(my_job_list, new_job(pid_fork, args[0], BACKGROUND));
					unblock_SIGCHLD();
				}//The job in background is deleted by the signal handler since it detects change in the state of the child processes
			} else if(pid_fork == 0) { /*Child process*/
				new_process_group(getpid());
				if(!background) set_terminal(getpid());//give the terminal control to the child process
				restore_terminal_signals();//We block it at the begining to avoid race conditions and restore it to be able to use the signals as usual

				// Redirections
				if(file_in) {//Read the data(<)
					int fd_in = open(file_in, O_RDONLY);//Opens in read only mode, and return a file descriptor number or -1 in case of error
					if(fd_in < 0) {
						perror("Input redirection error");
						exit(EXIT_FAILURE);
					}
					if(dup2(fd_in, STDIN_FILENO) < 0) {//Makes the file descriptor points to file in. In case it success return 0
						//ATDIN_FILENO == 0 since is the file descriptor number for standard input in UNIX and we are redirecting it to fd_in
						perror("dup2 error for input");
						close(fd_in);
						exit(EXIT_FAILURE);
					}
					close(fd_in);//Is necesary a close function after an open to avoid file descriptor leaks
				}
				if(file_out) {//Write and save the data(>)
					int fd_out = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					//O_WRONGLY-->Opne the file in write mode
					//O_CREAT-->If the file doesnt exit, create it
					//O_TRUNC-->If the file already exit, truncate it clearing its content and set size to 0
					//0644--> the owner can wirte and read and the other can only read
					if(fd_out < 0) {
						perror("Output redirection error");
						exit(EXIT_FAILURE);
					}
					if(dup2(fd_out, STDOUT_FILENO) < 0) {//STDOUT_FILENO == 1, standard output--> make the program stdout(1) now write to fd_out
						perror("dup2 error for output");
						close(fd_out);
						exit(EXIT_FAILURE);
					}
					close(fd_out);
				}
				//In case it process has exit, the child process will stop executing the code and it will start executing the requiered program pass as a argument
				execvp(args[0], args);//Replace the child process image with the externaol command that must be found in path.
				printf("Error, command not found: %s\n", args[0]);
				exit(EXIT_FAILURE);
			} else {
				perror("Fork error");
			}
		}
	} /* End while */
}
