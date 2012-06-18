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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/lp.h>
#include <fcntl.h>
#include "wlancom.h"
#include "lp_asus.h"
/*Lisa*/
#include	<stdlib.h>
#include	<getopt.h>
#include	<ctype.h>
#include	<fcntl.h>
#include	<netdb.h>
#include	<syslog.h>
#include	<sys/resource.h>
#include	<sys/stat.h>
#include 	<sys/ioctl.h>
#define		BASEPORT	9100
#ifdef 		DEBUG
#define		PRINT(...)
#else
#define		PRINT	printf
#endif
#define 	Raw_Printing_with_ASUS
#define 	LOGOPTS		(LOG_PERROR|LOG_PID|LOG_LPR|LOG_ERR)

//JY
#include "lp.h"
#define MAX(a, b) ((a) > (b) ? (a) : (b))//JY1112


extern int          errno;


#ifdef LPR_with_ASUS//JY1112
#define PNT_SVR_PORT_ASUS    3838
#endif
#define PNT_SVR_PORT_LPR 515
#define PRINT printf

#define UCHAR unsigned char

#define STATUS_FILE "/var/state/parport/svr_status"


int processReq(int sockfd); //Process the request from the client side
int closesocket(int sockfd); 
void sig_child(int sig);    //signal handler to the dead of child process
void sig_cleanup(int sig); 
void sig_remove(int sig);//JY1110 
//void checkstatus_usb_par();//JY1110
int waitsock(int sockfd , int sec , int usec);  //wait to socket until timeout
void reset_printer(int sec);
int check_par_usb_prn();//JY: 20031104 change to int from void
DWORD RECV(int sockfd , PBYTE pRcvbuf , DWORD dwlen , DWORD timeout);

int                 fdPRN=0; //File descriptor of the printer port
char				g_useUsb = FALSE;
char 				busy = FALSE; //Add by Lisa
main()
{
    int                 sockfd , clisockfd;
    unsigned int        clilen;
    int                 childpid;
    struct sockaddr_in  serv_addr,cli_addr;
#ifdef LPR_with_ASUS//JY1112
    int 		LPRflag = 0;
    fd_set		rfds, afds;
    int			nfds,nfds1;
    int			sockfd_ASUS;
    unsigned int        clilen_ASUS;
    int                 childpid_ASUS;
    struct sockaddr_in  serv_addr_ASUS,cli_addr_ASUS;
#endif
#ifdef Raw_Printing_with_ASUS  //Lisa
	int		netfd, fd, clientlen, one = 1;
	struct sockaddr_in	netaddr, client;
#endif

    //Initial the server the not busy
    lptstatus.busy = FALSE; 
 
    //Setup the signal handler
    signal(SIGCLD, sig_child); 
    signal(SIGINT, sig_cleanup); 
    signal(SIGQUIT, sig_cleanup); 
    signal(SIGKILL, sig_cleanup);
    signal(SIGUSR2, sig_remove);//JY1110 
    
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0 )
    {
        perror("can't open stream socket:");
        exit(0);
    }
    
    bzero((char *)&serv_addr , sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
    serv_addr.sin_port          = htons(PNT_SVR_PORT_LPR);

    
    if(bind(sockfd,(struct sockaddr *)&serv_addr , sizeof(serv_addr)) < 0 )
    {
        perror("can't bind:");
        exit(0);
    }
/*JY1111*/
int windowsize=2920;
setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize));

#if 1
		int currentpid=getpid();
		FILE *pidfileread;

		if((pidfileread=fopen("/var/run/lpdparent.pid", "r")) == NULL){
			pidfileread=fopen("/var/run/lpdparent.pid", "w");
			fprintf(pidfileread, "%d", currentpid);
			fclose(pidfileread);
        	}
                else{
			printf("another lpd daemon exists!!\n");
			fclose(pidfileread);
                	exit(0);
                }
#endif
/*JY1110
	int testusb = 0;
	testusb = check_par_usb_prn();
	if(testusb){
		printf("USB\n");//JY1112delete
		fd_print=open("/dev/usb/lp0", O_RDWR);
	}
	else{
		printf("PARALLEL\n");//JY1112delete
		fd_print=open("/dev/lp0", O_RDWR);
	}
	checkstatus_usb_par();
	close(fd_print);
111111*/
    
	listen(sockfd , 15);

