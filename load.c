#include "elftool.h"
#include "load.h"

int load_prog(const char *filepath){
	Elf64_Ehdr *elf_hdr;
	elf_hdr = (Elf64_Ehdr*)malloc(sizeof(Elf64_Ehdr));
	int fd = open(filepath, O_RDONLY);
	ssize_t count = read(fd,elf_hdr,sizeof(Elf64_Ehdr));
	printf("** program \'%s\' loaded. entry point 0x%lx\n", filepath, elf_hdr->e_entry);
	free(elf_hdr);
	return 0;
}	
