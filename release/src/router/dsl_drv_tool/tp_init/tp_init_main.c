// WIN32 notice
// DO NOT BIND OTHER PROTOCOLS
// Some protocols did not write well and no pass response to ADSLPROT.SYS.
// For best test result, please unbind all other protocols.

#ifndef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "basedef.h"
#else
#include "stdafx.h"
#include "windows.h"
#include "cprotdrv.h"
#include <conio.h>
static ProtDrv* m_PtDrv;
static BOOLEAN m_AdaptOpened;
#endif

#include "../dsl_define.h"

#include <stdarg.h>
#include "scanner.h"
#include "tp_msg.h"
#include "tp_init.h"
#include "ra_reg_rw.h"
#include "fw_conf.h"


static UCHAR m_RespBuf[MAX_RESP_BUF_NUM][MAX_TC_RESP_BUF_LEN];
static USHORT m_RespLen[MAX_RESP_BUF_NUM];

static UCHAR m_RespBufSave[MAX_RESP_BUF_NUM][MAX_TC_RESP_BUF_LEN];
static USHORT m_RespLenSave[MAX_RESP_BUF_NUM];


static UCHAR m_AdslMacAddr[6];
static UCHAR m_RouterMacAddr[6];

#ifndef WIN32
static struct sockaddr_ll m_SocketAddr;
#endif

static unsigned int m_AdaptIdx;
static int m_SocketSend = 0;
// those 4 default values are FALSE, FALSE, FALSE, FALSE
static int m_EnableDbgOutput = FALSE;
static int m_ShowPktLog  = FALSE;
static int m_DbgOutputRedirectToFile = FALSE;
static int g_AlwaysIgnoreAdslCmd = FALSE;

extern void nvram_load_pvcs(int config_num, int iptv_port, int internet_pvc);
extern int write_ipvc_mode(int config_num, int iptv_port);
extern int nvram_set_adsl_fw_setting(int config_num, int iptv_port, int internet_pvc, int chg_mode);
extern int nvram_load_config_num(int* iptv);
extern void SetPollingTimer();
extern int CreateMsgQ();
extern int RcvMsgQ();
extern void enable_polling(void);
extern int ReadyForInternet();
extern void nvram_adslsts(char* sts);
extern void nvram_adslsts_detail(char* sts);

#define MAX_ERR_MSG_LEN 1000
static char m_ErrMsgBuf[MAX_ERR_MSG_LEN+1]; // add one for zero
static int m_CurrErrMsgLen = 0;

char* strcpymax(char *to, const char*from, int max_to_len)
{
    int i;
    if (max_to_len < 1) return to;

    for (i=0; i<max_to_len-1; i++)
    {
        if (*from == 0) break;
        *to++ = *from++;
    }

    *to++ = 0;

    return to;
}

void PutErrMsg(char* msg)
{
    // copy error msg to buffer and change 0 to new line
    int NewLen;
    strcpymax(&m_ErrMsgBuf[m_CurrErrMsgLen], msg, MAX_ERR_MSG_LEN - m_CurrErrMsgLen);
    NewLen = m_CurrErrMsgLen + strlen(msg);
    if ((NewLen + 1) > MAX_ERR_MSG_LEN) m_CurrErrMsgLen = MAX_ERR_MSG_LEN - 1;
    else m_CurrErrMsgLen = NewLen;
    m_ErrMsgBuf[m_CurrErrMsgLen]='\n';
    m_CurrErrMsgLen++;
    m_ErrMsgBuf[m_CurrErrMsgLen]=0;
}

char* GetErrMsg(int* len)
{
    *len = m_CurrErrMsgLen;
    return m_ErrMsgBuf;
}

void myprintf(const char *fmt, ...)
{
	if (m_DbgOutputRedirectToFile)
	{
		char buf[1024];
		FILE* fp;
		if (m_EnableDbgOutput == FALSE) return;
		fp = fopen("/tmp/adsl/adsllog.log","a+");
		va_list arg;
		va_start(arg, fmt);
		vsprintf(buf, fmt, arg);
		fprintf(fp, buf);
		va_end(arg);
		fclose(fp);
	}
	else
	{
		// console
		if (m_EnableDbgOutput == FALSE) return;
		va_list arg;
		va_start(arg, fmt);
		vprintf(fmt, arg);
		va_end(arg);
	}
}

char* GET_RESP_STR(PUCHAR x)
{
    if (*(x+14) != 0x07)
    {
        return NULL;
    }
    return (char*)(x+15);
}

int GET_RESP_LEN(int len)
{
    if (len < 15)
    {
        myprintf("TC response critical error\r\n");
        return 0;
    }
    return (len-15);
}


void SleepMs(int ms)
{
#ifndef WIN32
    usleep(ms*1000); //convert to microseconds
#else
    Sleep(ms);
#endif
}

BOOLEAN SendPkt(PUCHAR SendBuf, USHORT SendBufLen)
{
#ifndef WIN32
    m_SocketAddr.sll_family   = PF_PACKET;
    // we don't use a protocoll above ethernet layer just use anything here
    m_SocketAddr.sll_protocol = htons(ETH_P_IP);
    m_SocketAddr.sll_ifindex = m_AdaptIdx;
    // use any thing here
    m_SocketAddr.sll_hatype   = 0x1234;
    // target is another host
    m_SocketAddr.sll_pkttype  = PACKET_OTHERHOST;
    // address length
    m_SocketAddr.sll_halen    = ETH_ALEN;
    int send_result = 0;
    // send the packet
    send_result = sendto(m_SocketSend, SendBuf, SendBufLen, 0,
                         (struct sockaddr*)&m_SocketAddr, sizeof(m_SocketAddr));

    if (send_result == -1)
    {
        myprintf("%s : err\n", __FUNCTION__);
    }
#else


    BOOLEAN RetVal = m_PtDrv->SendPkt(SendBuf, SendBufLen);
    if (RetVal == FALSE)
    {
        wprintf(L"Err send one pkt");
        return FALSE;
    }
    return TRUE;

#endif
}

BOOLEAN RcvPkt(PUCHAR RcvBuf, USHORT MaxRcvBufSize, PUSHORT pRcvBufLen)
{
#ifndef WIN32
/*
    int recv_len;
    char rcv_buffer[MAX_TC_RESP_BUF_LEN];

    *pRcvBufLen = 0;

    //recv_len = recvfrom(m_SocketRecv,rcv_buffer,sizeof(rcv_buffer),0,(struct sockaddr*)&m_SocketAddr, &sock_len);
    for (i=0; i<8; i++)
    {
        recv_len = recv(m_SocketRecv[i],rcv_buffer,sizeof(rcv_buffer),0);
    }
//    myprintf("recvfrom : %d bytes read\n",recv_len);
    if (recv_len < 0)
    {
        return FALSE;
    }

    unsigned short check_ethtype;

    check_ethtype = *((unsigned short*)(&rcv_buffer[12]));

    if (check_ethtype == 0xaaaa)
    {
        if (memcmp(rcv_buffer,m_RouterMacAddr,6) == 0)
        {
            if (recv_len < MaxRcvBufSize)
            {
                *pRcvBufLen = recv_len;
                memcpy(RcvBuf,rcv_buffer,recv_len);
            }
        }

    }
    */
    int ret = switch_rcv_okt(RcvBuf, MaxRcvBufSize, pRcvBufLen);
    if (ret == 0)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
#else

    BOOLEAN RetVal = m_PtDrv->RcvPkt(RcvBuf, MaxRcvBufSize, pRcvBufLen);
    if (RetVal)
    {
        return TRUE;
    }
    else
    {
        wprintf(L"RCV failed !!");
        return FALSE;
    }

#endif
}


