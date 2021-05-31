#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main() {
	alarm(3);
	pause();
	return 0;
}

