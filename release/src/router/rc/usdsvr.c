/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <bcmnvram.h>

#define MAXSENDLEN 256
#define PORT_DEVICEINFO 7031
#define PORT_CLIENTMSG 7032
#define LAN_DEV "br0"

int sockfd_broadcast;
int sockfd_unicast;

void sendInfo(char *buf, int len, int port)
{
	struct sockaddr_in clntaddr;
	int sendbytes;

	clntaddr.sin_family = AF_INET;
	clntaddr.sin_port = htons(port);
	clntaddr.sin_addr.s_addr = inet_addr("255.255.255.255");

	if (setsockopt(sockfd_broadcast, SOL_SOCKET, SO_BINDTODEVICE, LAN_DEV, IFNAMSIZ) != 0)
	{
		fprintf(stderr, "setsockopt error: %s\n", LAN_DEV);
		perror("setsockopt set:");
	}

	sendbytes = sendto(sockfd_broadcast, buf, len, 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr));

	if (setsockopt(sockfd_broadcast, SOL_SOCKET, SO_BINDTODEVICE, "", IFNAMSIZ) != 0)
	{
		fprintf(stderr, "setsockopt error: %s\n", "");
		perror("setsockopt reset:");
	}
}

void catch_sig(int sig)
{
	int sendbytes;
	char sendbuffer[MAXSENDLEN + 1];

	if (sig == SIGUSR1)
	{
		memset(sendbuffer, 0, MAXSENDLEN + 1);
		// response format: "USB device name","current status of the device","IP of cccupied user"
		if (!nvram_safe_get("u2ec_device"))
			sprintf(sendbuffer, "\"%s\",\"%s\"", "", "");
		else if (nvram_invmatch("u2ec_busyip", ""))
			sprintf(sendbuffer, "\"%s\",\"%s\",\"%s\"", nvram_safe_get("u2ec_device"), "Busy", nvram_safe_get("u2ec_busyip"));
		else
			sprintf(sendbuffer, "\"%s\",\"%s\"", nvram_safe_get("u2ec_device"), "Idle");
		sendbytes = (strlen(sendbuffer) > MAXSENDLEN) ? MAXSENDLEN : strlen(sendbuffer);

		sendInfo(sendbuffer, sendbytes, PORT_DEVICEINFO);
	}
	else if (sig == SIGUSR2)
	{
		memset(sendbuffer, 0, MAXSENDLEN + 1);
		strcpy(sendbuffer, nvram_safe_get("u2ec_msg_broadcast"));
		sendbytes = (strlen(sendbuffer) > MAXSENDLEN) ? MAXSENDLEN : strlen(sendbuffer);

		sendInfo(sendbuffer, sendbytes, PORT_CLIENTMSG);
	}
	else if (sig == SIGTSTP)
	{
		struct sockaddr_in clntaddr;

		clntaddr.sin_family = AF_INET;
		clntaddr.sin_port = htons(PORT_CLIENTMSG);
		clntaddr.sin_addr.s_addr = inet_addr(nvram_safe_get("u2ec_clntip_unicast"));

		memset(sendbuffer, 0, MAXSENDLEN + 1);
		sprintf(sendbuffer, nvram_safe_get("u2ec_msg_unicast"));
		sendbytes = (strlen(sendbuffer) > MAXSENDLEN) ? MAXSENDLEN : strlen(sendbuffer);

		sendto(sockfd_unicast, sendbuffer, sendbytes, 0, (struct sockaddr *)&clntaddr, sizeof(clntaddr));
	}
}

int usdsvr_broadcast()
{
	FILE *fp;

	/* write pid */
	if ((fp=fopen("/var/run/usdsvr_broadcast.pid", "w"))!=NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	int broadcast = 1;
	struct sockaddr_in servaddr;

	bzero((char *)&servaddr , sizeof(servaddr));
	servaddr.sin_family		= AF_INET;
	servaddr.sin_addr.s_addr	= htonl(INADDR_ANY);
	servaddr.sin_port		= htons(PORT_DEVICEINFO);

	if ((sockfd_broadcast = socket(AF_INET, SOCK_DGRAM,0)) < 0 )
	{
		perror("socket: SOCK_DGRAM");
		exit(0);
	}

	if (setsockopt(sockfd_broadcast , SOL_SOCKET , SO_BROADCAST ,(char *)&broadcast,sizeof(broadcast)) == -1)
	{
		perror("setsockopt: SO_BROADCAST");
		exit(0);
	}
    
	if (bind(sockfd_broadcast,(struct sockaddr *)&servaddr , sizeof(servaddr)) < 0 )
	{
		perror("bind: sockfd_broadcast");
		exit(0);
    	}

	signal(SIGUSR1, catch_sig);
	signal(SIGUSR2, catch_sig);

	while (1)
	{
		pause();
	}

	return 0;
}

int usdsvr_unicast()
{
	FILE *fp;

	/* write pid */
	if ((fp=fopen("/var/run/usdsvr_unicast.pid", "w"))!=NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	struct sockaddr_in servaddr;

	bzero((char *)&servaddr , sizeof(servaddr));
	servaddr.sin_family		= AF_INET;
	servaddr.sin_addr.s_addr	= htonl(INADDR_ANY);
	servaddr.sin_port		= htons(PORT_CLIENTMSG);

	if ((sockfd_unicast = socket(AF_INET, SOCK_DGRAM,0)) < 0 )
	{
		perror("socket: SOCK_DGRAM");
		exit(0);
	}
    
	if (bind(sockfd_unicast,(struct sockaddr *)&servaddr , sizeof(servaddr)) < 0 )
	{
		perror("bind: sockfd_unicast");
		exit(0);
    	}

	signal(SIGTSTP, catch_sig);

	while (1)
	{
		pause();
	}

	return 0;
}
