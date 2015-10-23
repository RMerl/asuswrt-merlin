#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <stdio.h>
//#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <bcmnvram.h>
#include "networkmap.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <asm/byteorder.h>
#include "iboxcom.h"
#include "../shared/shutils.h"

extern int scan_count;//from networkmap;
extern FILE *fp_upnp;
extern FILE *fp_smb;

static char *strip_chars(char *str, char *reject);
void interrupt();
int create_ssdp_socket_ctrlpt(char *ifname, ushort port);
int create_http_socket_ctrlpt(char *host, ushort destport);
int create_msearch_ctrlpt(int Mx);
int process_device_response(char *msg);
void store_description(char *msg);

int ssdp_fd;
int global_exit = FALSE;
ushort return_value = FALSE;
struct device_info description;

UCHAR emptyname[2] = {0x00, 0x00};
char NetBIOS_name[16]={0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
		       0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20}; //for SMB NBSS request
char SMB_OS[10];
char SMB_PriDomain[16];

void fixstr(const char *buf)
{
        char *p;
        int i;

        p = (char *) buf;

        for (i = 0; i < 16; i++)
        {
                if (*p < 0x21)
                        *p = 0x0;
                if (i == 15)
                        *p = '\0';
                p++;
        }

        return;
}

/***** Http Server detect function *****/
int SendHttpReq(unsigned char *des_ip)
{
        int getlen, sock_http;
        struct sockaddr_in dest;
        char buffer[512];
        char *dest_ip_ptr;
        struct timeval tv1, tv2, timeout={1, 0};

        /* create socket */
        sock_http = socket(AF_INET, SOCK_STREAM, 0);

        /* initialize value in dest */
        bzero(&dest, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(HTTP_PORT);
        memcpy(&dest.sin_addr, des_ip, 4);
        dest_ip_ptr = inet_ntoa(dest.sin_addr);

	setsockopt(sock_http, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));//set connect timeout
	setsockopt(sock_http, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));//set receive timeout

        /* Connecting to server */
        if( connect(sock_http, (struct sockaddr*)&dest, sizeof(dest))== -1 )
        {
		#ifdef DEBUG
                perror("Http: connect");
		#endif
                close(sock_http);
                return 0 ;
        }

	snprintf(buffer,  sizeof(buffer), "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", dest_ip_ptr);

        if (send(sock_http, buffer, strlen(buffer), 0) == -1)
        {
                perror("send:");
                close(sock_http);
                return 0 ;
        }

        gettimeofday(&tv1, NULL);

        while(1)
        {
        	bzero(buffer, 512);
                getlen = recv(sock_http, buffer, sizeof(buffer) - 1, 0);
                if (getlen > 0)
                {
                        NMP_DEBUG_F("Check http response: %s\n", buffer);
                        if(!memcmp(buffer, "HTTP/1.", 7) &&
			  (!memcmp(buffer+9, "2", 1)||!memcmp(buffer+9, "3", 1)||!memcmp(buffer+9, "401", 3)) )
                        {
				close(sock_http);
				return 1;
                        }
                }

                gettimeofday(&tv2, NULL);
		if( (((tv2.tv_sec)*1000000 + tv2.tv_usec)-((tv1.tv_sec)*1000000 + tv1.tv_usec)) > RCV_TIMEOUT*1000000 )
                {
                        NMP_DEBUG_F("Http receive timeout\n");
                        break;
                }
        }

        close(sock_http);
        return 0;
}

/***** NBNS Name Query function *****/
int Nbns_query(unsigned char *src_ip, unsigned char *dest_ip, P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab)
{
    struct sockaddr_in my_addr, other_addr1, other_addr2;
    int sock_nbns, status, sendlen, recvlen, retry=1, exit=0;
    socklen_t other_addr_len1, other_addr_len2;
    char sendbuf[50] = {0x87, 0x96, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x20, 0x43, 0x4b, 0x41,
                        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
                        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
                        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
                        0x41, 0x41, 0x41, 0x41, 0x41, 0x00, 0x00, 0x21,
                        0x00, 0x01};
    char recvbuf[512] = {0};
    char *other_ptr;
    NBNS_RESPONSE *nbns_response;
    struct timeval timeout={1, 500};
    int lock;

    memset(NetBIOS_name, 0x20, 16);

    sock_nbns = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sock_nbns)
    {
        NMP_DEBUG_F("NBNS: socket error.\n");
        return -1;
    }
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(NBNS_PORT); // my port
    memcpy(&my_addr.sin_addr, src_ip, 4);

    //2011.02 Yau add to fix bind error
    int flag=1;
    setsockopt(sock_nbns, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
    status = bind(sock_nbns, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (-1 == status)
    {
        NMP_DEBUG_F("NBNS: bind error.\n");
        return -1;
    }

    setsockopt(sock_nbns, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));//set timeout
    memset(&other_addr1, 0, sizeof(other_addr1));
    other_addr1.sin_family = AF_INET;
    other_addr1.sin_port = htons(NBNS_PORT);  // other port
    memcpy(&other_addr1.sin_addr, dest_ip, 4);  // other ip
    other_ptr = inet_ntoa(other_addr1.sin_addr);
    other_addr_len1 = sizeof(other_addr1);

    while(1)
    {
	sendlen = sendto(sock_nbns, sendbuf, sizeof(sendbuf), 0, (struct sockaddr*)&other_addr1, other_addr_len1);

        bzero(recvbuf, 512);
        memset(&other_addr2, 0, sizeof(other_addr2));
        other_addr_len2 = sizeof(other_addr2);

        recvlen = recvfrom(sock_nbns, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&other_addr2, &other_addr_len2);
        if( recvlen > 0 ) {
	    NMP_DEBUG_F("NBNS Response:\n");

	    #if 0 //def DEBUG_MORE
	    	int x;
	    	for(x=0; x<recvlen; x++)
	    	    NMP_DEBUG_F("%02x ",recvbuf[x]);
	    	NMP_DEBUG_F("\n");
	    #endif

	    nbns_response =(NBNS_RESPONSE *)recvbuf;
	    NMP_DEBUG_F("flags: %02x %02x, number of names= %d\n", 
		nbns_response->flags[0],nbns_response->flags[1],nbns_response->number_of_names);
	    if( nbns_response->number_of_names ==0 ) {
		exit++; //Not support NBNS name query
		if( exit==6 )
		    break;
	    }
	    else
	    if(((nbns_response->flags[0]>>4) == 8) && (nbns_response->number_of_names > 0)
	    && (other_addr2.sin_addr.s_addr = other_addr1.sin_addr.s_addr))
            {
		lock = file_lock("networkmap");
            	memcpy(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num], nbns_response->device_name1, 16);
		fixstr(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
		file_unlock(lock);
	    	memcpy(NetBIOS_name, nbns_response->device_name1, 15);
		NMP_DEBUG_F("Device name:%s~%s~\n", nbns_response->device_name1,
		p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
            	break;
	    }
	    else
	    {
                exit++; //Not support NBNS name query
                if( exit==6 ){
			NMP_DEBUG_F("Unknown error!\n");
			break;
		}
	    }
        }
	else
        {
       	    retry++;
            if( retry==3 )
	    {
		NMP_DEBUG_F("NBNS timeout...\n");
                break;
	    }
        }
	sleep(1); //2008.09.16 Yau add
    }
    close(sock_nbns);
    NMP_DEBUG_F("NBNS close socket\n");
    return 0;
}

/***** Printer server detect function *****/
int lpd515(unsigned char *dest_ip)
{
    	struct sockaddr_in other_addr1;
    	int sockfd1, sendlen1, recvlen1;
    	char sendbuf1[34] = {0};
    	char recvbuf1[MAXDATASIZE];
    	struct timeval tv1, timeout={1, 0};
    	struct LPDProtocol lpd;

    	if ((sockfd1 = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    	{
        	NMP_DEBUG_F("LPD515: socket create error.\n");
        	return -1;
    	}

    	memset(&other_addr1, 0, sizeof(other_addr1));
    	other_addr1.sin_family = AF_INET;
    	other_addr1.sin_port = htons(LPD_PORT);
    	memcpy(&other_addr1.sin_addr, dest_ip, 4);

	setsockopt(sockfd1, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd1, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    	if (connect(sockfd1, (struct sockaddr*)&other_addr1, sizeof(other_addr1)) == -1)
    	{
        	NMP_DEBUG_F("LPD515: socket connect failed!\n");
		close(sockfd1);
		return -1;
     	}

        /*  Send data... */
        lpd.cmd_code = LPR;
        strcpy(lpd.options, MODEL_NAME);   // Set the queue name  as you wish
        lpd.lf = 0x0a;
        sprintf(sendbuf1, "%c%s%c", lpd.cmd_code, lpd.options, lpd.lf);
        if ((sendlen1 = send(sockfd1, sendbuf1, strlen(sendbuf1), 0)) == -1)
        {
             	NMP_DEBUG_F("LPD515: Send packet failed!\n");
		close(sockfd1);
	    	return -1;
        }
        gettimeofday(&tv1, NULL);

        /* Receive data */
        recvlen1 = recv(sockfd1, recvbuf1, MAXDATASIZE, 0);
        memcpy(&lpd, recvbuf1, 1);
        if (lpd.cmd_code == LPR_RESPONSE)
        {
           	close(sockfd1);
               	return 0;
        }

	close(sockfd1);
       	return -1;
}

int raw9100(unsigned char *dest_ip)
{
    struct sockaddr_in other_addr2;
    int sockfd2;
    struct timeval timeout={0, 500000};
    if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        NMP_DEBUG_F("RAW9100: socket create error.\n");
        return -1;
    }


    memset(&other_addr2, 0, sizeof(other_addr2));
    other_addr2.sin_family = AF_INET;
    other_addr2.sin_port = htons(RAW_PORT);
    memcpy(&other_addr2.sin_addr, dest_ip, 4);

    setsockopt(sockfd2, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(sockfd2, (struct sockaddr*)&other_addr2, sizeof(other_addr2)) == -1)
    {
        NMP_DEBUG_F("RAW9100: socket connect failed!\n");
    	close(sockfd2);
    	return -1;
    }

    close(sockfd2);
    return 0;
}

/***** iTune Server detect function *****/
static int waitsock(int sockfd, int sec, int usec)
{
        struct timeval tv;
        fd_set  fdvar;
        int res;

        FD_ZERO(&fdvar);
        FD_SET(sockfd, &fdvar);
        tv.tv_sec = sec;
        tv.tv_usec = usec;
        res = select(sockfd + 1, &fdvar, NULL, NULL, &tv);

        return res;
}

int open_socket_ipv4( unsigned char *src_ip )
{
        struct sockaddr_in local;
        int fd = -1, ittl;
        unsigned char ttl;

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
                //printf("socket() failed: %s\n", strerror(errno));
                goto fail;
        }
        ttl = 255;
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
        {
                //printf("IP_MULTICAST_TTL failed: %s\n", strerror(errno));
                goto fail;
        }
        ittl = 255;
        if (setsockopt(fd, IPPROTO_IP, IP_TTL, &ittl, sizeof(ittl)) < 0)
        {
                //printf("IP_TTL failed: %s\n", strerror(errno));
                goto fail;
        }
        //select local ip address used to send the multicast packet,
        //for router its the lan interface's ip.
        struct in_addr any;
        memcpy(&any, src_ip, 4);
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &any, sizeof(struct in_addr)) < 0)
        {
                //printf("IP_MULTICAST_IF failed: %s\n", strerror(errno));
                return -1;
        }
	/* Set timeout. Cherry Cho added in 2009/2/20. */
	struct timeval timeout={1, 0};
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) < 0)
        {
                NMP_DEBUG_F("SO_RCVTIMEO failed: %s\n", strerror(errno));
                return -1;
        }

        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = 0;

        //2011.02 Yau add to fix bind error
        int flag=1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag)) < 0)
        {
                NMP_DEBUG_F("SO_REUSEADDR failed: %s\n", strerror(errno));
                return -1;
        }
        if(bind(fd, (struct sockaddr*) &local, sizeof(local)) < 0)
        {
                //printf("Bind failed: %s\n", strerror(errno));
                goto fail;
        }
        return fd;

