#ifndef __WB_UTIL_H__
#define __WB_UTIL_H__


#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int get_web_path_len(const char* trans_type, const char* server, const char* append)
{
	return (strlen(trans_type)+strlen(server)+strlen(append)+1);	
}


char* get_webpath(const char* trans_type, const char* server, const char* append)
{
	//	get web path length
	int		wp_len = get_web_path_len(trans_type, server, append);
	//	setup webpath
	char*	webpath = (char*)malloc(wp_len); memset(webpath, 0, wp_len);
	sprintf(webpath, "%s%s%s",trans_type, server, append);
	return webpath;
}

void free_webpath(char* webpath)
{
	free(webpath);
}

void free_append_data(char* append_data)
{
	free(append_data);
}


char* make_str(const char *fmt,	...)
{
		/* Guess	we need	no more	than 100 bytes.	*/
	int n, size = 100;
	char	*p,	*np;
	va_list ap;

	if ((p =	(char*)malloc(size)) == NULL)
		return NULL;

	while (1) {
		/* Try to print in the allocated	space. */
		va_start(ap,	fmt);
		n = vsnprintf(p,	size, fmt, ap);
		va_end(ap);
		/* If that worked, return the string. */
		if (n > -1 && n < size)
			return p;
		/* Else try again with more space. */
		if (n > -1)	  /* glibc 2.1 */
			size	= n+1; /* precisely	what is	needed */
		else			  /* glibc 2.0 */
			size	*= 2;  /* twice	the	old	size */
		if ((np = (char*)realloc (p, size))	== NULL) {
			free(p);
			return NULL;
		}else {
			p = np;
		}
	}
}



#endif
