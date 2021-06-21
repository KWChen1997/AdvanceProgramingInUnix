CC = gcc
CXX = g++
CFLAGS = -Wall -g
CLIB = -lcapstone
OBJS = hw4.o
TARGET = hw4

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@ $(CLIB)

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(CLIB)

clean:
	rm $(TARGET) *.o

# load.o: load.c load.h elftool.h
#	$(CC) $(CFLAGS) -o $@ -c $<