fail:
        if (fd >= 0)
                close(fd);

        return -1;
}

void dns_decode_servername(char *name, char **buf)
{
        int k, len, j;

	k = 0;
        len = *(*buf)++;
        for( j = 0; j<len ; j++)
                name[k++] = *(*buf)++;
        name[k++] = 0;
}

int dns_construct_name(char *name, char *encoded_name)
{
        int i,j,k,n;

        k = 0; /* k is the index to temp */
        i = 0; /* i is the index to name */
        while( name[i] )
        {
                 /* find the dist to the next '.' or the end of the string and add it*/
                 for( j = 0; name[i+j] && name[i+j] != '.'; j++);
                 encoded_name[k++] = j;
                 /* now copy the text till the next dot */
                 for( n = 0; n < j; n++)
                        encoded_name[k++] = name[i+n];
                 /* now move to the next dot */
                 i += j + 1;
                 /* check to see if last dot was not the end of the string */
                 if(!name[i-1])
                        break;
        }
        encoded_name[k++] = 0;

        return k;
}

static int dns_decode_respond(dns_header_s header,char * buf,int numread)
{
        char *buf_ip;
        buf_ip=buf;
        SET_UINT16( header.id, buf );
        if(header.id == 0x0086)
        {
                return 1;
        }
        else
        {
                return -1;
        }
}

int send_mdns_packet_ipv4 (unsigned char *src_ip, unsigned char *dest_ip)
{
        struct sockaddr_in dst_addr,from;
        char dnsbuf[MAXDATASIZE];
        int dnsbuflen;
        int sockfd;

        if((sockfd=open_socket_ipv4(src_ip))<0)
        {
              //printf("creat socket fail\n");
                return -1;
        }

        memset(&dst_addr, 0, sizeof(struct sockaddr_in));
        dst_addr.sin_family = AF_INET;
        dst_addr.sin_port = htons(MDNS_PORT);
        //inet_pton(AF_INET, IPV4_MCAST_GROUP, &dst_addr.sin_addr);
        memcpy(&dst_addr.sin_addr, dest_ip, 4);

        dnsbuflen = 0;

        dnsbuf[dnsbuflen++] = 0x00; //Transaction ID
        dnsbuf[dnsbuflen++] = 0x86;
        dnsbuf[dnsbuflen++] = 0x01; //Flag: Standard query
        dnsbuf[dnsbuflen++] = 0x00;
        dnsbuf[dnsbuflen++] = 0x00; //Question
        dnsbuf[dnsbuflen++] = 0x01;
        memset(&dnsbuf[dnsbuflen], 0x00, 6);//Answer RRs, Authority RRs, Additional RRs
        dnsbuflen+=6;
        dnsbuflen+=dns_construct_name(PROTOCOL_NAME, &dnsbuf[dnsbuflen]);
        dnsbuf[dnsbuflen++] = 0x00; // Type: PTR
        dnsbuf[dnsbuflen++] = 0x0c;
        dnsbuf[dnsbuflen++] = 0x00; // Class : inet
        dnsbuf[dnsbuflen++] = 0x01;

        if(sendto(sockfd , dnsbuf, dnsbuflen , 0 ,(struct sockaddr *) &dst_addr, sizeof(dst_addr)) == -1)
        {
             //perror("sendto\n");
                goto finish;
        }

        while(1)
        {
                if (waitsock(sockfd, RCV_TIMEOUT, 0) == 0)
                {
                        //perror("timeout\n");
                        goto finish;
                }
                int numread,fromlen;
                fromlen = sizeof(from);
                char recv_buf[MAXDATASIZE];

                if ((numread=recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, NULL, &fromlen)) == -1)
                {
                      //perror("recvfrom\n");
                        continue;
                }
                dns_header_s header;

                if (numread<sizeof(header))
                {
                      //perror("invalid packet\n");
                        continue;
                }

                if(dns_decode_respond(header,recv_buf,numread)==-1)
                {
                        continue;
                }
                else //Found iTune Server
                {
                        return 1;
                }
        }
finish:
        close(sockfd);
        return 0;
}
/***** End of iTune Server detection function *****/

