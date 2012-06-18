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
	{
		c = (char)rand()%256;
		if(isprint(c) || isspace(c))
		{
			printf("%c", c);
		}
	}
	return 0;
}
