/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  bcm63xx utility functions
    *  
    *  Created on :  04/18/2002  seanl
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
    
#define  BCMTAG_EXE_USE
#include "bcm63xx_util.h"

extern const char *get_system_type(void);
extern int readNvramData(void);

int changeBootLine(void);
BOOT_INFO bootInfo;
NVRAM_DATA nvramData;   

static int parseFilename(char *fn)
{
    if (strlen(fn) < BOOT_FILENAME_LEN)
        return 0;
    else
        return 1;
}

static int parseChoiceFh(char *choice)
{

    if (*choice == 'f' || *choice == 'h')
        return 0;
    else
        return 1;
}


static int parseChoice09(char *choice)
{
    int bChoice = *choice - '0';

    if (bChoice >= 0 && bChoice <= 9)
        return 0;
    else
        return 1;
}

static int parseIpAddr(char *ipStr);

static PARAMETER_SETTING gBootParam[] =
{
    // prompt name                  Error Prompt    Boot Define Boot Param  Validation function
    {"Board IP address             :", IP_PROMPT       , "e=", "", parseIpAddr},
    {"Host IP address              :", IP_PROMPT       , "h=", "", parseIpAddr},
    {"Gateway IP address           :", IP_PROMPT       , "g=", "", parseIpAddr},
    {"Run from flash/host (f/h)    :", RUN_FROM_PROMPT , "r=", "", parseChoiceFh},
    {"Default host run file name   :", HOST_FN_PROMPT  , "f=", "", parseFilename},
    {"Default host flash file name :", FLASH_FN_PROMPT , "i=", "", parseFilename},
    {"Boot delay (0-9 seconds)     :", BOOT_DELAY_PROMPT,"d=", "", parseChoice09},
    {NULL},
};


static const char hextable[16] = "0123456789ABCDEF";
void dumpHex(unsigned char *start, int len)
{
    unsigned char *ptr = start,
    *end = start + len;

    while (ptr < end)
    {
        long offset = ptr - start;
        unsigned char text[120],
        *p = text;
        while (ptr < end && p < &text[16 * 3])
        {
            *p++ = hextable[*ptr >> 4];
            *p++ = hextable[*ptr++ & 0xF];
            *p++ = ' ';
        }
        p[-1] = 0;
        printf("%4lX %s\n", offset, text);
    }
}



static int parsexdigit(char str)
{
    int digit;

    if ((str >= '0') && (str <= '9')) 
        digit = str - '0';
    else if ((str >= 'a') && (str <= 'f')) 
        digit = str - 'a' + 10;
    else if ((str >= 'A') && (str <= 'F')) 
        digit = str - 'A' + 10;
    else 
        return -1;

    return digit;
}


// convert in = fffffff00 to out=255.255.255.0
// return 0 = OK, 1 failed.
static int convertMaskStr(char *in, char *out)
{
    int i;
    char twoHex[2];
    uint8_t dest[4];
    char mask[BOOT_IP_LEN];

    if (strlen(in) != MASK_LEN)      // mask len has to 8
        return 1;

    memset(twoHex, 0, 2);
    for (i = 0; i < 4; i++)
    {
        twoHex[0] = (uint8_t)*in++;
        twoHex[1] = (uint8_t)*in++;
        if (parsexdigit(*twoHex) == -1)
            return 1;
        dest[i] = (uint8_t) xtoi(twoHex);
    }
    sprintf(mask, "%d.%d.%d.%d", dest[0], dest[1], dest[2], dest[3]);
    strcpy(out, mask);
    return 0;    
}

// return 0 - OK, !0 - Bad ip
static int parseIpAddr(char *ipStr)
{
    char *x;
    uint8_t dest[4];
    char mask[BOOT_IP_LEN];       
    char ipMaskStr[2*BOOT_IP_LEN];

    strcpy(ipMaskStr, ipStr);

    x = strchr(ipMaskStr,':');
    if (!x)                     // no mask
        return parseipaddr(ipMaskStr, dest);

    *x = '\0';

    if (parseipaddr(ipMaskStr, dest))        // ipStr is not ok
        return 1;

    x++;
    return convertMaskStr(x, mask);      // mask is not used here

}


