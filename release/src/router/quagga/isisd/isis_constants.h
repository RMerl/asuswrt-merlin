/*
 * IS-IS Rout(e)ing protocol - isis_constants.h   
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef ISIS_CONSTANTS_H
#define ISIS_CONSTANTS_H

/*
 * Architectural constant values from p. 35 of ISO/IEC 10589
 */

#define MAX_NARROW_LINK_METRIC        63
#define MAX_NARROW_PATH_METRIC        1023
#define MAX_WIDE_LINK_METRIC          0x00FFFFFF  /* RFC4444 */
#define MAX_WIDE_PATH_METRIC          0xFE000000  /* RFC3787 */
#define ISO_SAP                       0xFE
#define INTRADOMAIN_ROUTEING_SELECTOR 0
#define SEQUENCE_MODULUS              4294967296
#define RECEIVE_LSP_BUFFER_SIZE       1492

/*
 * implementation specific jitter values
 */

#define IIH_JITTER                    10	/* % */
#define MAX_AGE_JITTER                 5	/* % */
#define MAX_LSP_GEN_JITTER             5	/* % */
#define CSNP_JITTER                   10	/* % */
#define PSNP_JITTER                   10	/* % */

#define RANDOM_SPREAD           100000.0

#define ISIS_LEVELS                   2
#define ISIS_LEVEL1                   1
#define ISIS_LEVEL2                   2

/*
 * Default values
 * ISO - 10589 Section 7.3.21 - Parameters
 * RFC 4444
 */
#define MAX_AGE                       1200
#define ZERO_AGE_LIFETIME             60
#define MIN_LSP_LIFETIME              350
#define MAX_LSP_LIFETIME              65535
#define DEFAULT_LSP_LIFETIME          1200

#define MIN_MAX_LSP_GEN_INTERVAL      1
#define MAX_MAX_LSP_GEN_INTERVAL      65235
#define DEFAULT_MAX_LSP_GEN_INTERVAL  900

#define MIN_MIN_LSP_GEN_INTERVAL      1
#define MAX_MIN_LSP_GEN_INTERVAL      120  /* RFC 4444 says 65535 */
#define DEFAULT_MIN_LSP_GEN_INTERVAL  30

#define MIN_LSP_TRANS_INTERVAL        5

#define MIN_CSNP_INTERVAL             1
#define MAX_CSNP_INTERVAL             600
#define DEFAULT_CSNP_INTERVAL         10

#define MIN_PSNP_INTERVAL             1
#define MAX_PSNP_INTERVAL             120
#define DEFAULT_PSNP_INTERVAL         2

#define MIN_HELLO_INTERVAL            1
#define MAX_HELLO_INTERVAL            600
#define DEFAULT_HELLO_INTERVAL        3

#define MIN_HELLO_MULTIPLIER          2
#define MAX_HELLO_MULTIPLIER          100
#define DEFAULT_HELLO_MULTIPLIER      10

#define MIN_PRIORITY                  0
#define MAX_PRIORITY                  127
#define DEFAULT_PRIORITY              64

/* min and max metric varies by new vs old metric types */
#define DEFAULT_CIRCUIT_METRIC        10

#define METRICS_UNSUPPORTED           0x80

#define MINIMUM_SPF_INTERVAL          1

#define ISIS_MAX_PATH_SPLITS          64

/*
 * NLPID values
 */
#define NLPID_IP   204
#define NLPID_IPV6 142
#define NLPID_SNAP 128
#define NLPID_CLNP 129
#define NLPID_ESIS 130

/*
 * Return values for functions
 */
#define ISIS_OK       0
#define ISIS_WARNING  1
#define ISIS_ERROR    2
#define ISIS_CRITICAL 3

/*
 * IS-IS Circuit Types
 */

#define IS_LEVEL_1       1
#define IS_LEVEL_2       2
#define IS_LEVEL_1_AND_2 3

#define SNPA_ADDRSTRLEN 18
#define ISIS_SYS_ID_LEN  6
#define ISIS_NSEL_LEN    1
#define SYSID_STRLEN    24

/*
 * LSP bit masks
 */
#define LSPBIT_P   0x80
#define LSPBIT_ATT 0x78
#define LSPBIT_OL  0x04
#define LSPBIT_IST 0x03

/*
 * LSP bit masking macros
 * taken from tcpdumps
 * print-isoclns.c
 */

#define ISIS_MASK_LSP_OL_BIT(x)            ((x)&0x4)
#define ISIS_MASK_LSP_IS_L1_BIT(x)         ((x)&0x1)
#define ISIS_MASK_LSP_IS_L2_BIT(x)         ((x)&0x2)
#define ISIS_MASK_LSP_PARTITION_BIT(x)     ((x)&0x80)
#define ISIS_MASK_LSP_ATT_BITS(x)          ((x)&0x78)
#define ISIS_MASK_LSP_ATT_ERROR_BIT(x)     ((x)&0x40)
#define ISIS_MASK_LSP_ATT_EXPENSE_BIT(x)   ((x)&0x20)
#define ISIS_MASK_LSP_ATT_DELAY_BIT(x)     ((x)&0x10)
#define ISIS_MASK_LSP_ATT_DEFAULT_BIT(x)   ((x)&0x8)

#define LLC_LEN 3

/* we need to be aware of the fact we are using ISO sized
 * packets, using isomtu = mtu - LLC_LEN
 */
#define ISO_MTU(C) \
          ((if_is_broadcast ((C)->interface)) ? \
           (C->interface->mtu - LLC_LEN) : (C->interface->mtu))

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#endif /* ISIS_CONSTANTS_H */
