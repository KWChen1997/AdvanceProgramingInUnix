#include "elftool.h"
#include "hw4.h"

static char child_state;
static int child_status;
static char filename[1024];
static char scriptname[1024];
static pid_t child;
static Elf64_Ehdr *ehdr;
static struct bp* bp_list;
static int bp_cap;
static int bp_len;
static unsigned long long dumpaddr;
static unsigned long long dismaddr;
static char fdmp;
static char fdism;
static unsigned long long tsbegin;
static unsigned long long tsend;
static int prog_fd;
static char *elf_ptr;

void errquit(const char *msg) {
	perror(msg);
	exit(-1);
}

void prompt(){
	fprintf(stdout,"sdb> ");
}

unsigned char checkTerm(){
	if(WIFEXITED(child_status)){
		fprintf(stdout,"** child process %d terminated normally (code %d)\n", child, WEXITSTATUS(child_status));
		update_st(CHILD_LOADED);
		return 1;
	}
	return 0;
}

int findbp(unsigned long long addr){
	int i = 0;
	for(i = 0; i < bp_len; i++){
		if(bp_list[i].valid && addr == bp_list[i].addr)
			return i;
	}
	return -1;
}

void help(){
	fprintf(stdout,
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
		fprintf(stdout,"** program \'%s\' is already loaded. entry point 0x%lx\n", filename, ehdr->e_entry);
		return -1;
	}
	// check file exist
	if(access(filepath, F_OK) != 0){
		fprintf(stdout,"** %s\n",strerror(errno));
		return 0;
	}

	// save the name of the loaded program
	strcpy(filename,filepath);
	struct stat st;
	stat(filename,&st);

	// load elf
	Elf64_Shdr *shdr;

	prog_fd = open(filename, O_RDONLY);
	elf_ptr = mmap(0,st.st_size,PROT_READ,MAP_PRIVATE,prog_fd,0);

	ehdr = (Elf64_Ehdr*)elf_ptr;
	shdr = (Elf64_Shdr*)(elf_ptr + ehdr->e_shoff);
	
	int i = 0;
	Elf64_Shdr *sh_strtab = &shdr[ehdr->e_shstrndx];
	char *sh_strtab_p = elf_ptr + sh_strtab->sh_offset;
	for(i = 0; i < ehdr->e_shnum; i++){
		if(strcmp(sh_strtab_p + shdr[i].sh_name,".text") == 0){
			tsbegin = shdr[i].sh_addr;
			tsend = tsbegin + shdr[i].sh_size;
			fprintf(stdout,"** '.text' section @ 0x%llx - 0x%llx\n",tsbegin,tsend);
			break;
		}
	}
	
	fprintf(stdout,"** program \'%s\' loaded. entry point 0x%lx\n", filename, ehdr->e_entry);
	update_st(CHILD_LOADED);
	return 0;
}

void start(){
	if(!is_state(CHILD_LOADED)){
		fprintf(stdout,"** no program is loaded\n");
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
		fprintf(stdout,"** pid %d\n",child); 
		update_st(CHILD_RUNNING);
		assert(WIFSTOPPED(child_status));
		ptrace(PTRACE_SETOPTIONS,child,0,PTRACE_O_EXITKILL);
	}
	return;
}

void run(){
	if(!is_state(CHILD_ANY)){
		fprintf(stdout,"** No program is loaded or running\n");
		return;
	}

	if(is_state(CHILD_RUNNING)){
		fprintf(stdout,"** program %s is already running\n", filename);
	}
	if(is_state(CHILD_LOADED)){
		start();
	}
	
	cont();

	return;
}

void checkbp(){
	struct user_regs_struct regs;
	int idx;
	unsigned long code;
	ptrace(PTRACE_GETREGS,child,0,&regs);
	regs.rip--;
	idx = findbp(regs.rip);
	if(idx < 0){
		return;
	}
	code = ptrace(PTRACE_PEEKTEXT,child,bp_list[idx].addr,0);
	code = (code & 0xffffffffffffff00) | (bp_list[idx].code & 0x00000000000000ff);
	ptrace(PTRACE_POKETEXT,child,bp_list[idx].addr,code);
	ptrace(PTRACE_SETREGS,child,0,&regs);
	return;
}