VOID LogPktToConsole(int direction, unsigned char* data , int Size)
{
    if (direction == 0)
    {
        myprintf("\r\n  PC ---> ADSL , Len:%d\r\n", Size);
    }
    else
    {
        myprintf("\r\n  Adsl ---> PC , Len:%d\r\n", Size);
    }

    int i;
    int j;
    // log data
    for(i=0 ; i < Size ; i++)
    {
        if( i!=0 && i%16==0)   //if one line of hex printing is complete...
        {
            myprintf("         ",i);
            for(j=i-16 ; j<i ; j++)
            {
                if(data[j]>=32 && data[j]<=128)
                    myprintf("%c",(unsigned char)data[j]); //if its a number or alphabet

                else myprintf("."); //otherwise print a dot
            }
            myprintf("\n");
        }

        if(i%16==0) myprintf("   ");

        myprintf(" %02X",(unsigned int)data[i]);

        if( i==Size-1)  //print the last spaces
        {
            for(j=0; j<15-i%16; j++) myprintf("   "); //extra spaces

            myprintf("         ");

            for(j=i-i%16 ; j<=i ; j++)
            {
                if(data[j]>=32 && data[j]<=128) myprintf("%c",(unsigned char)data[j]);
                else myprintf(".");
            }
            myprintf("\n");
        }
    }
}

void CreateAdslErrorFile(char* fn, UCHAR cmd, PUCHAR pData, USHORT DataLen)
{
    FILE* fpBoot;
    char tmp[128];
    sprintf(tmp,"/tmp/adsl/ADSL_FATAL_%s_ERROR.txt",fn);
    fpBoot = fopen(tmp,"wb");
    if (fpBoot != NULL)
    {
        sprintf(tmp, "cmd=%d,len=%d",cmd,DataLen);
        fputs(tmp,fpBoot);
        fputs("\n",fpBoot);
        if (MAC_RTS_CONSOLE_CMD==cmd)
        {
            fputs((const char*)pData,fpBoot);
        }
        fclose(fpBoot);
    }
}

BOOLEAN SendCmdAndWaitResp(PUCHAR pResp[], USHORT MaxRespLen, PUSHORT pRespLen[], USHORT MaxResp, PUSHORT pRcvPktNum, PUCHAR pDst, UCHAR Cmd, PUCHAR pData, USHORT DataLen, UCHAR WaitRespNumInput)
{
    UCHAR WaitRespNum = WaitRespNumInput;
    UCHAR PktBuf[1600];
    USHORT Idx = 0;
    unsigned int PktBufLen = 0;

    if (g_AlwaysIgnoreAdslCmd) return FALSE;

    if (WaitRespNum == 0) WaitRespNum = (UCHAR)MaxResp;

    memcpy(&PktBuf[Idx], pDst, MAC_LEN);
    Idx+=MAC_LEN;
    memcpy(&PktBuf[Idx], m_RouterMacAddr, MAC_LEN);
    Idx+=MAC_LEN;
    memcpy(&PktBuf[Idx], ETH_TYPE, ETH_TYPE_LEN);
    Idx+=ETH_TYPE_LEN;
    PktBuf[Idx] = Cmd;
    Idx+=sizeof(Cmd);
    if (DataLen>0 && pData != NULL)
    {
        if (Idx+DataLen < sizeof(PktBuf))
        {
            memcpy(&PktBuf[Idx], pData, DataLen);
            Idx+=DataLen;
        }
    }

    PktBufLen = Idx;
    BOOLEAN RetVal = SendPkt(PktBuf, PktBufLen);
    if (RetVal == FALSE)
    {
        //g_AlwaysIgnoreAdslCmd = TRUE;
        CreateAdslErrorFile("SEND", Cmd, pData, DataLen);
        myprintf("TP_INIT : Err send\n");
        return FALSE;
    }

    if (m_ShowPktLog)
    {
        LogPktToConsole(DIR_PC_TO_ADSL, PktBuf, PktBufLen);
    }


    USHORT RetRespLen;
    *pRcvPktNum = 0;
    //*pRespLen = 0;

    int RespCnt = 0;

    int TimeoutCnt;
    for (TimeoutCnt = 0; TimeoutCnt<100; TimeoutCnt++)
    {
        RetVal = RcvPkt(pResp[RespCnt], MaxRespLen, &RetRespLen);
        if (RetVal)
        {
            if (RetRespLen == 0)
            {
                SleepMs(100);
                continue;
            }
            else
            {
                if (m_ShowPktLog)
                {
                    LogPktToConsole(DIR_ADSL_TO_PC, pResp[RespCnt], RetRespLen);
                }
                int PromptReceived = FALSE;
                if (WaitRespNumInput == 0)
                {
                    scanner_set(GET_RESP_STR(pResp[RespCnt]), GET_RESP_LEN(RetRespLen));
                    while (1)
                    {
                        token tok = scanner();
                        if (tok == TOKEN_EOF)
                        {
                            break;
                        }
                        if (tok == TOKEN_STRING)
                        {
                            char* pRespStr = scanner_get_str();
                            if (strcmp(pRespStr,"tc>") == 0)
                            {
                                PromptReceived = TRUE;
                            }
                        }
                    }
                }
                (*pRcvPktNum)+=1;
                *(pRespLen[RespCnt])=RetRespLen;
                RespCnt++;
                if (RespCnt >= MaxResp) break;
                // if we have received enough packet
                if ((*pRcvPktNum) >= WaitRespNum || PromptReceived)
                {
#if 0
                    //we wait 100 ms
                    SleepMs(100);
                    BOOLEAN CheckPendingRetVal = RcvPkt(pResp[RespCnt], MaxRespLen, &RetRespLen);
                    if (CheckPendingRetVal && RetRespLen > 0)
                    {
                        myprintf("*** Pended packet to received ***\r\n");
                    }
#endif
                    break;
                }
                SleepMs(100);
                continue;
            }
        }
        else
        {
            //g_AlwaysIgnoreAdslCmd = TRUE;
            CreateAdslErrorFile("RECV", Cmd, pData, DataLen);
            myprintf("TP_INIT : RCV failed !!\n");
            return FALSE;
        }
    }

    if (m_ShowPktLog)
    {
        myprintf("*** RESP : %d\n",*pRcvPktNum);
    }

    return TRUE;
}



void GetMacAddr(void)
{
#ifndef WIN32
    int fd;
    struct ifreq ifr;


    fd = socket(PF_INET, SOCK_PACKET, htons(ETH_P_ALL)); /* open socket */
#if DSL_N55U == 1
	strcpy(ifr.ifr_name, "eth2"); /* assuming we want eth0 */
#else
	#error "new model"
#endif

    ioctl(fd, SIOCGIFHWADDR, &ifr); /* retrieve MAC address */

    /*
    * at this point, the interface's HW address is in
    * 6 bytes in ifr.ifr_hwaddr.sa_data. You can copy them
    * out with memcpy() or something similar. You'll have to
    * translate the bytes into a readable hex notation.
    */

    close(fd);

    memcpy(m_RouterMacAddr, ifr.ifr_hwaddr.sa_data, 6);
#else

    memcpy(m_RouterMacAddr, m_PtDrv->GetMacAddr(), 6);

#endif

    myprintf("ROUTER MAC : %02x-%02x-%02x-%02x-%02x-%02x\n", m_RouterMacAddr[0], m_RouterMacAddr[1], m_RouterMacAddr[2],m_RouterMacAddr[3],m_RouterMacAddr[4],m_RouterMacAddr[5]);


}

