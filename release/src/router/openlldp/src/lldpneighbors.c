/*******************************************************************
 *
 * OpenLLDP Neighbor 
 *
 * See LICENSE file for more info.
 * 
 * File: lldpneighbors.c
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#define NEIGHBORLIST_SIZE 512
#define DEBUG 0

int main(int argc, char *argv[]) {   
  char msg[NEIGHBORLIST_SIZE];
  char *buf            = NULL;
  char *tmp            = NULL;
  int s                = 0;
  unsigned int msgSize = 0;
  size_t bytes         = 0;
  int result           = 0;

  buf = calloc(1, NEIGHBORLIST_SIZE);

  memset(&msg[0], 0x0, NEIGHBORLIST_SIZE);

#ifndef WIN32
  struct sockaddr_un addr;

  s = socket(AF_UNIX, SOCK_STREAM, 0);

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, "/var/run/lldpd.sock");
  
  result = connect(s, (struct sockaddr *)&addr, sizeof(addr));

  if(result < 0) {
    printf("\n%s couldn't connect to the OpenLLDP transport socket.  Is lldpd running?\n", argv[0]);
    
    goto cleanup;
  }

  while((bytes = recv(s, msg, NEIGHBORLIST_SIZE, 0))) {
    
    if(bytes > 0) {
      
      tmp = calloc(1, msgSize + bytes + 1);

      if(buf != NULL) {
	memcpy(tmp, buf, msgSize);
	free(buf);
	buf = NULL;
      }
      
      memcpy(&tmp[msgSize], msg, bytes);
      msgSize += bytes;
      
      buf = tmp;
      tmp = NULL;
    } else {
      if(DEBUG) {
	printf("Error reading %d bytes: %d:%s\n", NEIGHBORLIST_SIZE, errno, strerror(errno));
      }
    }
    
    if(DEBUG) {
      printf("Read %d bytes.  Total size is now: %d\n", (int)bytes, msgSize);
      printf("Buffer is: 0x%08X and Temporary Buffer is 0x%08X.\n", (int)&buf, (int)&tmp);
    }
  }

  if(buf != NULL) {
    printf("%s\n", buf);
  }

  
 cleanup:
  if(buf != NULL)
    {
      free(buf);
      msgSize = 0;
      buf = NULL;
    }

  close(s);
#endif

  return 0;
}

