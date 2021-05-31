#include "libmini.h"
int main() {
	sigset_t s;
	sigemptyset(&s);
	alarm(8);
	sleep(2);
	int i = alarm(3);
	char tmp[] = "0 seconds left\n";
	tmp[0] = (char)i + '0';
	write(1,tmp,sizeof(tmp));
	pause();
	return 0;
}