/*
BOOLEAN SendOnePktVlan(PUCHAR pDst, USHORT VlanId, PUCHAR pData, USHORT DataLen)
{
    UCHAR PktBuf[1600];
    USHORT Idx = 0;
    unsigned int PktBufLen = 0;
    USHORT VlanTag = 0x0081;

    memcpy(&PktBuf[Idx], pDst, MAC_LEN);
    Idx+=MAC_LEN;
    memcpy(&PktBuf[Idx], m_RouterMacAddr, MAC_LEN);
    Idx+=MAC_LEN;
    memcpy(&PktBuf[Idx], &VlanTag, sizeof(VlanTag));
    Idx+=sizeof(VlanTag);
    PktBuf[Idx++] = VlanId >> 8;
    PktBuf[Idx++] = VlanId & 0xff;

    if (DataLen>0 && pData != NULL)
    {
        if (Idx+DataLen < sizeof(PktBuf))
        {
            memcpy(&PktBuf[Idx], pData, DataLen);
            Idx+=DataLen;
        }
    }

    PktBufLen = Idx;
    int Cnt;
    for (Cnt = 0; Cnt < 100; Cnt++)
    {
        BOOLEAN RetVal = SendPkt(PktBuf, PktBufLen);
        if (RetVal == FALSE)
        {
            //AfxMessageBox(L"Err send one pkt");
            return FALSE;
        }
    }
    return TRUE;
}
*/


#define declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr) \
    PUCHAR pRespBuf[MAX_RESP_BUF_NUM]; \
    PUSHORT pRespLen[MAX_RESP_BUF_NUM]; \
    USHORT RespPktNum; \
    token tok; \
    char* pRespStr; \
    init_resp_buf (pRespBuf, pRespLen)


void init_resp_buf(PUCHAR pRespBuf[], PUSHORT pRespLen[])
{
    int i;
    for (i=0; i<MAX_RESP_BUF_NUM; i++)
    {
        pRespBuf[i] = &m_RespBuf[i][0];
        pRespLen[i] = &m_RespLen[i];
    }
}

void tok_assert(token tok, token expected_tok)
{
    if (tok != expected_tok)
    {
        PutErrMsg("tok is not expected value");
        myprintf("tok is not expected value\r\n");
        myprintf("%d",tok);
        myprintf("\r\n");
    }
}

void tok_assert_two(token tok, token expected_tok0, token expected_tok1)
{
    if (tok == expected_tok0 || tok == expected_tok1)
    {
    }
    else
    {
        PutErrMsg("tok is not expected value0 or value1");
        myprintf("tok is not expected value0 or value1\r\n");
        myprintf("%d",tok);
        myprintf("\r\n");
    }
}


void tok_assert_buf(char* tok_buf, char* tok_buf_expected)
{
    if (strcmp(tok_buf,tok_buf_expected) != 0)
    {
        PutErrMsg("tok buf is not expected");
        myprintf("tok buf is not expected\r\n");
        myprintf(tok_buf);
        myprintf("\r\n");
    }
}



typedef struct {
    char IpAddr[80];
} ADSL_LAN_IP;

int HandleAdslShowLan(ADSL_LAN_IP* pLanIp)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_LAN, GET_LEN(SHOW_LAN), 0);
    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"lanip") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner_get_ip();
                    strcpymax(pLanIp->IpAddr, scanner_get_str(), sizeof(pLanIp->IpAddr));
                }
            }
        }
    }
    return 0;
}

int SetAdslMode(int EnumAdslModeValue, int FromAteCmd)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL, GET_LEN(SHOW_ADSL),0);

    int i;
    BOOLEAN SetNewValue = FALSE;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"adslmode") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    if (EnumAdslModeValue != atoi(scanner_get_str()))
                    {
                        SetNewValue = TRUE;
                    }
                }
            }
        }
    }

    if (SetNewValue == FALSE)
    {
        //printf("Old value equal to new value. skip\n");
        return 0;
    }

    char InputCmd[10];
    char UserCmd[256];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    sprintf(InputCmd, "%d", EnumAdslModeValue);
    strcpy(UserCmd, SET_ADSL_MODE);
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);


    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL, GET_LEN(SHOW_ADSL),0);

    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"adslmode") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    if (EnumAdslModeValue != atoi(scanner_get_str()))
                    {
                        char buf[80];
                        sprintf(buf,"SetAdslMode,%d",EnumAdslModeValue);
                        PutErrMsg(buf);
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}
int SetAdslType(int EnumAdslTypeValue, int FromAteCmd)
{
#ifdef RTCONFIG_DSL_ANNEX_B /* Paul add 2012/8/22, for Annex B if set adsltype 0 again will change to Annex A. Which is a bug with ADSL driver, so just skip. */
	return 0;
#endif
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char InputCmd[10];
    char UserCmd[256];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL, GET_LEN(SHOW_ADSL),0);

    int i;

    /* Paul comment 2012/7/17, just issue set adsltype again, in order to have the Annex mode stick to that adsl type. */
    /*BOOLEAN SetNewValue = FALSE;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"adslmode") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "adsltype");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    if (EnumAdslTypeValue != atoi(scanner_get_str()))
                    {
                        SetNewValue = TRUE;
                    }
                }
            }
        }
    }

    if (SetNewValue == FALSE)
    {
        //printf("Old value equal to new value. skip\n");
        return 0;
    }*/

    sprintf(InputCmd, "%d", EnumAdslTypeValue);
    strcpy(UserCmd, SET_ADSL_TYPE);
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL, GET_LEN(SHOW_ADSL),0);

    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"adslmode") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "adsltype");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    if (EnumAdslTypeValue != atoi(scanner_get_str()))
                    {
                        char buf[80];
                        sprintf(buf,"SetAdslType,%d",EnumAdslTypeValue);
                        PutErrMsg(buf);
                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

/* Paul add start 2012/9/24, for SNR Margin tweaking. */
int SetSNRMOffset(int EnumSNRMOffsetValue, int FromAteCmd)
{
		if(EnumSNRMOffsetValue == 0) //Disabled
			return 0;
	
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char InputCmd[10];
    char UserCmd[64];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    
    sprintf(InputCmd, "%d", EnumSNRMOffsetValue);
    strcpy(UserCmd, SET_SNRM_Offset_ADSL);
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);

		UserCmd[0] = '\0';
    strcpy(UserCmd, SET_SNRM_Offset_ADSL2);
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, " ");
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);

    return 0;
}

/* Paul add start 2012/10/15, for setting SRA. */
int SetSRA(int EnumSRAValue, int FromAteCmd)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char InputCmd[10];
    char UserCmd[64];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    
    //SET :0/1/2/3:OlrOff/OlrOn/SRAOFF/SRAON
		if(EnumSRAValue == 1)
			EnumSRAValue = 3;
		else
			EnumSRAValue = 2;

    sprintf(InputCmd, "%d", EnumSRAValue);
    strcpy(UserCmd, SET_SRA_ADSL2);
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);

    return 0;
}

typedef struct {
    char FwVer[80];
    char AdslFwVer[80];
    char AdslFwVerDisp[32];
} ADSL_SYS_INFO;

int HandleAdslSysInfo(ADSL_SYS_INFO* pInfo)
{
    char AdslFwVerBuf[80];
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSINFO, GET_LEN(SYSINFO), 0);
    int i;
    int j;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"syspwd") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    for (j=0; j<6; j++)
                    {
                        tok = scanner();
                        tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                        tok = scanner();
                        tok_assert_two(tok,TOKEN_COLON,TOKEN_COMMA);
                    }

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "fwver");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    strcpymax(pInfo->FwVer, scanner_get_str(), sizeof(pInfo->FwVer));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "adslver");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner_get_line();
                    strcpymax(pInfo->AdslFwVer, scanner_get_str(), sizeof(pInfo->AdslFwVer));

                    strcpymax(AdslFwVerBuf, pInfo->AdslFwVer, sizeof(AdslFwVerBuf));
                    scanner_set(AdslFwVerBuf, strlen(AdslFwVerBuf));
                    tok = scanner();
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_EQUAL,TOKEN_COLON);
                    tok = scanner();
                    if (tok == TOKEN_NUMBER_LITERAL)
                    {
                        strcpymax(pInfo->AdslFwVerDisp, scanner_get_str(), sizeof(pInfo->AdslFwVerDisp));
                    }
                    break;
                }
            }
        }
    }
    return 0;
}