int parsehwaddr(unsigned char *str,uint8_t *hwaddr)
{
    int digit1,digit2;
    int idx = 6;

    if (strlen(str) != MAX_MAC_STR_LEN-2)
        return -1;
    if (*(str+2) != ':' || *(str+5) != ':' || *(str+8) != ':' || *(str+11) != ':' || *(str+14) != ':')
        return -1;
    
    while (*str && (idx > 0)) {
	    digit1 = parsexdigit(*str);
	    if (digit1 < 0)
            return -1;
	    str++;
	    if (!*str)
            return -1;

	    if (*str == ':') {
	        digit2 = digit1;
	        digit1 = 0;
	    }
	    else {
	        digit2 = parsexdigit(*str);
	        if (digit2 < 0)
                return -1;
            str++;
	    }

	    *hwaddr++ = (digit1 << 4) | digit2;
	    idx--;

	    if (*str == ':') 
            str++;
	}
    return 0;
}


int parseMacAddr(char * macStr)
{
    unsigned char tmpBuf[MAX_PROMPT_LEN];
    
    return (parsehwaddr(macStr, tmpBuf));
}


int parseBoardIdStr(char *boardIdStr)
{
    if (strlen(boardIdStr) > 0 && strlen(boardIdStr) <= NVRAM_BOARD_ID_STRING_LEN)
        return 0;
    else
        return 1;
}


int parseMacAddrCount(char *ctStr)
{
    int count = atoi(ctStr);

    if (count >= 1 && count <= NVRAM_MAC_COUNT_MAX)
        return 0;
    else
        return 1;
}


//**************************************************************************
// Function Name: macNumToStr
// Description  : convert MAC address from array of 6 bytes to string.
//                Ex: 0a0b0c0d0e0f -> 0a:0b:0c:0d:0e:0f
// Returns      : status.
//**************************************************************************
int macNumToStr(unsigned char *macAddr, unsigned char *str)
{
   if (macAddr == NULL || str == NULL) 
       return ERROR;

   sprintf(str, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
           macAddr[0], macAddr[1], macAddr[2], 
           macAddr[3], macAddr[4], macAddr[5]);
   return OK;
}



void enet_init(void)
{
    char ethCmd[150]; 
    unsigned char macAddr[ETH_ALEN];
    char macStr[MAX_MAC_STR_LEN];

    strcpy(ethCmd, "ifconfig eth0 -addr=");

    if (strlen(bootInfo.boardIp) > 0)
        strcat(ethCmd, bootInfo.boardIp);
    else
        strcat(ethCmd, DEFAULT_BOARD_IP);      // provide a default t ip

    strcat(ethCmd, " -mask=");
    if (strlen(bootInfo.boardMask) > 0)
        strcat(ethCmd, bootInfo.boardMask);
    else
        strcat(ethCmd, DEFAULT_MASK);
    
    if (strlen(bootInfo.gatewayIp) > 0)     // optional
    {
        strcat(ethCmd, " -gw=");
        strcat(ethCmd, bootInfo.gatewayIp);
    }

    memcpy(macAddr, nvramData.ucaBaseMacAddr, ETH_ALEN);
    memset(macStr, 0, MAX_MAC_STR_LEN);
    macNumToStr(macAddr, macStr);

    strcat(ethCmd, " -hwaddr=");
    strcat(ethCmd, macStr);

    ui_docommand(ethCmd);

}

/***************************************************************************
// Function Name: getCrc32
// Description  : caculate the CRC 32 of the given data.
// Parameters   : pdata - array of data.
//                size - number of input data bytes.
//                crc - either CRC32_INIT_VALUE or previous return value.
// Returns      : crc.
****************************************************************************/
UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc) 
{
    while (size-- > 0)
        crc = (crc >> 8) ^ Crc32_table[(crc ^ *pdata++) & 0xff];

    return crc;
}



