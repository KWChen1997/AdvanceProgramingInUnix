#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include "elftool.h"

#define CHILD_NONE	0x0
#define CHILD_LOADED	0x1
#define CHILD_RUNNING	0x2
#define CHILD_ANY	CHILD_LOADED | CHILD_RUNNING

#define DELIMITER "\t\n "

#define update_st(nst)\
	child_state = nst;

static char child_state;
static char filename[1024];
static char scriptname[1024];
static pid_t child;

void errquit(const char *msg) {
	perror(msg);
	exit(-1);
}

void prompt(){
	fprintf(stderr,"sdb> ");
}

void help(){
	fprintf(stderr,
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

int load_prog(const char *filepath){
	// check file exist
	if(access(filepath, F_OK) != 0){
		fprintf(stderr,"** %s\n",strerror(errno));
		return 0;
	}
	// load elf
	Elf64_Ehdr *elf_hdr;
	elf_hdr = (Elf64_Ehdr*)malloc(sizeof(Elf64_Ehdr));
	int fd = open(filepath, O_RDONLY);
	ssize_t count = read(fd,elf_hdr,sizeof(Elf64_Ehdr));
	fprintf(stderr,"** program \'%s\' loaded. entry point 0x%lx\n", filepath, elf_hdr->e_entry);
	free(elf_hdr);
	update_st(CHILD_LOADED);
	return 0;
}

void start(const char* filepath){
	if(child_state < CHILD_LOADED){
		fprintf(stderr,"** no program is loaded\n");
		return;
	}
	if(strlen(filepath) == 0){
		fprintf(stderr,"** filepath is empty\n");
		return;
	}
	if((child = fork()) < 0){
		errquit("start\n");
	}

	if(child == 0){
		ptrace(PTRACE_TRACEME,0,0,0);
		execvp(filepath,NULL);
		errquit("failed to execute\n");
	}
	else{
		int status;
		if(waitpid(child, &status, 0) < 0)	errquit("start: waitpid\n");
		fprintf(stderr,"** pid %d\n",child); 
		update_st(CHILD_RUNNING);
		assert(WIFSTOPPED(status));
		ptrace(PTRACE_SETOPTIONS,child,0,PTRACE_O_EXITKILL);
	}
}

void cont(){
	if(child_state < CHILD_RUNNING){
		fprintf(stderr,"** process is not running\n");
		return;
	}

	int status;
	ptrace(PTRACE_CONT,child,0,0);
	waitpid(child,&status,0);
	return;
}

void vmmap(){
	if(child_state < CHILD_RUNNING){
		fprintf(stderr,"** process is not running\n");
		return;
	}
	
	char proc_map[1024] = "";
	sprintf(proc_map,"/proc/%d/maps",child);
	FILE *pm = fopen(proc_map,"r");
	if(pm == NULL){
		fprintf(stderr,"** process is not running\n");
		return;
	}
	char *buf;
	size_t bufsize = 0;
	while(getline(&buf,&bufsize,pm) > 0){
		fprintf(stderr,"%s",buf);
	}
	free(buf);
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
		if(argc < 2){
			fprintf(stderr,"- load {path/to/a/program}: load a program\n");
			return 0;
		}
		strcpy(filename,argv[1]);
		load_prog(argv[1]);
	}
	else if(strcmp(argv[0],"start") == 0){
		start(filename);
	}
	else if(strcmp(argv[0],"cont") == 0){
		cont();
	}
	else if(strcmp(argv[0],"vmmap") == 0){
		vmmap();
	}

	return argv[0] != NULL && (strcmp(argv[0],"quit") == 0 || strcmp(argv[0],"q") == 0);
}

int main(int argc, char *argv[]) {
	child_state = CHILD_NONE;
	memset(filename,0,1024);
	memset(scriptname,0,1024);
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
	}
	free(cmdbuf);
	return 0;
}