typedef struct {
    char LanTx[10];
    char LanRx[10];
    char AdslTx[10];
    char AdslRx[10];
} ADSL_SYS_TRAFFIC;

int HandleAdslSysTraffic(ADSL_SYS_TRAFFIC* pTraffic)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSTRAFFIC, GET_LEN(SYSTRAFFIC), 0);
    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"lantx") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pTraffic->LanTx, scanner_get_str(), sizeof(pTraffic->LanTx));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "lanrx");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pTraffic->LanRx, scanner_get_str(), sizeof(pTraffic->LanRx));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "adsltx");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pTraffic->AdslTx, scanner_get_str(), sizeof(pTraffic->AdslTx));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "adslrx");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pTraffic->AdslRx, scanner_get_str(), sizeof(pTraffic->AdslRx));
                }
            }
        }
    }
    return 0;
}


typedef struct {
    char LineState[10];
    char Modulation[10];
    char SnrMarginUp[20];
    char SnrMarginDown[20];
    char LineAttenuationUp[20];
    char LineAttenuationDown[20];
    char DataRateUp[20];
    char DataRateDown[20];
} ADSL_SYS_STATUS;

int HandleAdslSysStatus(ADSL_SYS_STATUS* pSts)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSSTATUS, GET_LEN(SYSSTATUS), 0);

    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"index") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "linestate");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pSts->LineState, scanner_get_str(), sizeof(pSts->LineState));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "modulation");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pSts->Modulation, scanner_get_str(), sizeof(pSts->Modulation));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);


                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "snrup");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL,TOKEN_STRING);
                    strcpymax(pSts->SnrMarginUp, scanner_get_str(), sizeof(pSts->SnrMarginUp));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "snrdown");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL,TOKEN_STRING);
                    strcpymax(pSts->SnrMarginDown, scanner_get_str(), sizeof(pSts->SnrMarginDown));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "attenup");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                    strcpymax(pSts->LineAttenuationUp, scanner_get_str(), sizeof(pSts->LineAttenuationUp));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "attendown");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                    strcpymax(pSts->LineAttenuationDown, scanner_get_str(), sizeof(pSts->LineAttenuationDown));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "datarateup");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                    strcpymax(pSts->DataRateUp, scanner_get_str(), sizeof(pSts->DataRateUp));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "dataratedown");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                    strcpymax(pSts->DataRateDown, scanner_get_str(), sizeof(pSts->DataRateDown));
                    break;
                }
            }
        }
    }
    return 0;
}


typedef struct {
    char CrcDown[10];
    char CrcUp[10];
    char FecDown[10];
    char FecUp[10];
    char HecDown[10];
    char HecUp[10];
} ADSL_STATS;

int HandleAdslStats(ADSL_STATS* pStats)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL_STATS, GET_LEN(SHOW_ADSL_STATS), 0);

    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"crcdown") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pStats->CrcDown, scanner_get_str(), sizeof(pStats->CrcDown));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "crcup");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pStats->CrcUp, scanner_get_str(), sizeof(pStats->CrcUp));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "fecdown");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    strcpymax(pStats->FecDown, scanner_get_str(), sizeof(pStats->FecDown));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);


                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "fecup");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL,TOKEN_STRING);
                    strcpymax(pStats->FecUp, scanner_get_str(), sizeof(pStats->FecUp));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "hecdown");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL,TOKEN_STRING);
                    strcpymax(pStats->HecDown, scanner_get_str(), sizeof(pStats->HecDown));
                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "hecup");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert_two(tok,TOKEN_NUMBER_LITERAL, TOKEN_STRING);
                    strcpymax(pStats->HecUp, scanner_get_str(), sizeof(pStats->HecUp));
                    break;
                }
            }
        }
    }
    return 0;
}

//Paul add 2012/4/21
typedef struct {
    char annexMode[10];
} ADSL_ACTUAL_ANNEX_MODE;

//Paul add 2012/4/21
int HandleAdslActualAnnexMode(ADSL_ACTUAL_ANNEX_MODE* pStats)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL_ACTUAL_ANNEX_MODE, GET_LEN(SHOW_ADSL_ACTUAL_ANNEX_MODE), 0);

    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"Annex") == 0)
                {
                    tok = scanner_get_line();
                    strcpymax(pStats->annexMode, scanner_get_str(), sizeof(pStats->annexMode));
                    break;
                }
            }
        }
    }
    return 0;
}

typedef struct {
    char LineState[30];
} ADSL_LINK_STATUS;

int HandleAdslLinkStatus(ADSL_LINK_STATUS* pSts)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)GET_ADSL, GET_LEN(GET_ADSL), 0);

    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"current") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "modem");

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "status");

                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);

                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    strcpymax(pSts->LineState, scanner_get_str(), sizeof(pSts->LineState));
                }
            }
        }
    }
    return 0;
}


static ADSL_LINK_STATUS m_LinkSts;

void GetLinkSts(char* buf, int max_len)
{
    memset(&m_LinkSts,0,sizeof(m_LinkSts));
    HandleAdslLinkStatus(&m_LinkSts);
    strcpymax(buf,m_LinkSts.LineState,max_len);
}

static ADSL_SYS_TRAFFIC m_SysTraffic;
static ADSL_STATS m_Stats;
static ADSL_SYS_STATUS m_SysSts;
static ADSL_ACTUAL_ANNEX_MODE m_AnnexSts; //Paul add 2012/4/21