/************** UPNP Detect Function ****************/
int ctrlpt(unsigned char *dest_ip)
{
        fd_set rfds;
        char ifname[] = INTERFACE;
        ushort port = 1008;
        struct sockaddr_in destaddr;
        int nbytes, addrlen;
        char buf[UPNP_BUFSIZE];
        int n;

        if((ssdp_fd = create_ssdp_socket_ctrlpt(ifname, port)) == -1)
                return 0;

        create_msearch_ctrlpt(3);
        signal(SIGALRM, interrupt);
        alarm(4);

        global_exit = FALSE; //reset timeout flag
        memset(&description, 0, sizeof(struct device_info)); //reset description

        //enter the top of the loop.
        while(global_exit == FALSE)
        {
                addrlen = sizeof(destaddr);
                memset(&destaddr, 0, addrlen);

                FD_ZERO(&rfds);
                FD_SET(ssdp_fd, &rfds);

                n = select(ssdp_fd+1, &rfds, NULL, NULL, NULL);
                if(n > 0)
                {
                        if(FD_ISSET(ssdp_fd, &rfds))
                        {
                                nbytes = recvfrom(ssdp_fd, buf, sizeof(buf), 0, (struct sockaddr*)&destaddr, &addrlen);
                                buf[nbytes] = '\0';

                                //NMP_DEBUG_F("recv: %d from: %s\n", nbytes, inet_ntoa(destaddr.sin_addr));
                                if( !memcmp(&destaddr.sin_addr, dest_ip, 4) )
                                {
                                        if(MATCH_PREFIX(buf, "HTTP/1.1 200 OK"))
                                        {
                                                global_exit = TRUE;
                                                process_device_response(buf);
                                                return_value = TRUE;
                                        }
                                }
                        }
                }
        }
        close(ssdp_fd);
        if(return_value == FALSE)
                return 0;

        return 1;
}


/*******************************************************************************************/
static char *strip_chars(char *str, char *reject)
{
        char *end;

        str += strspn(str, reject);
        end = &str[strlen(str)-1];
        while (end > str && strpbrk(end, reject))
                *end-- = '\0';

        return str;
}

void interrupt()
{
        global_exit = TRUE;
        NMP_DEBUG_F("no upnp device of this ip\n");
        return_value = FALSE;
}

/*****************************************************************************************/
// create socket of ssdp
// socket, bind, join multicast group.
int create_ssdp_socket_ctrlpt(char *ifname, ushort port)
{
        int fd;
        int sockfd;
        struct sockaddr_in srcaddr;
        struct in_addr inaddr;
        struct ifreq ifreq;
        struct ip_mreqn mcreqn;
        int flag;
        u_char ttl;

        // create socket
        if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
                goto error;

        // get the src ip
        if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
                goto error;
        strcpy(ifreq.ifr_name, ifname);
        if(ioctl(sockfd, SIOCGIFADDR, &ifreq) < 0)
        {
                close(sockfd);
                goto error;
        }
        memcpy(&inaddr, &(((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr),sizeof(struct in_addr));
        close(sockfd);

        // make sure this interface is capable of MULTICAST ...
        memset(&ifreq, 0, sizeof(ifreq));
        strcpy(ifreq.ifr_name, ifname);
        if(ioctl(fd, SIOCGIFFLAGS, (int) &ifreq))
                goto error;
        if((ifreq.ifr_flags & IFF_MULTICAST) == 0)
                goto error;

        // bind the socket to an address and port.
        flag = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
        memset(&srcaddr, 0, sizeof(srcaddr));
        srcaddr.sin_addr = inaddr;
        srcaddr.sin_family = AF_INET;
        srcaddr.sin_port = htons(port);
        if ( bind(fd, (struct sockaddr *) &srcaddr, sizeof(srcaddr)) )
                goto error;

        // join the multicast group
        memset(&ifreq, 0, sizeof(ifreq));
        strcpy(ifreq.ifr_name, ifname);
        if(ioctl(fd, SIOCGIFINDEX, &ifreq))
                goto error;

        memset(&mcreqn, 0, sizeof(mcreqn));
        mcreqn.imr_multiaddr.s_addr = inet_addr(SSDP_IP);
        mcreqn.imr_address.s_addr = srcaddr.sin_addr.s_addr;
        mcreqn.imr_ifindex = ifreq.ifr_ifindex;
        if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcreqn, sizeof(mcreqn)))
                goto error;

        // restrict multicast messages sent on this socket
        // to only go out this interface and no other
        if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&srcaddr.sin_addr, sizeof(struct in_addr)))
                goto error;

        // set the multicast TTL
        ttl = 4;
        if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)))
                goto error;

        return fd;
error:
        close(fd);
        return -1;
}

/****************************************************************************************/
//construct the multicast address and send out the M-SEARCH advertisement packets.
int create_msearch_ctrlpt(int Mx)
{
        struct sockaddr_in addr;
        char *data;
        char tmp[50];

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(SSDP_IP);
        addr.sin_port = htons(SSDP_PORT);

        if(Mx < MIN_SEARCH_TIME)
                Mx = MIN_SEARCH_TIME;
        if(Mx >MAX_SEARCH_TIME)
                Mx = MAX_SEARCH_TIME;

        data = (char *)malloc(300 * sizeof(char));
        *data = '\0';
        sprintf(tmp, "M-SEARCH * HTTP/1.1\r\n");
        strcat(data, tmp);
        sprintf(tmp, "HOST:%s:%d\r\n", SSDP_IP, SSDP_PORT);
        strcat(data, tmp);
        sprintf(tmp, "ST:upnp:rootdevice\r\n");
        strcat(data, tmp);
        sprintf(tmp, "MAN:\"ssdp:discover\"\r\n");
        strcat(data, tmp);
        sprintf(tmp, "MX:%d\r\n\r\n", Mx);
        strcat(data, tmp);

        if(sendto(ssdp_fd, data, strlen(data), 0, (struct sockaddr *)&addr, sizeof(addr)) <0)
                return 0;

        return 1;

}

