
#include <bcmnvram.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "../msg_define.h"


void delete_msg_q(int msqid_from_d)
{
	// delete msg queue from kernel
	struct msqid_ds stMsgInfo;
	msgctl(msqid_from_d, IPC_RMID, &stMsgInfo);
}


int main(int argc, char* argv[])
{
	char buf[32];
	int msqid_to_d=0;
	int msqid_from_d=0;	
	msgbuf send_buf;
	int* pInt;
    msgbuf receive_buf;        
    int infolen;
	

	strcpy(buf,nvram_safe_get("dsltmp_tc_resp_to_d"));
	msqid_to_d=atoi(buf);

	if ((msqid_from_d=msgget(IPC_PRIVATE,0700))<0)
	{
		printf("msgget err\n");
		return;
	}
	else
	{
		printf("msgget ok\n");
	}

	printf("MSQ_to_daemon : %d\n",msqid_to_d);
	printf("MSQ_from_daemon : %d\n",msqid_from_d);

	send_buf.mtype=IPC_CLIENT_MSG_Q_ID;
	pInt = (int*)send_buf.mtext;
	*pInt = msqid_from_d;
	if(msgsnd(msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("msgsnd fail (IPC_CLIENT_MSG_Q_ID)\n");
		goto delete_msgq_and_quit;
	}
	else
	{
		printf("msgsnd ok (IPC_CLIENT_MSG_Q_ID)\n");
	}

	// necessary to stop timer first

    send_buf.mtype=IPC_START_AUTO_DET;
    strcpy(send_buf.mtext,"startdet");
    if(msgsnd(msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
    {
        printf("msgsnd fail (IPC_START_AUTO_DET)\n");
        goto delete_msgq_and_quit;
    }
    else
    {
        printf("msgsnd ok (IPC_START_AUTO_DET)\n");
    }

    // wait daemon response
    while (1)
    {
        if((infolen=msgrcv(msqid_from_d,&receive_buf,MAX_IPC_MSG_BUF,0,0))<0)
        {
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
	

    send_buf.mtype=IPC_RELOAD_PVC;
    strcpy(send_buf.mtext,"reload");
    if(msgsnd(msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
    {
        printf("msgsnd fail (IPC_RELOAD_PVC)\n");
        goto delete_msgq_and_quit;
    }
    else
    {
        printf("msgsnd ok (IPC_RELOAD_PVC)\n");
    }
    // wait daemon response
    while (1)
    {
        if((infolen=msgrcv(msqid_from_d,&receive_buf,MAX_IPC_MSG_BUF,0,0))<0)
        {
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

delete_msgq_and_quit:
	delete_msg_q(msqid_from_d);

	printf("EXIT req_dsl_drv\n");
	

}


