#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "rc.h"
#include "pc.h"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define PC_BLOCK_PID_FILE "/var/run/pc_block.pid"
#define DFT_SERV_PORT "18099"
#define POLL_INTERVAL_SEC 3
#define MAX_CONN 100
#define MAX_LEN 2048

int serv_socket, max_fd;
fd_set allset, rdset;
int client[MAX_CONN];
int fd_idx, fd_cur;

extern pc_s *get_all_pc_list(pc_s **pc_list);
extern pc_s *match_enabled_pc_list(pc_s *pc_list, pc_s **target_list, int enabled);
extern void free_pc_list(pc_s **target_list);
