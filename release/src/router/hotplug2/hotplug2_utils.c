#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <linux/types.h>
#include <linux/netlink.h>

#include "hotplug2_utils.h"

/**
 * A trivial function that reads kernel seqnum from sysfs.
 *
 * Returns: Seqnum as read from sysfs
 */
inline event_seqnum_t get_kernel_seqnum() {
	FILE *fp;
	
	char filename[64];
	char seqnum[64];
	
	strcpy(filename, sysfs_seqnum_path);
	
	fp = fopen(filename, "r");
	if (fp == NULL)
		return 0;
	
	fread(seqnum, 1, 64, fp);
	fclose(fp);
	
	return strtoull(seqnum, NULL, 0);
}

/**
 * Opens a PF_NETLINK socket into the kernel, to read uevents.
 *
 * @1 Specifies type of socket (whether we bind or whether we connect) 
 *
 * Returns: Socket fd if succesful, -1 otherwise.
 */
inline int init_netlink_socket(int type) {
	int netlink_socket;
	struct sockaddr_nl snl;
	int buffersize = 16 * 1024 * 1024;
	
	memset(&snl, 0x00, sizeof(struct sockaddr_nl));
	snl.nl_family = AF_NETLINK;
	snl.nl_pid = getpid();
	snl.nl_groups = 1;
	netlink_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT); 
	if (netlink_socket == -1) {
		ERROR("opening netlink","Failed socket: %s.", strerror(errno));
		return -1;
	}
	
	/*
	 * We're trying to override buffer size. If we fail, we attempt to set a big buffer and pray.
	 */
	if (setsockopt(netlink_socket, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize))) {
		ERROR("opening netlink","Failed setsockopt: %s. (non-critical)", strerror(errno));
		
		/* Somewhat safe default. */
		buffersize = 106496;
		
		if (setsockopt(netlink_socket, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize))) {
			ERROR("opening netlink","Failed setsockopt: %s. (critical)", strerror(errno));
		}
	}
	
	/*
	 * hotplug2-dnode performs connect, while hotplug2 daemon binds
	 */
	switch (type) {
		case NETLINK_CONNECT:
			if (connect(netlink_socket, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl))) {
				ERROR("opening netlink","Failed connect: %s.", strerror(errno));
				close(netlink_socket);
				return -1;
			}
			
		default:
			if (bind(netlink_socket, (struct sockaddr *) &snl, sizeof(struct sockaddr_nl))) {
				ERROR("opening netlink","Failed bind: %s.", strerror(errno));
				close(netlink_socket);
				return -1;
			}
	}
	
	return netlink_socket;
}
