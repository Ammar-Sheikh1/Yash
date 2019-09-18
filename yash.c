#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>

//Used for open, mode_t
#include <sys/stat.h>
#include <fcntl.h>

////Used for strtok & strlen
#include <string.h>

//inlcude 
static int TEMP = 1;

struct job_t{
	int jobid; 				//Jobid
	pid_t pgid; 			//Process group id
	char* jstring;			//Job String
	int status;				// status: 1:+ , 0:"-"
	
	//Keeps track of job that are running , and weather they are in the foreground
	enum State_t {RUNNING,STOPPED,DONE} state;		
	bool foreground;	 

	struct job_t* next;
};


/*Maintains a linked list of jobs

 Rules: for job control
 1.Most recent created job is always first the come back

*/
struct jobList_t{
	struct job_t* head;
	struct job_t* tail;
	int size;
};

void update_jobList_status(struct jobList_t* list){
	if(!list){
		return;
	}
	struct job_t* iter = list->head;
	while(iter != list->tail){
		iter->status = 0;
		iter = iter->next;
	}
	list->tail->status = 1;

	return;
}

int highest_jobid(struct jobList_t* list){
	if(!list->size){
		return 0;
	}
	struct job_t* iter = list->head;
	int ret = iter->jobid;
	while(iter != NULL) {
		if(iter->jobid > ret){
			ret = iter->jobid;
		}
		iter = iter->next;
	}
	return ret;
}



void appendJob(struct jobList_t* list,int pg,char* str){

	//Create the job
	struct job_t* job = malloc(sizeof(struct job_t));
	job->jobid = highest_jobid(list) + 1;
	job->pgid = pg;
	job->jstring = malloc(sizeof(char) * (strlen(str)+1));
	strcpy(job->jstring,str);

	job->status = 1;
	job->foreground = true;
	job->state = RUNNING;
	job->next = NULL;


	//Add the job to the list
	if(list->head == NULL){
		list->head = job;
		list->tail = job;
	}
	else{
		list->tail->next = job;
		list->tail = job;
	}

	update_jobList_status(list);

	list->size++;
	return;
}


void init_jobList(struct jobList_t* list){
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
}


void printJob(struct job_t* job){
	if(job->state == RUNNING){
		if(job->status){
			if(job->foreground){
				printf("[%d]+\tRunning\t\t%s\n",job->jobid,job->jstring);
			}
			else{
				printf("[%d]+\tRunning\t\t%s &\n",job->jobid,job->jstring);
			}	
		}
		else{
			if(job->foreground){
				printf("[%d]-\tRunning\t\t%s\n",job->jobid,job->jstring);
			}
			else{
				printf("[%d]-\tRunning\t\t%s & \n",job->jobid,job->jstring);
			}
		}
	}
	else if(job->state == STOPPED){
		if(job->status){
			if(job->foreground){
				printf("[%d]+\tStopped\t\t%s\n",job->jobid,job->jstring);
			}
			else{
				printf("[%d]+\tStopped\t\t%s &\n",job->jobid,job->jstring);
			}
		}
		else{
			if(job->foreground){
				printf("[%d]-\tStopped\t\t%s\n",job->jobid,job->jstring);
			}
			else{
				printf("[%d]-\tRunning\t\t%s & \n",job->jobid,job->jstring);
			}
		}

	}
	else if(job->state == DONE){
		if(job->status){
			if(job->foreground){
				printf("[%d]+\tDone\t\t%s\n",job->jobid,job->jstring);
			}
			else{
				printf("[%d]+\tDone\t\t%s &\n",job->jobid,job->jstring);
			}
		}
		else{
			if (job->foreground){
				printf("[%d]-\tDone\t\t%s\n",job->jobid,job->jstring);
			}
			else{
				printf("[%d]-\tDone\t\t%s &\n",job->jobid,job->jstring);
			}
		}
	}

}

void printJobList(struct jobList_t* jobList){
	if(!jobList->head){
		printf("List is empty\n");
		return;
	}
	struct job_t* iter = jobList->head;
	
	while(iter != NULL){
		printJob(iter);
		iter = iter->next;
	}	
	return;
}


/*Note: strtok does not Malloc memory!, it only manipulates the string we pass in*/
char** parseArgs(char* input){
	int argCount = 1;
	int index = 0;
	while(input[index] != '\0'){
		if(index > 0 && input[index-1] == ' '){
			++argCount;
		}
		index++;
	}
	char** ret = malloc(sizeof(char*) * (argCount+1));
	ret[0] = strtok(input," ");
	
	int i=1;
	while((ret[i] = strtok(NULL," ")) != NULL){
		i++;
	}
	ret[argCount] = NULL;

	return ret;
}


void set_job_to_foreground(struct job_t* job,bool b){
	if(!job){
		return;
	}
	job->foreground = b;
}

bool bgCheckAndRemove(char* input){
	if(strrchr(input,'&') != NULL){
		for(int i=0;input[i] != '\0';i++){
			if(input[i] == '&'){
				input[i] = ' ';
			}
		}
		return true;
	}
	return false;
}

//Need to be careful with this function
pid_t get_foreground_processGroup(struct jobList_t* list){
	if(!list){
		return 0;
	}
	struct job_t* iter = list->head;
	while(iter != NULL){
		if(iter->status == 1){
			return iter->pgid;
		}
		iter = iter->next;
	}
	return 0;
}

