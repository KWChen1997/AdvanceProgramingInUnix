#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef ELFTOOL
#define ELFTOOL
#define EI_NIDENT	16
#define ElfN_Addr	uint64_t
#define ElfN_Off	uint64_t

typedef struct {
	unsigned char	e_ident[EI_NIDENT];
	uint16_t	e_type;
	uint16_t      e_machine;
	uint32_t      e_version;
	ElfN_Addr     e_entry;
	ElfN_Off      e_phoff;
	ElfN_Off      e_shoff;
	uint32_t      e_flags;
	uint16_t      e_ehsize;
	uint16_t      e_phentsize;
	uint16_t      e_phnum;
	uint16_t      e_shentsize;
	uint16_t      e_shnum;
	uint16_t      e_shstrndx;
} Elf64_Ehdr;
#endif