#ifdef Raw_Printing_with_ASUS //Lisa
	if ((netfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
	{
//		syslog(LOGOPTS, "socket: %m\n");
		exit(1);
	}
	if (setsockopt(netfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
	{
//		syslog(LOGOPTS, "setsocketopt: %m\n");
		exit(1);
	}
	netaddr.sin_port = htons(BASEPORT);
	netaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(netaddr.sin_zero, 0, sizeof(netaddr.sin_zero));
	if (bind(netfd, (struct sockaddr*) &netaddr, sizeof(netaddr)) < 0)
	{
//		syslog(LOGOPTS, "bind: %m\n");
		exit(1);
	}
	if (listen(netfd, 5) < 0)
	{
//		syslog(LOGOPTS, "listen: %m\n");
		exit(1);
	}
	//clientlen = sizeof(client);
	//memset(&client, 0, sizeof(client));
#endif


#ifdef LPR_with_ASUS//JY1112
	if((sockfd_ASUS = socket(AF_INET,SOCK_STREAM,0)) < 0 )
	{
	        perror("can't open stream socket:");
	        exit(0);
	}
	bzero((char *)&serv_addr_ASUS , sizeof(serv_addr_ASUS));
	serv_addr_ASUS.sin_family        = AF_INET;
	serv_addr_ASUS.sin_addr.s_addr   = htonl(INADDR_ANY);
	serv_addr_ASUS.sin_port          = htons(PNT_SVR_PORT_ASUS);

	if(bind(sockfd_ASUS,(struct sockaddr *)&serv_addr_ASUS , sizeof(serv_addr_ASUS)) < 0 )
	{
	        perror("can't bind:");
		exit(0);
	}

	setsockopt(sockfd_ASUS, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize));

	listen(sockfd_ASUS , 15);
    
    /*set the fds*/
	nfds1=MAX(sockfd, sockfd_ASUS);
#ifdef Raw_Printing_with_ASUS //Lisa
	nfds=MAX(nfds1, netfd);
#endif
	FD_ZERO(&afds);
#endif


    
    while(TRUE)
    {
#ifdef Raw_Printing_with_ASUS //Lisa
	FD_SET(netfd, &afds);
#endif
#ifdef LPR_with_ASUS//JY1112
	FD_SET(sockfd, &afds);
	FD_SET(sockfd_ASUS, &afds);

	int err_select;//JY1113
	memcpy(&rfds, &afds, sizeof(rfds));
	if((err_select=select(nfds+1, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0 )) < 0) {
//JY1120		printf("select error on sockfd: error=%d\n", errno);
		/**/
//		printf("sockfd_FD_ISSET=%d\n", FD_ISSET(sockfd, &rfds));
//JY1120		printf("sockfd_ASUS FD_ISSET=%d\n", FD_ISSET(sockfd_ASUS, &rfds));
		/**/
//		if(errno != 4)//JY1113: delete
			continue;
	}
#endif
        clilen      = sizeof(cli_addr);
	
	if (busy == FALSE) //Add by Lisa
	{
		busy = TRUE; // Add by Lisa
#ifdef LPR_with_ASUS//JY1112 
	if(FD_ISSET(sockfd, &rfds))
	{
		LPRflag = 1;
		clisockfd   = accept(sockfd,(struct sockaddr *)&cli_addr, &clilen);
//JY1120		
		printf("LPR sock received...\n");//JY1114: delete
	}
	else if(FD_ISSET(sockfd_ASUS, &rfds))
	{
		LPRflag = 0;
		clisockfd   = accept(sockfd_ASUS,(struct sockaddr *)&cli_addr, &clilen);
//JY1120		
		printf("ASUSRemote port sock receved...\n");//JY1114: delete
	}
#ifdef Raw_Printing_with_ASUS //Lisa
	else if(FD_ISSET(netfd, &rfds))
	{
		LPRflag = 2;
		clisockfd = accept(netfd, (struct sockaddr*) &cli_addr, &clilen);
		printf("Raw Printing sock received...\n");
	}
#endif
	else{
		perror("lpr: no data received...");
		continue;
	}

#else
        clisockfd   = accept(sockfd,(struct sockaddr *)&cli_addr, &clilen);
#endif
	strcpy(clientaddr , inet_ntoa(cli_addr.sin_addr));
	if(clisockfd < 0)
        {
            perror("accept error:");
            continue;
        }
        
        //remark PRINT("Accept OK ,client address -- %s\n",inet_ntoa(cli_addr.sin_addr)); 
        
        if( (childpid = fork() ) < 0)
        {
            perror("fork error:");
        }
        else if(childpid == 0) 
        {
		printf("child start\n");
            //child process starts here
//#ifdef LPR_SUPPORT
//JY1120		printf("LPR process....\n");
#ifdef LPR_with_ASUS//JY1114 
//JY1120		printf("lptstatus.busy=%s\n", lptstatus.busy);//JY1114
//JY1120		printf("lptstatus.pid=%d\n", lptstatus.pid);//JY1114
		if(LPRflag == 1)
		{
			printf("LPRflag_remote=%d \n",LPRflag);
			processReq_LPR(clisockfd);
		}
#ifdef Raw_Printing_with_ASUS //Lisa
		else if (LPRflag == 2)
		{
			printf("LPRflag_raw=%d \n",LPRflag);
			processReq_Raw(clisockfd);
		}
#endif
		else
#endif
		{
			printf("LPRflag_LPR=%d \n",LPRflag);
			processReq(clisockfd); 
		}
			
//#else
//            processReq(clisockfd); 
//#endif
            //PRINT("client exit\n");
		if(LPRflag == 1)
			printf("LPR client exit\n");//JY1113delete
		else if (LPRflag == 2)
			printf("Raw Printing client exit\n");
		else
			printf("ASUS Remote client exit\n");//JY1113delete
//		check_prn_status(ONLINE, "");
		close(sockfd);//
		close(sockfd_ASUS);//
        	exit(0);
        }
       
        
        //parents process goes on here
        //remark PRINT("fork -- childpid %d\n",childpid);
        
        if(lptstatus.busy == FALSE)
        {
            //someone is using the printer now
            //lptstatus.busy = TRUE;
            
            //record the infomation of the client process
            strcpy(lptstatus.addr , inet_ntoa(cli_addr.sin_addr));
            lptstatus.pid = childpid;
        }
	}
        //PRINT("lptstatus.addr %s\n",lptstatus.addr);
                    
        close(clisockfd);
    }

}


int processReq(int sockfd)
{
    
    char                recv_buf[4]; //buffer to receive header
    static char         waittime = 1;
    int                 iCount;
    struct print_buffer pbuf;
    char                chPortOpened = FALSE;
    
    /***************************************/
    /**  We reset the printer only when   **/
    /**  user wants to cancel a job or    **/
    /**  error occurs                     **/
    /***************************************/
    
    //Process the request from cleint 
    //return when error or job complete
    while(TRUE)
    {
        LPT_CMD_PKT_HDR     *pHdrCmd  = NULL;
        LPT_DATA_PKT_HDR    *pHdrData = NULL;
        LPT_RES_PKT_HDR     pktRes;
        WORD                body[1];
        int                 rcv; //records how many bytes being received
        char                *para_buf = NULL; //buffer to store parameter
        
        memset(recv_buf,0,sizeof(recv_buf));
        iCount = sizeof(recv_buf);

        if(waittime < 5)
        {
            waittime ++;
        }

        //Receive the complete header here
        while( iCount > 0 )
        {
            rcv = RECV(sockfd , recv_buf + (4 - iCount) , iCount , 60);

            if( rcv < 1)
            {
                //receive error
                PRINT("1. rcv -> %d\n",rcv);

                if(rcv < 0)
                {
                	perror("ERR:");             
                }
                
                closesocket(sockfd);
                if(chPortOpened == TRUE)
                {
                    reset_printer(10);
                }
                
                return 0;
            }
            
            iCount = iCount - rcv;
        }


        //Check Service ID

        switch(recv_buf[0])
        {
            case NET_SERVICE_ID_LPT_EMU:
                //PRINT("Service ID -> NET_SERVICE_ID_LPT_EMU \n");
                break;
            
            default:
                //PRINT("Service ID -> Not Supported \n");
                closesocket(sockfd);
                if(chPortOpened == TRUE)
                {
                    reset_printer(10);
                }
                return(0);
                break;
        }


        //Check Packet Type

        switch(recv_buf[1])
        {
            case NET_PACKET_TYPE_CMD:
                //PRINT(">>> TYPE_CMD ");
                pHdrCmd = (LPT_CMD_PKT_HDR *)recv_buf;
                break;
            
            case NET_PACKET_TYPE_RES:
                //We should not recevice Response Packet in Server
                //PRINT("Packet Type -> NET_PACKET_TYPE_RES Error!!! \n");
                closesocket(sockfd);
                if(chPortOpened == TRUE)
                {
                    reset_printer(10);
                }
                return(0);
                break;
            
            case NET_PACKET_TYPE_DATA:
                //PRINT("$$$ TYPE_DATA ");
                pHdrData = (LPT_DATA_PKT_HDR *)recv_buf;
                break;
            
            default:
                //PRINT("Packet Type -> Not Supported \n");
                closesocket(sockfd);
                if(chPortOpened == TRUE)
                {
                    reset_printer(10);
                }
                return(0);
                break;
        
        }

        if( pHdrCmd != NULL) 
        {
            //We receive command
            
            para_buf = NULL;

            iCount = pHdrCmd->ParaLength;

	    if (iCount!=0)
	    {
             	(void *)para_buf = malloc(pHdrCmd->ParaLength);
            }

	    PRINT("HdrCmd Length %d\n", iCount);

	    // para_buf may be NULL but this command still work
	    // 2004/06/07 by Joey
            if(iCount!=0 && para_buf == NULL)
            {
                perror("malloc error 1:");
                closesocket(sockfd);
                if(chPortOpened == TRUE)
                {
                    reset_printer(10);
                }
                return(0);
            }

            while( iCount > 0 )
            {
                rcv = RECV(sockfd , (para_buf + (pHdrCmd->ParaLength - iCount )) , iCount , 30);

                if( rcv < 1)
                {
                    //receive error
                    perror("2. RECV ERR:");             
                    closesocket(sockfd);
                    free(para_buf);
                    if(chPortOpened == TRUE)
                    {
                        reset_printer(10);
                    }
                    return 0;
                }
                
                iCount = iCount - rcv;
            }


            switch(pHdrCmd->CommandID)
            {
                case NET_CMD_ID_OPEN:
                    //remark PRINT("NET_CMD_ID_OPEN\n"); 
                                                                                                
                    /************************************/
                    /************************************/
                    /*** TODO: add code here to check ***/
                    /*** the printer status           ***/
                    /************************************/
                    /************************************/

                    pktRes.ServiceID = NET_SERVICE_ID_LPT_EMU;
                    pktRes.PacketType = NET_PACKET_TYPE_RES;
                    pktRes.CommandID = NET_CMD_ID_OPEN;
                    pktRes.ResLength = 2;

                    if(lptstatus.busy == FALSE)
                    {
                        int prnstatus=0;
			            FILE *statusFp = NULL;
                        
                        
                        //remark PRINT("--------lptstatus.busy == FALSE\n"); 
                        
                        /* add by James Yeh to support usb printer */
                        /* 2002/12/25 							   */
                        
                        
                        check_par_usb_prn();
                        
                        //Open printer port -modified by PaN
                        if(g_useUsb == TRUE)
                        {
                        	fdPRN = open("/dev/usb/lp0",O_RDWR);
                        }
                        else
                        {
                        	fdPRN = open("/dev/lp0",O_RDWR);
                        }

                        if(fdPRN == 0)
                        {
                            //Failed to open printer port
                            PRINT("Can not open lp0 errno -> %d \n",errno);
                            perror("ERR:");
    
                            //Send header
                            send(sockfd , (const char *)&pktRes , sizeof(pktRes) , 0);
                            //Send body
                            body[0] = ERR_SERVER_LPT_FAIL;
                            send(sockfd , body , sizeof(body) , 0);
                            free(para_buf);
                            return(0);
                        }
                        
                        ioctl(fdPRN,LPGETSTATUS,&prnstatus);
                        
                        if(prnstatus != 0)
                        {
                            //remark PRINT("prnstatus != 0\n"); 
                            body[0] = ERR_SERVER_OCCUPIED;
                            
                            /*******************************************************************************/
                            /* why we are using ERR_SERVER_OCCUPIED instead of ERR_SERVER_LPT_FAIL here ?? */
                            /* Because ERR_SERVER_OCCUPIED will let user try again & again. Using          */
                            /* ERR_SERVER_LPT_FAIL will fail the reques                                    */
                            /*******************************************************************************/
    
                            //Send header
                            send(sockfd , (const char *)&pktRes , sizeof(pktRes) , 0);
                            //Send body 
                            send(sockfd , body , sizeof(body) , 0);
                            free(para_buf);
                            return(0);
                        }
                        
			            statusFp = fopen(STATUS_FILE , "w");
            
            			if(statusFp != NULL)
            			{
                			fprintf(statusFp,"PRN_CLIENT=\"%s\"\n",lptstatus.addr);
                			fclose(statusFp);
            			}
            			else
            			{
                			perror("Open status file failed: ");
            			}

                        chPortOpened = TRUE;
                        
                        body[0] = ERR_SUCCESS;
                    }
                    else
                    {
                        //PRINT("*********lptstatus.busy == TRUE\n");
                        body[0] = ERR_SERVER_OCCUPIED;

                        //Send header
                        send(sockfd , (const char *)&pktRes , sizeof(pktRes) , 0);
                        //Send body 
                        send(sockfd , body , sizeof(body) , 0);
                        free(para_buf);
                        return(0);
                    }

                    //Send header
                    send(sockfd , (const char *)&pktRes , sizeof(pktRes) , 0);
                    //Send body 
                    send(sockfd , body , sizeof(body) , 0);

                    break;

                case NET_CMD_ID_CLOSE:
                {
                    char bCancel = FALSE;
                    
                     
                    //remark PRINT("NET_CMD_ID_CLOSE\n");

                    /*****************************************************/
                    /* Check if user normally or abnormally end this job */
                    /*                                 James 2002/06/05  */
                    /*****************************************************/
                    
                    if(pHdrCmd->ParaLength != 1) //Length should be 1 byte long
                    {
                        //remark PRINT("NET_CMD_ID_CLOSE length error -- %d\n",pHdrCmd->ParaLength);
                        closesocket(sockfd);
                        free(para_buf);
                        if(chPortOpened == TRUE)
                        {
                            reset_printer(10);
                        }
                        return(0);
                    }
                    
                    //remark PRINT("para_buf[0] - %02X\n",para_buf[0]); 
                    
                    if(para_buf[0] == 1)
                    {
                        bCancel = TRUE;
                    }
                    

                    pktRes.ServiceID = NET_SERVICE_ID_LPT_EMU;
                    pktRes.PacketType = NET_PACKET_TYPE_RES;
                    pktRes.CommandID = NET_CMD_ID_CLOSE;
                    pktRes.ResLength = 2;

                
                    //Send header
                    send(sockfd , (const char *)&pktRes , sizeof(pktRes) , 0);

                    //Send body 
                    body[0] = ERR_SUCCESS;
                    send(sockfd , body , sizeof(body) , 0);

                    closesocket(sockfd);
                    free(para_buf);
                    
                    //close printer port modified by PaN
                    sleep(1);

                    if(bCancel == TRUE)
                    {
                        //reset printer
                        //PRINT("Reset Printer\n");
                        reset_printer(10);
                    }

                    close(fdPRN); 

                    return(0);                              
                    break;
                }//case NET_CMD_ID_CLOSE:
                    
                case NET_CMD_ID_READ:
                {
                    LPT_DATA_PKT_HDR    pktData;
                    WORD                len;
                    int                 res_fread = 0;
                    PBYTE               pReadbuf;

                    len = (para_buf[1] << 8) + para_buf[0];
                    
                    //remark PRINT("NET_CMD_ID_READ len -> %d\n",len);

                    /************************************/
                    /************************************/
                    /*** TODO: add code here to read  ***/
                    /*** the printer port             ***/
                    /************************************/
                    /************************************/
                    
                    (void *)pReadbuf = malloc(len + 1);
                    
                    if(pReadbuf == NULL)
                    {
                        perror("malloc error 2:");
                        closesocket(sockfd);
                        free(para_buf);
                        if(chPortOpened == TRUE)
                        {
                            reset_printer(10);
                        }
                        return(0);
                    }
                    
                    pbuf.len = len;
                    pbuf.buf = pReadbuf;
                    //PRINT("Start Read\n");
                    res_fread = ioctl(fdPRN,LPREADDATA,&pbuf);
                    //PRINT("End Read\n");
                    //PRINT("---- res_fread %d\n",res_fread);
                    
                    if(res_fread > 0)
                    {
                        len = res_fread;
                        pReadbuf[res_fread]= 0;
                        //PRINT("*** %s\n",pReadbuf);
                    }
                    else
                    {
                        len = 0;
                    }

                    pktData.ServiceID = NET_SERVICE_ID_LPT_EMU;
                    pktData.PacketType = NET_PACKET_TYPE_DATA;
                    pktData.DataLength = len; //can not exceed 0xFFFF

                    /**********    Sending Header     **********/
                    send(sockfd , (const char *)&pktData , sizeof(pktData) , 0);
                
                    if( len > 0)
                    {
                        /**********    Sending Body       **********/
                        send(sockfd , (const char *)pReadbuf , len , 0);
                    }
                    free(pReadbuf);                     
                    
                    break;
                }

                free(para_buf);
            }//switch(pHdrCmd->CommandID)
        }//if( pHdrCmd != NULL) 


        if( pHdrData != NULL) 
        {
            //We receive Data
            int     res_fwrite;
            int     res_total_fwrite;
            int     write_len;
            int     total_len;
            PBYTE   write_buf;
            int prnstatus=0;

            iCount = pHdrData->DataLength;

            if (iCount!=0)
	    {
            	(void *)para_buf = malloc(pHdrData->DataLength);
            }
            //remark 
	    //PRINT("pHdrData->DataLength -- %d\n",pHdrData->DataLength);

            if(iCount!=0 && para_buf == NULL)
            {
                perror("malloc error 3:");
                closesocket(sockfd);
                if(chPortOpened == TRUE)
                {
                    reset_printer(10);
                }
                return(0);
            }
            
            PRINT("DATA HDR OK...\n");

            while( iCount > 0 )
            {
                rcv = RECV(sockfd , (para_buf + (pHdrData->DataLength - iCount )) , iCount , 30);

                if( rcv < 1)
                {
                    //receive error
                    perror("3. RECV ERR:");             
                    closesocket(sockfd);
                    free(para_buf);
                    if(chPortOpened == TRUE)
                    {
                        reset_printer(10);
                    }
                    return 0;
                }

                iCount = iCount - rcv;
            }


            //PRINT("DATA BODY OK...\n");

            pbuf.len = pHdrData->DataLength;
            pbuf.buf = para_buf;
            
            write_len = 0;
            total_len = pHdrData->DataLength;
            write_buf = para_buf;
            res_fwrite = 0;
            res_total_fwrite = 0;
            
            //remark PRINT("total_len %d\n",total_len);
            
            while(total_len > 0)
            {
                if(total_len > 4096)
                {
                    pbuf.len = 4096;
                    pbuf.buf = write_buf;
                    res_fwrite = ioctl(fdPRN,LPWRITEDATA,&pbuf);

                    if(res_fwrite != 4096)
                    {
                        DWORD retry = 0;
                        
                        ioctl(fdPRN,LPGETSTATUS,&prnstatus);
                        
                        //remark PRINT("prnstatus %d\n",prnstatus);

                        while( ((prnstatus == 0) || (prnstatus & LP_PBUSY)) && (res_fwrite != 4096) && (retry < 3))
                        {
                            //remark PRINT("}}}}}}}}}}}} res_fwrite %d\n",res_fwrite);
                            pbuf.len = 4096 - res_fwrite;
                            pbuf.buf = &(write_buf[res_fwrite]);
                            //usleep(500); //why we don't use usleep here ?? becuse usleep will sleep longer then we expect in iBox
                            sleep(1);
                            res_fwrite = res_fwrite + ioctl(fdPRN,LPWRITEDATA,&pbuf);
                            //remark PRINT("}}}}}}}}}}}} res_fwrite %d\n",res_fwrite);
                            ioctl(fdPRN,LPGETSTATUS,&prnstatus);

                            //remark PRINT("retry %d\n",retry);
                            retry ++;   
                        }
                        
                    }
                    
                    res_total_fwrite = res_total_fwrite + res_fwrite;
                    
                    if(res_fwrite != 4096)
                    {
                        break;
                    }
                    else
                    {
                        total_len = total_len - 4096;
                        if(total_len == 0)
                        {
                            //remark PRINT("total_len == 0 \n");
                            break;
                        }
                        write_buf = &(write_buf[4096]);
                    }

                    //remark PRINT("res_total_fwrite %d -- res_fwrite %d \n",res_total_fwrite,res_fwrite);
                    
                }
                else
                {
                    pbuf.len = total_len;
                    pbuf.buf = write_buf;
                    res_fwrite = ioctl(fdPRN,LPWRITEDATA,&pbuf);

                    //remark PRINT("PPPPPPP res_fwrite %d\n",res_fwrite);
                    if(res_fwrite != total_len)
                    {
                        DWORD retry = 0;

                        ioctl(fdPRN,LPGETSTATUS,&prnstatus);
                        //remark PRINT("prnstatus %d\n",prnstatus);
                        
                        while( ((prnstatus == 0) || (prnstatus & LP_PBUSY))  && (res_fwrite != total_len) && (retry < 3))
                        {
                            pbuf.len = total_len - res_fwrite;
                            pbuf.buf = &(write_buf[res_fwrite]);
                            //usleep(500); //why we don't use usleep here ?? becuse usleep will sleep longer then we expect in iBox
                            sleep(1);
                            res_fwrite = res_fwrite + ioctl(fdPRN,LPWRITEDATA,&pbuf);
                            //remark PRINT("}}}}}}}}}}}} res_fwrite %d\n",res_fwrite);
                            ioctl(fdPRN,LPGETSTATUS,&prnstatus);
                            //remark PRINT("retry %d\n",retry);
                            retry ++;   
                        }
                        
                    }

                    res_total_fwrite = res_total_fwrite + res_fwrite;
                    //remark PRINT("res_total_fwrite %d -- res_fwrite %d \n",res_total_fwrite,res_fwrite);
                    break;
                }
                
            }
                
                
            //remark PRINT("WritePrint %d - %d bytes\n",res_total_fwrite,pHdrData->DataLength);

            if(res_total_fwrite != pHdrData->DataLength)
            {
                int tmp=0;
                //remark PRINT("res_total_fwrite != pHdrData->DataLength \n");
                ioctl(fdPRN,LPGETSTATUS,&prnstatus);
                
                //remark PRINT("prnstatus %08X \n",prnstatus);
                
                if((prnstatus & LP_PERRORP) == 1 ) //Printer off-line
                {
                    //remark PRINT("Printer off-line -- prnstatus %d\n",prnstatus);
                    res_total_fwrite = res_total_fwrite|0x8000;
                }
                else
                {
                    //remark PRINT("Paper Empty -- prnstatus %d\n",prnstatus);
                    res_total_fwrite = res_total_fwrite|0x4000;
                }
		check_prn_status("BUSY or ERROR", clientaddr);
            }
            else{//JY1113 add
/*JY1113*/
		check_prn_status("Printing", clientaddr);
/**/
            }
            //remark PRINT("res_total_fwrite %08X\n",res_total_fwrite);

            body[0] = res_total_fwrite;

            pktRes.ServiceID = NET_SERVICE_ID_LPT_EMU;
            pktRes.PacketType = NET_PACKET_TYPE_RES;
            pktRes.CommandID = NET_CMD_ID_DATA_RES;
            pktRes.ResLength = 2;

            //Send header
            send(sockfd , (const char *)&pktRes , sizeof(pktRes) , 0);

            //Send body 
            send(sockfd , body , sizeof(body) , 0);
 
            free(para_buf);
        }//if( pHdrData != NULL) 
    }
    //remark PRINT("Thread Over\n");

    return(0);                              
    
}


void sig_child(int sig)
{
    int childpid;
    
    
    childpid = waitpid(-1, NULL , WNOHANG);

    while( childpid > 0)
    {
    
    	//remark PRINT("sig_child %d\n",childpid);
    
	    if( lptstatus.pid == childpid )
    	{
	        FILE *statusFp = NULL;

        	statusFp = fopen(STATUS_FILE , "w");
            
        	if(statusFp != NULL)
        	{
            	fprintf(statusFp,"PRN_CLIENT=\"\"\n");
            	fclose(statusFp);
        	}
        	else
        	{
            	perror("Open status file failed: ");
        	}


    	    /*** Wait 10 seconds here      ***/
        	/*** Because some slow printer ***/
        	/*** need some time to consume ***/
        	/*** the data...               ***/
        	sleep(10);
                
		busy = FALSE;
        	lptstatus.busy = FALSE;
        	lptstatus.pid  = 0;
		check_prn_status(ONLINE, "");
           
    	}

    	childpid = waitpid(-1, NULL , WNOHANG);
	}

   	//remark PRINT("waitpid -- childpid%d\n",childpid); 
    
}

void sig_cleanup(int sig)
{
    //remark PRINT("sig_cleanup\n");
    exit(0);
}

//JY1110
void sig_remove(int sig)
{
	if(lptstatus.pid != 0){
		kill(lptstatus.pid, SIGKILL);	
	}
	else
		return;
}


int closesocket(int sockfd)
{
    //shutdown(sockfd,SHUT_RDWR);
    close(sockfd);
}

/************************************/
/***  Receive the socket          ***/
/***  with a timeout value        ***/
/************************************/
DWORD RECV(int sockfd , PBYTE pRcvbuf , DWORD dwlen , DWORD timeout)
{

    if( waitsock(sockfd , timeout , 0) == 0)
    {
        //timeout
        PRINT("RECV timeout %d\n",timeout);
        return -1;
    }

    return recv(sockfd , pRcvbuf  , dwlen , 0 );
}


int waitsock(int sockfd , int sec , int usec)
{
    struct timeval  tv;
    fd_set          fdvar;
    int             res;
    
    FD_ZERO(&fdvar);
    FD_SET(sockfd, &fdvar);
    
    tv.tv_sec  = sec;
    tv.tv_usec = usec; 
    
    res = select( sockfd + 1 , &fdvar , NULL , NULL , &tv);
    
    return res;
}

void reset_printer(int sec)
{
    sleep(sec);
    ioctl(fdPRN,LPRESET);
}


/* to check use usb or parport printer */
/* James Yeh 2002/12/25 02:13	       */
int check_par_usb_prn()//JY: 20031104 change to int from void
{
    char    buf[1024];
    char    *token;
    FILE    *fp;

#ifdef USBONLY
    g_useUsb = TRUE;
    return(g_useUsb);
#else
    fp=fopen("/proc/sys/dev/parport/parport0/devices/lp/deviceid","r");

    if( fp != NULL)
    {
        while ( fgets(buf, sizeof(buf), fp) != NULL )  
        {
            if(buf[0] == '\n')
            {
                //PRINT("skip empty line\n");
                continue;
            }
    
            if(strncmp(buf , "status: " , strlen("status: "))   == 0)
            {
            	token= buf + strlen("status: ");
                
                //PRINT("token[0] %d\n",token[0]);

	  	//printf("token=%s\n", token);//JY1104                
                if(token[0] == '0')
                {
                	g_useUsb = TRUE;
			printf("USB\n");//JY1112: delete               
                }
                else
                {
                    	g_useUsb = FALSE;
			printf("PARALLEL\n");//JY1112: delete               
                }
			return(g_useUsb);//JY: 1104
                break;
            }
            
        }
        
        fclose(fp);
    }
#endif
    
}

/*1110test status
void checkstatus_usb_par()
{
	if(fd_print <= 0 || fd_print == NULL){
		check_prn_status("Off-line", "");
	}
	else {
		check_prn_status(ONLINE, "");
	}
}
*/

/*JY1114: check printer status*/
void check_prn_status(char *status_prn, char *cliadd_prn)
{
	STATUSFILE=fopen("/var/state/printstatus.txt", "w");
	if(cliadd_prn == NULL)
	{
		fprintf(STATUSFILE, "PRINTER_USER=\"\"\n");
	}
	else
	{
		fprintf(STATUSFILE, "PRINTER_USER=\"%s\"\n", cliadd_prn);
	}
	fprintf(STATUSFILE, "PRINTER_STATUS=\"%s\"\n", status_prn);
	fclose(STATUSFILE);
	//strcpy(printerstatus, status_prn);
}
/*JY1114: get printer queue name for LPR*/
int get_queue_name(char *input)
{
	char QueueName_got[32];
	char *index1;
	int rps_i=0, rps_j=0;
	while(index1 = strrchr(input, ' '))
		index1[0] = 0;
	rps_i = 0;
	strcpy(QueueName_got, input);
	return(strcmp(QueueName_got, "LPRServer"));
}

/*JY1120: send ack*/
void send_ack_packet(int *talk, int ack)
{
	char buffertosend[SMALLBUFFER];
	buffertosend[0] = ack;
	buffertosend[1] = 0;
	printf("send_ack_packet...\n");//JY1120: delete
	if( write( *talk, buffertosend, strlen(buffertosend) ) < 0 ) 
		printf("send_ack_packet: can not write socket...\n");
}

/*#######Lisa: Raw printing##########
 * open printer()
 * copy stream()
 * ProcessReq_Raw()
 ##################################*/
int open_printer(void)  
{
        int		f;

	check_par_usb_prn();
	if(g_useUsb==TRUE)
	{		
		if ((f=open("/dev/usb/lp0",O_RDWR)) < 0 ) 
		{
			//syslog(LOGOPTS, "%s: %m\n", device);
			exit(1);
		}
	}
	else
	{
		if ((f=open("dev/lp0",O_RDWR)) < 0)
		{
//			syslog(LOGOPTS, "Open Parallel port error");
			exit(1);
		} 
	}
	return (f);
}

int copy_stream(int fd,int f)
{
	int		nread,nwrite;
	char		buffer[8192];
	int 		timeout=20,wait;
	PRINT("copy_stream\n");
	while ((nread = read(fd, buffer, sizeof(buffer))) > 0)
	{
		int 		index=0,countread;
	//	nwrite=fwrite(buffer, sizeof(char), nread, f);    
               /* Lisa:fwrite => write */
		while	(index < nread )
		{
			countread=nread-index;
			nwrite=write(f,&buffer[index],countread);
			index += nwrite;
			if ((wait=waitsock(f,timeout,0))==0)
				check_prn_status("Busy or Error",clientaddr);	
		}			                
                check_prn_status("Printing",clientaddr);  //Add by Lisa
	}
	//(void)fflush(f);
        check_prn_status("ONLINE",""); //Add by Lisa
	return (nread);
}

void processReq_Raw(int fd)
{
	int f1;
	//PRINT("fd=%d \n",fd);
	//strcpy(clientaddr , inet_ntoa(client.sin_addr));

	//PRINT("Connection from %s prot %d accepted \n",inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	//syslog(LOGOPTS, "Connection from %s port %hd accepted\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
	/*write(fd, "Printing", 8);*/
	if (lptstatus.busy == FALSE)
	{
		if ((f1 = open_printer()) >= 0)   //modify by Lisa
		{
			if (copy_stream(fd, f1) < 0)
				syslog(LOGOPTS, "read: %m\n");
			close(f1);
		}
		(void)close(fd);
	}
	else
		return 0;
	
}
