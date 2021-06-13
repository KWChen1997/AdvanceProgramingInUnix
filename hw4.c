#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include "elftool.h"
#include "load.h"

#define DELIMITER "\t\n "

static char filename[1024];
static char scriptname[1024];

void errquit(const char *msg) {
	perror(msg);
	exit(-1);
}

void prompt(){
	printf("sdb> ");
}

void help(){
	printf(
"- break {instruction-address}: add a break point\n\
- cont: continue execution\n\
- delete {break-point-id}: remove a break point\n\
- disasm addr: disassemble instructions in a file or a memory region\n\
- dump addr [length]: dump memory content\n\
- exit: terminate the debugger\n\
- get reg: get a single value from a register\n\
- getregs: show registers\n\
- help: show this message\n\
- list: list break points\n\
- load {path/to/a/program}: load a program\n\
- run: run the program\n\
- vmmap: show memory layout\n\
- set reg val: get a single value to a register\n\
- si: step into instruction\n\
- start: start the program and stop at the first instruction\n");
	return;
}

unsigned char handle_cmd(char *cmdbuf){
	int argc = 0;
	char *argv[3] = {};
	char *token;
	for(token = strtok(cmdbuf,DELIMITER); token != NULL && argc < 3; token = strtok(NULL,DELIMITER)){
		argv[argc++] = token;
	}
	
	if(argv[0] == NULL){
		return 0;
	}
	else if(strcmp(argv[0],"help") == 0){
		help();
	}
	else if(strcmp(argv[0],"load") == 0){
		load_prog(argv[1]);
	}

	return argv[0] != NULL && (strcmp(argv[0],"quit") == 0 || strcmp(argv[0],"q") == 0);
}

int main(int argc, char *argv[]) {
	int opt;
	unsigned char file_given = 0;
	unsigned char script_given = 0;
	
	while( (opt = getopt(argc, argv, "s:") ) != -1 ){
		switch(opt){
			case 's':
				strncpy(scriptname,optarg, sizeof(scriptname));
				script_given = 1;
				break;
			default:
				fprintf(stderr,"usage: ./hw4 [-s script] [program]\n");
				exit(-1);
				break;
		}
	}

	if(optind < argc){	// program name is given
		strncpy(filename,argv[optind],sizeof(filename));
		file_given = 1;
	}

	// printf("filename: %s\nscriptname: %s\n",filename,scriptname);
	
	char *cmdbuf = NULL;
	unsigned long bufsize = 0;
	int ret;
	
	if(file_given)
		load_prog(filename);

	while(1){
		prompt();
		ret = getline(&cmdbuf,&bufsize,stdin);
		if(handle_cmd(cmdbuf))
			break;
		free(cmdbuf);
		cmdbuf = NULL;
	}
	return 0;
}

