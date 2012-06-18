#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main()
{
	char c;
	srand(time(NULL) * getpid());

	while(1)
		printf("%c", (char)rand()%256);

	return 0;
}
