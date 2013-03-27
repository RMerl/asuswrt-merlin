#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

int child(void* arg)
{
	write(1, "clone\n", 6);
	return 0;
}

int main(int argc, char *argv[])
{
	char stack[4096];
	clone(child, stack+4000, CLONE_VM|CLONE_FS|CLONE_FILES, NULL);
	write(1, "original\n", 9);
	exit(0);
}
