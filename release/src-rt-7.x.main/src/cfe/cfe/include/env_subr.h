/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  Environment helper routines		File: env_subr.h
    *  
    *  Definitions and prototypes for environment variable subroutines
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



/*  *********************************************************************
    *  Constants
    ********************************************************************* */


/*
 * TLV types.  These codes are used in the "type-length-value"
 * encoding of the items stored in the NVRAM device (flash or EEPROM)
 *
 * The layout of the flash/nvram is as follows:
 *
 * <type> <length> <data ...> <type> <length> <data ...> <type_end>
 *
 * The type code of "ENV_TLV_TYPE_END" marks the end of the list.
 * The "length" field marks the length of the data section, not
 * including the type and length fields.
 *
 * Environment variables are stored as follows:
 *
 * <type_env> <length> <flags> <name> = <value>
 *
 * If bit 0 (low bit) is set, the length is an 8-bit value.
 * If bit 0 (low bit) is clear, the length is a 16-bit value
 * 
 * Bit 7 set indicates "user" TLVs.  In this case, bit 0 still
 * indicates the size of the length field.  
 *
 * Flags are from the constants below:
 *
 */

#define ENV_LENGTH_16BITS	0x00	/* for low bit */
#define ENV_LENGTH_8BITS	0x01

#define ENV_TYPE_USER		0x80

#define ENV_CODE_SYS(n,l) (((n)<<1)|(l))
#define ENV_CODE_USER(n,l) ((((n)<<1)|(l)) | ENV_TYPE_USER)

/*
 * The actual TLV types we support
 */

#define ENV_TLV_TYPE_END	0x00	
#define ENV_TLV_TYPE_ENV	ENV_CODE_SYS(0,ENV_LENGTH_8BITS)

/*
 * Environment variable flags 
 */

#define ENV_FLG_NORMAL		0x00	/* normal read/write */
#define ENV_FLG_BUILTIN		0x01	/* builtin - not stored in flash */
#define ENV_FLG_READONLY	0x02	/* read-only - cannot be changed */

#define ENV_FLG_MASK		0xFF	/* mask of attributes we keep */
#define ENV_FLG_ADMIN		0x100	/* lets us internally override permissions */

/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int env_delenv(const char *name);
char *env_getenv(const char *name);
int env_setenv(const char *name,char *value,int flags);
int env_load(void);
int env_save(void);
int env_enum(int idx,char *name,int *namelen,char *val,int *vallen);
int env_envtype(const char *name);
