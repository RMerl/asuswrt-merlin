/*
 * arpping_thread .h
 */

#ifndef ARPPING_THREAD_H
#define ARPPING_THREAD_H

#define MAC_BCAST_ADDR  "\xff\xff\xff\xff\xff\xff"

#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>


//- Jerry add
typedef struct smb_srv_info_s {
	int id;
	buffer *ip;
	buffer *mac;
	buffer *name;
	struct smb_srv_info_s *prev, *next;
}smb_srv_info_t;
smb_srv_info_t *smb_srv_info_list;


#endif
