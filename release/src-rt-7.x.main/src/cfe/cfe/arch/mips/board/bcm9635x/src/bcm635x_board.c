/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  bcm635x_board.c   utility functions for bcm635X board
    *  
    *  Created on :  09/25/2002  seanl
    *
    *********************************************************************

<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
    
#include "bcm63xx_util.h"
extern NVRAM_DATA nvramData;   
extern int readNvramData(void);

static int parsePsiSize(char *psiStr)
{
    int psiSize = atoi(psiStr);

    if (psiSize >= 1 && psiSize <= NVRAM_PSI_MAX_SIZE)
        return 0;
    else
        return 1;
}

static int parseMemConfig(char *memSizeStr)
{
    int memSize = atoi(memSizeStr);

    if (memSize >= MEMORY_635X_16MB_1_CHIP && memSize <= MEMORY_635X_64MB_2_CHIP)
        return 0;
    else
        return 1;
}

static PARAMETER_SETTING gBoardParam[] =
{
    // prompt name                      Error Prompt Define Param  Validation function
    {"Board Id String(max 16 char) :", BOARDID_STR_PROMPT, "", "", parseBoardIdStr},
    {"Psi size in KB (default:127) :", PSI_SIZE_PROMPT, "", "", parsePsiSize},
    {"Number of MAC Addresses(1-32):", MAC_CT_PROMPT, "", "", parseMacAddrCount},
    {"Base MAC Address             :", MAC_ADDR_PROMPT, "", "", parseMacAddr},
    {"Memory size:   Range 0-2\n\
16MB (1 chip)  ------- 0\n\
32MB (2 chips) ------- 1\n\
64MB (2 chips) ------- 2     :", MEM_CONFIG_PROMPT, "", "", parseMemConfig},
    {NULL}
};


// getBoardParam:  convert the board param data and put them in the gBoardParam struct
//
void getBoardParam(void)
{
    int memType;
    
    if (nvramData.szBoardId[0] != (char)0xff)
        memcpy(gBoardParam[0].parameter, nvramData.szBoardId, NVRAM_BOARD_ID_STRING_LEN);
    else
        memset(gBoardParam[0].parameter, 0, NVRAM_BOARD_ID_STRING_LEN);

    if (nvramData.ulPsiSize == 0xffffffff)
        nvramData.ulPsiSize = NVRAM_PSI_DEFAULT_635X;
    sprintf(gBoardParam[1].parameter, "%d", nvramData.ulPsiSize);

    if (nvramData.ulNumMacAddrs == 0xffffffff)
        nvramData.ulNumMacAddrs = NVRAM_MAC_COUNT_DEFAULT_635X;
    sprintf(gBoardParam[2].parameter, "%d", nvramData.ulNumMacAddrs);

    if (nvramData.ucaBaseMacAddr[0] == 0xff && nvramData.ucaBaseMacAddr[1] == 0xff &&
        nvramData.ucaBaseMacAddr[2] == 0xff && nvramData.ucaBaseMacAddr[3] == 0xff)
        memset(gBoardParam[3].parameter, 0, NVRAM_MAC_ADDRESS_LEN);
    else
        macNumToStr(nvramData.ucaBaseMacAddr, gBoardParam[3].parameter);  

    memType = kerSysMemoryTypeGet();

    if (!(memType >= MEMORY_635X_16MB_1_CHIP && memType <= MEMORY_635X_64MB_2_CHIP))
        memType = MEMORY_635X_16MB_1_CHIP;
    sprintf(gBoardParam[4].parameter, "%d", memType);
}


// setBoardParam: Set the board Id string, mac addresses, psi size, etc...
//
int setBoardParam(void)
{
    int bChange = FALSE;
    int count = 0;
    PPARAMETER_SETTING tmpPtr = gBoardParam;    
    int memType;

    while (tmpPtr->promptName !=  NULL)
    {
        count++;
        tmpPtr++;
    }

    getBoardParam();

    while (1)
    {
        bChange = processPrompt(gBoardParam, count);
        if (strlen(gBoardParam[3].parameter) != 0)
            break;
        printf("Base Mac address is not entered.  Try it again!\n");
    }

    if (bChange)
    {
        // fill in board id string, psi and mac count
        nvramData.ulVersion = NVRAM_VERSION_NUMBER;
        nvramData.ulEnetModeFlag = 0xffffffff;          // all ffs for enet mode flag
        memset(nvramData.szBoardId, 0, NVRAM_BOARD_ID_STRING_LEN);
        memcpy(nvramData.szBoardId, gBoardParam[0].parameter, NVRAM_BOARD_ID_STRING_LEN);
        nvramData.ulPsiSize = atoi(gBoardParam[1].parameter);
        nvramData.ulNumMacAddrs = atoi(gBoardParam[2].parameter);
        parsehwaddr(gBoardParam[3].parameter, nvramData.ucaBaseMacAddr);

        // set memory type thing
        memType = atoi(gBoardParam[4].parameter);
        kerSysMemoryTypeSet((int) FLASH63XX_ADDR_BOOT_ROM, (char *)&memType, sizeof(int));

        if (nvramData.ulPsiSize != NVRAM_PSI_DEFAULT_635X)
        {
            printf("Are you REALLY sure persistent storage size is %d?", nvramData.ulPsiSize);
            if (yesno())
            {
                nvramData.ulPsiSize = NVRAM_PSI_DEFAULT_635X;
                sprintf(gBoardParam[1].parameter, "%d", nvramData.ulPsiSize);
                return 0;
            }
        }
        // save the buf to nvram
        writeNvramData();

        printf("Press any key to reset the board: \n");
        while (1)
            if (console_status())
                kerSysMipsSoftReset();
    }

    return 0;

}  // setBoardParam


void displayBoardParam(void)
{
    PPARAMETER_SETTING tmpPtr = gBoardParam;
    int count = 0;
    int i;
    int memType;
    char memoryStr[] = "Memory size                  :";
    char megaByte[30];

    while (tmpPtr->promptName !=  NULL)
    {
        count++;
        tmpPtr++;
    }
    
    getBoardParam();
    for (i = 0; i < (count-1); i++)
        printf("%s %s  \n", gBoardParam[i].promptName, gBoardParam[i].parameter);
    // print last memory type seperately
    memType = kerSysMemoryTypeGet();
    switch (memType)
    {
    case MEMORY_635X_16MB_1_CHIP:
        strcpy(megaByte, "16MB");
        break;
    case MEMORY_635X_32MB_2_CHIP:
        strcpy(megaByte, "32MB");
        break;
    case MEMORY_635X_64MB_2_CHIP:
        strcpy(megaByte, "64MB");
        break;
    default:
        strcpy(megaByte, "***Not defined?***");
        break;
    }
    printf("%s %s\n\n", memoryStr, megaByte);
}


void setDefaultBootline(void)
{
    memset(nvramData.szBootline, 0, NVRAM_BOOTLINE_LEN);
    strncpy(nvramData.szBootline, DEFAULT_BOOTLINE_635X, strlen(DEFAULT_BOOTLINE_635X));
    printf("Use default boot line parameters: %s\n", DEFAULT_BOOTLINE_635X);
    writeNvramData();
    readNvramData();
    convertBootInfo();
}
