#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <capstone/capstone.h>

#define BP_ADD 1000

#define CHILD_NONE      0x1
#define CHILD_LOADED    0x2
#define CHILD_RUNNING   0x4
#define CHILD_ANY       (CHILD_LOADED | CHILD_RUNNING)

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
		
struct bp{
	unsigned long long addr;
	unsigned long code;
	unsigned char valid;
};

void errquit();
void prompt();
unsigned char checkTerm();
int load_prog(const char*);
void start();
void run();
void cont();
void vmmap();
void printRegs(struct user_regs_struct);
void getregs();
void getreg(const char*);
void si();
void setreg(const char*,const char*);
unsigned char handle_cmd(char*);