/************************************************************************************************/
// process the device response "HTTP/1.1 200 OK"
int process_device_response(char *msg)
{
        char *line, *body, *p;                  // temporary variables
        char *location = NULL;                  // the LOCATION: header
        char host[16], port[6];                 // the ip and port of the device
        ushort destport;                        // the integer type of device port
        char *data = NULL;                      // the data in packet
        int http_fd;                            // the http socket fd
        int nbytes;                             // recv number
        int i;
        char *descri = NULL;
        int len;
	struct timeval timeout={10, 0};

        //search "\r\n\r\n" or "\r\n" first appear place and judge whether msg have blank.
        if( (body = strstr(msg, "\r\n\r\n")) != NULL)
                body +=4;
        else if ( (body = strstr(msg, "\r\n")) != NULL)
                body +=2;
        else
                return 0;

        p = msg;
        // find the LOCATION information.
        while( p!= NULL && p < body)
        {
                line = strsep(&p, "\r\n");      //divide up string
                if((strncmp(line, "LOCATION:", 9) == 0) || (strncmp(line, "Location:", 9) == 0))
                {
                        location = strip_chars(&line[9], "\t");
                        location = strip_chars(&line[9], " ");
                        break;
                }
        }
        NMP_DEBUG_F("UPnP location=%s\n", location);
        //fprintf(fp_upnp, "UPnP location=%s\n", location);//Yau     

        // get the destination ip
        location += 7;
	i = 0;
	while( (*location != ':') && (*location != '/')) {
                host[i] = *location++;
		i++;
	}
        host[i] = '\0';
        //get the destination port
        if(*location == ':') {
            	for(location++, i =0; *location != '/'; i++)
                	port[i] = *location++;
            	port[i] = '\0';
            	destport = (ushort)atoi(port);
	}
	else
		destport = 80;

        //create a socket of http
        if ( (http_fd = create_http_socket_ctrlpt(host, destport)) == -1)
                goto error;

        //set the send data information.
        data = (char *)malloc(1500 * sizeof(char));
        memset(data, 0, 1500);
        *data = '\0';
	snprintf(data, sizeof(data), "GET %s HTTP/1.1\r\nHOST: %s:%s\r\nACCEPT-LANGUAGE: zh-cn\r\n\r\n",\
                        location, host, port);
        //printf("%s\n",data);

        //send the request to get the device description
        if((nbytes = send(http_fd, data, strlen(data), 0)) == -1)
                goto error;
        free(data);
        data = (char *)malloc(1501 * sizeof(char));
        memset(data, 0, 1501);
        *data = '\0';
        descri = (char *) malloc(6001*sizeof(char));
        memset(descri, 0, 6001);
        *descri ='\0';

        //receive data of http socket
        len = 0;
	setsockopt(http_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
	while((nbytes = recv(http_fd, data,1500, 0)) > 0)
        {
                len += nbytes;
                if(len > 6000)
                        break;
                data[nbytes] ='\0';
                strcat(descri, data);
        }
        //printf("%s\n", descri);
        //printf("len = %d", len);
	//if(fp_upnp!=NULL)
	//	fprintf(fp_upnp, "%s\n\n", descri);//Yau

        //store the useful information.
        store_description(descri);

        free(descri);
        free(data);
        return 1;
error:
        http_fd = -1;
        free(data);
        free(descri);
        return 0;
}

/*******************************************************************************************/
//create a socket of http
int create_http_socket_ctrlpt(char *host, ushort destport)
{
        struct sockaddr_in destaddr;            // the device address information
        int fd;

	NMP_DEBUG_F("UPnP create http socket to: %s:%d\n", host,destport);
        // create out http socket
        if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
                return -1;

        //set the tcp connect destination information
        destaddr.sin_addr.s_addr = inet_addr(host);
        destaddr.sin_family = AF_INET;
        destaddr.sin_port = htons(destport);

        //connect to the device.
        if(connect(fd, (struct sockaddr *)&destaddr, sizeof(destaddr)) == -1)
                return -1;

        return fd;

}

/***************************************************************/
// store the descirption information to sturct device_info.
void store_description(char *msg)
{
        char line[200], *body, *p, *mxend;
        char tmp[200];
        int i, j;
        int s_num = 0;
        int type = 0;
        char *opts[] = {"<friendlyName>","<manufacturer>", "<presentationURL>",
                        "<modelDescription>", "<modelName>", "<modelNumber>",
                        "<serviceType>", "<SCPDURL>",
                        };
        // pointer to the end of msg
        p = strstr(msg, "<?xml");
        //printf("%s", p);
        body = strstr(msg, "</root>");

        while( p!= NULL && p < body)
        {
                // get rid of 'TAB' or 'Space' in the start of a line.
                while((*p == '\r' || *p == '\n' || *p == '\t' || *p == ' ') && p < body)
                        p ++;

                // get a line.
                i = 0;
                while(*p != '>' && p < body)
		{
		    if(i<199)
                        line[i++] = *p++;
		    else
			*p++;
		}
                if(p == body)
                {
                        //printf("end of the data\n");
                        break;
                }

                line[i++] = *p++;
                line[i] ='\0';

                if(p == body)
                {
                        //printf("end of the data 2\n");
                        break;
                }

                // judge whether this line is useful.
                for(type = 0; type< sizeof(opts)/sizeof(*opts); type++)
                        if(strncmp(line, opts[type], strlen(opts[type])) == 0)
                                break;
                if(type == sizeof(opts)/sizeof(*opts))
                        continue;

                // get the information.
                // eg. <manufacturer> information </manufacturer>
                i = 0;
                while(*p != '>' && p < body)
		{
		    if(i<199)
                        line[i++] = *p++;
		    else
		  	*p++;
		}
                line[i++] = *p++;
                line[i] ='\0';

                for(j =0; line[j] != '<'; j++)
                        tmp[j] = line[j];
                tmp[j] = '\0';
                //printf("tmp = %s\n", tmp);

                switch(type)
                {
                case 0:
                        strcpy(description.friendlyname, tmp);
                        NMP_DEBUG_F("friendlyname = %s\n", tmp);
#ifdef NMP_DEBUG_F
	if(strstr(tmp, "WDTVLive")) {
        	FILE *fp = fopen("/var/networkmap.upnp", "w");
        	if(fp != NULL) {
                	fprintf(fp, "%s", msg);
	                fclose(fp);
        	}
	}
#endif
                        break;
                case 1:
                        strcpy(description.manufacturer, tmp);
                        NMP_DEBUG_F("manufacturer = %s\n", tmp);
                        break;
                case 2:
                        strcpy(description.presentation, tmp);
                        NMP_DEBUG_F("presentation = %s\n", tmp);
                        break;
                case 3:
                        strcpy(description.description, tmp);
                        NMP_DEBUG_F("description = %s\n", tmp);
                        break;
                case 4:
                        strcpy(description.modelname, tmp);
                        NMP_DEBUG_F("modelname = %s\n", tmp);
                        break;
                case 5:
                        strcpy(description.modelnumber, tmp);
                        NMP_DEBUG_F("modelnumber = %s\n", tmp);
                        break;
                case 6: // tmp="urn:schemas-upnp-org:service:serviceType:v"
                        mxend = tmp;
                        i = 0; j = 0;
                        while(i != 4)
                        {
                                if(i == 3)
                                        tmp[j++] = *mxend;
                                if(*mxend == ':')
                                        i++;
                                mxend++;
                        }
                        tmp[j-1] = '\0';
                        strcpy(description.service[s_num].name, tmp);
                        NMP_DEBUG_F("service %d name = %s\n", s_num, tmp);
                        break;
                case 7:
                        strcpy(description.service[s_num].url, tmp);
                        NMP_DEBUG_F("service %d url = %s\n", s_num, tmp);
                        s_num++;
                        break;
                }
        }
        description.service_num = s_num;
}
/***** End of UPNP Detect Function *****/

/************* SMB Function ************/
// 0xAB, 0xA+41,0xB+41
int EncodeName(unsigned char *name, unsigned char *buffer, unsigned short length)
{
        int i;
        buffer[0] = 0x20;
        buffer[length*2+1] = 0x00;
        for (i = 0; i < length; i++)
        {
                buffer[i*2+1] = ((name[i] & 0xF0)>>4) +65;
                buffer[i*2+2] = (name[i] & 0x0F) + 65;
        }
        return length*2+2;
}

int TranUnicode(UCHAR *uni, UCHAR *asc, USHORT length)
{
        int i;
        for (i=0; i<length; i++)
        {
                uni[i*2] = asc[i];
                uni[i*2+1] = 0x00;
        }
        return length*2;
}

int SendSMBReq(UCHAR *des_ip, MY_DEVICE_INFO *my_info)
{
        int sockfd, numbytes;
        int i, j, operate;
        unsigned char buf[MAXDATASIZE] ;
        char ptr[32];
        struct sockaddr_in des_addr;
        struct hostent *hptr;
        struct in_addr *hipaddr;
        fd_set rfds;
        int ret;
        struct timeval tv1, tv2, tm;
        int error=-1, len;
        len = sizeof(int);
        UCHAR securitymode;
        UCHAR  WordCount;               // Count of parameter words
        USHORT ParameterWords[1024];    // The parameter words
        USHORT ByteCount;               // Count of bytes
        UCHAR  Buffer[1024];            // The bytes
        int offsetlen = 0;
        USHORT tmplen = 0, tmplen2, tmplen3;
        int pos1, pos2, pos3, slip;
	int smbretry_flag = 0;

	unsigned char nbss_buf[4];
        unsigned char nbss_header[4] = {0x81, 0x00, 0x00, 0x44};

        UCHAR des_nbss_name[34];
        UCHAR my_nbss_name[34];
	UCHAR smb_buf[98] = {
	0x02, 0x50, 0x43, 0x20, 0x4e, 0x45, 0x54, 0x57, 0x4f, 0x52, 0x4b, 0x20, 0x50, 0x52, 0x4f, 0x47,
	0x52, 0x41, 0x4d, 0x20, 0x31, 0x2e, 0x30, 0x00, 0x02, 0x4c, 0x41, 0x4e, 0x4d, 0x41, 0x4e, 0x31,
	0x2e, 0x30, 0x00, 0x02, 0x57, 0x69, 0x6e, 0x64, 0x6f, 0x77, 0x73, 0x20, 0x66, 0x6f, 0x72, 0x20,
	0x57, 0x6f, 0x72, 0x6b, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x73, 0x20, 0x33, 0x2e, 0x31, 0x61, 0x00,
	0x02, 0x4c, 0x4d, 0x31, 0x2e, 0x32, 0x58, 0x30, 0x30, 0x32, 0x00, 0x02, 0x4c, 0x41, 0x4e, 0x4d,
	0x41, 0x4e, 0x32, 0x2e, 0x31, 0x00, 0x02, 0x4e, 0x54, 0x20, 0x4c, 0x4d, 0x20, 0x30, 0x2e, 0x31,
	0x32, 0x00};
	UCHAR sessionX_buf[204] = {
	0x0c, 0xff, 0x00, 0xec, 0x00, 0x04, 0x11, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4a,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00, 0xa0, 0xb1, 0x00, 0x60, 0x48, 0x06, 0x06, 0x2b,
	0x06, 0x01, 0x05, 0x05, 0x02, 0xa0, 0x3e, 0x30, 0x3c, 0xa0, 0x0e, 0x30, 0x0c, 0x06, 0x0a, 0x2b,
	0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x02, 0x0a, 0xa2, 0x2a, 0x04, 0x28, 0x4e, 0x54, 0x4c,
	0x4d, 0x53, 0x53, 0x50, 0x00, 0x01, 0x00, 0x00, 0x00, 0x97, 0x82, 0x08, 0xe2, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x01, 0x28,
	0x0a, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x57, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x64, 0x00, 0x6f, 0x00,
	0x77, 0x00, 0x73, 0x00, 0x20, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x32, 0x00, 0x20, 0x00,
	0x53, 0x00, 0x65, 0x00, 0x72, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00, 0x20, 0x00,
	0x50, 0x00, 0x61, 0x00, 0x63, 0x00, 0x6b, 0x00, 0x20, 0x00, 0x32, 0x00, 0x20, 0x00, 0x32, 0x00,
	0x36, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00, 0x57, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x64, 0x00,
	0x6f, 0x00, 0x77, 0x00, 0x73, 0x00, 0x20, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x32, 0x00,
	0x20, 0x00, 0x35, 0x00, 0x2e, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	UCHAR securityblob[74] = {
	0x60, 0x48, 0x06, 0x06, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x02, 0xa0, 0x3e, 0x30, 0x3c, 0xa0, 0x0e,
	0x30, 0x0c, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x02, 0x0a, 0xa2, 0x2a,
	0x04, 0x28, 0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00, 0x01, 0x00, 0x00, 0x00, 0x97, 0x82,
	0x08, 0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x05, 0x01, 0x28, 0x0a, 0x00, 0x00, 0x00, 0x0f};

	if((int)*(des_ip+3)==255)
              return 0;

SMBretry:
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
                 perror("SAMBA: create socket err");
                 return -1 ;
        }
        des_addr.sin_family = AF_INET; /* host byte order */
        des_addr.sin_port = htons(NBSS_PORT); /* short, network byte order */
        memcpy(&des_addr.sin_addr, des_ip, 4);
        inet_ntop(AF_INET, &des_addr.sin_addr, ptr, 32);
        bzero(&(des_addr.sin_zero), 8); /* zero the rest of the struct */

        unsigned long ul = 1;
        ioctl(sockfd, FIONBIO, &ul);
         if (connect(sockfd, (struct sockaddr *)&des_addr, sizeof(struct sockaddr)) == -1)
        {
                tm.tv_sec = TIME_OUT_TIME;
                tm.tv_usec = 0;
                FD_ZERO(&rfds);
                FD_SET(sockfd, &rfds);
                if( select(sockfd+1, NULL, &rfds, NULL, &tm) > 0)
                {
                        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
                        if(error != 0)
                        {
                                perror("SAMBA: get socket opt err");
                                return -1 ;
                        }
                }
                else
                {
                        perror("SAMBA: select err");
                        return -1 ;
                }
        }
        ul = 0;
        ioctl(sockfd, FIONBIO, &ul);
        NMP_DEBUG_F("NetBIOS(NBSS) connected\n");

        operate = NBSS_REQ;
        while(operate != 0)
        {
		NMP_DEBUG_F("Operate= %d\n", operate);
		if(fp_smb!=NULL)
			fprintf(fp_smb, "Operate= %d\n", operate);
                switch(operate)
                {
                        case NBSS_REQ:  // first send nbss request
				offsetlen = 0;
                                bzero(buf, MAXDATASIZE);
                                EncodeName(my_info->des_hostname, des_nbss_name, my_info->des_hostname_len);
                                EncodeName(my_info->my_hostname, my_nbss_name, my_info->my_hostname_len);

                                memcpy(buf, nbss_header, sizeof(nbss_header));        // nbss base header
                                offsetlen += sizeof(nbss_header); // 4

                                memcpy(buf+offsetlen, des_nbss_name, sizeof(des_nbss_name)); // destination device host name, 34 bytes
                                offsetlen += sizeof(des_nbss_name);

                                memcpy(buf+offsetlen, my_nbss_name, sizeof(my_nbss_name)); // client host name, 34 bytes
                                offsetlen += sizeof(my_nbss_name);

                                if (send(sockfd, buf, offsetlen, 0) == -1)
                                {
                                        perror("connect") ;
                                        return -1 ;
                                }
                                gettimeofday(&tv1, NULL);       // set nbss statrt time
                                operate = NBSS_RSP;
                                break;
                        case NBSS_RSP:  // second receive nbss response
                                FD_ZERO(&rfds);
                                 FD_SET(sockfd, &rfds);
                                struct timeval timeout={1,0};
                                 ret = select( sockfd + 1, &rfds, NULL, NULL, &timeout);
                                if(ret)
                                {
                                         if(FD_ISSET(sockfd,&rfds))//Using TCP client socket
                                        {
                                                bzero(nbss_buf, sizeof(nbss_buf));
                                                if ((numbytes=recv(sockfd, nbss_buf, sizeof(nbss_buf), 0)) == -1)
                                                {
                                                 perror("recv");
                                                 return -1 ;
                                                }
                                                if(numbytes > 0)
                                                {
                                                        if(nbss_buf[0] == 0x83)
							{
								NMP_DEBUG_F("Called Name Error!\n");
								smbretry_flag++;
                                                        	close(sockfd);
                                                                sleep(1);
								if(smbretry_flag == 5)
								{
									operate = 0;
									break;
								}
								else
									goto SMBretry;
							}
							else if(nbss_buf[0] == 0x82)
							{
                                                        	operate = SMB_NEGOTIATE_REQ;
                                                        	break;
							}
                                                }
                                        }
                                }
                                gettimeofday(&tv2, NULL);
                                if((tv2.tv_sec - tv1.tv_sec) > RCV_TIMEOUT)
                                {
                                        NMP_DEBUG_F("NBSS receive timeout\n");
                                        operate = 0;
                                        break;
                                }
                        break;
                        case SMB_NEGOTIATE_REQ:
                        {
                                NBSS_HEADER nbss_hdr;
                                SMB_HEADER  smb_hdr;
                                bzero(buf, sizeof(buf));

                                nbss_hdr.msg_type = 0x00;
                                nbss_hdr.flags = 0x00;
                                nbss_hdr.length = htons(133);
                                memcpy(buf, &nbss_hdr, 4);      // add nbss header

                                smb_hdr.Protocol[0] = 0xFF;
                                smb_hdr.Protocol[1] = 0x53;
                                smb_hdr.Protocol[2] = 0x4d;
                                smb_hdr.Protocol[3] = 0x42;
                                smb_hdr.Command = 0x72;
                                bzero(smb_hdr.Status, 4);
                                smb_hdr.Flags = 0x18;
                                smb_hdr.Flags2 = __cpu_to_le16(0xc853);
                                bzero(&smb_hdr.Pad, 12);
                                smb_hdr.Tid = 0x0000;
                                smb_hdr.Pid = __cpu_to_le16(0xFEFF);
                                smb_hdr.Uid = 0x0000;
                                smb_hdr.Mid = 0x0000;
                                memcpy(buf+4, &smb_hdr, 32);
                                WordCount = 0x00;
                                memcpy(buf+36, &WordCount, 1);
                                ByteCount = __cpu_to_le16(0x0062);
                                memcpy(buf+37, &ByteCount, 2);
                                memcpy(buf+39, smb_buf, 98);

                                if (send(sockfd, buf, 137, 0) == -1)
                                {
                                        perror("connect") ;
                                        return -1 ;
                                }
                                gettimeofday(&tv1, NULL);       // set nbss statrt time

                                operate = SMB_NEGOTIATE_RSP;
                        }
                        break;
                        case SMB_NEGOTIATE_RSP:
                        {
                                FD_ZERO(&rfds);
                                FD_SET(sockfd, &rfds);
                                struct timeval timeout={1,0};
                                ret = select( sockfd + 1, &rfds, NULL, NULL, &timeout);
                                securitymode = 0x00;
                                if(ret)
                                {
                                         if(FD_ISSET(sockfd,&rfds))//Using TCP client socket
                                        {
                                                bzero(buf, sizeof(buf));
                                                if ((numbytes=recv(sockfd, buf, sizeof(buf), 0)) == -1)
                                                {
                                                 perror("recv");
                                                 return -1 ;
                                                }
                                                if(numbytes > 0)
                                                {
							if(fp_smb!=NULL)
								fprintf(fp_smb, "SMB_NEGOTIATE_RSP: %s\n", buf);//Yau
                                                        if(buf[4] == 0xFF && buf[5] == 0x53 && buf[6]==0x4d
                                                                && buf[7]==0x42 && buf[8]==0x72)
                                                        {
                                                                NMP_DEBUG_F("SMS rev%02x\n", buf[0]);
                                                                securitymode = buf[39];
                                                                operate = SMB_SESSON_ANDX_REQ;
                                                        }
                                                        break;
                                                }
                                        }
                                }
                                gettimeofday(&tv2, NULL);
                                if((tv2.tv_sec - tv1.tv_sec) > RCV_TIMEOUT)
                                {
                                        operate = 0;
                                        break;
                                }

                        }
                        break;
                        case SMB_SESSON_ANDX_REQ:
                        {
                                NBSS_HEADER nbss_hdr;
                                SMB_HEADER  smb_hdr;
                                SMB_SESSION_SETUPX_REQ seX_req;
                                SMB_CLIENT_INFO smb_info;
                                bzero(buf, sizeof(buf));
                                offsetlen = 0;
                                //--------------------------------------------------
                                nbss_hdr.msg_type = 0x00;
                                nbss_hdr.flags = 0x00;
                                if(securitymode == 0x02)        // Samba
                                {
                                        nbss_hdr.length = htons(130);
                                }
                                else    // Windows
                                {
                                        nbss_hdr.length = htons(236);
                                }
                                memcpy(buf, &nbss_hdr, 4);
                                offsetlen += 4;
                                pos1 = offsetlen-2;     // first length position
                                //-------------------------------------------------- nbss header
                                smb_hdr.Protocol[0] = 0xFF;
                                smb_hdr.Protocol[1] = 0x53;
                                smb_hdr.Protocol[2] = 0x4d;
                                smb_hdr.Protocol[3] = 0x42;
                                smb_hdr.Command = 0x73;
                                bzero(smb_hdr.Status, 4);
                                smb_hdr.Flags = 0x18;
                                smb_hdr.Flags2 = __cpu_to_le16(0xc807);
                                bzero(&smb_hdr.Pad, 12);
                                smb_hdr.Tid = 0x0000;
                                smb_hdr.Pid = __cpu_to_le16(0xFEFF);
                                smb_hdr.Uid = 0x0000;
                                smb_hdr.Mid = __cpu_to_le16(0x0010);
                                memcpy(buf+offsetlen, &smb_hdr, 32);
                                offsetlen += 32; // 36
                                //-------------------------------------------------- SMB base header
                                if(securitymode == 0x02)
                                {
                                seX_req.WordCount = 0x0d; // 13 Samba
                                memcpy(buf+offsetlen, &seX_req.WordCount, 1);
                                offsetlen += 1; // 37
                                seX_req.AndXCommand = 0xff; // no further commands
                                memcpy(buf+offsetlen, &seX_req.AndXCommand, 1);
                                offsetlen += 1; // 38
                                seX_req.AndXReserved = 0x00;
                                memcpy(buf+offsetlen, &seX_req.AndXReserved, 1);
                                offsetlen += 1; // 39
                                seX_req.AndXOffset = 0x0000; //0x0082;
                                memcpy(buf+offsetlen, &seX_req.AndXOffset, 2);
                                pos2 = offsetlen;
                                offsetlen += 2; // 41
                                seX_req.MaxBufferSize = 4356;
                                memcpy(buf+offsetlen, &seX_req.MaxBufferSize, 2);
                                offsetlen += 2;  // 43
                                seX_req.MaxMpxCount = __cpu_to_le16(0x000a);
                                memcpy(buf+offsetlen, &seX_req.MaxMpxCount, 2);
                                offsetlen += 2; // 45
                                seX_req.VcNumber = 0x0000;
                                memcpy(buf+offsetlen, &seX_req.VcNumber, 2);
                                offsetlen += 2; // 47
                                seX_req.SessionKey = 0x00000000;
                                memcpy(buf+offsetlen, &seX_req.SessionKey, 4);
                                offsetlen += 4; // 51
                                seX_req.CaseInsensitivePasswordLength = __cpu_to_le16(0x0001);
                                memcpy(buf+offsetlen, &seX_req.CaseInsensitivePasswordLength, 2);
                                offsetlen += 2; // 53
                                seX_req.CaseSensitivePasswordLength = __cpu_to_le16(0x0001);
                                memcpy(buf+offsetlen, &seX_req.CaseSensitivePasswordLength, 2);
                                offsetlen += 2; // 55
                                seX_req.Reserved = 0x00000000;
                                memcpy(buf+offsetlen, &seX_req.Reserved, 4);
                                offsetlen += 4; // 59
                                seX_req.Capabilities = __cpu_to_le32(0x000000d4);
                                memcpy(buf+offsetlen, &seX_req.Capabilities, 4);
                                offsetlen += 4; // 63
                                //---------------------------------------------------------------SMB Session and X header
                                smb_info.ByteCount = 0x0000; //0x0045;
                                memcpy(buf+offsetlen, &smb_info.ByteCount, 2);
                                pos3 = offsetlen;
                                offsetlen += 2; // 65
                                slip = offsetlen;
                                bzero(smb_info.CaseInsensitivePassword, 32);
                                memcpy(buf+offsetlen, smb_info.CaseInsensitivePassword, 1);
                                offsetlen += 1; // 66
                                bzero(smb_info.CaseSensitivePassword, 32);
                                memcpy(buf+offsetlen, smb_info.CaseInsensitivePassword, 1);
                                offsetlen += 1; // 67
                                memcpy(buf+offsetlen, smb_info.CaseInsensitivePassword, 1);
                                offsetlen += 1; // 68
                                bzero(smb_info.AccountName, 32);
                                tmplen = TranUnicode(smb_info.AccountName, my_info->account, my_info->account_len);
                                memcpy(buf+offsetlen, smb_info.AccountName, tmplen+2);
                                offsetlen += tmplen+2; // 78
                                bzero(smb_info.PrimaryDomain, 32);
                                tmplen = TranUnicode(smb_info.PrimaryDomain, my_info->primarydomain, my_info->primarydomain_len);
                                memcpy(buf+offsetlen, smb_info.PrimaryDomain, tmplen+2);
                                offsetlen += tmplen+2; // 98
                                bzero(smb_info.NativeOS, 128);
                                tmplen = TranUnicode(smb_info.NativeOS, my_info->nativeOS, my_info->nativeOS_len);
                                memcpy(buf+offsetlen, smb_info.NativeOS, tmplen+2);
                                offsetlen += tmplen+2; // 110
                                bzero(smb_info.NativeLanMan, 128);
                                tmplen = TranUnicode(smb_info.NativeLanMan, my_info->nativeLanMan, my_info->nativeLanMan_len);
                                memcpy(buf+offsetlen, smb_info.NativeLanMan, tmplen+2);
                                offsetlen += tmplen+2; //
                                tmplen = htons(offsetlen-4);
                                memcpy(buf+pos1, &tmplen, 2);
                                tmplen = offsetlen-4;
                                memcpy(buf+pos2, &tmplen, 2);
                                tmplen = offsetlen-slip;
                                memcpy(buf+pos3, &tmplen, 2);
                                }
                                else
                                {
                                        memcpy(buf+offsetlen, sessionX_buf, 204);
                                        offsetlen += 204; // 36
                                }
                                //---------------------------------------------------------------SMB Client infomation
                                if (send(sockfd, buf, offsetlen, 0) == -1)
                                {
                                        perror("connect") ;
                                        return -1 ;
                                }
                                gettimeofday(&tv1, NULL);       // set SMB SESSON ANDX REQ statrt time
                                operate = SMB_SESSON_ANDX_RSP;
                        }
                        break;
                        case SMB_SESSON_ANDX_RSP:
                        {
                                FD_ZERO(&rfds);
                                FD_SET(sockfd, &rfds);
                                struct timeval timeout={3,0};
                                ret = select( sockfd + 1, &rfds, NULL, NULL, &timeout);
                                if(ret)
                                {
                                        if(FD_ISSET(sockfd,&rfds))//Using TCP client socket
                                        {
                                                bzero(buf, sizeof(buf));
                                                if ((numbytes=recv(sockfd, buf, sizeof(buf), 0)) == -1)
                                                {
                                                 	perror("recv");
                                                 	return -1 ;
                                                }
                                                if(numbytes > 0)
                                                {
							if(fp_smb!=NULL)
								fprintf(fp_smb, "SMB_SESSON_ANDX_RSP:\n%s\n", buf);//Yau
                                                        if(buf[4] == 0xFF && buf[5] == 0x53 && buf[6]==0x4d
                                                                && buf[7]==0x42 && buf[8]==0x73)
                                                        {
                                                                if(securitymode == 0x02) // Samba
                                                                {
                                                                        tmplen = buf[36];
                                                                        UCHAR tmpch[2] = {0x00, 0x00};
                                                                        i=36+1+tmplen*2+2;

                                                                        while(memcmp(buf+i, tmpch, 2) != 0)
                                                                        {
										snprintf(SMB_OS, sizeof(SMB_OS), "%s%c", SMB_OS, buf[i]);
                                                                                i++;
                                                                        }
                                                                        i += 2;
									NMP_DEBUG_F("\nNativeOS: %s\n", SMB_OS);

                                                                        while(memcmp(buf+i, tmpch, 2) != 0)
                                                                        {
                                                                                i++;
                                                                        }
                                                                        i += 2;

                                                                        while(memcmp(buf+i, tmpch, 2) != 0)
                                                                        {
										snprintf(SMB_PriDomain, sizeof(SMB_PriDomain), "%s%c", SMB_PriDomain, buf[i]);
                                                                                i++;
                                                                        }
                                                                        NMP_DEBUG_F("Primary Domain: %s\n", SMB_PriDomain);
                                                                }
                                                                else //Windows
                                                                {
                                                                        tmplen2 = (USHORT)buf[43];
                                                                        tmplen3 = (USHORT)buf[44];
                                                                        tmplen = tmplen3*256+tmplen2;
                                                                        UCHAR tmpch[2] = {0x00, 0x00};
                                                                        i=47+tmplen;

                                                                        while(memcmp(buf+i, tmpch, 2) != 0)
                                                                        {
										snprintf(SMB_OS, sizeof(SMB_OS), "%s%c", SMB_OS, buf[i]);
                                                                                i++;
                                                                        }
                                                                        i += 2;
									NMP_DEBUG_F("\nNativeOS: %s\n", SMB_OS);

                                                                        while(memcmp(buf+i, tmpch, 2) != 0)
                                                                        {
                                                                                i++;
                                                                        }
                                                                        i += 2;
                                                                        while(memcmp(buf+i, tmpch, 2) != 0)
                                                                        {
										snprintf(SMB_PriDomain, sizeof(SMB_PriDomain), "%s%c", SMB_PriDomain, buf[i]);
                                                                                i++;
                                                                        }
									NMP_DEBUG_F("Primary Domain: %s\n", SMB_PriDomain);
                                                                }
                                                                operate = 0;
                                                        }
                                                        break;
                                                }
                                        }
                                }
                                gettimeofday(&tv2, NULL);

                                if((tv2.tv_sec - tv1.tv_sec) > RCV_TIMEOUT)
                                {
                                        NMP_DEBUG_F("SMB receive timeout\n");
                                        operate = 0;
                                        break;
                                }
                        }
                        break;
                }
        }
	close(sockfd);
	return 0;
}

/******** End of SMB function **********/

/* ASUS Device Discovery */
int PackGetInfo(char *pdubuf)
{
        IBOX_COMM_PKT_HDR_EX *hdr;

        hdr=(IBOX_COMM_PKT_HDR_EX *)pdubuf;
        hdr->ServiceID = NET_SERVICE_ID_IBOX_INFO;
        hdr->PacketType = NET_PACKET_TYPE_CMD;
        hdr->OpCode = NET_CMD_ID_GETINFO;
        hdr->Info = 0;

        return (0);
}

int UnpackGetInfo(char *pdubuf, PKT_GET_INFO *Info)
{
        IBOX_COMM_PKT_RES_EX *hdr;

        hdr = (IBOX_COMM_PKT_RES_EX *)pdubuf;

        if (hdr->ServiceID!=NET_SERVICE_ID_IBOX_INFO ||
            hdr->PacketType!=NET_PACKET_TYPE_RES ||
            hdr->OpCode!=NET_CMD_ID_GETINFO)
            return 0;


        memcpy(Info, pdubuf+sizeof(IBOX_COMM_PKT_RES), sizeof(PKT_GET_INFO));
        return 1;
}

int
Asus_Device_Discovery(unsigned char *src_ip, unsigned char *dest_ip, P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab)
{
    	struct sockaddr_in my_addr, other_addr1, other_addr2;
    	int sock_dd, status, other_addr_len1, other_addr_len2, sendlen, recvlen, retry=3;
    	char txPdubuf[512] = {0};
    	char *other_ptr;
    	struct timeval timeout={1, 500};
	PKT_GET_INFO get_info;
        fd_set  fdvar;
        int res;

    	sock_dd = socket(AF_INET, SOCK_DGRAM, 0);
    	if (-1 == sock_dd)
    	{
        	NMP_DEBUG_F("DD: socket error.\n");
        	return -1;
    	}

    	memset(&my_addr, 0, sizeof(my_addr));
    	my_addr.sin_family = AF_INET;
    	my_addr.sin_port = htons(9999); //infosvr port
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        int flag=1;
        if (setsockopt(sock_dd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag)) < 0)
        {
                NMP_DEBUG_F("DD: SO_REUSEADDR failed: %s\n", strerror(errno));
                return -1;
        }

    	status = bind(sock_dd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    	if (-1 == status)
    	{
        	NMP_DEBUG_F("DD: bind error.\n");
        	return -1;
    	}

    	setsockopt(sock_dd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));//set timeout
    	memset(&other_addr1, 0, sizeof(other_addr1));
    	other_addr1.sin_family = AF_INET;
    	other_addr1.sin_port = htons(9999);  // infosvr port
    	memcpy(&other_addr1.sin_addr, dest_ip, 4);  // dist ip
    	other_ptr = inet_ntoa(other_addr1.sin_addr);
    	other_addr_len1 = sizeof(other_addr1);

    	while (retry > 0 /*&& ED*/)
    	{
		PackGetInfo(txPdubuf);
        	sendlen = sendto(sock_dd, txPdubuf, sizeof(txPdubuf), 0, (struct sockaddr*)&other_addr1, other_addr_len1);

	        bzero(txPdubuf, 512);
	        other_addr_len2 = sizeof(other_addr2);
		recvlen = recv(sock_dd, txPdubuf, sizeof(txPdubuf), 0);

	        if( recvlen > 0 ) {
			if(UnpackGetInfo(txPdubuf, &get_info)) {
				NMP_DEBUG_F("DD: productID= %s~\n", get_info.ProductID);
				memcpy(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num], get_info.ProductID, 16);
				fixstr(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
				p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 3;
				break;
			}
			else
				retry--;
        	}
	        else
        	    	retry--;
    	}
	close(sock_dd);
	return 1;
}
/* End of discovery */

