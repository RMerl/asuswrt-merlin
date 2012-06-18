//
// programming note for router SOC
// DO NOT USE fscanf/fprintf
//



#ifndef WIN32
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <bcmnvram.h>
#include "ra_reg_rw_ate.h"
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define msgsnd(...) 0
#define msgrcv(...) 0
#define EINTR 0
#endif

#include "../dsl_define.h"


enum {
    EnumAdslModeT1,
    EnumAdslModeGlite,
    EnumAdslModeGdmt,
    EnumAdslModeAdsl2,
    EnumAdslModeAdsl2plus,
    EnumAdslModeMultimode,
};

enum {
    EnumAdslTypeA,
    EnumAdslTypeL,
    EnumAdslTypeA_L,
    EnumAdslTypeM,
    EnumAdslTypeA_I_J_L_M,    
};



static int m_msqid_to_d=0;
static int m_msqid_from_d=0;

void ate_delete_msg_q(void)
{
    // delete msg queue from kernel
#ifndef WIN32    
    struct msqid_ds stMsgInfo;
    msgctl(m_msqid_from_d, IPC_RMID, &stMsgInfo);
#endif    
}

// exchange two lines to enable debug message
#define myprintf(...)
//#define myprintf printf

// TBD:
// for autodet only
//IPC_CHECK_PVC_PKTS_INCOMING

