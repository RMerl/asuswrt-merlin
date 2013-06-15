/***************************************
	wsc main function
****************************************/
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h> 
#include <sys/ioctl.h> 
	   

#include <pthread.h>
#include "wsc_common.h"
//#include "wsc_netlink.h"
#include "wsc_msg.h"
#include "wsc_upnp.h"
#include "upnp.h"

int wsc_debug_level = RT_DBG_OFF;

int ioctl_sock = -1;
unsigned char UUID[16]= {0};

int enableDaemon = 0;
	
int stopThread = 1;
static int wscUPnPSDKInit = FALSE;
static int wscUPnPDevInit = FALSE;
static int wscUPnPCPInit = FALSE;
static int wscK2UMInit = FALSE;
static int wscU2KMInit = FALSE;
static int wscMsgQInit = FALSE;


int WscUPnPOpMode;
char HostDescURL[WSC_UPNP_DESC_URL_LEN] = {0};	// Used to save the DescriptionURL of local host.
unsigned char HostMacAddr[MAC_ADDR_LEN]={0};			// Used to save the MAC address of local host.
unsigned int HostIPAddr;								// Used to save the binded IP address of local host.
char WSC_IOCTL_IF[IFNAMSIZ];

#ifdef MULTIPLE_CARD_SUPPORT
#define FILE_PATH_LEN	256
static char pid_file_path[FILE_PATH_LEN];
#endif // MULTIPLE_CARD_SUPPORT //

void usage(void)
{
	printf("Usage: wscd [-i infName] [-a ipaddress] [-p port] [-f descDoc] [-w webRootDir] -m UPnPOpMode -D [-d debugLevel] -h\n");
	printf("\t-i:	   Interface name this daemon will run wsc protocol(if not set, will use the default interface name - ra0)\n");
	printf("\t\t\te.g.: ra0\n");
	printf("\t-a:       IP address of the device (if not set, will auto assigned)\n");
	printf("\t\t\t e.g.: 192.168.0.1\n");
	printf("\t-p:       Port number for receiving UPnP messages (if not set, will auto assigned)\n");
	printf("\t\t\t e.g.: 54312\n");
	printf("\t-f:       Name of device description document (If not set, will used default value)\n");
	printf("\t\t\t e.g.: WFADeviceDesc.xml\n");
	printf("\t-w:       Filesystem path where descDoc and web files related to the device are stored\n");
	printf("\t\t\t e.g.: /etc/xml/\n");
	printf("\t-m:       UPnP system operation mode\n");
	printf("\t\t\t 1: Enable UPnP Device service(Support Enrolle or Proxy functions)\n");
	printf("\t\t\t 2: Enable UPnP Control Point service(Support Registratr function)\n");
	printf("\t\t\t 3: Enable both UPnP device service and Control Point services.\n");
	printf("\t-D:       Run program in daemon mode.\n");
	printf("\t-d:       Debug level\n");
	printf("\t\t\t 0: debug printing off\n");
	printf("\t\t\t 1: DEBUG_ERROR\n");
	printf("\t\t\t 2: DEBUG_PKT\n");
	printf("\t\t\t 3: DEBUG_INFO\n");
	
	exit(1);

}


void sig_handler(int sigNum)
{
	sig_atomic_t status;

	DBGPRINTF(RT_DBG_INFO, "Get a signal=%d!\n", sigNum);
	switch(sigNum)
	{
		case SIGCHLD:
			wait3(&status, WNOHANG, NULL);
			break;
		default:
			break;
	}

}

int wscSystemInit(void)
{
	int retVal;
	
	struct sigaction sig;
	memset(&sig, 0, sizeof(struct sigaction));
	sig.sa_handler= &sig_handler;
	sigaction(SIGCHLD, &sig, NULL);
	
	retVal = wscMsgSubSystemInit();
	
	return retVal;
}


int wscSystemStop(void)
{
	/* Stop the K2U and U2K msg subsystem */
	if (wscU2KMInit && ioctl_sock >= 0) 
	{
		// Close the ioctl socket.
		close(ioctl_sock);
		ioctl_sock = -1;
	}
	stopThread = 1;
	
	if (wscK2UMInit)
	{

	}

	if(wscUPnPDevInit)
		WscUPnPDevStop();
	if(wscUPnPCPInit)
		WscUPnPCPStop();
	
	if(wscUPnPSDKInit)
		UpnpFinish();

	if( wscMsgQInit)
	{
		wscMsgSubSystemStop();
	}
	exit(0);
}