/* Prints out the job as finished and then removes it from the list
*/
void removeJob(struct jobList_t* list,int group,bool background){
	struct job_t* job = list->head;
	struct job_t* prev = NULL;
	while(job != NULL){
		
		if(job->pgid == group){
			job->state = DONE;
			if(background){
				printJob(job);
			}
			if(job == list->head){
				list->head = job->next; 
			}
			else if(job == list->tail){
				prev->next = job->next;
				list->tail = prev;
				update_jobList_status(list);
			}
			else{
				prev->next = job->next;
			}
			free(job);
			list->size--;
			return;
		}
		prev = job;
		job = job->next;
	}

	return;
}

struct job_t* get_job_group(struct jobList_t* list,pid_t group){
	if(!list){
		return NULL;
	}
	struct job_t* job = list->head;
	while(job != NULL){
		if(job->pgid == group){
			return job;
		}
		job = job->next;
	}
	return NULL;
}

/*	Sets a job wtih pit_t == group state to STOPPED
*/
void updateStoppedJob(struct jobList_t* list,pid_t group){
	struct job_t* job = get_job_group(list,group);
	if(job){
		job->state = STOPPED;
	}
}
/*	Finds job with matching pid_t to group and sets state to DONE
*/
void updateFinishedJob(struct jobList_t* list, pid_t group){
	struct job_t* job = get_job_group(list,group);
	if(job){
		job->state = DONE;
	}
}

void updateRunningJob(struct jobList_t* list, pid_t group){
	struct job_t* job = get_job_group(list,group);
	if(job){
		job->state = RUNNING;
	}
}


//Global that stores job information, This is needed for interrupts...
struct jobList_t jobList;


void sig_child(int signum){
	
	pid_t child;
	int status;

	child = waitpid(-1,&status,WNOHANG); 
	if(WIFEXITED(status)){
		removeJob(&jobList,child,true);	
	}
}

struct job_t* most_recent_stopped(struct jobList_t* list){
	if(!list->head){
		return NULL;
	}
	struct job_t* job = list->head;
	struct job_t* recent = NULL;
	while(job != NULL){
		if(job->state == STOPPED){
			recent = job;
		}
		job = job->next;
	}
	return recent;
}


void kill_all_Jobs(struct jobList_t* list){
	struct job_t* iter = list->head;
	while(iter != NULL){
		kill(-(iter->pgid),SIGINT);
	}
}



int main(int argc,char** argv){
	const char* prompt = "# ";

	//Mantain List of jobs
	init_jobList(&jobList);

	signal(SIGCHLD,sig_child);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGINT,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);


	while(true){

		char* inputCopy;
		char* input = readline(prompt);
		if(!input){
			kill_all_Jobs(&jobList);
			exit(0);
		}


		inputCopy = malloc(sizeof(char) * (strlen(input)+1));
		strcpy(inputCopy,input);
		bool bgArgPresent = bgCheckAndRemove(input);
		bgCheckAndRemove(inputCopy);

		char** args = parseArgs(input);

		pid_t child;
	
		if(!strcmp(input,"fg")){
			if(jobList.size > 0){
				int status;
				pid_t fore = get_foreground_processGroup(&jobList);
				if(fore){
					struct job_t* job= get_job_group(&jobList,fore);
					set_job_to_foreground(job,true);
					printf("%s\n",job->jstring);
					
					
					kill(fore,SIGCONT);
					tcsetpgrp(STDIN_FILENO,fore);
					waitpid(fore,&status,WUNTRACED);
					tcsetpgrp(STDIN_FILENO,getpgrp());
					
					if(WIFSTOPPED(status)){
						updateStoppedJob(&jobList,fore);
					}
					else if(WIFSIGNALED(status) || WIFEXITED(status)){
						removeJob(&jobList,fore,false);
					}
				}
			}

		}
		else if(!strcmp(input,"bg")){
			//What if there are mutiple bgs?
			if(jobList.size > 0){
				struct job_t* r = most_recent_stopped(&jobList);
				if(r){
					pid_t recent = r->pgid;
					struct job_t* job = get_job_group(&jobList,recent);
					set_job_to_foreground(job,false);
					if(job->status){
						printf("[%d]+ %s &\n",job->jobid,job->jstring);
					}
					else{
						printf("[%d]- %s &\n",job->jobid,job->jstring);
					}
					kill(recent,SIGCONT);
					updateRunningJob(&jobList,recent); 
				}
			}
		}
		else if(!strcmp(input,"jobs")){
			if(jobList.size>0){
				printJobList(&jobList);
			}
		}
		
	
		else {
			child = fork();
			if(child == 0){
				setpgid(0,0);						//Create new process group for child
				execvp("./job",args); 
			}

			appendJob(&jobList, child,inputCopy);

			if(bgArgPresent){

				set_job_to_foreground(get_job_group(&jobList,child),false);
			}
			else{ 
				int status;
				tcsetpgrp(STDIN_FILENO,child); 				//Gives control of the terminal to job.c
				int res = waitpid(child,&status,WUNTRACED); //Blocks until the process exits or has been stopped
				tcsetpgrp(STDIN_FILENO,getpgrp()); 			//Yash Takes back the terminal
				
				if(WIFEXITED(status) || WIFSIGNALED(status)){
					removeJob(&jobList,child,false);
				}
				else if(WIFSTOPPED(status)){
					updateStoppedJob(&jobList,child);

				}
	
			}
		}
		
		free(inputCopy);
		free(input);
		free(args);
	}

	
	//free(jobList.list); //Will have to free jobList
	return 0;		
}



