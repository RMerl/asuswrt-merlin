//
// programming note for router SOC
// DO NOT USE fscanf/fprintf
//

//
// if autodetect enabled, adsl status polling MUST be disabled (no timer function)
// This is because multiple send and wait resp is not allowed
//

#ifndef WIN32
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define msgsnd(...) 0
#define msgrcv(...) 0
#define EINTR 0
#endif

#include "../msg_define.h"

static int m_Cnt = 0;

extern void UpdateAllAdslSts(int cnt, int from_ate);
extern void DispAllPvc();
extern int AddPvc(int idx, int vlan_id, int vpi, int vci, int encap, int mode);
extern int DelAllPvc();
extern int RestoreDefault(); /* Paul add 2012/8/7 */
extern void GetLinkSts(char* buf, int max_len);
extern void myprintf(const char *fmt, ...);
extern void SleepMs(int ms);
extern int SetAdslMode(int EnumAdslModeValue, int FromAteCmd);
extern int SetAdslType(int EnumAdslTypeValue, int FromAteCmd);

// sync variables
volatile int m_start_polling=0;
volatile int m_stop_polling_ok=0;
volatile int m_req_auto_det_pvc=0;

void enable_polling(void)
{
    m_req_auto_det_pvc = 0;
    m_start_polling = 1;
}

void disable_polling(void)
{
    m_req_auto_det_pvc = 1;            
}

void wait_polling_stop(void)
{
    while (1)
    {
        if (m_stop_polling_ok == 1)
        {
            break;
        } 
        SleepMs(10);
    }
    m_stop_polling_ok = 0;
}


void polling_tc_info(int signo) 
{ 
    if (m_req_auto_det_pvc)
    {
        m_start_polling = 0;
        m_stop_polling_ok = 1;
    }
    else
    {
        if (m_start_polling)
        {
            UpdateAllAdslSts(m_Cnt,0);
        }
    }
    m_Cnt++;
} 

#ifndef WIN32

void init_sigaction(void) 
{ 
	if (signal(SIGALRM, polling_tc_info) == SIG_ERR)
	{
		myprintf("Cannot handle SIGALRM!\n");
	}	 
}


void init_time() 
{ 
    struct itimerval value; 
    value.it_value.tv_sec=3; 
    value.it_value.tv_usec=0; 
    value.it_interval=value.it_value; 
    setitimer(ITIMER_REAL,&value,NULL); 
}


#endif


void SetPollingTimer()
{
#ifndef WIN32
    init_sigaction(); 
    init_time(); 
#endif    
}

static int m_msqid_to_d=0;
static int m_msqid_from_d=0;

int CreateMsgQ()
{
#ifndef WIN32
// create IPC
// adslate <-> tp_init
// auto_det <-> tp_init
	char buf[32];
	FILE* fp;
	int ret_val = 0;
	if ((m_msqid_to_d=msgget(IPC_PRIVATE,0700))<0)
	{
		myprintf("msgget err\n");
		ret_val = -1;
	}
	else
	{
		myprintf("msgget ok\n");
	}		 
	sprintf(buf,"%d",m_msqid_to_d);
	nvram_set("dsltmp_tc_resp_to_d", buf);
	return ret_val;
#else
	return 0;
#endif
}