/******************************************************************************
 * WscDeviceCommandLoop
 *
 * Description: 
 *       Function that receives commands from the user at the command prompt
 *       during the lifetime of the device, and calls the appropriate
 *       functions for those commands. Only one command, exit, is currently
 *       defined.
 *
 * Parameters:
 *    None
 *
 *****************************************************************************/
void *
WscDeviceCommandLoop( void *args )
{
    int stoploop = 0;
    char cmdline[100];
    char cmd[100];

    while( !stoploop ) {
        sprintf( cmdline, " " );
        sprintf( cmd, " " );

		printf( "\n>> " );

        // Get a command line
        fgets( cmdline, 100, stdin );

        sscanf( cmdline, "%s", cmd );

        if( strcasecmp( cmd, "exit" ) == 0 ) {
			printf( "Shutting down...\n" );
			wscSystemStop();
        } else {
			printf("\n   Unknown command: %s\n\n", cmd);
			printf("   Valid Commands:\n" );
			printf("     Exit\n\n" );
        }

    }

    return NULL;
}


/******************************************************************************
 * main
 *
 * Description: 
 *       Main entry point for WSC device application.
 *       Initializes and registers with the sdk.
 *       Initializes the state stables of the service.
 *       Starts the command loop.
 *
 * Parameters:
 *    int argc  - count of arguments
 *    char ** argv -arguments. The application 
 *                  accepts the following optional arguments:
 *
 *          -ip 	ipAddress
 *          -port 	port
 *		    -desc 	descDoc
 *	        -webdir webRootDir"
 *		    -help 
 *          -i      ioctl binding interface.
 *
 *****************************************************************************/