// return -1: fail.
//         0: OK.
int flashImage(uint8_t *imagePtr)
{
    UINT32 crc;
    FLASH_ADDR_INFO info;
    PFILE_TAG pTag = (PFILE_TAG) imagePtr;
    int totalImageSize = 0;
    int cfeSize, rootfsSize, kernelSize;
    int cfeAddr, rootfsAddr, kernelAddr;
    int status = 0;
    int boardIdLen;
    int memType;

    kerSysFlashAddrInfoGet( &info );

    if( strcmp( pTag->tagVersion, BCM_TAG_VER ) )
    {
        printf("File tag version %s is not supported. Version %s must be used.\n",
            pTag->tagVersion, BCM_TAG_VER);
        return -1;
    }

    boardIdLen = strlen(pTag->boardId);
    if (memcmp(nvramData.szBoardId, pTag->boardId, boardIdLen) != 0)
    {
        printf( "Image with id '" );
        printf( pTag->boardId );
        printf( "' cannot be flashed onto board '" );
        printf( nvramData.szBoardId );
        printf( "'\n" );
        return -1;
    }

    // check tag validate token first
    crc = CRC32_INIT_VALUE;
    crc = getCrc32(imagePtr, (UINT32)TAG_LEN-TOKEN_LEN, crc);      

    if (crc != (UINT32)(*(UINT32*)(pTag->tagValidationToken)))
    {
        printf("Illegal image ! Tag crc failed.\n");
        return -1;
    }

    // check imageCrc
    totalImageSize = atoi(pTag->totalImageLen);
    crc = CRC32_INIT_VALUE;
    crc = getCrc32((imagePtr+TAG_LEN), (UINT32) totalImageSize, crc);      

    if (crc != (UINT32) (*(UINT32*)(pTag->imageValidationToken)))
    {
        printf(" Illegal image ! Image crc failed.\n");
        return -1;
    }
      
    cfeSize = rootfsSize = kernelSize = cfeAddr = rootfsAddr = kernelAddr = 0;

    // check cfe's existence
    cfeAddr = atoi(pTag->cfeAddress);
    if (cfeAddr)
    {
        cfeSize = atoi(pTag->cfeLen);
        if( (cfeSize <= 0) )
        {
            printf("Illegal cfe size [%d].\n", cfeSize );
            return -1;
        }
        
        // save the memory type
        memType = kerSysMemoryTypeGet();;

        printf("\nFlashing CFE: ");
        if ((status = kerSysBcmImageSet(cfeAddr, imagePtr+TAG_LEN, cfeSize, 0)) != 0)
        {
            printf("Failed to flash CFE. Error: %d\n", status);
            return status;
        }

        // restore the memory type
        kerSysMemoryTypeSet((int) FLASH63XX_ADDR_BOOT_ROM, (char *) &memType, sizeof(int));
    }

    // check root filesystem and kernel existence
    rootfsAddr = atoi(pTag->rootfsAddress);
    kernelAddr = atoi(pTag->kernelAddress);

#if defined(DEBUG_FLASH)
    printf("kernelAddr=0x%x, info->flash_nvram_start_blk=%d\n", kernelAddr, info.flash_nvram_start_blk); //~~~
#endif

    if( rootfsAddr && kernelAddr )
    {
        unsigned int psiAddr = (unsigned int) flash_get_memptr((byte)info.flash_persistent_start_blk);

        psiAddr = psiAddr + info.flash_persistent_blk_offset;

#if defined(DEBUG_FLASH)
        printf("psiAddr = 0x%x\n", psiAddr);
#endif

        rootfsSize = atoi(pTag->rootfsLen);
        kernelSize = atoi(pTag->kernelLen);
        if((rootfsSize + kernelSize + TAG_LEN) > (unsigned int) (psiAddr - rootfsAddr))
        {
            printf("Illegal root file system [%d] and kernel [%d] combined sizes " \
                " [%d].\nSizes must not be greater than [%d]\n", rootfsSize,
                kernelSize, rootfsSize + kernelSize + TAG_LEN, (unsigned int) psiAddr - rootfsAddr);
            return -1;
        }
        printf("\nFlashing root file system: ");
        if ((status = kerSysBcmImageSet(rootfsAddr, imagePtr+TAG_LEN+cfeSize, rootfsSize, 0)) != 0)
        {
            printf("Failed to flash root file system. Error: %d\n", status);
            return status;
        }

        printf("\nFlashing kernel: ");
        // tag is alway at the sector start and followed by kernel
        memcpy(imagePtr+cfeSize+rootfsSize, imagePtr, TAG_LEN);

#if defined(DEBUG_FLASH)
        printf("kernelAddr=0x%x\n", kernelAddr);
#endif

        if ((status = kerSysBcmImageSet(kernelAddr, 
            imagePtr+cfeSize+rootfsSize, kernelSize+TAG_LEN, 0)) != 0)
        {
            printf("Failed to flash kernel. Error: %d\n", status);
            return status;
        }
    }

    printf(".\n*** Image flash done *** !\n");

    return status;
}


