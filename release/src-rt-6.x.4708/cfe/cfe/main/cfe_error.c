/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Error strings				File: cfe_error.h 
    *  
    *  This file contains a mapping from error codes to strings
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */


#include "lib_types.h"
#include "cfe.h"
#include "cfe_error.h"

/*  *********************************************************************
    *  Types
    ********************************************************************* */


typedef struct errmap_s {
    int errcode;
    const char *string;
} errmap_t;

/*  *********************************************************************
    *  Error code list
    ********************************************************************* */

errmap_t cfe_errorstrings[] = {
    {CFE_OK		    ,"No error"},
    {CFE_ERR                ,"Error"},
    {CFE_ERR_INV_COMMAND    ,"Invalid command"},
    {CFE_ERR_EOF	    ,"End of file reached"},
    {CFE_ERR_IOERR	    ,"I/O error"},
    {CFE_ERR_NOMEM	    ,"Insufficient memory"},
    {CFE_ERR_DEVNOTFOUND    ,"Device not found"},
    {CFE_ERR_DEVOPEN	    ,"Device is open"},
    {CFE_ERR_INV_PARAM	    ,"Invalid parameter"},
    {CFE_ERR_ENVNOTFOUND    ,"Environment variable not found"},
    {CFE_ERR_ENVREADONLY    ,"Environment variable is read-only"},
    {CFE_ERR_NOTELF	    ,"Not an ELF-format executable"},
    {CFE_ERR_NOT32BIT 	    ,"Not a 32-bit executable"},
    {CFE_ERR_WRONGENDIAN    ,"Executable is wrong-endian"},
    {CFE_ERR_BADELFVERS     ,"Invalid ELF file version"},
    {CFE_ERR_NOTMIPS 	    ,"Not a MIPS ELF file"},
    {CFE_ERR_BADELFFMT 	    ,"Invalid ELF file"},
    {CFE_ERR_BADADDR 	    ,"Section would load outside available DRAM"},
    {CFE_ERR_FILENOTFOUND   ,"File not found"},
    {CFE_ERR_UNSUPPORTED    ,"Unsupported function"},
    {CFE_ERR_HOSTUNKNOWN    ,"Host name unknown"},
    {CFE_ERR_TIMEOUT	    ,"Timeout occured"},
    {CFE_ERR_PROTOCOLERR    ,"Network protocol error"},
    {CFE_ERR_NETDOWN	    ,"Network is down"},
    {CFE_ERR_NONAMESERVER   ,"No name server configured"},
    {CFE_ERR_NOHANDLES	    ,"No more handles"},
    {CFE_ERR_ALREADYBOUND   ,"Already bound"},
    {CFE_ERR_CANNOTSET	    ,"Cannot set network parameter"},
    {CFE_ERR_NOMORE         ,"No more enumerated items"},
    {CFE_ERR_BADFILESYS     ,"File system not recognized"},
    {CFE_ERR_FSNOTAVAIL     ,"File system not available"},
    {CFE_ERR_INVBOOTBLOCK   ,"Invalid boot block on disk"},
    {CFE_ERR_WRONGDEVTYPE   ,"Device type is incorrect for boot method"},
    {CFE_ERR_BBCHECKSUM     ,"Boot block checksum is invalid"},
    {CFE_ERR_BOOTPROGCHKSUM ,"Boot program checksum is invalid"},
    {CFE_ERR_LDRNOTAVAIL,    "Loader is not available"},
    {CFE_ERR_NOTREADY,       "Device is not ready"},
    {CFE_ERR_GETMEM,         "Cannot get memory at specified address"},
    {CFE_ERR_SETMEM,         "Cannot set memory at specified address"},
    {CFE_ERR_NOTCONN,	     "Socket is not connected"},
    {CFE_ERR_ADDRINUSE,	     "Address is in use"},
    {CFE_ERR_INTR,	     "Interrupted"},
    {0,NULL}};


/*  *********************************************************************
    *  cfe_errortext(err)
    *  
    *  Returns the text corresponding to a CFE error code
    *  
    *  Input parameters: 
    *  	   err - error code
    *  	   
    *  Return value:
    *  	   string description of error
    ********************************************************************* */

const char *cfe_errortext(int err)
{
    errmap_t *e = cfe_errorstrings;

    while (e->string) {
	if (e->errcode == err) return e->string;
	e++;
	}

    return (const char *) "Unknown error";
}
