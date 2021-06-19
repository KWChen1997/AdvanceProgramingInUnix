CC = gcc
CFLAGS = -Wall -g
OBJS = hw4.o
TARGET = hw4

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm $(TARGET) *.o

# load.o: load.c load.h elftool.h
#	$(CC) $(CFLAGS) -o $@ -c $<
