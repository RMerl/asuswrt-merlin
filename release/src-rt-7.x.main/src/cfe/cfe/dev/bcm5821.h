/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  BCM5821 cryptoaccelerator		       File: bcm5821.h
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

#ifndef _BCM5821_H_
#define _BCM5821_H_

/* Register and field definitions for the Broadcom BCM5821 crypto
   accelerator.  The BCM5820 implements a compatible (modulo bugs)
   subset of the BCM5821. */

#define K_PCI_VENDOR_BROADCOM    0x14e4
#define K_PCI_ID_BCM5820         0x5820
#define K_PCI_ID_BCM5821         0x5821

#define _DD_MAKEMASK1(n) (1 << (n))
#define _DD_MAKEMASK(v,n) ((((1)<<(v))-1) << (n))
#define _DD_MAKEVALUE(v,n) ((v) << (n))
#define _DD_GETVALUE(v,n,m) (((v) & (m)) >> (n))


/* DMA Control and Status Register offsets */

#define R_MCR1                   0x00
#define R_DMA_CTRL               0x04
#define R_DMA_STAT               0x08
#define R_DMA_ERR                0x0C
#define R_MCR2                   0x10


/* 0x00  MCR1@: Master Command Record 1 Address */
/* 0x10  MCR2@: Master Command Record 2 Address */


/* 0x04  DMA Control */

#define  M_DMA_CTRL_WR_BURST     _DD_MAKEMASK1(16)   /* Not 5820 */

#define  K_DMA_WR_BURST_128      0
#define  K_DMA_WR_BURST_240      M_DMA_CRTL_WR_BURST

#define M_DMA_CTRL_MOD_NORM      _DD_MAKEMASK1(22)
#define M_DMA_CTRL_RNG_MODE      _DD_MAKEMASK1(23)
#define M_DMA_CTRL_DMAERR_EN     _DD_MAKEMASK1(25)
#define M_DMA_CTRL_NORM_PCI      _DD_MAKEMASK1(26)   /* Not 5820 */
#define M_DMA_CTRL_LE_CRYPTO     _DD_MAKEMASK1(27)   /* Not 5820 */
#define M_DMA_CTRL_MCR1_INT_EN   _DD_MAKEMASK1(29)
#define M_DMA_CTRL_MCR2_INT_EN   _DD_MAKEMASK1(30)
#define M_DMA_CTRL_RESET         _DD_MAKEMASK1(31)


/* 0x08  DMA Status */

#define M_DMA_STAT_MCR2_EMPTY    _DD_MAKEMASK1(24)   /* Not 5820 */
#define M_DMA_STAT_MCR1_EMPTY    _DD_MAKEMASK1(25)   /* Not 5820 */
#define M_DMA_STAT_MCR2_INTR     _DD_MAKEMASK1(26)
#define M_DMA_STAT_MCR2_FULL     _DD_MAKEMASK1(27)
#define M_DMA_STAT_DMAERR_INTR   _DD_MAKEMASK1(28)
#define M_DMA_STAT_MCR1_INTR     _DD_MAKEMASK1(29)
#define M_DMA_STAT_MCR1_FULL     _DD_MAKEMASK1(30)
#define M_DMA_STAT_MSTR_ACCESS   _DD_MAKEMASK1(31)

/* 0x0C  DMA Error Address */

#define M_DMA_ERR_RD_FAULT       _DD_MAKEMASK1(1)
#define M_DMA_ERR_ADDR           0xFFFFFFFC


/* Master Command Record Header Format */

#define S_MCR_NUM_PACKETS        0
#define M_MCR_NUM_PACKETS        _DD_MAKEMASK(16,S_MCR_NUM_PACKETS)
#define V_MCR_NUM_PACKETS(x)     _DD_MAKEVALUE(x,S_MCR_NUM_PACKETS)
#define G_MCR_NUM_PACKETS(x)     _DD_GETVALUE(x,S_MCR_NUM_PACKETS,M_MCR_NUM_PACKETS)

/* Input flags */

#define M_MCR_SUPPRESS_INTR      _DD_MAKEMASK1(31)

/* Output flags */

#define M_MCR_DONE               _DD_MAKEMASK1(16)
#define M_MCR_ERROR              _DD_MAKEMASK1(17)

#define S_MCR_ERROR_CODE         24
#define M_MCR_ERROR_CODE         _DD_MAKEMASK(8,S_MCR_ERROR_CODE)
#define V_MCR_ERROR_CODE(x)      _DD_MAKEVALUE(x,S_MCR_ERROR_CODE)
#define G_MCR_ERROR_CODE(x)      _DD_GETVALUE(x,S_MCR_ERROR_CODE,M_MCR_ERROR_CODE)
#define K_MCR_ERROR_OK           0
#define K_MCR_ERROR_UNKNOWN_OP   1
#define K_MCR_ERROR_DSA_SHORT    2
#define K_MCR_ERROR_PKI_SHORT    3
#define K_MCR_ERROR_PKO_SHORT    4                    /* Not 5820 */
#define K_MCR_ERROR_CHAIN_SHORT  5                    /* Not 5820 */
#define K_MCR_ERROR_FIFO         6                    /* Not 5820 */

/* In both cases, the header word is followed by an array of N PD entries:
     commandContext[0]
     dataBuffer[0]
     pktLen[0]
     outputBuffer[0]
     ...
     commandContext[n-1]
     dataBuffer[n-1]
     pktLen[n-1]
     outputBuffer[n-1]
*/

#define MCR_WORDS(n)  (1+8*(n))
#define MCR_BYTES(n)  ((1+8*(n))*4)


/* Data Buffer Chain Entry Offsets */

#define DBC_ADDR                 0
#define DBC_NEXT                 4
#define DBC_LEN                  8

