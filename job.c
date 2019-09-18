#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

//waitpid
#include <sys/wait.h>

#include <string.h>
#include <stdlib.h>

//File redirection
#include <sys/stat.h>
#include <fcntl.h>


int pipeArgPresent(char** args){
	for(int i=0;args[i] != NULL;i++){
		if(strcmp(args[i],"|") == 0){
			return i;
		}
	}
	return -1;
}

void splitPipeArgs(char** dest,char** src1, char** src2){
	
	int i=0;
	for(;strcmp(dest[i],"|") != 0;i++){
		src1[i] = dest[i];
	}
	src1[i] = NULL;
	
	i++;
	for(int j=0;dest[i] != NULL;i++,j++){
		src2[j] = dest[i];
	}
	src2[i] = NULL;
	return;
}

/*Returns the index of the matching string*/
int stringSearch(char** args,char* str){
	for(int i=0;args[i] != NULL;i++){
		if(strcmp(args[i],str) == 0){
			return i;
		}
	}
	return -1;
}

void remove_redirect_args(char** args,int fileInArg,int fileOutArg,int errArg){
	for(int i=0;args[i] != NULL;i++){
		if(i == fileInArg || i == fileOutArg || i == errArg){
			args[i] = NULL;
			return;
		}
	}
	return;
}



//Sends SIGTSTP to everyone in process group
void sig_tstp(int signum){
	signal(SIGTSTP,SIG_DFL); 					//Tells job to handle signal as default, since it would cause recursion
	kill(-getpgrp(),SIGTSTP);
}

void sig_cont(int signum){
	signal(SIGTSTP,sig_tstp);
	signal(SIGCONT,SIG_DFL);
	kill(-getpgrp(),SIGCONT);
}

void sig_int(int signum){
	signal(SIGINT,SIG_DFL);
	kill(-getpgrp(),SIGINT);

}


int main(int argc,char** argv){

	signal(SIGCONT,sig_cont);
	signal(SIGTSTP,sig_tstp);
	signal(SIGINT,sig_int);

	char** arg1 = NULL;
	char** arg2 = NULL;
	int pipefd[2];


	int pipeArg = stringSearch(argv,"|");
	if(pipeArg >= 0){
		arg1 = malloc(sizeof(char*) * argc); //Over-allocating memory here, ideadlly find a way to fix that
		arg2 = malloc(sizeof(char*) * argc);
		splitPipeArgs(argv,arg1,arg2);
	}

	if(arg2){

		//Create the Pipe
		pipe(pipefd);

		int fileInArg = stringSearch(arg2,"<");
		int fileOutArg = stringSearch(arg2,">");
		int errArg = stringSearch(arg2,"2>");

		int inFd2 = 0;
		int outFd2 = 0;
		int errFd2 = 0;

		if(fileOutArg >=0){
			outFd2 = open(arg2[fileOutArg+1], O_RDWR | O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		}
		if(fileInArg >= 0){
			inFd2 = open(arg2[fileInArg+1],O_RDONLY);
			if(inFd2 == -1){
				exit(0);
			}
		}
		if(errArg >= 0){
			errFd2 = open(arg2[errArg+1],O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		}	
		remove_redirect_args(arg2,fileInArg,fileOutArg,errArg);

		//Do the same to arg1
		fileInArg = stringSearch(arg1,"<");
		fileOutArg = stringSearch(arg1,">");
		errArg = stringSearch(arg1,"2>");

		int inFd1 = 0;
		int outFd1 = 0;
		int errFd1 = 0;

		if(fileOutArg >=0){
			outFd1 = open(arg1[fileOutArg+1], O_RDWR | O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		}
		if(fileInArg >= 0){
			inFd1 = open(arg1[fileInArg+1],O_RDONLY);
			if(inFd1 == -1){
				exit(0);
			}
		}
		if(errArg >= 0){
			errFd1 = open(arg1[errArg+1], O_RDWR | O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		}	


		remove_redirect_args(arg1,fileInArg,fileOutArg,errArg);
		//Start children

		pid_t child1 = fork();
		if(child1 == 0){
			close(pipefd[0]);		
			dup2(pipefd[1],STDOUT_FILENO);		
			if(inFd1 && inFd1 != -1){
				dup2(inFd1,STDIN_FILENO);
			}
			if(outFd1){
				dup2(outFd1,STDOUT_FILENO);
			}
			if(errFd1){
				dup2(errFd1,STDERR_FILENO);
			}
			execvp(arg1[0],arg1);
		}

		pid_t child2 = fork();
		if(child2 == 0){

			close(pipefd[1]); 				// Close pipes write end
			dup2(pipefd[0],STDIN_FILENO); 	// Replace read STDIN with pipe-read

			if(inFd2 && inFd2 != -1){
				dup2(inFd2,STDIN_FILENO);
			}
			if(outFd2){
				dup2(outFd2,STDOUT_FILENO);
			}
			if(errFd2){
				dup2(errFd2,STDERR_FILENO);
			}
			execvp(arg2[0],arg2);
		}

		//Parents closes its ends of the pipe, and its own file descriptors
		close(pipefd[0]);
		close(pipefd[1]);

		//Is this right?
		waitpid(child1,NULL,0); //Blocks until the child finishes
		waitpid(child2,NULL,0); //Blocks until the child finishes

	}

	else{
		int fileInArg = stringSearch(argv,"<");
		int fileOutArg = stringSearch(argv,">");
		int errArg = stringSearch(argv,"2>");
		
		int inFd = 0;
		int outFd = 0;
		int errFd = 0;

		if(fileOutArg >=0){
			outFd = open(argv[fileOutArg+1], O_RDWR | O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		}
		if(fileInArg >= 0){
			inFd = open(argv[fileInArg+1],O_RDONLY);
			if(inFd == -1){
				exit(0);
			}
		}
		if(errArg >= 0){
			errFd = open(argv[errArg+1],O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		}	

		remove_redirect_args(argv,fileInArg,fileOutArg,errArg);
		
		pid_t child;
		child = fork();
		if(child == 0){
			if(outFd){
				dup2(outFd,STDOUT_FILENO); //replace stdout with file descritpr
			}
			if(inFd && inFd != -1){
				dup2(inFd,STDIN_FILENO); //replace stdin with file desrciptor
			}
			if(errFd){
				dup2(errFd,STDERR_FILENO); //replace stderr with file descriptor
			}

			execvp(argv[0],argv);
		}
		pid_t exit;
		waitpid(child,NULL,0); //Blocks until the child finishes

	}

	return 0;
}