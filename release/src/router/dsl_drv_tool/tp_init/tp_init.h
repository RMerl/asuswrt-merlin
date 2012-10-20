
#define MAC_RTS_START 1
#define MAC_RTS_RESPONSE 2
#define MAC_RTS_STOP 3
#define MAC_RTS_ON 4
#define MAC_RTS_CONSOLE_ON 5
#define MAC_RTS_CONSOLE_CMD 6
#define MAC_RTS_CONSOLE_DATA 7

#define DIR_PC_TO_ADSL 0
#define DIR_ADSL_TO_PC 1

#define MAX_RESP_BUF_NUM 24
#define MAX_RESP_BUF_SIZE 256





#define BROADCAST_ADDR (PUCHAR)"\xff\xff\xff\xff\xff\xff"
//#define TEST_MAC_ADDR (PUCHAR)"\x00\x11\x22\x33\x44\x55"
#define CMD_CRLF (PUCHAR)"\x0d\x0a"
#define CMD_CRLF_LEN 2
#define MAC_LEN 6
#define ETH_TYPE "\xaa\xaa"
#define ETH_TYPE_LEN 2
#define GET_LEN(x) (sizeof(x)-1)


#define PSWD "admin\x0d\x0a"
#define SYSINFO "sys tpget sys show sysinfo\x0d\x0a"
#define SYSSTATUS "sys tpget sys show sysstatus\x0d\x0a"
#define SYSTRAFFIC "sys tpget sys show systraffic\x0d\x0a"
#define SHOW_WAN "sys tpget sys show wan\x0d\x0a"
#define SHOW_LAN "sys tpget sys show lan\x0d\x0a"
#define SHOW_ADSL "sys tpget sys show adsl\x0d\x0a"
#define SYS_DEFAULT "sys default\x0d\x0a"

#define SET_ADSL "sys tpset wan atm pvc add 2 1234 0 35 0 0\x0d\x0a"

#define PVC_DISP "sys tpget wan atm pvc disp\x0d\x0a"
#define GET_ADSL "sys tpget wan adsl status\x0d\x0a"

#define SET_ADSL_MODE "sys tpset wan adsl mode "
#define SET_ADSL_TYPE "sys tpset wan adsl type "
#define SHOW_ADSL_STATS "sys tpget wan adsl statistics\x0d\x0a"
#define SHOW_ADSL_ACTUAL_ANNEX_MODE "w ghs show op\x0d\x0a" //Paul add 2012/4/21

/* Paul add 2012/9/24, for SNR Margin tweaking. */
#define SET_SNRM_Offset_ADSL "w dmt set snrm "
#define SET_SNRM_Offset_ADSL2 "w dmt2 set snrm "

#define SET_SRA_ADSL2 "w dmt2 set olr " /* Paul add 2012/10/15, for setting SRA. */

#define SYSVER "sys ver\x0d\x0a"

#define CON_START_RESP_NUM 1
#define CON_ON_RESP_NUM 1
#define CON_CRLF_RESP_NUM 2
#define CON_PSWD_RESP_NUM 4
/*
#define CON_SYSINFO_RESP_NUM 3
#define CON_SYSSTATUS_RESP_NUM 3
#define CON_SYSTRAFFIC_RESP_NUM 3 
#define CON_SHOWAN_RESP_NUM 3
#define CON_SHOWADSL_RESP_NUM 3
#define CON_SHOWLAN_RESP_NUM 3 
#define CON_SETADSL_RESP_NUM 3 
#define CON_PVCDISP_RESP_NUM 11
#define CON_GETADSL_RESP_NUM 3 
*/

enum {
    QOS_UBR_NO_PCR,
    QOS_UBR,    
    QOS_CBR,
    QOS_VBR,
    QOS_GFR,
    QOS_NRT_VBR
};