void UpdateAllAdslSts(int cnt, int from_ate)
{
    if (from_ate == 0)
    {
        memset(&m_SysTraffic, 0, sizeof(m_SysTraffic));
        HandleAdslSysTraffic(&m_SysTraffic);
        memset(&m_AnnexSts, 0, sizeof(m_AnnexSts));
        HandleAdslActualAnnexMode(&m_AnnexSts); //Paul add 2012/4/21
    }
    memset(&m_Stats, 0, sizeof(m_Stats));
    HandleAdslStats(&m_Stats);
    memset(&m_SysSts, 0, sizeof(m_SysSts));
    HandleAdslSysStatus(&m_SysSts);

    FILE* fp;

    if (from_ate)
    {
#ifdef WIN32
        fp = fopen("\\adsl_stats.txt","wb");
#else
        fp = fopen("/tmp/adsl/adslate.log","wb");
#endif
        if (fp == NULL) return;

        fputs("CRC Down : ",fp);
        fputs(m_Stats.CrcDown,fp);
        fputs("\n",fp);
        fputs("CRC Up : ",fp);
        fputs(m_Stats.CrcUp,fp);
        fputs("\n",fp);
        fputs("FEC Down : ",fp);
        fputs(m_Stats.FecDown,fp);
        fputs("\n",fp);
        fputs("FEC Up : ",fp);
        fputs(m_Stats.FecUp,fp);
        fputs("\n",fp);
        fputs("HEC Down : ",fp);
        fputs(m_Stats.HecDown,fp);
        fputs("\n",fp);
        fputs("HEC Up : ",fp);
        fputs(m_Stats.HecUp,fp);
        fputs("\n",fp);
        fputs("Line State : ",fp);
        fputs(m_SysSts.LineState,fp);
        fputs("\n",fp);
        fputs("Modulation : ",fp);
        fputs(m_SysSts.Modulation,fp);
        fputs("\n",fp);
        fputs("SNR Up : ",fp);
        fputs(m_SysSts.SnrMarginUp,fp);
        fputs("\n",fp);
        fputs("SNR Down : ",fp);
        fputs(m_SysSts.SnrMarginDown,fp);
        fputs("\n",fp);
        fputs("Line Attenuation Up : ",fp);
        fputs(m_SysSts.LineAttenuationUp,fp);
        fputs("\n",fp);
        fputs("Line Attenuation Down : ",fp);
        fputs(m_SysSts.LineAttenuationDown,fp);
        fputs("\n",fp);
        fputs("Data Rate Up : ",fp);
        fputs(m_SysSts.DataRateUp,fp);
        fputs("\n",fp);
        fputs("Data Rate Down : ",fp);
        fputs(m_SysSts.DataRateDown,fp);
        //fputs("\n",fp);

        fclose(fp);
        return;
    }


    if (strcmp(m_SysSts.LineState,"0") == 0)
    {
		nvram_adslsts("down");
		nvram_adslsts_detail("down");
    }
    else if (strcmp(m_SysSts.LineState,"1") == 0)
    {
		nvram_adslsts("wait for init"); //Paul modify 2012/6/19
		nvram_adslsts_detail("wait_for_init");
    }
    else if (strcmp(m_SysSts.LineState,"2") == 0)
    {
		nvram_adslsts("init"); //Paul modify 2012/6/19
		nvram_adslsts_detail("init");
    }
    else if (strcmp(m_SysSts.LineState,"3") == 0)
    {
		nvram_adslsts("up");
		nvram_adslsts_detail("up");
    }

	if (m_DbgOutputRedirectToFile) return;

//    FILE* fp;
#ifdef WIN32
    fp = fopen("\\adsl_stats.txt","wb");
#else
    fp = fopen("/tmp/adsl/adsllog.log","wb");
#endif
    if (fp == NULL) return;

    char buf[64];
    sprintf(buf, "Update Counter : %d\n", cnt);
    fputs(buf,fp);
    fputs("Modulation : ",fp);
    fputs(m_SysSts.Modulation,fp);
    fputs("\n",fp);
    fputs("Annex Mode : Annex ",fp);
    fputs(m_AnnexSts.annexMode,fp); //Paul add 2012/4/21
    fputs("\n",fp);
    fputs("Line State : ",fp);
    fputs(m_SysSts.LineState,fp);
    fputs("\n",fp);
    fputs("Lan Tx : ",fp);
    fputs(m_SysTraffic.LanTx,fp);
    fputs("\n",fp);
    fputs("Lan Rx : ",fp);
    fputs(m_SysTraffic.LanRx,fp);
    fputs("\n",fp);
    fputs("ADSL Tx : ",fp);
    fputs(m_SysTraffic.AdslTx,fp);
    fputs("\n",fp);
    fputs("ADSL Rx : ",fp);
    fputs(m_SysTraffic.AdslRx,fp);
    fputs("\n",fp);
    fputs("CRC Down : ",fp);
    fputs(m_Stats.CrcDown,fp);
    fputs("\n",fp);
    fputs("CRC Up : ",fp);
    fputs(m_Stats.CrcUp,fp);
    fputs("\n",fp);
    fputs("FEC Down : ",fp);
    fputs(m_Stats.FecDown,fp);
    fputs("\n",fp);
    fputs("FEC Up : ",fp);
    fputs(m_Stats.FecUp,fp);
    fputs("\n",fp);
    fputs("HEC Down : ",fp);
    fputs(m_Stats.HecDown,fp);
    fputs("\n",fp);
    fputs("HEC Up : ",fp);
    fputs(m_Stats.HecUp,fp);
    fputs("\n",fp);
    fputs("SNR Up : ",fp);
    fputs(m_SysSts.SnrMarginUp,fp);
    fputs("\n",fp);
    fputs("SNR Down : ",fp);
    fputs(m_SysSts.SnrMarginDown,fp);
    fputs("\n",fp);
    fputs("Line Attenuation Up : ",fp);
    fputs(m_SysSts.LineAttenuationUp,fp);
    fputs("\n",fp);
    fputs("Line Attenuation Down : ",fp);
    fputs(m_SysSts.LineAttenuationDown,fp);
    fputs("\n",fp);
    fputs("Data Rate Up : ",fp);
    fputs(m_SysSts.DataRateUp,fp);
    fputs("\n",fp);
    fputs("Data Rate Down : ",fp);
    fputs(m_SysSts.DataRateDown,fp);
    fputs("\n",fp);

    fclose(fp);
}

void GetPvcStats(char* outpkts, char* inpkts, char* incrcerr, int vpi, int vci)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char cmdbuf[80];
    int i;
    *outpkts = 0;
    *inpkts = 0;
    *incrcerr = 0;
    sprintf(cmdbuf, "sys tpget wan hwsar statistics %d %d\x0d\x0a", vpi, vci);
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)cmdbuf, strlen(cmdbuf), 0);
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"outPkts") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL_HEX_FORMAT);

                    strcpy(outpkts,scanner_get_str());

                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "inPkts");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL_HEX_FORMAT);

                    strcpy(inpkts,scanner_get_str());

                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "inCrcErr");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL_HEX_FORMAT);

                    strcpy(incrcerr,scanner_get_str());
                }
            }
        }
    }
}

int GetAllActivePvc(int idx[], int vlanid[], int vpi[], int vci[], int enc[], int mode[], int qostype[], int pcr[], int scr[], int mbs[])
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)PVC_DISP, GET_LEN(PVC_DISP), 0);
    int i;
    int pvc_cnt = 0;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"index") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);

                    idx[pvc_cnt] = atoi(scanner_get_str());

                    tok = scanner();
                    tok_assert(tok,TOKEN_COMMA);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok_assert_buf(scanner_get_str(), "active");
                    tok = scanner();
                    tok_assert(tok,TOKEN_EQUAL);
                    tok = scanner();
                    tok_assert(tok,TOKEN_NUMBER_LITERAL);
                    if (1==atoi(scanner_get_str()))
                    {
                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "encap");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        enc[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "mode");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        mode[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "vid");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        vlanid[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "vpi");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        vpi[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "vci");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        vci[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "qostype");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        qostype[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "pcr");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        pcr[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "scr");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        scr[pvc_cnt] = atoi(scanner_get_str());

                        tok = scanner();
                        tok_assert(tok,TOKEN_COMMA);
                        tok = scanner();
                        tok_assert(tok,TOKEN_STRING);
                        tok_assert_buf(scanner_get_str(), "mbs");
                        tok = scanner();
                        tok_assert(tok,TOKEN_EQUAL);
                        tok = scanner();
                        tok_assert(tok,TOKEN_NUMBER_LITERAL);

                        mbs[pvc_cnt] = atoi(scanner_get_str());

                        pvc_cnt++;
                    }
                }
            }
        }
    }

    return pvc_cnt;
}

void DispAllPvc()
{
    FILE* fp;
    int i;
    int pvc_cnt;
    int idx[MAX_PVC];
    int vlanid[MAX_PVC];
    int vpi[MAX_PVC];
    int vci[MAX_PVC];
    int enc[MAX_PVC];
    int mode[MAX_PVC];
    int qostype[MAX_PVC];
    int pcr[MAX_PVC];
    int scr[MAX_PVC];
    int mbs[MAX_PVC];
    char outpkts[MAX_PVC][16];
    char inpkts[MAX_PVC][16];
    char incrcerr[MAX_PVC][16];
    char linebuf[120];
    char* ErrMsgBuf;
    int ErrMsgLen;

    pvc_cnt = GetAllActivePvc(idx, vlanid, vpi, vci, enc, mode, qostype, pcr, scr, mbs);
    //printf("disppvc:%d\n",pvc_cnt);
    for (i=0; i<pvc_cnt; i++)
    {
        GetPvcStats(outpkts[i], inpkts[i], incrcerr[i], vpi[i], vci[i]);
    }

#ifdef WIN32
    fp = fopen("\\adsl_stats.txt","wb");
#else
    fp = fopen("/tmp/adsl/adslate.log","wa");
#endif
    if (fp == NULL) return;


    for (i=0; i<pvc_cnt; i++)
    {
        sprintf(linebuf,"idx:%d,vlanid:%d,vpi:%d,vci:%d,enc:%d,mode:%d\n",
            idx[i],vlanid[i],vpi[i],vci[i],enc[i],mode[i]);
        fputs(linebuf, fp);
        sprintf(linebuf,"idx:%d,qostype:%d,pcr:%d,scr:%d,mbs:%d\n",
            idx[i],qostype[i],pcr[i],scr[i],mbs[i]);
        fputs(linebuf, fp);
        fputs("outpkts:", fp);
        fputs(outpkts[i], fp);
        fputs(",inpkts:", fp);
        fputs(inpkts[i], fp);
        fputs(",incrcerr:", fp);
        fputs(incrcerr[i], fp);
        fputs("\n", fp);
    }
    ErrMsgBuf = GetErrMsg(&ErrMsgLen);
    if (ErrMsgLen > 0)
    {
        fputs(ErrMsgBuf, fp);
    }
    fclose(fp);
}