void adsl_ate(int argc, char* argv[])
{
    char buf[32];
    msgbuf send_buf;    
    msgbuf receive_buf;        
    int infolen;
    FILE* fp;
    unsigned short* pWord;
    int* pInt;    
    
    if (argc < 2) return;
    
    if (strcmp(argv[1],"waitadsl") == 0)    
    {
        int AdslReady = 0;
        int WaitAdslCnt;
        
        // ask auto_det to quit
		nvram_set("dsltmp_adslatequit","1");
        for (WaitAdslCnt = 0; WaitAdslCnt<60; WaitAdslCnt++)
        {
				strcpy(buf,nvram_safe_get("dsltmp_tcbootup"));
        
                if (!strcmp(buf,"1"))
                {
                        AdslReady = 1;
                        break;
                }
#ifndef WIN32                    
                usleep(1000*1000*1);
#endif                
                printf("%d\n",WaitAdslCnt);
        }
        
        if (AdslReady) printf("ADSL OK\n");
        else printf("ADSL FAIL\n");
        return;
    }        

// those old mechanism should be abandoned
#if 0    
    
    if (strcmp(argv[1],"makeretfile") == 0)    
    {
        FILE* fpQuit;
        fpQuit = fopen("/tmp/cmdret.txt","wb");
        fputs("cmdret.txt",fpQuit);
        fclose(fpQuit);
        return;
    }            
    
    
    if (strcmp(argv[1],"waitcmdret") == 0)    
    {
        int RetReady = 0;
        int WaitCnt;
        for (WaitCnt = 0; WaitCnt<60; WaitCnt++)
        {
                FILE* fp;
                fp = fopen("/tmp/cmdret.txt","rb");
                if (fp != NULL)
                {
                        RetReady = 1;
                        fclose(fp);
                        break;
                }
#ifndef WIN32                    
                usleep(1000*1000*1);
#endif                
                printf("%d\n",WaitCnt);
        }
        if (RetReady) printf("CMDRET OK\n");
        else printf("CMDRET FAIL\n");
        return;
    }            
    

    
    
    if (strcmp(argv[1],"makeretfile2") == 0)    
    {
        FILE* fpQuit;
        fpQuit = fopen("/tmp/cmdret2.txt","wb");
        fputs("cmdret2.txt",fpQuit);
        fclose(fpQuit);
        return;
    }            
    
    
    if (strcmp(argv[1],"waitcmdret2") == 0)    
    {
        int RetReady = 0;
        int WaitCnt;
        for (WaitCnt = 0; WaitCnt<60; WaitCnt++)
        {
                FILE* fp;
                fp = fopen("/tmp/cmdret2.txt","rb");
                if (fp != NULL)
                {
                        RetReady = 1;
                        fclose(fp);
                        break;
                }
#ifndef WIN32                    
                usleep(1000*1000*1);
#endif                
                //printf("%d\n",WaitCnt);
        }
        if (RetReady) printf("CMDRET2 OK\n");
        else printf("CMDRET2 FAIL\n");
        return;
    }            

#endif    

    
	strcpy(buf,nvram_safe_get("dsltmp_tc_resp_to_d"));
	m_msqid_to_d=atoi(buf);
   
#ifndef WIN32
    if ((m_msqid_from_d=msgget(IPC_PRIVATE,0700))<0)
    {
        printf("msgget err\n");
        return;
    }
    else
    {
        myprintf("msgget ok\n");
    }    
#endif    
    
    myprintf("MSQ_to_daemon : %d\n",m_msqid_to_d);
    myprintf("MSQ_from_daemon : %d\n",m_msqid_from_d);    
    
#ifndef WIN32    
    if(switch_init() < 0)
    {
        myprintf("err switch init\n");
        goto delete_msgq_and_quit;
    }
#endif                                
    
    send_buf.mtype=IPC_CLIENT_MSG_Q_ID;
    pInt = (int*)send_buf.mtext;      
    *pInt = m_msqid_from_d;
    if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
    {
        printf("msgsnd fail (IPC_CLIENT_MSG_Q_ID)\n");
        goto delete_msgq_and_quit;
    }
    else
    {
        myprintf("msgsnd ok (IPC_CLIENT_MSG_Q_ID)\n");
    }    
    
    send_buf.mtype=IPC_START_AUTO_DET;
    strcpy(send_buf.mtext,"startdet");
    if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
    {
        printf("msgsnd fail (IPC_START_AUTO_DET)\n");
        goto delete_msgq_and_quit;
    }
    else
    {
        myprintf("msgsnd ok (IPC_START_AUTO_DET)\n");
    }
    // wait daemon response
    while (1)
    {
        if((infolen=msgrcv(m_msqid_from_d,&receive_buf,MAX_IPC_MSG_BUF,0,0))<0)
        {
        // WIN32: uninit warning is ok
            if (errno == EINTR)
            {
              continue;
            }
            else
            {
              printf("msgrcv err %d\n",errno);
              break;
            }                    
        }
        else
        {
            break;
        }
    }    
    

#ifndef WIN32
    if (strcmp(argv[1],"bridgemode") == 0)
    {
        // tell driver that we want bridge mode
        switch_bridge_mode();
        // delete iptv interfaces    
#if DSL_N55U == 1
		system("ifconfig eth2.1.1 down");
		system("ifconfig eth2.1.2 down");
		system("ifconfig eth2.1.3 down");
		system("ifconfig eth2.1.4 down");
		system("ifconfig eth2.1.5 down");
		system("ifconfig eth2.1.6 down");
		system("ifconfig eth2.1.7 down");
		system("ifconfig eth2.1.8 down");
		// close wifi 	   
		system("ifconfig ra0 down");
		system("ifconfig rai0 down");
		// reset switch IC and link all port together
		// 8 = change switich ic VLAN
		// 100 = reset and enable adsl bridge
		system("8367r 8 100");
#else
#error "new model"
#endif
        system("brctl show");
        goto delete_msgq_and_quit;
    }
#endif


    
    
    
    if (strcmp(argv[1],"show") == 0)
    {
        myprintf("IPC_ATE_ADSL_SHOW\n");    
        send_buf.mtype=IPC_ATE_ADSL_SHOW;
        strcpy(send_buf.mtext,"ateshow");
        myprintf("msg :%s\n",send_buf.mtext);
    }
    else if (strcmp(argv[1],"quitdrv") == 0)
    {
        myprintf("IPC_ATE_ADSL_QUIT_DRV\n");    
        send_buf.mtype=IPC_ATE_ADSL_QUIT_DRV;
        strcpy(send_buf.mtext,"atequitdrv");
        myprintf("msg :%s\n",send_buf.mtext);
    }    
    else if (strcmp(argv[1],"disppvc") == 0)
    {
        myprintf("IPC_ATE_ADSL_DISP_PVC\n");    
        send_buf.mtype=IPC_ATE_ADSL_DISP_PVC;
        strcpy(send_buf.mtext,"atedisppvc");
        myprintf("msg :%s\n",send_buf.mtext);
    }      
    else if (strcmp(argv[1],"link") == 0)
    {
        myprintf("IPC_LINK_STATE\n");    
        send_buf.mtype=IPC_LINK_STATE;
        strcpy(send_buf.mtext,"linksts");
        myprintf("msg :%s\n",send_buf.mtext);
    }
    else if (strcmp(argv[1],"addpvc") == 0)
    {
        int vpi,vci,encap,mode;
        vpi = atoi(argv[2]);
        vci = atoi(argv[3]);
        encap = atoi(argv[4]);
        mode = atoi(argv[5]);
        myprintf("IPC_ADD_PVC\n");    
        send_buf.mtype=IPC_ADD_PVC;
        pWord = (unsigned short*)send_buf.mtext;
        *pWord++ = 0; // idx (0-7)
        *pWord++ = 1; // always equal to idx+1
        *pWord++ = vpi; // vpi
        *pWord++ = vci; // vci
        *pWord++ = encap; // encap
        *pWord++ = mode; // mode
        myprintf("msg :%s\n","addpvc");
    }  
    else if (strcmp(argv[1],"delallpvc") == 0)
    {
        myprintf("IPC_DEL_ALL_PVC\n");    
        send_buf.mtype=IPC_DEL_ALL_PVC;
        strcpy(send_buf.mtext,"delall");
        myprintf("msg :%s\n",send_buf.mtext);
    }    
    else if (strcmp(argv[1],"setadslmode") == 0)
    {
        int mode,type;    
        mode = atoi(argv[2]);
        type = atoi(argv[3]);        
        myprintf("IPC_ATE_SET_ADSL_MODE\n");    
        send_buf.mtype=IPC_ATE_SET_ADSL_MODE;
        pWord = (unsigned short*)send_buf.mtext;
        *pWord++ = mode;
        *pWord++ = type;
        myprintf("msg :%s\n",send_buf.mtext);
    }        
    else 
    {
        printf("error input\n");
        goto delete_msgq_and_quit;
    }

    if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
    {
        printf("msgsnd fail\n");
        goto delete_msgq_and_quit;
    }
    else
    {
        myprintf("msgsnd ok\n");
        
        while (1)
        {
            if((infolen=msgrcv(m_msqid_from_d,&receive_buf,MAX_IPC_MSG_BUF,0,0))<0)
            {
            // WIN32: uninit warning is ok
                if (errno == EINTR)
                {
                  continue;
                }
                else
                {
                  printf("msgrcv err %d\n",errno);
                  break;
                }                    
            }
            else
            {
                //printf("%d",receive_buf.mtype);
                if (IPC_ATE_ADSL_SHOW == receive_buf.mtype || IPC_ATE_ADSL_DISP_PVC == receive_buf.mtype)
                {
                    FILE* fpLog;
                    char bufLog[256];
                    fpLog = fopen("/tmp/adsl/adslate.log","rb");
                    if (fpLog != NULL)
                    {
                        while(1)
                        {
                            char* ret;
                            ret = fgets(bufLog,sizeof(bufLog),fpLog);
                            // eof or something wrong
                            if (ret == NULL) break;
                            printf(bufLog);
                            //printf("%d\n",ret);
                        }
                        fclose(fpLog);                   
                    }
                }
                printf(receive_buf.mtext);
                printf("\n");
                break;
            }
        }
    }

delete_msgq_and_quit:
#ifndef WIN32
    switch_fini();
#endif    
    ate_delete_msg_q();
}


#ifndef WIN32
int main(int argc, char* argv[])
#else
int ate_test_main(int argc, char* argv[])
#endif
{
    adsl_ate(argc, argv);
    return 0;
}
