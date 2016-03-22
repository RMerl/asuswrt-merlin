#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <linux/ip.h>
#include <linux/icmp.h>

unsigned short in_cksum(unsigned short *, int);

int icmp_check(char *src_addr, char *dst_addr)
{
	struct iphdr* ip;
	struct iphdr* ip_reply;
	struct icmphdr* icmp;
	struct sockaddr_in connection;
	char* packet;
	int payload_size = 5;
	char* buffer;
	int sockfd;
	int optval;
	int addrlen;
	int siz;
	int ping_result;

	printf("Source address: %s\n", src_addr);
	printf("Destination address: %s\n", dst_addr);

	/* allocate all necessary memory */
	int packet_size = sizeof(struct iphdr) + sizeof(struct icmphdr) + payload_size ;
	packet = malloc(packet_size);
	if(packet == NULL){
		perror("malloc packet");
		return 0;
	}
	buffer = malloc(sizeof(struct iphdr) + sizeof(struct icmphdr));
	if(buffer == NULL){
		perror("malloc buffer");
		return 0;
	}

	memset(packet, 0, packet_size);
	ip = (struct iphdr*) packet;
	icmp = (struct icmphdr*) (packet + sizeof(struct iphdr));

	/* here the ip packet is set up */
	ip->ihl = 5;
	ip->version = 4;
	ip->tos = 0;
	ip->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
	ip->id = htons(0);
	ip->frag_off = 0;
	ip->ttl = 1;
	ip->protocol = IPPROTO_ICMP;
	ip->saddr = inet_addr(src_addr);
	ip->daddr = inet_addr(dst_addr);
	ip->check = in_cksum((unsigned short *)ip, sizeof(struct iphdr));

	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1){
		free(packet);
		free(buffer);
		perror("socket");
		exit(EXIT_FAILURE);
		return 0;
	}

	connection.sin_family = AF_INET;
	connection.sin_addr.s_addr = inet_addr(dst_addr);
	memset(&connection.sin_zero, 0, sizeof (connection.sin_zero));

	/* 
	 *  IP_HDRINCL must be set on the socket so that
	 *  the kernel does not attempt to automatically add
	 *  a default ip header to the packet
	 */
	optval = 1;
	if(setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval)) == -1)
	{
        perror("setsockopt");
        return (0);
	}

	if ( fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0 )
	{
		perror("Request nonblocking I/O");
		return 1;
	}

	/*
	 * here the icmp packet is created
	 * also the ip checksum is generated
	 */
	icmp->type = ICMP_ECHO;
	icmp->code = 0;
	icmp->un.echo.id = random();
	icmp->un.echo.sequence = random();
	icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));

	/*
	 *  now the packet is sent
	 */

	// sendto(sockfd, packet, ip->tot_len, 0, (struct sockaddr *)&connection, sizeof(struct sockaddr));
	sendto(sockfd, packet, ip->tot_len, 0, (struct sockaddr *)&connection, sizeof(connection));
	printf("Sent %d byte packet to %s\n", sizeof(packet), dst_addr);

	int rc;
	struct timeval timeout = {3, 0}; //wait max 3 seconds for a reply
	fd_set read_set;

	memset(&read_set, 0, sizeof read_set);
	FD_ZERO(&read_set);
	FD_SET(sockfd, &read_set);

	rc = select(sockfd +1, &read_set, NULL, NULL, &timeout);
	if(rc == 0){
		puts("Got no reply\n");
		ping_result = 0;
		return 0;
	}else if(rc < 0){
		perror("Select");
		ping_result = 0;
	}

	/*
	 *  now we listen for responses
	 */
	addrlen = sizeof(connection);
	if (( siz = recvfrom(sockfd, buffer, sizeof(struct iphdr) + sizeof(struct icmphdr), 0,
				(struct sockaddr *)&connection, (socklen_t *)&addrlen)) == -1){
		perror("recv");
		ping_result = 0;
	}else{
		printf("Received %d byte reply from %s:\n", siz , dst_addr);
		// ip_reply = (struct iphdr*) buffer;
		// printf("ID: %d\n", ntohs(ip_reply->id));
		// printf("TTL: %d\n", ip_reply->ttl);
		ping_result = 1;
	}

	free(packet);
	free(buffer);
	FD_ZERO(&read_set);
	close(sockfd);
	FD_CLR(sockfd, &read_set);

	if(ping_result == 0){
		return 0;
	}else{
		return 1;
	}
}

/*
 * in_cksum --
 * Checksum routine for Internet Protocol
 * family headers (C Version)
 */
unsigned short in_cksum(unsigned short *addr, int len)
{
	register int sum = 0;
	u_short answer = 0;
	register u_short *w = addr;
	register int nleft = len;
	/*
	* Our algorithm is simple, using a 32 bit accumulator (sum), we add
	* sequential 16 bit words to it, and at the end, fold back all the
	* carry bits from the top 16 bits into the lower 16 bits.
	*/
	while (nleft > 1)
	{
	  sum += *w++;
	  nleft -= 2;
	}
	/* mop up an odd byte, if necessary */
	if (nleft == 1)
	{
	  *(u_char *) (&answer) = *(u_char *) w;
	  sum += answer;
	}
	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);       /* add hi 16 to low 16 */
	sum += (sum >> 16);               /* add carry */
	answer = ~sum;              /* truncate to 16 bits */
	return (answer);
}

