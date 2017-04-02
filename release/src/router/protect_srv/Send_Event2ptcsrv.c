#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libptcsrv.h>

int main(int argc, char *argv[]) {
	
	
	if(argc < 3) {
		printf("Parameter Num Error.\nUsage: %s \"Service Type\" \"Message\"\n", argv[0]);
		return 0;
	}
	
	SEND_PTCSRV_EVENT(strtol(argv[1], NULL, 10), argv[2], argv[3]);
	
	return 0;
}

