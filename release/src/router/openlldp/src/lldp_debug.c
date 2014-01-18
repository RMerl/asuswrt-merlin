/** @file lldp_debug.c
 *
 * OpenLLDP Debug Core
 *
 * See LICENSE file for more info.
 * 
 * File: lldp_debug.c
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 * 
 * Some code below was borrowed from the Open1X project.
 *
 *******************************************************************/

#ifndef WIN32
#include <strings.h>
#include <unistd.h>
#include <syslog.h>
#include <stdint.h>
#else
#include "stdintwin.h"
#endif // WIN32


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


#include "lldp_debug.h"

/* Borrowed from Open1X */
unsigned char debug_level = 0;
int isdaemon = 0;
int syslogging = 0;
FILE *logfile = NULL;

/* Borrowed from Open1X */
static char to_hex_char(int val)
{
    return("0123456789abcdef"[val & 0xf]);
}

/* Borrowed from Open1X */
/**
 *
 * Dump hex values, without the ascii versions.
 *
 */
void debug_hex_printf(uint32_t level, uint8_t *hextodump, int size)
{
  int i;
  int len = 0;
  char *logstr = NULL;
  
	logstr = (char *)calloc(1, (size * 3) + 2);
	if (logstr == NULL)
	{
		printf("Couldn't allocate memory to store temporary logging string!\n");
		return;
	}

	memset(logstr, 0x00, ((size * 3)+2));

  if ((!(debug_level & level)) && (level != 0))
  {
	  free(logstr);
	  return;
  }
  
  if (hextodump == NULL)
  {
    free(logstr);
    return;
  }
  
  for (i = 0; i < size; i++)
    {
      logstr[len++] = to_hex_char(hextodump[i] >> 4);
      logstr[len++] = to_hex_char(hextodump[i]);
      logstr[len++] = ' ';
    }
  
  logstr[len++] = '\n';
  logstr[len] = 0;
  ufprintf(logfile, logstr, level);
  free(logstr);
}

/**
 *
 * Depending on the value of fh, we will either print to the screen, or
 * a log file.
 *
 */
void ufprintf(FILE *fh, char *instr, int level)
{
  // No decide where else to log to.
  if (((isdaemon == 2) || (fh == NULL)) && (syslogging != 1))
    {
      printf("%s", instr);
#ifndef WINDOWS
      fflush(stdout);
#endif
    } else if (syslogging ==1) {
      // XXX Consider ways of using other log levels.
#ifndef WINDOWS
      syslog(LOG_ALERT, "%s", instr);
#endif
    } else {
      fprintf(fh, "%s", instr);
      fflush(fh);
    }
}

/**
 *
 * dump some hex values -- also
 * show the ascii version of the dump.
 *
 */
void debug_hex_dump(unsigned char level, uint8_t *hextodump, int size)
{
    int i;
    char buf[80];
    int str_idx = 0;
    int chr_idx = 0;
    int count;
    int total;
    int tmp;

    if ((!(debug_level & level)) && (level != 0))
        return;

    if (hextodump == NULL)
        return;

    /* Initialize constant fields */
    memset(buf, ' ', sizeof(buf));
    buf[4]  = '|';
    buf[54] = '|';
    buf[72] = '\n';
    buf[73] = 0;

    count = 0;
    total = 0;
    for (i = 0; i < size; i++)
    {
        if (count == 0)
        {
            str_idx = 6;
            chr_idx = 56;

            buf[0] = to_hex_char(total >> 8);
            buf[1] = to_hex_char(total >> 4);
            buf[2] = to_hex_char(total);
        }

        /* store the number */
        tmp = hextodump[i];
        buf[str_idx++] = to_hex_char(tmp >> 4);
        buf[str_idx++] = to_hex_char(tmp);
        str_idx++;

        /* store the character */
        buf[chr_idx++] = isprint(tmp) ? tmp : '.';

        total++;
        count++;
        if (count >= 16)
        {
            count = 0;
            ufprintf(logfile, buf, level);
        }
    }

    /* Print partial line if any */
    if (count != 0)
    {
        /* Clear out any junk */
        while (count < 16)
        {
            buf[str_idx]   = ' ';   /* MSB hex */
            buf[str_idx+1] = ' ';   /* LSB hex */
            str_idx += 3;

            buf[chr_idx++] = ' ';

            count++;
        }
        ufprintf(logfile, buf, level);
    }
}


