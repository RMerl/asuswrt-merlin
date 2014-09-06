/*
 * m2m.h
 */

struct get_req_state {
    int             type;
    void           *info;
};

/*
 * values for type field in get_req_state 
 */
#define ALARM_GET_REQ	1
#define EVENT_GET_REQ	2

/*
 * the following define is used to document a routine or variable which
 * ** is not static to a module.
 */
#define Export

/*
 * values for EntryStatus 
 */
#define ENTRY_ACTIVE		1
#define ENTRY_NOTINSERVICE	2
#define ENTRY_NOTREADY		3
#define ENTRY_CREATEANDGO	4
#define ENTRY_CREATEANDWAIT	5
#define ENTRY_DESTROY		6

/*
 * maximum length for an OwnerString variable 
 */
#define MAX_OWNER_STR_LEN 128

/*
 * maximum length for a description field 
 */
#define MAX_DESCRIPTION_LEN 128

/*
 * defines for noting whether the incoming packet is unicast, broadcast,
 * ** or multicast
 */
#define PKT_UNICAST 0
#define PKT_BROADCAST 1
#define PKT_MULTICAST 2

/*
 * macro to compare two ethernet addresses.  addr1 is a pointer to a
 * ** struct ether_addr; addr2 is just a struct ether_addr.
 */
#define sameEtherAddr(addr1, addr2) \
	((*((short *)((addr1)->ether_addr_octet)) == \
					*((short *)((addr2).ether_addr_octet))) &&\
	 (*((short *)(((addr1)->ether_addr_octet) + 2)) == \
					*((short *)((((addr2).ether_addr_octet) + 2)))) &&\
	 (*((short *)(((addr1)->ether_addr_octet) + 4)) == \
					*((short *)((((addr2).ether_addr_octet) + 4)))))
