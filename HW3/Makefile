CC		= gcc
CXX		= g++
ASM64		= yasm -f elf64 -DYASM -D__x86_64__ -DPIC

LIB_CFLAGS	= -g -Wall -fno-stack-protector -fPIC -nostdlib
CFLAGS		= -g -Wall -fno-stack-protector -nostdlib -I. -I.. -DUSEMINI

LIBS		= libmini.so start.o

all: $(LIBS)

# default is x86_64
%.o: %.asm
	$(ASM64) $< -o $@

%: %.asm
	$(ASM64) $< -o $@.o
	ld -m elf_x86_64 -o $@ $@.o
	
%: %.c
	gcc -c -g -Wall -fno-stack-protector -nostdlib -I. -I.. -DUSEMINI $@.c
	ld -m elf_x86_64 --dynamic-linker /lib64/ld-linux-x86-64.so.2 -o $@ $@.o start.o -L. -L.. -lmini
	rm $@.o

%: %.cpp
	$(CXX) -o $@ $(CFLAGS) $<

%: %.cc
	$(CXX) -o $@ $(CFLAGS) $<

libmini.o: libmini.c
	$(CC) -c $(LIB_CFLAGS) $^

libmini.so: libmini64.o libmini.o
	ld -shared -o $@ $^

clean:
	rm -f a.out *.o $(LIBS) peda-*

