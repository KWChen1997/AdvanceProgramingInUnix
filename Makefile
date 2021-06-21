CC = gcc
CXX = g++
CFLAGS = -Wall -g
OBJS = hw4.o
TARGET = hw4

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CXX) $(CFLAGS) $^ -o $@

clean:
	rm $(TARGET) *.o

# load.o: load.c load.h elftool.h
#	$(CC) $(CFLAGS) -o $@ -c $<
