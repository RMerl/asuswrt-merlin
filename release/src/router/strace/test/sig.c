#include <stdlib.h>
#include <signal.h>

void interrupt()
{
	write(2, "xyzzy\n", 6);
}

int main(int argc, char *argv[])
{
	char buf[1024];

	signal(SIGINT, interrupt);
	read(0, buf, 1024);
	write(2, "qwerty\n", 7);

	return 0;
}