void
get_name_from_dhcp_lease(unsigned char *mac, char *dev_name)
{
        FILE *fp;
        char line[256], dev_mac[18];
        char *hwaddr, *ipaddr, *name, *next, *ret;
        unsigned int expires;

        if (!nvram_get_int("dhcp_enable_x"))
                return;

        sprintf(dev_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                *mac,*(mac+1),*(mac+2),*(mac+3),*(mac+4),*(mac+5));

	NMP_DEBUG_F("Check dhcp lease table\n");
        /* Read leases file */
        if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
                return;

        while ((next = fgets(line, sizeof(line), fp)) != NULL) {
                /* line should start from numeric value */
                if (sscanf(next, "%u ", &expires) != 1)
                        continue;

                strsep(&next, " ");
                hwaddr = strsep(&next, " ") ? : "";
                ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if(!strcmp(dev_mac, hwaddr)) {
			NMP_DEBUG_F("Find the same MAC(%s)! copy device name\n", dev_mac);
			strlcpy(dev_name, name, 16);
			break;
		}
	}
	fclose(fp);

	return;
}
void toLowerCase(char *str) {
    char *p;

    for(p=str;*p!='\0';p++)
        if('A'<=*p&&*p<='Z')*p+=32;

}

int FindAllApp(unsigned char *src_ip, P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab)
{
 	unsigned char *dest_ip = p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->detail_info_num];
	int found_type = 0, ret = 0;
        UCHAR account[32];
        UCHAR primarydomain[32];
        UCHAR nativeOS[32];
        UCHAR nativeLanMan[32];
	int lock;

        NMP_DEBUG_F("*FindAllApp: %d -> %d.%d.%d.%d-%02X:%02X:%02X:%02X:%02X:%02X\n",p_client_detail_info_tab->detail_info_num,
                p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->detail_info_num][0],
		p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->detail_info_num][1],
		p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->detail_info_num][2],
		p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->detail_info_num][3],
 	        p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num][0],
                p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num][1],
                p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num][2],
                p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num][3],
                p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num][4],
                p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num][5]
	);

	//NBSS Called and Calling Name
        UCHAR des_hostname[16] = {
                                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
        UCHAR my_hostname[16] = {
                                0x43, 0x48, 0x49, 0x4E, 0x45, 0x53, 0x45, 0x2d,
                                0x43, 0x45, 0x52, 0x45, 0x52, 0x45, 0x52, 0x00};
	MY_DEVICE_INFO my_dvinfo;

        memcpy(account, "root", 4);
        memcpy(primarydomain, "WORKGROUP", 9);
        memcpy(nativeOS, "Linux", 5);
        memcpy(nativeLanMan, "Samba", 5);

	//ASUS Device Discovery
	//Asus_Device_Discovery(src_ip, dest_ip, p_client_detail_info_tab);

        //http service detect
        ret = SendHttpReq(dest_ip);
	lock = file_lock("networkmap");
	if(ret) {
		NMP_DEBUG_F("Found HTTP\n");
		p_client_detail_info_tab->http[p_client_detail_info_tab->detail_info_num] = 1;
	}
	else {
		NMP_DEBUG_F("HTTP Not Found\n");
		p_client_detail_info_tab->http[p_client_detail_info_tab->detail_info_num] = 0;
	}
	file_unlock(lock);
	if(scan_count==0) //leave when click refresh
		return 0;

        //printer server detect
        ret = lpd515(dest_ip);
	if(!ret) {
		lock = file_lock("networkmap");
                NMP_DEBUG_F("LPR Printer Server found!\n");
                p_client_detail_info_tab->printer[p_client_detail_info_tab->detail_info_num] = 1;
		file_unlock(lock);
        }
        else {
		ret = raw9100(dest_ip);
		lock = file_lock("networkmap");
		if(!ret) {
                	NMP_DEBUG_F("RAW Printer Server found!\n");
                	p_client_detail_info_tab->printer[p_client_detail_info_tab->detail_info_num] = 2;
        	}
        	else {
                	NMP_DEBUG_F("Printer Server not found!\n");
                	p_client_detail_info_tab->printer[p_client_detail_info_tab->detail_info_num] = 0;
        	}
		file_unlock(lock);
	}
        if(scan_count==0) //leave when click refresh
                return 0;

        //iTune Server detect
        ret = send_mdns_packet_ipv4(src_ip, dest_ip);
	lock = file_lock("networkmap");
	if(ret) {
		NMP_DEBUG_F("Found iTune Server!\n");
                p_client_detail_info_tab->itune[p_client_detail_info_tab->detail_info_num] = 1;
        }
        else {
                NMP_DEBUG_F("No iTune Server!\n");
                p_client_detail_info_tab->itune[p_client_detail_info_tab->detail_info_num] = 0;
        }
	file_unlock(lock);
        if(scan_count==0) //leave when click refresh
                return 0;

	if(p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num]==0) {
            if( ctrlpt(dest_ip) )//UPNP detect
            {
		lock = file_lock("networkmap");
		NMP_DEBUG_F("Find UPnP device: description= %s, modelname= %s\n",description.description, description.modelname);
	        //parse description
	        toLowerCase(description.description);
		if( strstr(description.description, "router")!=NULL )
        	{
                	p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 2;
        	}
	        else if( (strstr(description.description, "ap")!=NULL) ||
        	         (strstr(description.description, "access pointer")!=NULL) ||
			 (strstr(description.description, "wireless device")!=NULL) )
	        {
        	        p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 3;
	        }
		else if( strstr(description.description, "nas") )
        	{
                	p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 4;
        	}
	        else if( strstr(description.description, "cam") )
	        {
        	        p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 5;
	        }
                else if( strstr(description.modelname, "Xbox") )
                {
                        p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 8;
                }

		//Copy modelname to device name if exist.
		if(strcmp("",description.modelname) &&
		  !strcmp("",p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num])) {
			strncpy(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num], description.modelname, 15);
			p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num][15]='\0';
		}
		file_unlock(lock);
	    }
	    else 
		NMP_DEBUG_F("UPnP no response!\n");
	}
        if(scan_count==0) //leave when click refresh
                return 0;

        //nbns name query
	NMP_DEBUG_F("NBNS Name Query...\n");
       	Nbns_query(src_ip, dest_ip, p_client_detail_info_tab);
        if(scan_count==0) //leave when click refresh
                return 0;

	if(p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num]==0)
	{
        	//Check SMB data
        	NMP_DEBUG_F("Check Samba... \n");
        	memcpy(des_hostname, NetBIOS_name, 16);
		strcpy(my_hostname, MODEL_NAME);
	       	my_dvinfo.des_hostname= des_hostname;
        	my_dvinfo.des_hostname_len = 16;
	        my_dvinfo.my_hostname = my_hostname;
	        my_dvinfo.my_hostname_len = 16;
        	my_dvinfo.account= account;
	        my_dvinfo.account_len = 4;
        	my_dvinfo.primarydomain= primarydomain;
	        my_dvinfo.primarydomain_len = 9;
        	my_dvinfo.nativeOS= nativeOS;
	        my_dvinfo.nativeOS_len = 5;
        	my_dvinfo.nativeLanMan= nativeLanMan;
	        my_dvinfo.nativeLanMan_len = 5;
		bzero(SMB_OS, sizeof(SMB_OS));
	        bzero(SMB_PriDomain, sizeof(SMB_PriDomain));
		sleep(1);

		lock = file_lock("networkmap");
		if(!SendSMBReq(dest_ip, &my_dvinfo))
        	{
			if( strstr(SMB_OS, "Windows")!=NULL )
			{
				p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 1;
				NMP_DEBUG_F("Find: PC!\n");
			}
                	else if( strstr(SMB_PriDomain, "NAS")!=NULL )
			{
                        	p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num] = 4;
				NMP_DEBUG_F("Find: NAS Server!\n");
			}
                }
		file_unlock(lock);
	}

	if(!strcmp("", p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num])) {
		get_name_from_dhcp_lease(p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->detail_info_num],
					 p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
		fixstr(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
		NMP_DEBUG_F("Get device name from dhcp lease: %s\n", 
		p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
	}

	return 1;
}