void cont(){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** process is not running\n");
		return;
	}

	struct user_regs_struct regs;
	unsigned long code;
	int idx;
	ptrace(PTRACE_GETREGS,child,0,&regs);
	if((idx = findbp(regs.rip)) >= 0){
		ptrace(PTRACE_SINGLESTEP,child,0,0);
		waitpid(child,&child_status,0);
		code = ptrace(PTRACE_PEEKTEXT,child,bp_list[idx].addr,0);
		bp_list[idx].code = code;
		ptrace(PTRACE_POKETEXT,child,bp_list[idx].addr,(code & 0xffffffffffffff00) | 0xcc);
	}
	ptrace(PTRACE_CONT,child,0,0);
	waitpid(child,&child_status,0);
	

	if(!checkTerm()){
		ptrace(PTRACE_GETREGS,child,0,&regs);
		regs.rip--;
		fprintf(stdout,"** breakpoint @\t%llx:\n",regs.rip);
		ptrace(PTRACE_SETREGS,child,0,&regs);
	}

	return;
}

void vmmap(){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** process is not running\n");
		return;
	}
	
	char proc_map[1024] = "";
	sprintf(proc_map,"/proc/%d/maps",child);
	FILE *pm = fopen(proc_map,"r");
	if(pm == NULL){
		fprintf(stdout,"** process is not running\n");
		return;
	}
	char *buf;
	size_t bufsize = 0;
	char *token;
	while(getline(&buf,&bufsize,pm) > 0){
		int i = 0;
		token = strtok(buf," ");
		while(token != NULL){
			if(i != 2 && i != 3){
				fprintf(stdout,"%s ",token);
			}
			token = strtok(NULL," ");
			i++;
		}
		fprintf(stdout,"\n");
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
		fprintf(stdout,"** No running process\n");
		return;
	}

	if(WIFSTOPPED(child_status)){
		struct user_regs_struct regs;
		ptrace(PTRACE_GETREGS,child,0,&regs);
		printRegs(regs);
	}
}

void getreg(const char* regstr){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No running process\n");
		return;
	}
	struct user_regs_struct regs;
	unsigned long long reg;
	ptrace(PTRACE_GETREGS,child,0,&regs);
	if(strcmp(regstr,"r15") == 0) reg = regs.r15;
	else if(strcmp(regstr,"r14") == 0) reg = regs.r14;
	else if(strcmp(regstr,"r13") == 0) reg = regs.r13;
	else if(strcmp(regstr,"r12") == 0) reg = regs.r12;
	else if(strcmp(regstr,"rbp") == 0) reg = regs.rbp;
	else if(strcmp(regstr,"rbx") == 0) reg = regs.rbx;
	else if(strcmp(regstr,"r11") == 0) reg = regs.r11;
	else if(strcmp(regstr,"r10") == 0) reg = regs.r10;
	else if(strcmp(regstr,"r9") == 0) reg = regs.r9;
	else if(strcmp(regstr,"r8") == 0) reg = regs.r8;
	else if(strcmp(regstr,"rax") == 0) reg = regs.rax;
	else if(strcmp(regstr,"rcx") == 0) reg = regs.rbx;
	else if(strcmp(regstr,"rdx") == 0) reg = regs.rdx;
	else if(strcmp(regstr,"rsi") == 0) reg = regs.rsi;
	else if(strcmp(regstr,"rdi") == 0) reg = regs.rdi;
	else if(strcmp(regstr,"orig_rax") == 0) reg = regs.orig_rax;
	else if(strcmp(regstr,"rip") == 0) reg = regs.rip;
	else if(strcmp(regstr,"cs") == 0) reg = regs.cs;
	else if(strcmp(regstr,"eflags") == 0) reg = regs.eflags;
	else if(strcmp(regstr,"rsp") == 0) reg = regs.rsp;
	else if(strcmp(regstr,"ss") == 0) reg = regs.ss;
	else if(strcmp(regstr,"fs_base") == 0) reg = regs.fs_base;
	else if(strcmp(regstr,"gs_base") == 0) reg = regs.gs_base;
	else if(strcmp(regstr,"ds") == 0) reg = regs.ds;
	else if(strcmp(regstr,"es") == 0) reg = regs.es;
	else if(strcmp(regstr,"fs") == 0) reg = regs.fs;
	else if(strcmp(regstr,"gs") == 0) reg = regs.gs;
	else{
		fprintf(stdout,"** No register named %s\n", regstr);
		return;
	}
	fprintf(stdout,"%s = %llu (0x%llx)\n", regstr, reg, reg);
	return;
}