#define CHAIN_WORDS              3

#define S_DBC_DATA_LEN           0
#define M_DBC_DATA_LEN           _DD_MAKEMASK(16,S_DBC_DATA_LEN)
#define V_DBC_DATA_LEN(x)        _DD_MAKEVALUE(x,S_DBC_DATA_LEN)
#define G_DBC_DATA_LEN(x)        _DD_GETVALUE(x,S_DBC_DATA_LEN,M_DBC_DATA_LEN)

/* Packet Descriptor Offsets */

#define PD_CC_ADDR               0
#define PD_INPUT_FRAG            4
#define PD_INPUT_FRAG_ADDR       (PD_INPUT_FRAG+DBC_ADDR)
#define PD_INPUT_FRAG_NEXT       (PD_INPUT_FRAG+DBC_NEXT)
#define PD_INPUT_FRAG_LEN        (PD_INPUT_FRAG+DBC_LEN)
#define PD_PKT_LEN               16
#define PD_OUTPUT_FRAG           20
#define PD_OUTPUT_FRAG_ADDR      (PD_OUTPUT_FRAG+DBC_ADDR)
#define PD_OUTPUT_FRAG_NEXT      (PD_OUTPUT_FRAG+DBC_NEXT)
#define PD_OUTPUT_FRAG_LEN       (PD_OUTPUT_FRAG+DBC_LEN)

#define PD_SIZE                  32

#define S_PD_PKT_LEN             16
#define M_PD_PKT_LEN             _DD_MAKEMASK(16,S_PD_PKT_LEN)
#define V_PD_PKT_LEN(x)          _DD_MAKEVALUE(x,S_PD_PKT_LEN)
#define G_PD_PKT_LEN(x)          _DD_GETVALUE(x,S_PD_PKT_LEN,M_PD_PKT_LEN)


/* Crypotographic Operations */

/* MCR1 only (symmetric) */
#define K_IPSEC_3DES             0x0000                /* Not 5820 */
#define K_SSL_MAC                0x0001
#define K_TLS_HMAC               0x0002
#define K_SSL_3DES               0x0003
#define K_ARC4                   0x0004
#define K_HASH                   0x0005

/* MCR2 only (asymmetric) */
#define K_DH_PK_GEN              0x0001
#define K_DH_SK_GEN              0x0002
#define K_RSA_PK_OP              0x0003
#define K_RSA_SK_OP              0x0004
#define K_DSA_SIGN               0x0005
#define K_DSA_VERIF              0x0006
#define K_RNG_DIRECT             0x0041
#define K_RNG_SHA1               0x0042
#define K_MOD_ADD                0x0043
#define K_MOD_SUB                0x0044
#define K_MOD_MUL                0x0045
#define K_MOD_REDUCE             0x0046
#define K_MOD_EXP                0x0047
#define K_MOD_INV                0x0048               /* Not 5821 */
#define K_MOD_2EXP               0x0049               /* Not 5820 */


/* Command Context Header */

/* Word 0 */

#define S_CC_OPCODE              16
#define M_CC_OPCODE              _DD_MAKEMASK(16,S_CC_OPCODE)
#define V_CC_OPCODE(x)           _DD_MAKEVALUE(x,S_CC_OPCODE)
#define G_CC_OPCODE(x)           _DD_GETVALUE(x,S_CC_OPCODE,M_CC_OPCODE)

#define S_CC_LEN                 0
#define M_CC_LEN                 _DD_MAKEMASK(16,S_CC_LEN)
#define V_CC_LEN(x)              _DD_MAKEVALUE(x,S_CC_LEN)
#define G_CC_LEN(x)              _DD_GETVALUE(x,S_CC_LEN,M_CC_LEN)

/* Word 1 */

#define S_CC_FLAGS               12
#define M_CC_FLAGS               _DD_MAKEMASK(4,S_CC_FLAGS)
#define V_CC_FLAGS(x)            _DD_MAKEVALUE(x,S_CC_FLAGS)
#define G_CC_FLAGS(x)            _DD_GETVALUE(x,S_CC_OPCODE,M_CC_OPCODE)

/* The remaining command context fields depend on the opcode. */

/* IPSEC 3DES (K_IPSEC_3DES) */

/* SSL MAC (K_SSL_MAC) */
/* TLS HMAC (K_TLS_HMAC) */
/* Pure MD5/SHA-1 Hash (K_HASH) */

#define K_HASH_FLAGS_MD5         1
#define K_HASH_FLAGS_SHA1        2

/* SSL MAC (K_SSL_MAC) */

#define SSL_MAC_CMD_WORDS        22

/* TLS HMAC (K_TLS_HMAC) */

#define TLS_HMAC_CMD_WORDS       16

/* Pure MD5/SHA-1 Hash (K_HASH) */

/* SSL/TLS DES/3DES (K_SSL_3DES) */

/* ARCFOUR (K_ARC4) */

#define ARC4_STATE_WORDS         (1 + 256/4)
#define ARC4_CMD_WORDS           (2 + ARC4_STATE_WORDS)

#define M_ARC4_FLAGS_KEY         _DD_MAKEMASK1(10)
#define M_ARC4_FLAGS_WRITEBACK   _DD_MAKEMASK1(11)
#define M_ARC4_FLAGS_NULLDATA    _DD_MAKEMASK1(12)


/* Random number generation (K_RNG_DIRECT, K_RNG_SHA1) */

/* Modular arithmetic (K_MOD_ADD, K_MOD_SUB, K_MOD_MUL) */

/* Modular Remainder (K_MOD_REDUCE) */

/* Modular Exponentiation (K_MOD_EXP) */

/* Double Modular Exponentiation (K_MOD_2EXP) */


#endif /* _BCM_5821_H_ */
