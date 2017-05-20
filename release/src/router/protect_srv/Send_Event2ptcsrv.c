#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libptcsrv.h>

int main(int argc, char *argv[]) {
	
	
	if(argc < 4) {
		printf("Parameter Num Error.\nUsage: %s \"Service Type\" \"Login State\" \"IPaddr\" \"Message\"\n", argv[0]);
		return 0;
	}
	
	SEND_PTCSRV_EVENT(strtol(argv[1], NULL, 10), strtol(argv[2], NULL, 10), argv[3], argv[4]);
	
	return 0;
}