int main(int argc, char **argv)
{
	unsigned int portTemp = 0;
	char *ipAddr = NULL, *descDoc = NULL, *webRootDir = NULL, *infName = NULL;
	unsigned short port = 0;
	int sig;
	sigset_t sigs_to_catch;
	FILE *fp;
	int retVal;
	int opt;

	if (argc < 2)
		usage();
	
	/* first, parsing the input options */
	while((opt = getopt(argc, argv, "a:i:p:f:w:m:d:Dh"))!= -1)
	{
		switch (opt)
		{
			case 'a':
				ipAddr = optarg;
				break;
			case 'p':
				sscanf(optarg, "%u", &portTemp);
				break;
			case 'f':
				descDoc = optarg;
				break;
			case 'i':
				infName = optarg;
				memset(&WSC_IOCTL_IF[0], 0, IFNAMSIZ);
				if (strlen(infName))
					strncpy(&WSC_IOCTL_IF[0], infName, IFNAMSIZ);
				else
					strcpy(&WSC_IOCTL_IF[0], "ra0");
				break;
			case 'w':
				webRootDir = optarg;
				break;
			case 'm':
				WscUPnPOpMode = strtol(optarg, NULL, 10);
				if (WscUPnPOpMode < UPNP_OPMODE_DEV || WscUPnPOpMode > UPNP_OPMODE_BOTH)
					usage();
				break;
			case 'D':
			enableDaemon = 1;
				break;
			case 'd':
				wsc_debug_level = strtol(optarg, NULL, 10);
				break;
			case 'h':
				usage();
				break;
        }
    }
	
	if ((WscUPnPOpMode < 1) || (WscUPnPOpMode > 3))
	{
		fprintf(stderr, "Wrong UPnP Operation Mode: %d\n", WscUPnPOpMode);
		usage();
	}
	if ((wsc_debug_level > RT_DBG_ALL)  || (wsc_debug_level < RT_DBG_OFF))
	{
		fprintf(stderr, "Wrong Debug Level: %d\n", wsc_debug_level);
		usage();
	}				
    port = (unsigned short)portTemp;

	if (enableDaemon)
	{
		pid_t childPid;

		childPid = fork();
		if(childPid < 0)
		{
			fprintf(stderr, "Run in deamon mode failed --ErrMsg=%s!\n", strerror(errno));
			exit(0);
		} else if (childPid >0)
			exit(0);
		else {
			close(0);
			close(1);
		}	
	}
		
	// Write the pid file
#ifdef MULTIPLE_CARD_SUPPORT
	memset(&pid_file_path[0], 0, FILE_PATH_LEN);
	sprintf(pid_file_path, "%s.%s", DEFAULT_PID_FILE_PATH, WSC_IOCTL_IF);
	DBGPRINTF(RT_DBG_INFO, "The pid file is: %s!\n", pid_file_path);
	if ((fp = fopen(pid_file_path, "w")) != NULL)
#else
	if((fp = fopen(DEFAULT_PID_FILE_PATH, "w"))!= NULL)
#endif // MULTIPLE_CARD_SUPPORT //
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
	
	/* Systme paramters initialization */
	if (wscSystemInit() == WSC_SYS_ERROR){
		fprintf(stderr, "wsc MsgQ System Initialization failed!\n");
		goto STOP;
	}
	wscMsgQInit = TRUE;
	
	/* Initialize the netlink interface from kernel space */
	if(wscK2UModuleInit() != WSC_SYS_SUCCESS)
	{	
		fprintf(stderr, "creat netlink socket thread failed!\n");
		goto STOP;
	} else {
		DBGPRINTF(RT_DBG_INFO, "Create netlink socket thread success!\n");
		wscK2UMInit = TRUE;
	}
	
	/* Initialize the ioctl interface for data path to kernel space */
	ioctl_sock = wscU2KModuleInit();
	if(ioctl_sock == -1)
	{
		fprintf(stderr, "creat ioctl socket failed!err=%d!\n", errno);
		goto STOP;
	}else {
		DBGPRINTF(RT_DBG_INFO, "Create ioctl socket(%d) success!\n", ioctl_sock);
		wscU2KMInit = TRUE;
	}

	/* Initialize the upnp related data structure and start upnp service */
	if(WscUPnPOpMode)
	{
		struct ifreq  ifr;
			
		// Initializing UPnP SDK
		if ((retVal = UpnpInit(ipAddr, port)) != UPNP_E_SUCCESS)
		{
			DBGPRINTF(RT_DBG_ERROR, "Error with UpnpInit -- %d\n", retVal);
			UpnpFinish();
			goto STOP;
		}
		wscUPnPSDKInit = TRUE;

		// Get the IP/Port the UPnP services want to bind.
		if (ipAddr == NULL)
			ipAddr = UpnpGetServerIpAddress();
		if (port == 0)
			port = UpnpGetServerPort();
		inet_aton(ipAddr, (struct in_addr *)&HostIPAddr);

		// Get the Mac Address of wireless interface
		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, WSC_IOCTL_IF); 
		if (ioctl(ioctl_sock, SIOCGIFHWADDR, &ifr) > 0) 
        { 
			perror("ioctl to get Mac Address");
			goto STOP;
		}
		memcpy(HostMacAddr, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);
			
		DBGPRINTF(RT_DBG_INFO, "UPnP Initialized\n \t IP-Addr: %s Port: %d\n", ipAddr, port);
		DBGPRINTF(RT_DBG_INFO, "\t HW-Addr: %02x:%02x:%02x:%02x:%02x:%02x!\n", HostMacAddr[0], HostMacAddr[1], 
				HostMacAddr[2], HostMacAddr[3], HostMacAddr[4], HostMacAddr[5]);

		// Start UPnP Device Service.
		if (WscUPnPOpMode & UPNP_OPMODE_DEV)
		{	  
		    retVal = WscUPnPDevStart(ipAddr, port, descDoc, webRootDir);
			if (retVal != WSC_SYS_SUCCESS)
				goto STOP;
			port++;
			wscUPnPDevInit = TRUE;
		}

		// Start UPnP Control Point Service.
		if(WscUPnPOpMode & UPNP_OPMODE_CP)
		{
			retVal = WscUPnPCPStart(ipAddr, port);
			if (retVal != WSC_SYS_SUCCESS)
				goto STOP;
			wscUPnPCPInit = TRUE;
		}
	}

    /* Catch Ctrl-C and properly shutdown */
    sigemptyset(&sigs_to_catch);
    sigaddset(&sigs_to_catch, SIGINT);
//YYHuang@Ralink 07/11/06
    sigaddset(&sigs_to_catch, SIGTERM);
    sigwait(&sigs_to_catch, &sig);

    DBGPRINTF(RT_DBG_INFO, "Shutting down on signal %d...\n", sig);
    wscSystemStop();
	
STOP:

	// Trigger other thread to stop the procedures.
	stopThread = 1;
#ifdef MULTIPLE_CARD_SUPPORT
	unlink(pid_file_path);
#else
	unlink(DEFAULT_PID_FILE_PATH);
#endif // MULTIPLE_CARD_SUPPORT //

    exit(0);
}