/**
 *
 * dump some hex values -- also
 * show the ascii version of the dump.
 *
 */
void debug_hex_strcat(uint8_t *dst, uint8_t *hextodump, int size)
{
  int i;
  int len = 0;
  char *logstr = NULL;
  
	logstr = (char *)calloc(1, (size * 3) + 2);
	if (logstr == NULL)
	{
		printf("Couldn't allocate memory to store temporary logging string!\n");
		return;
	}

	memset(logstr, 0x00, ((size * 3)+2));

  if (hextodump == NULL)
  {
    free(logstr);
    return;
  }
  
  for (i = 0; i < size; i++)
    {
      logstr[len++] = to_hex_char(hextodump[i] >> 4);
      logstr[len++] = to_hex_char(hextodump[i]);
      logstr[len++] = ' ';
    }
  
  //  logstr[len++] = '\n';
  logstr[len] = 0;
  strcat((char *)dst, logstr);
  free(logstr);
}

/**
 *
 * Display some information.  But only if we are at a debug level that
 * should display it.
 *
 */
void debug_printf(unsigned char level, char *fmt, ...)
{
    char dumpstr[2048], temp[2048];

    if (((level & debug_level) || (level == 0)) && (fmt != NULL))
    {
        va_list ap;
        va_start(ap, fmt);

        memset((char *)&dumpstr, 0x00, 2048);
        memset((char *)&temp, 0x00, 2048);

        // Print out a tag that identifies the type of debug message being used.
        switch (level)
        {
            case DEBUG_NORMAL:
                break;

            case DEBUG_CONFIG:
                strcpy((char *)&dumpstr, "[CONFIG] ");
                break;

            case DEBUG_STATE:
                strcpy((char *)&dumpstr, "[STATE] ");
                break;

            case DEBUG_TLV:
                strcpy((char *)&dumpstr, "[TLV] ");
                break;

	    case DEBUG_MSAP:
	        strcpy((char *)&dumpstr, "[MSAP] ");
		break;

            case DEBUG_INT:
                strcpy((char *)&dumpstr, "[INT] ");
                break;

            case DEBUG_EVERYTHING:
                strcpy((char *)&dumpstr, "[ALL] ");
                break;

            case DEBUG_EXCESSIVE:
                strcpy((char *)&dumpstr, "[EXCESSIVE] ");
                break;
        }

        vsnprintf((char *)&temp, 2048, fmt, ap);

        strcat((char *)&dumpstr, (char *)&temp);

        ufprintf(logfile, dumpstr, level);

        va_end(ap);
    }
}


/**
 *
 * Set flags based on the numeric value that was passed in.
 *
 */
void debug_set_flags(int new_flags)
{
    if (new_flags >= 1) debug_level |= DEBUG_CONFIG;
    if (new_flags >= 2) debug_level |= DEBUG_STATE;
    if (new_flags >= 3) debug_level |= DEBUG_TLV;
    if (new_flags >= 4) debug_level |= DEBUG_MSAP;
    if (new_flags >= 5) debug_level |= DEBUG_INT;
    if (new_flags >= 6) debug_level |= DEBUG_SNMP;
    if (new_flags >= 7) debug_level |= DEBUG_EVERYTHING;
    if (new_flags >= 8) debug_level |= DEBUG_EXCESSIVE;
}

/**
 *
 * Set flags based on an ASCII string that was passed in.
 *
 */
void debug_alpha_set_flags(char *new_flags)
{
    int i;

    debug_level = 0;

    for (i=0;i<strlen(new_flags);i++)
    {
        switch (new_flags[i])
        {
            case 'c':
                debug_level |= DEBUG_CONFIG;
                break;

            case 's':
                debug_level |= DEBUG_STATE;
                break;

            case 't':
                debug_level |= DEBUG_TLV;
                break;

            case 'm':
                debug_level |= DEBUG_MSAP;
                break;

            case 'i':
                debug_level |= DEBUG_INT;
                break;

            case 'n':
                debug_level |= DEBUG_SNMP;
                break;

            case 'e':
                debug_level |= DEBUG_EVERYTHING;
                break;

            case 'x':
                debug_level |= DEBUG_EXCESSIVE;
                break;

            case 'A':
                debug_level |= 0xff;   // Set all flags.
                break;
        }
    }
}
