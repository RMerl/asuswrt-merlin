#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	if (fork() == 0) {
		write(1, "child\n", 6);
	} else {
		wait(0);
		write(1, "parent\n", 7);
	}

	exit(0);
}