// return -1: fail.
//         0: OK.
int writeWholeImage(uint8_t *imagePtr, int wholeImageSize)
{
    UINT32 crc;
    int status = 0;
    int imageSize = wholeImageSize - TOKEN_LEN;
    unsigned char crcBuf[CRC_LEN];

    // check tag validate token first
    crc = CRC32_INIT_VALUE;
    crc = getCrc32(imagePtr, (UINT32)imageSize, crc);      
    memcpy(crcBuf, imagePtr+imageSize, CRC_LEN);
    if (memcmp(&crc, crcBuf, CRC_LEN) != 0)
    {
        printf("Illegal whole flash image\n");
        return -1;
    }

    if ((status = kerSysBcmImageSet((unsigned int)FLASH63XX_ADDR_BOOT_ROM, imagePtr, imageSize, 1)) != 0)
    {
        printf("Failed to flash whole image. Error: %d\n", status);
        return status;
    }
    return status;
}



int processPrompt(PPARAMETER_SETTING promptPtr, int promptCt)
{
    unsigned char tmpBuf[MAX_PROMPT_LEN];
    int i = 0;
    int bChange = FALSE;

    printf("Press:  <enter> to use current value\r\n");
    printf("        '-' to go previous parameter\r\n");
    printf("        '.' to clear the current value\r\n");
    printf("        'x' to exit this command\r\n");

    do 
    {
        if (strlen((promptPtr+i)->parameter) > 0)
            printf("%s  %s  ", (promptPtr+i)->promptName, (promptPtr+i)->parameter);
        else
            printf("%s  %s", (promptPtr+i)->promptName, (promptPtr+i)->parameter);

	    memset(tmpBuf, 0, MAX_PROMPT_LEN);
	    console_readline ("", tmpBuf, sizeof (tmpBuf));

        switch (tmpBuf[0])
        {
            case '-':          // go back one parameter
                if (i > 0)
                    i--;
                break;
            case '.':          // clear the current parameter and advance
                memset((promptPtr+i)->parameter, 0, MAX_PROMPT_LEN);
                i++;
                break;
            case 'x':          // get out the b command
                i = promptCt;
                break;
            case 0:            // no input; use default
                i++;
                break;
            default:            // new parameter
                if ((promptPtr+i)->func != 0)  // validate function is supplied, do a check
                {
                    if ((promptPtr+i)->func(tmpBuf))
                    {
                        printf("\n%s;  Try again!\n", (promptPtr+i)->errorPrompt);
                        break;
                    }
                }
                memset((promptPtr+i)->parameter, 0, MAX_PROMPT_LEN);
                memcpy((promptPtr+i)->parameter, tmpBuf, strlen(tmpBuf));
                i++;
                bChange = TRUE;
        }
    } while (i < promptCt);

    return bChange;

} // processPrompt

// write the nvramData struct to nvram after CRC is calculated 
void writeNvramData(void)
{
    UINT32 crc = CRC32_INIT_VALUE;
    
    nvramData.ulCheckSum = 0;
    crc = getCrc32((char *)&nvramData, (UINT32) sizeof(NVRAM_DATA), crc);      
    nvramData.ulCheckSum = crc;
    kerSysNvRamSet((char *)&nvramData, sizeof(NVRAM_DATA), NVRAM_VERSION_NUMBER_ADDRESS);
}

// read the nvramData struct from nvram 
// return -1:  crc fail, 0 ok
int readNvramData(void)
{
    UINT32 crc = CRC32_INIT_VALUE, savedCrc;
    
    kerSysNvRamGet((char *)&nvramData, sizeof(NVRAM_DATA), NVRAM_VERSION_NUMBER_ADDRESS);
    savedCrc = nvramData.ulCheckSum;
    nvramData.ulCheckSum = 0;
    crc = getCrc32((char *)&nvramData, (UINT32) sizeof(NVRAM_DATA), crc);      
    if (savedCrc != crc)
        return -1;
    
    return 0;
}

void convertBootInfo(void)
{
    char *x;

    memset(&bootInfo, 0, sizeof(BOOT_INFO));
    strcpy(bootInfo.boardIp, gBootParam[0].parameter);

    if ((x = strchr(bootInfo.boardIp, ':')))        // has mask
    {
        *x = '\0';
        convertMaskStr((x+1), bootInfo.boardMask);
    }
    strcpy(bootInfo.hostIp, gBootParam[1].parameter);
    if ((x = strchr(bootInfo.hostIp, ':')))        // ignore host mask
        *x = '\0';
    strcpy(bootInfo.gatewayIp, gBootParam[2].parameter);
    if ((x = strchr(bootInfo.gatewayIp, ':')))        // ignore gw mask
        *x = '\0';
    bootInfo.runFrom = gBootParam[3].parameter[0];
    strcpy(bootInfo.hostFileName, gBootParam[4].parameter);
    strcpy(bootInfo.flashFileName, gBootParam[5].parameter);
    bootInfo.bootDelay = (int) (gBootParam[6].parameter[0] - '0');
}