int RcvMsgQ()
{
    int infolen;
    int bAskQuit = 0;
    msgbuf send_buf;        
    msgbuf receive_buf;    
    if((infolen=msgrcv(m_msqid_to_d,&receive_buf,MAX_IPC_MSG_BUF,0,0))<0)
    {
        if (errno == EINTR)
        {
          return 0;
        }
        else
        {
          myprintf("TP_INIT:msgrcv err\n");
          return -1;
        }
    }
    else
    {
        //printf("msgrcv ok\n");
        int bSendResp = 1;
        if (IPC_CLIENT_MSG_Q_ID == receive_buf.mtype)
        {
            int* pInt;
            pInt=(int*)(&receive_buf.mtext[0]);                    
            myprintf("TP_INIT:IPC_CLIENT_MSG_Q_ID\n");        
            m_msqid_from_d = *pInt;
            myprintf("%d\n",m_msqid_from_d);                    
            bSendResp = 0;
        }
        else if (IPC_START_AUTO_DET == receive_buf.mtype)
        {
            myprintf("TP_INIT:IPC_START_AUTO_DET\n");
            // delete timer
            disable_polling();
            // wait timer function exit
            wait_polling_stop();            
            send_buf.mtype=IPC_START_AUTO_DET;
            strcpy(send_buf.mtext,"Start Done");        
        }
        else if (IPC_ADD_PVC == receive_buf.mtype)
        {
            unsigned short idx;
            unsigned short vlan_id;
            unsigned short vpi;
            unsigned short vci;
            unsigned short encap;
            unsigned short mode;
            unsigned short* pWord;
            pWord=(unsigned short*)(&receive_buf.mtext[0]);            
            int add_ret;            
            myprintf("TP_INIT:IPC_ADD_PVC\n");                        
            idx = *pWord++;
            vlan_id = *pWord++;
            vpi = *pWord++;
            vci = *pWord++;
            encap = *pWord++;
            mode = *pWord;
            myprintf("ADDPVC:%d %d %d %d %d %d\n",idx,vlan_id,vpi,vci,encap,mode);
            add_ret = AddPvc(idx,vlan_id,vpi,vci,encap,mode);
            send_buf.mtype=IPC_ADD_PVC;
            if (add_ret == 0) strcpy(send_buf.mtext,"OK");
            else strcpy(send_buf.mtext,"FAIL");
        }        
        else if (IPC_DEL_ALL_PVC == receive_buf.mtype)
        {
            int del_ret;
            myprintf("TP_INIT:IPC_DEL_ALL_PVC\n");                        
            del_ret = DelAllPvc();
            send_buf.mtype=IPC_DEL_ALL_PVC;
            if (del_ret == 0) strcpy(send_buf.mtext,"OK");
            else strcpy(send_buf.mtext,"FAIL");
        }
				/* Paul add 2012/8/7 */
        else if (IPC_ATE_ADSL_RESTORE_DEFAULT == receive_buf.mtype)
        {
            int rst_ret;
            myprintf("TP_INIT:IPC_ATE_ADSL_RESTORE_DEFAULT\n");                        
            rst_ret = RestoreDefault();
            send_buf.mtype=IPC_ATE_ADSL_RESTORE_DEFAULT;
            if (rst_ret == 0) strcpy(send_buf.mtext,"OK");
            else strcpy(send_buf.mtext,"FAIL");
        }
        else if (IPC_LINK_STATE == receive_buf.mtype)
        {
			char LinkSts[30];
            myprintf("TP_INIT:IPC_LINK_STATE\n");                                
            GetLinkSts(LinkSts, sizeof(LinkSts));
            send_buf.mtype=IPC_LINK_STATE;
			if (!strcmp(LinkSts,"wait for initializing"))
			{
				// nvram could not allow space character
				strcpymax(send_buf.mtext,"wait_for_init",MAX_IPC_MSG_BUF);				
			}
			else
			{
				strcpymax(send_buf.mtext,LinkSts,MAX_IPC_MSG_BUF);								
			}
        }
        else if (IPC_STOP_AUTO_DET == receive_buf.mtype)
        {
            myprintf("TP_INIT:IPC_STOP_AUTO_DET\n");                                        
            send_buf.mtype=IPC_STOP_AUTO_DET;
            strcpy(send_buf.mtext,"Stop Done");
            enable_polling();
        }                
        else if (IPC_ATE_ADSL_SHOW == receive_buf.mtype)
        {
            myprintf("TP_INIT:IPC_ATE_ADSL_SHOW\n");                                        
            UpdateAllAdslSts(0,1);
            send_buf.mtype=IPC_ATE_ADSL_SHOW;
            strcpy(send_buf.mtext,"AteSetDone");           
        }                
        else if (IPC_ATE_ADSL_QUIT_DRV == receive_buf.mtype)
        {
            myprintf("TP_INIT:IPC_ATE_ADSL_QUIT_DRV\n");                                        
            bAskQuit = 1;
            send_buf.mtype=IPC_ATE_ADSL_QUIT_DRV;
            strcpy(send_buf.mtext,"AteSetDone");           
        }           
        else if (IPC_ATE_ADSL_DISP_PVC == receive_buf.mtype)
        {
            myprintf("TP_INIT:IPC_ATE_ADSL_DISP_PVC\n");                                        
            DispAllPvc();
            send_buf.mtype=IPC_ATE_ADSL_DISP_PVC;
            strcpy(send_buf.mtext,"AteSetDone");           
        }                        
        else if (IPC_ATE_SET_ADSL_MODE == receive_buf.mtype)
        {
            unsigned short mode;
            unsigned short type;
            unsigned short* pWord;
            pWord=(unsigned short*)(&receive_buf.mtext[0]);            
            int set_ret_mode, set_ret_type;            
            myprintf("TP_INIT:IPC_ATE_SET_ADSL_MODE\n");                                        
            mode = *pWord++;
            type = *pWord++;
            myprintf("SET MODE:%d %d\n",mode,type);
            set_ret_mode = SetAdslMode(mode,1);
            set_ret_type = SetAdslType(type,1);
            send_buf.mtype=IPC_ATE_SET_ADSL_MODE;
            if (set_ret_mode == 0 && set_ret_type) strcpy(send_buf.mtext,"OK");
            else strcpy(send_buf.mtext,"FAIL");            
        }    
        else if (IPC_RELOAD_PVC == receive_buf.mtype)
        {
            int del_ret;
            myprintf("TP_INIT:IPC_RELOAD_PVC\n");                        
            del_ret = DelAllPvc();
			reload_pvc();
            send_buf.mtype=IPC_RELOAD_PVC;			
            strcpy(send_buf.mtext,"OK");
        }
        
        if (bSendResp)
        {
            if(msgsnd(m_msqid_from_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
            {
                myprintf("msgsnd fail\n");
                return -1;
            }
        }
    }
  
    if (bAskQuit == 1) return 1;
    return 0;
}