void si(){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No running process\n");
		return;
	}
	
	struct user_regs_struct regs;
	int idx;
	unsigned long code;
	ptrace(PTRACE_GETREGS,child,0,&regs);
	idx = findbp(regs.rip);
	code = ptrace(PTRACE_PEEKTEXT,child,bp_list[idx].addr,0);
	if((code & 0x00000000000000ff) == 0xcc){
		code = (code & 0xffffffffffffff00) | (bp_list[idx].code & 0x00000000000000ff);
		ptrace(PTRACE_POKETEXT,child,regs.rip,code);
		fprintf(stdout,"** breakpoint @\t%llx:\n",regs.rip);
		return;
	}

	ptrace(PTRACE_SINGLESTEP,child,0,0);
	waitpid(child,&child_status,0);

	if(!checkTerm()){
		code = ptrace(PTRACE_PEEKTEXT,child,bp_list[idx].addr,0);
		bp_list[idx].code = code;
		ptrace(PTRACE_POKETEXT,child,bp_list[idx].addr,(code & 0xffffffffffffff00) | 0xcc);
	}

	return;
}

void setreg(const char* regstr, const char* valstr){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No running process\n");
		return;
	}
	char *Eptr;
	unsigned long long val = strtoull(valstr,&Eptr,0);

	if(*Eptr){
		fprintf(stdout,"** Wrong integer format\n");
		return;
	}
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS,child,0,&regs);
	if(strcmp(regstr,"r15") == 0) regs.r15 = val;
	else if(strcmp(regstr,"r14") == 0) regs.r14 = val;
	else if(strcmp(regstr,"r13") == 0) regs.r13 = val;
	else if(strcmp(regstr,"r12") == 0) regs.r12 = val;
	else if(strcmp(regstr,"rbp") == 0) regs.rbp = val;
	else if(strcmp(regstr,"rbx") == 0) regs.rbx = val;
	else if(strcmp(regstr,"r11") == 0) regs.r11 = val;
	else if(strcmp(regstr,"r10") == 0) regs.r10 = val;
	else if(strcmp(regstr,"r9") == 0) regs.r9 = val;
	else if(strcmp(regstr,"r8") == 0) regs.r8 = val;
	else if(strcmp(regstr,"rax") == 0) regs.rax = val;
	else if(strcmp(regstr,"rcx") == 0) regs.rbx = val;
	else if(strcmp(regstr,"rdx") == 0) regs.rdx = val;
	else if(strcmp(regstr,"rsi") == 0) regs.rsi = val;
	else if(strcmp(regstr,"rdi") == 0) regs.rdi = val;
	else if(strcmp(regstr,"orig_rax") == 0) regs.orig_rax = val;
	else if(strcmp(regstr,"rip") == 0) regs.rip = val;
	else if(strcmp(regstr,"cs") == 0) regs.cs = val;
	else if(strcmp(regstr,"eflags") == 0) regs.eflags = val;
	else if(strcmp(regstr,"rsp") == 0) regs.rsp = val;
	else if(strcmp(regstr,"ss") == 0) regs.ss = val;
	else if(strcmp(regstr,"fs_base") == 0) regs.fs_base = val;
	else if(strcmp(regstr,"gs_base") == 0) regs.gs_base = val;
	else if(strcmp(regstr,"ds") == 0) regs.ds = val;
	else if(strcmp(regstr,"es") == 0) regs.es = val;
	else if(strcmp(regstr,"fs") == 0) regs.fs = val;
	else if(strcmp(regstr,"gs") == 0) regs.gs = val;
	else{
		fprintf(stdout,"** No register named %s\n", regstr);
		return;
	}

	ptrace(PTRACE_SETREGS,child,0,&regs);
	return;
}

void list(){
	if(!is_state(CHILD_ANY)){
		fprintf(stdout,"** No program is loaded\n");
		return;
	}
	int i = 0;
	for(i = 0; i < bp_len; i++){
		if(bp_list[i].valid == 0)
			continue;
		fprintf(stdout,"%d: 0x%llx\n", i, bp_list[i].addr);
	}
	return;
}