static void getBootLine(int bootCt)
{
    int i;
    int blLen;
    char *curPtr;
    char *dPtr;

    // the struct is filled up in kerSysFlashInit
    if ((nvramData.szBootline[0] == 'b' && nvramData.szBootline[1] == 'c' && nvramData.szBootline[2] == 'm') ||
        (nvramData.szBootline[0] == (char)0xff))
        setDefaultBootline();

    do 
    {
        readNvramData();
        curPtr = nvramData.szBootline;
        blLen = NVRAM_BOOTLINE_LEN;

        for (i = 0; i < bootCt; i++)
        {
		    curPtr = strnchr(curPtr, '=', blLen); 
            if (curPtr) // found '=' and get the param.
            {
                dPtr = strnchr(curPtr, ' ', blLen);   // find param. deliminator ' '
                if (dPtr == NULL)
                {
                    setDefaultBootline();
                    break;  //break for loop and start over again
                }
                memset(gBootParam[i].parameter, 0, MAX_PROMPT_LEN);
                memcpy(gBootParam[i].parameter, curPtr+1, dPtr-curPtr-1);
            }
            else
            {
                setDefaultBootline();
                break;  //break for loop and start over again
            }
            // move to next param.
            curPtr = dPtr;  
        } // for loop

    } while (i != bootCt);   // do until the all the parameters are accounted for

}



// print the bootline and board parameter info and fill in the struct for later use
// 
int printSysInfo(void)
{
    int i, count;
    PPARAMETER_SETTING tmpPtr = gBootParam;
    count = 0;
    
    if (readNvramData() != 0)
    {
        while ((nvramData.szBoardId[0] == (char)0xff) || 
            (nvramData.ulPsiSize < NVRAM_PSI_MIN_SIZE && nvramData.ulPsiSize > NVRAM_PSI_MAX_SIZE) ||
            (nvramData.ulNumMacAddrs < 1 && nvramData.ulNumMacAddrs > NVRAM_MAC_COUNT_MAX))
        {
            printf("*** Board is not initialized properly ***\n\n");
            setBoardParam();
        }
    }

    // display the bootline info
    while (tmpPtr->promptName !=  NULL)
    {
        count++;
        tmpPtr++;
    }
    getBootLine(count);

    for (i = 0; i < count; i++)
        printf("%s %s  \n", gBootParam[i].promptName, gBootParam[i].parameter);
    convertBootInfo();

    // print the board param
    displayBoardParam();

    return 0;
}


//**************************************************************************
// Function Name: changeBootLine
// Description  : Use vxWorks bootrom style parameter input method:
//                Press <enter> to use default, '-' to go to previous parameter  
//                Note: Parameter is set to current value in the menu.
// Returns      : None.
//**************************************************************************
int changeBootLine(void)
{
    int i, count;
    PPARAMETER_SETTING tmpPtr = gBootParam;
    int bChange = FALSE;

    count = 0;
    while (tmpPtr->promptName !=  NULL)
    {
        count++;
        tmpPtr++;
    }

    getBootLine(count);

    bChange = processPrompt(gBootParam, count);

    if (bChange)
    {
        char *blPtr = nvramData.szBootline;
        int paramLen;

        memset(blPtr, 0, NVRAM_BOOTLINE_LEN);
        for (i = 0; i < count; i++)
        {
            memcpy(blPtr, gBootParam[i].promptDefine, PROMPT_DEFINE_LEN);
            blPtr += PROMPT_DEFINE_LEN;
            paramLen = strlen(gBootParam[i].parameter);
            memcpy(blPtr, gBootParam[i].parameter, paramLen);
            blPtr += paramLen;
            if (!(gBootParam[i].parameter[0] == ' '))
            {
                memcpy(blPtr, " ", 1);
                blPtr += 1;
            }
        }
        writeNvramData();
    }

    readNvramData();
    convertBootInfo();

    enet_init();

    return 0;

}  
