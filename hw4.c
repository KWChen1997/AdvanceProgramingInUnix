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
#include "func.h"

#define CHILD_NONE	0x1
#define CHILD_LOADED	0x2
#define CHILD_RUNNING	0x4
#define CHILD_ANY	(CHILD_LOADED | CHILD_RUNNING)

#define DELIMITER "\t\n "

#define REGS_FORMAT \
"RAX %llx\t\t\tRBX %llx\t\t\tRCX %llx\t\t\tRDX %llx\n\
R8  %llx\t\t\tR9  %llx\t\t\tR10 %llx\t\t\tR11 %llx\n\
R12 %llx\t\t\tR13 %llx\t\t\tR14 %llx\t\t\tR15 %llx\n\
RDI %llx\t\t\tRSI %llx\t\t\tRBP %llx\t\t\tRSP %llx\n\
RIP %llx\t\tFLAGS %016llx\n"

#define update_st(nst)\
	child_state = nst;
#define is_state(st)\
	(child_state & st)

static char child_state;
static int child_status;
static char filename[1024];
static char scriptname[1024];
static pid_t child;
static Elf64_Ehdr *elf_hdr;

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
	if(!is_state(CHILD_NONE)){
		fprintf(stderr,"** program \'%s\' is already loaded. entry point 0x%lx\n", filename, elf_hdr->e_entry);
		return -1;
	}
	// check file exist
	if(access(filename, F_OK) != 0){
		fprintf(stderr,"** %s\n",strerror(errno));
		return 0;
	}

	// save the name of the loaded program
	strcpy(filename,filepath);

	// load elf
	elf_hdr = (Elf64_Ehdr*)malloc(sizeof(Elf64_Ehdr));
	int fd = open(filename, O_RDONLY);
	ssize_t count = read(fd,elf_hdr,sizeof(Elf64_Ehdr));
	fprintf(stderr,"** program \'%s\' loaded. entry point 0x%lx\n", filename, elf_hdr->e_entry);
	update_st(CHILD_LOADED);
	return 0;
}

void start(){
	if(!is_state(CHILD_LOADED)){
		fprintf(stderr,"** no program is loaded\n");
		return;
	}

	if((child = fork()) < 0){
		errquit("start\n");
	}

	if(child == 0){
		ptrace(PTRACE_TRACEME,0,0,0);
		execvp(filename,NULL);
		errquit("failed to execute\n");
	}
	else{
		if(waitpid(child, &child_status, 0) < 0)	errquit("start: waitpid\n");
		fprintf(stderr,"** pid %d\n",child); 
		update_st(CHILD_RUNNING);
		assert(WIFSTOPPED(child_status));
		ptrace(PTRACE_SETOPTIONS,child,0,PTRACE_O_EXITKILL);
	}
	return;
}

void run(){
	if(!is_state(CHILD_ANY)){
		fprintf(stderr,"** No program is loaded or running\n");
		return;
	}

	if(is_state(CHILD_RUNNING)){
		fprintf(stderr,"** program %s is already running\n", filename);
	}
	if(is_state(CHILD_LOADED)){
		start();
	}
	
	cont();

	return;
}

void cont(){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stderr,"** process is not running\n");
		return;
	}

	ptrace(PTRACE_CONT,child,0,0);
	waitpid(child,&child_status,0);
	if(WIFEXITED(child_status)){
		fprintf(stderr,"** child process %d terminated normally (code %d)\n", child, WEXITSTATUS(child_status));
		update_st(CHILD_LOADED);
	}
	return;
}

void vmmap(){
	if(!is_state(CHILD_RUNNING)){
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

void printRegs(struct user_regs_struct regs){
	fprintf(stdout,REGS_FORMAT,	regs.rax, regs.rbx, regs.rcx, regs.rdx,
		       			regs.r8,  regs.r9,  regs.r10, regs.r11,
					regs.r12, regs.r13, regs.r14, regs.r15,
					regs.rdi, regs.rsi, regs.rbp, regs.rsp,
					regs.rip, regs.eflags);
	return;
}

void getregs(){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stderr,"** No running process\n");
		return;
	}

	if(WIFSTOPPED(child_status)){
		struct user_regs_struct regs;
		ptrace(PTRACE_GETREGS,child,0,&regs);
		printRegs(regs);
	}
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
		load_prog(argv[1]);
	}
	else if(strcmp(argv[0],"start") == 0){
		start();
	}
	else if(strcmp(argv[0],"run") == 0){
		run();
	}
	else if(strcmp(argv[0],"cont") == 0){
		cont();
	}
	else if(strcmp(argv[0],"vmmap") == 0){
		vmmap();
	}
	else if(strcmp(argv[0],"getregs") == 0){
		getregs();
	}

	return argv[0] != NULL && (strcmp(argv[0],"exit") == 0 || strcmp(argv[0],"q") == 0);
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