int DelPvc(int vpi, int vci)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char InputCmd[64];
    char UserCmd[256];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    sprintf(InputCmd, "%d %d", vpi, vci);
    strcpy(UserCmd, "sys tpset wan atm pvc del ");
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
    int i;
    int ret = -1;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (ret == -1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"OK") == 0)
                {
                    ret = 0;
                    break;
                }
                else
                {
                    PutErrMsg("DelPvc");
                }
            }
        }
    }
    return ret;
}


int DelAllPvc()
{
    int i;
    int ret = 0;
    int ret_del_pvc;
    int pvc_cnt;
    int idx[MAX_PVC];
    int vlanid[MAX_PVC];
    int vpi[MAX_PVC];
    int vci[MAX_PVC];
    int enc[MAX_PVC];
    int mode[MAX_PVC];
    int qostype[MAX_PVC];
    int pcr[MAX_PVC];
    int scr[MAX_PVC];
    int mbs[MAX_PVC];

    pvc_cnt = GetAllActivePvc(idx, vlanid, vpi, vci, enc, mode, qostype, pcr, scr, mbs);

    for (i=0; i<pvc_cnt; i++)
    {
        ret_del_pvc = DelPvc(vpi[i],vci[i]);
        if (ret_del_pvc != 0)
        {
            ret = -1;
            break;
        }
    }

    return ret;
}

/* Paul add 2012/8/7 */
int RestoreDefault()
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char UserCmd[64];
		UserCmd[0] = '\0';
    strcpy(UserCmd, "sys default");
    strcat(UserCmd, "\x0d\x0a");
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
    int i;
    int ret = -1;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (ret == -1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"OK") == 0)
                {
                    ret = 0;
                    break;
                }
                else
                {
                    PutErrMsg("sysdefault");
                }
            }
        }
    }
    return ret;
}

int SetQosToPvc(int idx, int SvcCat, int Pcr, int Scr, int Mbs)
{
//
// Trend-chip document says that we require to send a another command again to query the result
// The system have no error recovery so we do not send another command to confirm the API result
//
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char InputCmd[64];
    char UserCmd[256];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    sprintf(InputCmd, " %d %d %d %d", Pcr, Scr, Mbs, idx+1);
    strcpy(UserCmd, "sys tpset wan atm setqos ");
    if (QOS_UBR==SvcCat || QOS_UBR_NO_PCR==SvcCat) strcat(UserCmd, "ubr"); /* Paul modify 2012/8/6 */
    else if (QOS_CBR==SvcCat) strcat(UserCmd, "cbr");
    else if (QOS_VBR==SvcCat) strcat(UserCmd, "vbr");
    else if (QOS_GFR==SvcCat) strcat(UserCmd, "gfr");/* Paul modify 2012/8/6 */
    else if (QOS_NRT_VBR==SvcCat) strcat(UserCmd, "nrt_vbr");
    strcat(UserCmd,InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
    int i;
    int ret = -1;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (ret == -1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"OK") == 0)
                {
                    ret = 0;
                    break;
                }
            }
        }
    }
    return ret;
}



int AddPvc(int idx, int vlan_id, int vpi, int vci, int encap, int mode)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    char InputCmd[64];
    char UserCmd[256];
		InputCmd[0] = '\0';
		UserCmd[0] = '\0';
    sprintf(InputCmd, "%d %d %d %d %d %d", idx, vlan_id, vpi, vci, encap, mode);
    strcpy(UserCmd, "sys tpset wan atm pvc add ");
    strcat(UserCmd, InputCmd);
    strcat(UserCmd, "\x0d\x0a");
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
    int i;
    int ret = -1;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (ret == -1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"OK") == 0)
                {
                    ret = 0;
                    break;
                }
                else
                {
                    PutErrMsg("AddPvc Error");
                }
            }
        }
    }
    return ret;
}

void SettingSysDefault()
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SYS_DEFAULT, GET_LEN(SYS_DEFAULT), 1);
}

void GetVer(void)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
    FILE* fp;
    FILE* fp2;
    char BufStr[160];


#ifdef WIN32
    fp = fopen("\\tc_ver_info.txt","wb");
#else
    fp = fopen("/tmp/adsl/tc_ver_info.txt","wb");
#endif

    if (fp == NULL) return;

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSVER, GET_LEN(SYSVER), 0);

    int i;
    for(i=0; i<RespPktNum; i++)
    {
        scanner_set(GET_RESP_STR(pRespBuf[i]), GET_RESP_LEN(*pRespLen[i]));
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
                if (strcmp(pRespStr,"Bootbase") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("Bootbase:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
                }
                else if (strcmp(pRespStr,"RAS") == 0)
                {
					int buf_idx;
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("RAS:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
					// save RAS version to a single file
					for (buf_idx=0; buf_idx<sizeof(BufStr); buf_idx++) {
						if (BufStr[buf_idx]==0x0d || BufStr[buf_idx]==0x0a || BufStr[buf_idx]==0x20) BufStr[buf_idx] = 0;
						if (BufStr[buf_idx] == 0) break;
					}
					fp2 = fopen("/tmp/adsl/tc_ras_ver.txt","wb");
					if (fp2 != NULL)
					{
						fputs(BufStr,fp2);
						fclose(fp2);
					}
                }
                else if (strcmp(pRespStr,"System") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("System:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
                }
                else if (strcmp(pRespStr,"DSL") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("DSL:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
                }
                else if (strcmp(pRespStr,"Standard") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("Standard:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
                }
                else if (strcmp(pRespStr,"Country") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("Country:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
                }
                else if (strcmp(pRespStr,"MAC") == 0)
                {
                    tok = scanner();
                    tok_assert(tok,TOKEN_STRING);
                    tok = scanner();
                    tok_assert(tok,TOKEN_COLON);
                    tok = scanner_get_line();
                    fputs("MAC:",fp);
                    strcpymax(BufStr, scanner_get_str(), sizeof(BufStr));
                    fputs(BufStr,fp);
                    fputs("\n",fp);
                }
            }
        }
    }

    fclose(fp);

}

void InitTc(void)
{
    declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);

    // this command should wait TC firmware boot completed
    int Retry;
    for (Retry = 0; Retry < 10; Retry++)
    {
        SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, BROADCAST_ADDR,
            MAC_RTS_START, NULL, 0, CON_START_RESP_NUM);
        if (RespPktNum > 0) break;
    }
    if (RespPktNum > 0)
    {
        memcpy(m_AdslMacAddr,&pRespBuf[0][6],6);
    }
    else
    {
        FILE* fpBoot;
        g_AlwaysIgnoreAdslCmd = TRUE;
        fpBoot = fopen("/tmp/adsl/ADSL_FATAL_BOOT_ERROR.txt","wb");
        if (fpBoot != NULL)
        {
            fputs("init TC failed",fpBoot);
            fputs("\n",fpBoot);
            fclose(fpBoot);
        }
        return;
    }
    myprintf("ADSL MAC IS %02x-%02x-%02x-%02x-%02x-%02x",m_AdslMacAddr[0],m_AdslMacAddr[1],m_AdslMacAddr[2],
        m_AdslMacAddr[3],m_AdslMacAddr[4],m_AdslMacAddr[5]);

    SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
        MAC_RTS_CONSOLE_ON, NULL, 0, CON_ON_RESP_NUM);

    for (Retry = 0; Retry < 10; Retry++)
    {
		SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
			MAC_RTS_CONSOLE_CMD, CMD_CRLF, CMD_CRLF_LEN, CON_CRLF_RESP_NUM);

		if (RespPktNum > 1)
		{
			scanner_set(GET_RESP_STR(pRespBuf[1]), GET_RESP_LEN(*pRespLen[1]));
			while (1)
			{
				tok = scanner();
				if (tok == TOKEN_EOF)
				{
					break;
				}
				if (tok == TOKEN_STRING)
				{
					pRespStr = scanner_get_str();
					if (strcmp(pRespStr,"Password") == 0)
					{
						SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
							MAC_RTS_CONSOLE_CMD, (PUCHAR)PSWD, GET_LEN(PSWD), CON_PSWD_RESP_NUM);
					}
				}
			}
		}
	}
}