void breakpoint(const char* addrstr){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No process is running\n");
		return;
	}
	if(bp_cap == bp_len){
		struct bp *tmp;
		tmp = (struct bp*)malloc((bp_cap+BP_ADD)*sizeof(struct bp));
		memcpy(tmp,bp_list,bp_cap*sizeof(struct bp));
		free(bp_list);
		bp_list = tmp;
		bp_cap = bp_cap + BP_ADD;
	}
	unsigned long code;
	char *Eptr;
	unsigned long long addr = strtoull(addrstr,&Eptr,0);
	if(*Eptr){
		fprintf(stdout,"** Not a valid address\n");
	}
	code = ptrace(PTRACE_PEEKTEXT,child,addr,0);
	bp_list[bp_len].addr = addr;
	bp_list[bp_len].code = code;
	bp_list[bp_len].valid = 1;
	ptrace(PTRACE_POKETEXT,child,addr, (code & 0xffffffffffffff00) | 0xcc);
	bp_len++;
	return;
}

void del(int idx){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No process running\n");
		return;
	}

	unsigned long code;
	code = ptrace(PTRACE_PEEKTEXT,child,bp_list[idx].addr,0);
	code = (code & 0xffffffffffffff00) | (bp_list[idx].code & 0x00000000000000ff);
	ptrace(PTRACE_POKETEXT,child,bp_list[idx].addr,code);
	bp_list[idx].valid = 0;
	fprintf(stdout,"** delete breakpoint %d @ %lx\n",idx,code);
}

void dump(const char* addrstr){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No process running\n");
		return;
	}

	unsigned long long addr = 0;
	char *Eptr;
	if(addrstr != NULL){
		addr = strtoull(addrstr,&Eptr,0);
		if(*Eptr){
			fprintf(stdout,"** Wrong address format\n");
			return;
		}
	}
	else{
		addr = dumpaddr;
	}

	int i = 0;
	long code1;
	long code2;
	unsigned char *ptr1 = (unsigned char*) &code1;
	unsigned char *ptr2 = (unsigned char*) &code2;
	for(i = 0; i < 80; i+=16){
		code1 = ptrace(PTRACE_PEEKTEXT,child,addr,0);
		code2 = ptrace(PTRACE_PEEKTEXT,child,addr+8,0);
		fprintf(stdout,"0x%llx: %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x  %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x",
				addr,ptr1[0],ptr1[1],ptr1[2],ptr1[3],ptr1[4],ptr1[5],ptr1[6],ptr1[7]
				,ptr2[0],ptr2[1],ptr2[2],ptr2[3],ptr2[4],ptr2[5],ptr2[6],ptr2[7]);
		int j = 0;
		for(j = 0; j < 8; j++){
			if(!isprint(ptr1[j])) ptr1[j] = '.';
			if(!isprint(ptr2[j])) ptr2[j] = '.';
		}
		fprintf(stdout," |%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c|\n"
				,ptr1[0],ptr1[1],ptr1[2],ptr1[3],ptr1[4],ptr1[5],ptr1[6],ptr1[7]
				,ptr2[0],ptr2[1],ptr2[2],ptr2[3],ptr2[4],ptr2[5],ptr2[6],ptr2[7]);
		addr += 16;
	}	
	dumpaddr = addr;
	fdmp = 0;
}

