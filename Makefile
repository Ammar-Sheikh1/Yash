#COMPILER
CXX = gcc
#FLAGS
#N/A

all: yash job

yash: yash.c
	$(CXX) -g -o yash yash.c -lreadline

job: job.c
	$(CXX) -g -o job job.c	

