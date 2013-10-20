#include <stdio.h>
#include <stdlib.h>
#include <push_log.h>

#define NOTIFYLOG_FILE "/tmp/notifylog.log"

int main(int argc, char *argv[]){
	int ret;

	if(argc != 1 && argc != 2){
		printf("Usage: push_notify [interval seconds]\n");
		return -1;
	}

	if(argc == 1)
		ret = getlogbyinterval(NOTIFYLOG_FILE, 0);
	else
		ret = getlogbyinterval(NOTIFYLOG_FILE, atoi(argv[1]));

	return ret;
}