static ADSL_SYS_INFO m_SysInfo;
static ADSL_LAN_IP m_IpAddr;

void WriteAdslFwInfoToFile()
{
    char buf[32];
    memset(&m_SysInfo, 0, sizeof(m_SysInfo));
    HandleAdslSysInfo(&m_SysInfo);
    memset(&m_IpAddr, 0, sizeof(m_IpAddr));
    HandleAdslShowLan(&m_IpAddr);

    // store IP address to /tmp/adsl/tc_ip_addr.txt
    // for adsl firmware upgrade
    FILE* fp;
#ifdef WIN32
    fp = fopen("\\tc_ip_addr.txt","wb");
#else
    fp = fopen("/tmp/adsl/tc_ip_addr.txt","wb");
#endif
    if (fp == NULL) return;

    fputs(m_IpAddr.IpAddr,fp);
    //fputs("\n",fp);
    fclose(fp);

    // store version to /tmp/adsl/tc_fw_ver.txt
    // for adsl status web page

#ifdef WIN32
    fp = fopen("\\tc_fw_ver.txt","wb");
#else
    fp = fopen("/tmp/adsl/tc_fw_ver.txt","wb");
#endif
    if (fp == NULL) return;

    fputs(m_SysInfo.AdslFwVer,fp);
    //fputs("\n",fp);
    fclose(fp);

#ifdef WIN32
    fp = fopen("\\tc_fw_ver_short.txt","wb");
#else
    fp = fopen("/tmp/adsl/tc_fw_ver_short.txt","wb");
#endif
    if (fp == NULL) return;

    fputs(m_SysInfo.AdslFwVerDisp,fp);
    //fputs("\n",fp);
    fclose(fp);


#ifdef WIN32
    fp = fopen("\\tc_mac.txt","wb");
#else
    fp = fopen("/tmp/adsl/tc_mac.txt","wb");
#endif
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",m_AdslMacAddr[0],m_AdslMacAddr[1],m_AdslMacAddr[2],
        m_AdslMacAddr[3],m_AdslMacAddr[4],m_AdslMacAddr[5]);

    if (fp == NULL) return;
    fputs(buf,fp);
    fputs("\n",fp);
    fclose(fp);
}


static int m_Exit = 0;
void mysigterm()
{
	//printf("I caught the SIGTERM signal!\n");
	m_Exit = 1;
	return;
}


