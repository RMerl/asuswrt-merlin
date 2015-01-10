#ifndef VSF_UTILITY_H
#define VSF_UTILITY_H

#include <stdio.h>

#define ftp_dbg(fmt, args...) do{ \
		FILE *fp = fopen("/dev/console", "a+"); \
		if(fp){ \
			fprintf(fp, "[ftp: %s] ", __FUNCTION__); \
			fprintf(fp, fmt, ## args); \
			fclose(fp); \
		} \
	}while(0)

struct mystr;

/* die()
 * PURPOSE
 * Terminate execution of the process, due to an abnormal (but non-bug)
 * situation.
 * PARAMETERS
 * p_text       - text string describing why the process is exiting
 */
void die(const char* p_text);

/* die2()
 * PURPOSE
 * Terminate execution of the process, due to an abnormal (but non-bug)
 * situation.
 * PARAMETERS
 * p_text1      - text string describing why the process is exiting
 * p_text2      - text to safely concatenate to p_text1
 */
void die2(const char* p_text1, const char* p_text2);

/* bug()
 * PURPOSE
 * Terminate execution of the process, due to a suspected bug, trying to emit
 * the reason this happened down the network in FTP response format.
 * PARAMETERS
 * p_text       - text string describing what bug trap has triggered
 *       */
void bug(const char* p_text);

/* vsf_exit()
 * PURPOSE
 * Terminate execution of the process, writing out the specified text string
 * in the process.
 * PARAMETERS
 * p_text       - text string describing why the process is exiting
 */
void vsf_exit(const char* p_text);

char *local2remote(const char *buf);
char *remote2local(const char *buf);
#endif

