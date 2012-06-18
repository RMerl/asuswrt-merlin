#include "includes.h"

extern bool AllowDebugChange;

static int ServerFD= -1;
static bool RootPort = false;

/****************************************************************************
 Open the socket communication.
**************************************************************************/

static bool open_mysockets(void)
{
	struct sockaddr_storage ss;
	const char *sock_addr = lp_socket_address();
	
	if (!interpret_string_addr(&ss, sock_addr,
				AI_NUMERICHOST|AI_PASSIVE)) {
		DEBUG(0,("open_mysockets: unable to get socket address "
					"from string %s", sock_addr));
		return false;
	}
	
	ServerFD = open_socket_in( SOCK_DGRAM,
				(RootPort ? 137 : 0),
				(RootPort ?   0 : 3),
				&ss, true );
	
	if (ServerFD == -1) {
		return false;
	}
	
	set_socket_options( ServerFD, "SO_BROADCAST" );
	
	DEBUG(3, ("Socket opened.\n"));
	return true;
}

/****************************************************************************
 Do a node status query.
****************************************************************************/
static bool do_mynode_status(int fd,
		const char *name,
		int type,
		struct sockaddr_storage *pss,
		char** hostname)
{
	struct nmb_name nname;
	int count, i, j;
	NODE_STATUS_STRUCT *status;
	struct node_status_extra extra;
	char addr[INET6_ADDRSTRLEN];
	bool result = false;
	
	print_sockaddr(addr, sizeof(addr), pss);	
	make_nmb_name(&nname, name, type);
	status = node_status_query(fd, &nname, pss, &count, &extra, 500);
	if (status) {
		if(count>0){
			int i;
			for(i=0; i<count; i++){
				//fprintf(stderr, "%s#%02x: flags = 0x%02x\n", status[i].name, status[i].type, status[i].flags);			   	
				if(status[i].type==0x20){
					*hostname = SMB_STRDUP(status[i].name);
					//fprintf(stderr, "Looking up status of [%s], host name = [%s], [%02x]\n\n", addr,*hostname, status[i].type);
					result = true;
					break;
				}
			}
			//*hostname = SMB_STRDUP(status[0].name);			
			//fprintf(stderr, "Looking up status of [%s], host name = [%s] %d,  %x\n\n", addr,*hostname, count, status[0].type);			
			//result = true;
		}
		SAFE_FREE(status);
	}

	return result;
}

static bool query_hostname_byip(const char *lookup_ip, char** hostname)
{	
	load_interfaces();
	
	if (!open_mysockets()) {
		return false;
	}		
	
	bool res;
	char *p;
	unsigned int lookup_type = 0x00;
	struct in_addr ip;
	static bool lookup_by_ip = True;

	p = SMB_STRDUP(lookup_ip);	
	
	struct sockaddr_storage ss;
	ip = interpret_addr2(p);
	in_addr_to_sockaddr_storage(&ss, ip);
	fstrcpy((char*)p,"*");

	//fprintf(stderr, "enter query_hostname_byip %s\n", lookup_ip);
	res = do_mynode_status(ServerFD, p, lookup_type, &ss, hostname);
	//fprintf(stderr, "leave query_hostname_byip %s\n", lookup_ip);

	close(ServerFD);

	SAFE_FREE(p);

	return res;
	
	/*
	struct sockaddr_storage ss;
	ip = interpret_addr2(lookup_ip);
	in_addr_to_sockaddr_storage(&ss, ip);
	fstrcpy((char*)lookup_ip,"*");
	return do_mynode_status(ServerFD, lookup_ip, lookup_type, &ss, hostname);	
	*/
}