void disasm(const char* addrstr){
	if(!is_state(CHILD_RUNNING)){
		fprintf(stdout,"** No process is running\n");
		return;
	}

	unsigned char *codebyte;
	long code;

	codebyte = &code;

	char *Eptr;
	unsigned long long addr;
	if(addrstr){
		addr = strtoull(addrstr,&Eptr,0);
		if(*Eptr){
			fprintf(stdout,"** Wrong address format\n");
			return;
		}
	}
	else {
		addr = dismaddr;
	}

	if(addr < tsbegin || addr >= tsend){
		fprintf(stdout,"** Address %llx out of .text segment\n",addr);
		return;
	}

	csh handle;
	cs_insn *insn;
	int ins_l = 10;
	size_t count;

	if(cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
			return;

	while(ins_l){
		if(addr < tsbegin || addr >= tsend){
			break;
		}
		int bp_idx = -1;
		if((bp_idx = findbp(addr)) >= 0 ){
			code = bp_list[bp_idx].code;
		}
		else{
			code = ptrace(PTRACE_PEEKTEXT,child,addr,0);
		}

		count = cs_disasm(handle, codebyte, 8, addr,0, &insn);

		if (count > 0) {
			fprintf(stdout,"%lx:",insn[0].address);
			int j = 0;
			for(j = 0; j < insn[0].size; j++){
				fprintf(stdout," %2.2x",codebyte[j]);
			}
			while(j++ < 5){
				fprintf(stdout,"   ");
			}
			printf("\t\t%s\t%s\n", insn[0].mnemonic, insn[0].op_str);
			addr += insn[0].size;
			ins_l--;
	
			cs_free(insn, count);
		} 
		else if(ins_l == 10){
			fprintf(stdout,"** Wrong alignment\n");
			break;
		}
	}
	cs_close(&handle);
	if(ins_l == 0){
		dismaddr = addr;
		fdism = 0;
	}
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
	else if(strcmp(argv[0],"help") == 0 || strcmp(argv[0],"h") == 0){
		help();
	}
	else if(strcmp(argv[0],"load") == 0){
		if(argc < 2){
			fprintf(stdout,"** - load {path/to/a/program}: load a program\n");
			return 0;
		}
		load_prog(argv[1]);
	}
	else if(strcmp(argv[0],"start") == 0){
		start();
	}
	else if(strcmp(argv[0],"run") == 0 || strcmp(argv[0],"r") == 0){
		run();
	}
	else if(strcmp(argv[0],"cont") == 0 || strcmp(argv[0],"c") == 0){
		cont();
	}
	else if(strcmp(argv[0],"vmmap") == 0 || strcmp(argv[0],"m") == 0){
		vmmap();
	}
	else if(strcmp(argv[0],"getregs") == 0){
		getregs();
	}
	else if(strcmp(argv[0],"get") == 0 || strcmp(argv[0],"g") == 0){
		if(argc < 2){
			fprintf(stdout,"** - get reg: get a single value from a register\n");
			return 0;
		}
		getreg(argv[1]);
	}
	else if(strcmp(argv[0],"set") == 0 || strcmp(argv[0],"s") == 0){
		if(argc < 3){
			fprintf(stdout,"** - set reg val: get a single value to a register\n");
			return 0;
		}
		setreg(argv[1],argv[2]);
	}
	else if(strcmp(argv[0],"si") == 0){
		si();
	}
	else if(strcmp(argv[0],"list") == 0 || strcmp(argv[0],"l") == 0){
		list();
	}
	else if(strcmp(argv[0],"break") == 0 || strcmp(argv[0],"b") == 0){
		breakpoint(argv[1]);
	}
	else if(strcmp(argv[0],"delete") == 0){
		if(argc < 2){
			fprintf(stdout,"** - delete {break-point-id}: remove a break point\n");
			return 0;
		}
		int idx = atoi(argv[1]);
		del(idx);
	}
	else if(strcmp(argv[0],"dump") == 0 || strcmp(argv[0],"x") == 0){
		if(argc < 2){
			fprintf(stdout,"** No addr given\n");
		}
		else if(argc < 2){
			dump(NULL);
		}
		else{
			dump(argv[1]);
		}
	}
	else if(strcmp(argv[0],"disasm") == 0 || strcmp(argv[0],"d") == 0){
		if(argc < 2){
			fprintf(stdout,"** No addr given\n");
		}
		else if(argc < 2){
			disasm(NULL);
		}
		else{
			disasm(argv[1]);
		}
	}


	return argv[0] != NULL && (strcmp(argv[0],"exit") == 0 || strcmp(argv[0],"q") == 0);
}



int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	child_state = CHILD_NONE;
	dumpaddr = 0;
	dismaddr = 0;
	fdmp = 1;
	fdism = 1;
	memset(filename,0,1024);
	memset(scriptname,0,1024);
	bp_list = (struct bp*)malloc(BP_ADD*sizeof(struct bp));
	bp_cap = BP_ADD;
	bp_len = 0;
	memset(bp_list,0,BP_ADD*sizeof(struct bp));
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
				fprintf(stdout,"usage: ./hw4 [-s script] [program]\n");
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

	if(script_given){
		// check file exist
		if(access(scriptname, F_OK) != 0){
			fprintf(stdout,"** %s\n",strerror(errno));
			return 0;
		}

		FILE *fin;

		fin = fopen(scriptname,"r");
		while(getline(&cmdbuf,&bufsize,fin) >= 0){
			if(handle_cmd(cmdbuf))
				break;
		}
		fprintf(stdout,"Bye.\n");
	}

	while(script_given == 0){
		prompt();
		ret = getline(&cmdbuf,&bufsize,stdin);
		if(handle_cmd(cmdbuf))
			break;
	}
	free(cmdbuf);
	return 0;
}

