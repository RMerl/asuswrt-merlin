/** @file lldp_debug.h
 *
 * OpenLLDP Debug Header
 *
 * See LICENSE file for more info.
 * 
 * File: lldp_debug.h
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *
 *******************************************************************/

#ifndef LLDP_DEBUG_H
#define LLDP_DEBUG_H

#include <stdio.h>

/* Borrowed from Open1X */
#define DEBUG_NORMAL     0

#define NUM_DEBUG_CONFIG     1
#define DEBUG_CONFIG         0x01

#define NUM_DEBUG_STATE      2
#define DEBUG_STATE          0x02

#define NUM_DEBUG_TLV        3
#define DEBUG_TLV            0x04

#define NUM_DEBUG_MSAP       4
#define DEBUG_MSAP           0x08

#define NUM_DEBUG_INT        5
#define DEBUG_INT            0x10

#define NUM_DEBUG_SNMP       6
#define DEBUG_SNMP           0x20

#define NUM_DEBUG_EVERYTHING 7
#define DEBUG_EVERYTHING     0x40

#define NUM_DEBUG_EXCESSIVE  8
#define DEBUG_EXCESSIVE      0x80

/* Borrowed from Open1X */
int logfile_setup(char *);
void logfile_cleanup();
void lowercase(char *);
void debug_setdaemon(int);
void debug_printf(unsigned char, char *, ...);
void debug_printf_nl(unsigned char, char *, ...);
void debug_hex_printf(uint32_t, uint8_t *, int);
void debug_hex_dump(unsigned char, uint8_t *, int);
void debug_hex_strcat(uint8_t *dest, uint8_t *hextodump, int size);
void debug_set_flags(int);
void debug_alpha_set_flags(char *);
int xsup_assert_long(int, char *, int, char *, int, const char *);
int debug_getlevel();
void ufprintf(FILE *fh, char *instr, int level);

#ifndef LLDP_FUNCTION_DEBUG
#define LLDP_FUNCTION_DEBUG debug_printf(DEBUG_EXCESSIVE, "[DEBUG] %s:%d:%s()\n", __FILE__, __LINE__, __FUNCTION__);
#endif

#endif /* LLDP_DEBUG_H */
