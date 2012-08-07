/***************************************************************************
 * Broadcom Corp. Confidential
 * Copyright 2001, 2002 Broadcom Corp. All Rights Reserved.
 *
 * THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED 
 * SOFTWARE LICENSE AGREEMENT BETWEEN THE USER AND BROADCOM. 
 * YOU HAVE NO RIGHT TO USE OR EXPLOIT THIS MATERIAL EXCEPT 
 * SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
 *
 ***************************************************************************
 * File Name  : bcm63xx_util.h 
 *
 * Created on :  04/18/2002  seanl
 ***************************************************************************/

#if !defined(_BCM63XX_UTIL_H_)
#define _BCM63XX_UTIL_H_

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_console.h"
#include "cfe_devfuncs.h"
#include "cfe_timer.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"
#include "env_subr.h"
#include "ui_command.h"
#include "cfe.h"
#include "net_ebuf.h"
#include "net_ether.h"
#include "net_api.h"
#include "cfe_fileops.h"
#include "bsp_config.h"
#include "cfe_mem.h"
#include "cfe_loader.h"
#include "addrspace.h"

#include "dev_bcm63xx_flash.h"
#include "board.h"
#include "bcmTag.h"

#define ETH_ALEN            6
#define OK                  0
#define ERROR               -1
#define MAX_PROMPT_LEN      50         // assume no one wants to type more than 50 chars 
#define MAX_MAC_STR_LEN     19         // mac address string 18+1 in regular format   
#define PROMPT_DEFINE_LEN   2
#define MASK_LEN            8           // vxworks like ffffff00


typedef struct 
{
   char* promptName;
   char* errorPrompt;
   char  promptDefine[PROMPT_DEFINE_LEN];
   char  parameter[MAX_PROMPT_LEN];
   int   (*func)(char *);
} PARAMETER_SETTING, *PPARAMETER_SETTING;

#define IP_PROMPT           "Invalid ip address. eg. 192.168.1.200[:ffffff00]"
#define RUN_FROM_PROMPT     "f = jump to flash; h = tftpd from host"
#define HOST_FN_PROMPT      "eg. vmlinux" 
#define FLASH_FN_PROMPT     "eg. bcm963xx_fs_kernel"
#define BOOT_DELAY_PROMPT   "range 0-9, 0=forever prompt"

// error input prompts
#define BOARDID_STR_PROMPT  "Invalid board id string size: 1 - 16 bytes"
#define MAC_CT_PROMPT       "Invalid Mac addresses number: 1 - 32"
#define MAC_ADDR_PROMPT     "Invalid Mac address format!  eg. 12:34:56:ab:cd:ef"
#define PSI_SIZE_PROMPT     "Invalid PSI size (in KB): 1 - 128"
#define MEM_CONFIG_PROMPT   "Invalid memory configuration type: 0 - 3"

#define DEFAULT_BOOTLINE_6345   "e=192.168.1.1:ffffff00 h=192.168.1.100 g= r=f f=vmlinux i=bcm96345_fs_kernel d=1 "
#define DEFAULT_BOOTLINE_635X   "e=192.168.1.1:ffffff00 h=192.168.1.100 g= r=f f=vmlinux i=bcm9635x_fs_kernel d=1 "

#define DEFAULT_BOARD_IP    "192.168.1.1"
#define DEFAULT_MASK        "255.255.255.0"
// bootline definition:
// Space is the deliminator of the parameters.  Currently supports following parameters:
// f=vmlinux (if r=h) i=bcm96345_fs_kernel d=3 (default delay, range 0-9, 0=forever prompt)

#define BOOT_IP_LEN         18
#define BOOT_FILENAME_LEN   50	// "f=vmlinux"

typedef struct				
{
    char boardIp[BOOT_IP_LEN];
    char boardMask[BOOT_IP_LEN];        // set for the board only and ignore for the host/gw. fmt :ffffff00
    char hostIp[BOOT_IP_LEN];
    char gatewayIp[BOOT_IP_LEN];
    char runFrom;
    char hostFileName[BOOT_FILENAME_LEN];
    char flashFileName[BOOT_FILENAME_LEN];
    int  bootDelay;
} BOOT_INFO, *PBOOT_INFO;

extern void setDefaultBootline(void);
extern void convertBootInfo(void);
extern int printSysInfo(void);
extern int changeBootLine(void);
extern void dumpHex(unsigned char *start, int len);
extern BOOT_INFO bootInfo;
extern void enet_init(void);
extern int flashImage(uint8_t *ptr);
extern int  writeWholeImage(uint8_t *ptr, int size);
extern void bcm63xx_run(void);

extern int setBoardParam(void);
extern void getBoardParam(void);
extern void displayBoardParam(void);
extern int parseBoardIdStr(char *);
extern int parseMacAddrCount(char *);
extern int parseMacAddr(char *);
extern int macNumToStr(unsigned char *macAddr, unsigned char *str);
extern int processPrompt(PPARAMETER_SETTING promptPtr, int promptCt);
extern int parsehwaddr(unsigned char *str,uint8_t *hwaddr);
extern int parseBoardIdStr(char *boardIdStr);
extern UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc);
extern int yesno(void);
extern void writeNvramData(void);

#endif // _BCM63XX_UTIL_H_
