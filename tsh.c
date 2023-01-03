/*
 * tsh - A tiny shell program with job control
 *
 * Jordan Andrade
 * Partner: Shawn Colby
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
        char *argv[MAXARGS]; // initiliaze list of arguments
        int bg = parseline(cmdline, argv); // call parseline function
	int built = builtin_cmd(argv);
	sigset_t mask; // initialize variable of sigset and pid
    	pid_t pid;

	if(argv[0] == '/0') { // if the command line is blank return nothing
		return;
	}

        if(builtin_cmd(argv) == 1) { // if a built in command is called
		return;
	}

        if(built == 0) { // fork
        	sigprocmask(SIG_BLOCK, &mask, NULL); // blocking child signal
        	pid = fork(); // forking

		if(pid == 0) { // if it is a child
			setpgid(0, 0); // puts child in new process group
            		sigprocmask(SIG_UNBLOCK, &mask, NULL); // unblocks child signal

           		int file_descript1 = 0; // initialize variables for file desciptors
            		int file_descript2 = 0;
            		int file_descript3 = 0;
            		int file_descript4 = 0;
            		int file_descript5 [2];
            		int stdoutApp = 0; // initialize variables for true/false
            		int stderror = 0;
            		int stdinput = 0;
            		int stdoutput = 0;
            		pid_t pipeID; // intialize pipe
            		char in[64]; // initialize list for what goes in and out
            		char out[64];
			int i=0;

			while(argv[i] != NULL) { // iterate through command line
                		if(strcmp(argv[i], "<") == 0) { // if "<"
                    			argv[i] = NULL; // remove from command line
                    			strcpy(in, argv[i+1]); // copy to in
                    			stdinput = 1; // set stdinput to 1 for if statement below
                		}

                		else if(strcmp(argv[i], ">") == 0) { // if ">"
                    			argv[i] = NULL;
                    			strcpy(out, argv[i+1]); // copy to out
                    			stdoutput = 1;
                		}

                                else if(strcmp(argv[i], "2>") == 0) { // if "2>"
                                        argv[i] = NULL;
                                        strcpy(out, argv[i+1]);
                                        stderror = 1;
                                }

                		else if(strcmp(argv[i], ">>") == 0) { // if ">>"
                    			argv[i] = NULL;
                    			strcpy(out, argv[i+1]);
                    			stdoutApp = 1;
                		}

                		else if(strcmp(argv[i], "|") == 0) { // if pipe
                    			fflush(stdout);
                    			int j=0;
                    			char** left = (char**)malloc(32); // memory for leftside of pipe
                    			while (j < i) {
                        			left[j] = argv[j]; // adds leftside into array
                        			j = j + 1; // increment
                    			}

                    			int k = 0;
                    			argv[i] = NULL; // remove "|"
                    			char** right = (char**)malloc(32); // memory for rightside of pipe
                    			while (argv[i+1] != NULL) { // while not empty
                        			right[k] = argv[i+1]; // adds rightside into array
                        			k = k + 1; // increment
                        			i = i + 1;
                    			}

                    			pipe(file_descript5); // starts the pipe
                    			pipeID = fork(); // creates child
                    			if(pipeID == 0) { // if it is a child
                        			dup2(file_descript5[0],0); // read side of pipe dup to stdin
                        			close(file_descript5[1]); // close pipe
                        			execve(right[0], right, environ);
                    			}

                    			else { // parent
                        			dup2(file_descript5[1],1); // write side of pipe dup to stdout
                        			close(file_descript5[0]); //	close read side of pipe
                        			execve(left[0], left, environ);
                    			}
				}
				i = i + 1;
			}

            		if (stdinput == 1) { // if stdinput is true
                		file_descript1 = open(in, O_RDWR, 0); // set to open with flag
                		dup2(file_descript1, 0);
                		close(file_descript1); // close file
                		stdinput = 0; // set varible back to 0

            		}

                        if (stdoutApp == 1) { // if stdout is true
                                file_descript3 = open(out, O_WRONLY | O_APPEND); //set to open with flags
                                dup2(file_descript3, 1);
                                close(file_descript3); // close file
                                stdoutApp = 0; // set variable back to 0
                        }

            		if (stdoutput == 1) { // if stdoutput is true
                		file_descript2 = creat(out, 0777); // creat() == open() with flags O_CREAT|O_WRONLY|O_TRUNC
                		dup2(file_descript2, 1);
                		close(file_descript2); //  close file
                		stdoutput = 0; // set variable back to 0
            		}

            		if (stderror == 1) { // if stderror is true
                		file_descript4 = creat(out, 0777); // creat() == open() with flags O_CREAT|O_WRONLY|O_TRUNC
                		dup2(file_descript4, 2);
                		close(file_descript4); // close file
                		stderror = 0; // set varibale back to 0
            		}

			if(execve(argv[0], argv, environ) < 0) { // if execve fails
                		printf("%s: Command not found\n", argv[0]);
				exit(0);
			}
		}

		else { // if it is a parent
			sigprocmask(SIG_UNBLOCK, &mask, NULL); // unblocks child signal

			if(bg == 0) { // if it is a foreground job
				addjob(jobs, pid, FG, cmdline); // adds to the FG jobs
                		waitfg(pid); // waits to finish it
			}
			else { // if it is a background job
				addjob(jobs, pid, BG, cmdline); // add to the BG jobs
                		printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
			}
		}
	}
return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv)
{
        if(strcmp(argv[0], "quit") == 0) { // if the built in command is quit
                sigchld_handler(1); // reaps all children
		int i;
		for(i=0;i<MAXJOBS;i++) { // loops through all jobs
                	jobs[i].state == ST; // chnages job state
		}
                exit(0); // exits program successfully
                return 1;
                }

        if(strcmp(argv[0], "jobs") == 0) { // if the built in command is jobs
                listjobs(jobs);
                return 1;
                }

        if(strcmp(argv[0], "bg") == 0) { // if the built in command is bg
                do_bgfg(argv);
                return 1;
                }

        if(strcmp(argv[0], "fg") == 0) { // if the built in command is fg
                do_bgfg(argv);
                return 1;
                }

    return 0; // if it is not a builtin command
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
	pid_t pid; // initialize variables
	struct job_t *job;
	int jid;

	if(argv[1] == NULL) { // if there is no argument
		printf("%s command requires PID or %%jobid argument\n", argv[0]);
		return;
	}

	if(argv[1][0] == '%') { // if it is a job ID
		jid = atoi(&argv[1][1]); // converts the job ID into an int

		if (jid == 0) { // atoi is 0 if there is no value to convert to int
			printf("%s: argument must be a PID or %%jobid\n", argv[0]);
			return;
		}

		job = getjobjid(jobs, jid);
		if (job == NULL) { // if the job does not exist
			printf("%s: No such job\n", argv[1]);
			return;
		}
	}

	else { // if the argument is a process ID
		pid = atoi(argv[1]); // converts process id to int

		if (pid == 0) { // atoi is 0 if there is no value to convert to int
			printf("%s: argument must be a PID or %%jobid\n", argv[0]);
			return;
		}

		job = getjobpid(jobs, pid);
		if (job == NULL) { // if the process does not exist
			printf("(%d): No such process\n", pid);
			return;
		}
	}

	kill(job->pid, SIGCONT);

        if(strcmp(argv[0], "fg") == 0) { // if argv is fg
                job->state = FG; // change to foreground
                waitfg(job->pid);
        }

	if(strcmp(argv[0], "bg") == 0) { // if argv is bg
		job->state = BG; // change to background
    		printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
	}

	return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
        while(fgpid(jobs) == pid) { // wait until process ID not in fg
                sleep(1); // sleep while in fg
        }
        return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig)
{
	pid_t pid;
	int status;
	struct job_t *job;

	while((pid = waitpid(fgpid(jobs), &status, WNOHANG | WUNTRACED)) > 0) { // reaping zombie children
		job = getjobpid(jobs, pid); // gets job ID

		if (job != NULL) { // if not NULL
			struct job_t* job = getjobpid(jobs,pid);

			if(WIFSIGNALED(status)) { // if terminate signal is received
				printf("Job [%d] (%d) terminated by signal %d\n",pid2jid(pid),pid,WTERMSIG(status));
                		deletejob(jobs,pid); // delete job
       			 }

        		if(WIFSTOPPED(status)) { // if child is stopped
            			job->state = ST; // changes state to ST
            			printf("Job [%d] (%d) stopped by signal %d\n",pid2jid(pid),pid,WSTOPSIG(status));
       			 }

        		if(WIFEXITED(status)){ // exit
            			deletejob(jobs,pid);
       			 }
      	 	}
	}
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
	pid_t pid = fgpid(jobs); // sends SIGINT to every process in group

	if(pid != 0) { // if pid is not zero
		kill(-pid,sig); // terminate process
    	}
	return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
        pid_t pid = fgpid(jobs); // sends SIGSTP to every process in group

        if(pid != 0) { // if pid is not zero
                kill(-pid,sig);

    	}
	return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