int main(int argc, char* argv[])
{
#ifndef WIN32
	sigset_t sigs_to_catch;


	// SOME SIGNALS ARE BLOCKED BY RC
	// TO USE TIMER SIGNAL, IT MUST USE THE FOLLOWING CODE TO ENABLE TIMER
	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

    if (signal(SIGTERM, mysigterm) == SIG_ERR)
    {
    	myprintf("Cannot handle SIGTERM!\n");
    	return -1;
	}

    if(switch_init() < 0)
    {
        myprintf("err\n");
        return -1;
    }


    //printf("\n************** TP_INIT ***************\n");

#if DSL_N55U == 1
	m_AdaptIdx = if_nametoindex("eth2.1");
#else
    #error "new model"
#endif

    m_SocketSend = socket(AF_PACKET,SOCK_RAW, htons(ETH_P_ALL));
    if (m_SocketSend== -1)
    {
        myprintf("err\n");
    }

// not working , SNDBUF could not set
/*
    int nReadSend;
    getsockopt(m_SocketSend,SOL_SOCKET,SO_SNDBUF,&nReadSend,sizeof(int));
    myprintf("nRecvSend:%d\n",nReadSend);

    int nNewSend=2*1024;
    setsockopt(m_SocketSend,SOL_SOCKET,SO_SNDBUF,(const char*)&nNewSend,sizeof(int));
    getsockopt(m_SocketSend,SOL_SOCKET,SO_SNDBUF,&nReadSend,sizeof(int));
    myprintf("nNewSend:%d\n",nReadSend);
*/

#else

    m_PtDrv = NULL;
	m_PtDrv = new ProtDrv;
	if (m_PtDrv == NULL)
	{
	    exit(0);
	}
	BOOLEAN RetVal;
	RetVal = m_PtDrv->InitAdapterInfo();
	if (RetVal == FALSE)
	{
		printf("error InitAdapterInfo()...");
		return TRUE;
	}
	int NumCard;
	NumCard = m_PtDrv->GetNumAdapterInfo();
	if (NumCard == 0)
	{
		printf("No card binded to prot drv...");
		return TRUE;
	}
	int AdaptInfoIdx;
	for (AdaptInfoIdx = 0; AdaptInfoIdx < NumCard; AdaptInfoIdx++)
	{
		wprintf(m_PtDrv->GetAdapterConnName(AdaptInfoIdx));
	}
	m_AdaptOpened = FALSE;
	if (NumCard>1)
	{
		wprintf(L"Too much NIC binded to prot drv.");
        wprintf(L"Please unbind prot drv from unused NIC.");
	}
	else
	{
		wprintf(m_PtDrv->GetAdapterConnName(0));
		m_PtDrv->OpenAdapter(0);
		wprintf(L"\r\nOpen adapter......\r\n");
		m_AdaptOpened = TRUE;
    }
        // rcv all pkt
    m_PtDrv->SetRcvMode(0);

#endif

    GetMacAddr();

    if (argc == 2)
    {
    	if (!strcmp(argv[1],"clear_modem_var"))
    	{
	        InitTc();
	        SettingSysDefault();
	        printf("sys default\n");
	        return 0;
        }
    	if (!strcmp(argv[1],"eth_wan_mode_only"))
    	{
			InitTc();
			GetVer();
			WriteAdslFwInfoToFile();
			DelAllPvc();
	        return 0;
        }
    }

#ifdef WIN32
    m_ShowPktLog  = TRUE;
    m_EnableDbgOutput = TRUE;
#else
	int dbg_flag = nvram_get_dbg_flag();
	if (dbg_flag == 1)
	{
		m_EnableDbgOutput = TRUE;
		m_ShowPktLog  = TRUE;
		m_DbgOutputRedirectToFile = FALSE;
	}
	else if (dbg_flag == 2)
	{
		m_EnableDbgOutput = TRUE;
		m_ShowPktLog  = TRUE;
		m_DbgOutputRedirectToFile = TRUE;
	}
	else if (dbg_flag == 3)
	{
		m_EnableDbgOutput = TRUE;
		m_ShowPktLog  = FALSE;
		m_DbgOutputRedirectToFile = FALSE;
	}
#endif

    if (argc == 1)
    {
        int config_num;
        int iptv_port;
        int internet_pvc;
        config_num = nvram_load_config_num(&iptv_port);
        internet_pvc = write_ipvc_mode(config_num, iptv_port);
        nvram_load_pvcs(config_num, iptv_port, internet_pvc);


        CreateMsgQ();
        InitTc();
        GetVer();
        /*
        AddPvc(0,1,0,33,0,0);
        AddPvc(1,2,0,35,0,0);
        AddPvc(6,3,0,37,0,0);
        AddPvc(3,5,0,39,0,0);
        DelAllPvc();
        */

        WriteAdslFwInfoToFile();

        DelAllPvc();


//---------------  test err msg start

        /*
        PutErrMsg("11111");
        PutErrMsg("2222222222");
        PutErrMsg("3333333333");
        {
            int tt;
            for (tt=0; tt<200; tt++)
            {
                PutErrMsg("ZZZZZZZZZZZZZZZZZZZZZZZZZZX");
            }
        }
        {
            char* msgbuf;
            int len;
            msgbuf = GetErrMsg(&len);
            char test;
            test = *(msgbuf+len);
        }
        */

//---------------  test err msg end

//---------------  QOS test case start

        //AddPvc(0,1,0,33,0,0);
        //AddPvc(1,2,0,35,0,0);
        //AddPvc(2,3,0,36,0,0);
        //AddPvc(3,4,0,37,0,0);
        //AddPvc(4,5,0,38,0,0);
        //AddPvc(5,6,0,39,0,0);
        /*
        SetQosToPvc(0,QOS_CBR,500,0,0);
        SetQosToPvc(1,QOS_CBR,501,0,0);
        SetQosToPvc(2,QOS_CBR,502,0,0);
        SetQosToPvc(3,QOS_CBR,503,0,0);
        SetQosToPvc(4,QOS_CBR,504,0,0);
        SetQosToPvc(5,QOS_CBR,505,0,0);
        */

        //SetQosToPvc(0,QOS_UBR,0,0,0);
        //SetQosToPvc(1,QOS_UBR,1000,0,0);
        //SetQosToPvc(2,QOS_CBR,500,0,0);
        //SetQosToPvc(3,QOS_VBR,300,299,300);
        //SetQosToPvc(4,QOS_GFR,600,100,200);
        //SetQosToPvc(5,QOS_NRT_VBR,1000,100,200);

        //DispAllPvc();

//--------------  QOS test case end


/*
        AddPvc(0,1,0,33,0,0);
        AddPvc(1,2,0,34,0,0);
        AddPvc(2,3,0,35,0,0);
        AddPvc(3,4,0,36,0,0);
        AddPvc(4,5,0,37,0,0);
        AddPvc(5,6,0,38,0,0);
        AddPvc(6,7,0,39,0,0);
        AddPvc(7,8,0,40,0,0);
*/

        nvram_set_adsl_fw_setting(config_num, iptv_port, internet_pvc, 1);
        SetPollingTimer();
        if (config_num > 0)
        {
            enable_polling();
        }

#ifndef WIN32

	nvram_adslsts("down");
	nvram_adslsts_detail("down");


	// tell auto detection tp_init is ready
	// this is also for ate/telnet program sync
	// This is for telnetd sync up. While telnetd available, ADSL commands all sent to TC.
	nvram_set("dsltmp_tcbootup","1");

    while(m_Exit == 0)
    {
//        SleepMs(500);
        int ret_rcv_msg;
        ret_rcv_msg = RcvMsgQ();
        if (ret_rcv_msg == -1)
        {
            // error
            break;
        }
        else if (ret_rcv_msg == 1)
        {
            // someone ask tp_init quit
            break;
        }
    }

    printf("exit tp_init\n");
    return 0;

#else

    int cnt = 0;
    while(1)
    {
        SleepMs(10*1000);
        UpdateAllAdslSts(cnt++, 0);
    }


#endif

    }
    else
    {
        m_ShowPktLog  = TRUE;
        m_EnableDbgOutput = TRUE;
        while (1)
        {
            declare_resp_handling_vars(pRespBuf, pRespLen, RespPktNum, tok, pRespStr);
            char InputChar;
            char InputCmd[100];
            char UserCmd[256];
						InputCmd[0] = '\0';
						UserCmd[0] = '\0';
            //int InputVpi;
            //int InputVci;
            ShowMsgAndInput(&InputChar, InputCmd);

            if (InputChar == '0')
            {
                InitTc();
                // test , modify
//                DelAllPvc();
/*
                m_ShowPktLog = FALSE;
                AddPvc(0,1,0,33,0,0);
                AddPvc(1,2,0,35,0,0);
                AddPvc(2,3,0,36,0,0);
                AddPvc(3,4,0,37,0,0);
                DispAllPvc();
                m_ShowPktLog = TRUE;
*/
				// askey
				// ipoa
                //AddPvc(0,1,0,32,0,2);
				// pppoa
				// 0,38 vc
//                AddPvc(0,1,0,38,1,1);
            }

            if (InputChar == '1')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSINFO, GET_LEN(SYSINFO), 0);
            }

            if (InputChar == '2')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSSTATUS, GET_LEN(SYSSTATUS), 0);
            }

            if (InputChar == '3')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)SYSTRAFFIC, GET_LEN(SYSTRAFFIC), 0);
            }

            if (InputChar == '4')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_WAN, GET_LEN(SHOW_WAN),0);
            }

            if (InputChar == '5')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_ADSL, GET_LEN(SHOW_ADSL),0);
            }
            if (InputChar == '6')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)SHOW_LAN, GET_LEN(SHOW_LAN),0);
            }
            if (InputChar == '7')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)PVC_DISP, GET_LEN(PVC_DISP), 0);
            }
            if (InputChar == '8')
            {
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)GET_ADSL, GET_LEN(GET_ADSL), 0);
            }

            if (InputChar == 'a' || InputChar == 'v')
            {
                strcpy(UserCmd, "sys tpset wan atm pvc add ");
                strcat(UserCmd, InputCmd);
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
            }

            if (InputChar == 'c')
            {
                strcpy(UserCmd, InputCmd);
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
            }

            /*
            if (InputChar == 's')
            {
                USHORT VlanId = 1234;
                UCHAR SendPktData[] = "\x08\x06\x00\x01\x08\x00\x06\x04\x00\x01\x00\x0a\x79\x60\x16\xb1\x8c\x70\x43\xb0\x00\x00\x00\x00\x00\x00\x8c\x70\x43\xab\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
                SendOnePktVlan(BROADCAST_ADDR, VlanId, SendPktData, sizeof(SendPktData)-1);
            }
            */

            if (InputChar == 'i')
            {
                strcpy(UserCmd, "sys tpset wan atm pvc add ");
                strcat(UserCmd, "2 1 0 35 0 0");
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
								UserCmd[0] = '\0';
                strcpy(UserCmd, "sys tpset wan atm pvc add ");
                strcat(UserCmd, "3 2 0 36 0 0");
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
								UserCmd[0] = '\0';
                strcpy(UserCmd, "sys tpset wan atm pvc add ");
                strcat(UserCmd, "4 3 0 37 0 0");
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
            }

            if (InputChar == 's')
            {
                strcpy(UserCmd, "sys tpget wan hwsar statistics ");
                strcat(UserCmd, "0 35");
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
								UserCmd[0] = '\0';
                strcpy(UserCmd, "sys tpget wan hwsar statistics ");
                strcat(UserCmd, "0 36");
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
								UserCmd[0] = '\0';
                strcpy(UserCmd, "sys tpget wan hwsar statistics ");
                strcat(UserCmd, "0 37");
                strcat(UserCmd, "\x0d\x0a");
                SendCmdAndWaitResp(pRespBuf, MAX_RESP_BUF_SIZE, pRespLen, MAX_RESP_BUF_NUM, &RespPktNum, m_AdslMacAddr,
                    MAC_RTS_CONSOLE_CMD, (PUCHAR)UserCmd, strlen(UserCmd), 0);
            }



            if (InputChar == 'q')
            {
                printf("exit\r\n");
                break;
            }


        }
    }




#ifndef WIN32

    switch_fini();

    if (m_SocketSend) close(m_SocketSend);

#else
    _getch();
#endif

}


