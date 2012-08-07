/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *  
    *  string prototypes		    	File: lib_string.h
    *  
    *  Function prototypes for the string routines
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


char *lib_strcpy(char *dest,const char *src);
char *lib_strncpy(char *dest,const char *src,size_t cnt);
size_t lib_xstrncpy(char *dest,const char *src,size_t cnt);
size_t lib_strlen(const char *str);

int lib_strncmp(const char *dest, const char *src, size_t cnt);
int lib_strcmp(const char *dest,const char *src);
int lib_strcmpi(const char *dest,const char *src);
char *lib_strchr(const char *dest,int c);
char *lib_strrchr(const char *dest,int c);
int lib_memcmp(const void *dest,const void *src,size_t cnt);
void *lib_memcpy(void *dest,const void *src,size_t cnt);
void *lib_memset(void *dest,int c,size_t cnt);
char *lib_strdup(char *str);
void lib_trimleading(char *str);
void lib_chop_filename(char *str,char **host,char **file);
void lib_strupr(char *s);
char lib_toupper(char c);
char *lib_strcat(char *dest,const char *src);
char *lib_gettoken(char **str);
char *lib_strnchr(const char *dest,int c,size_t cnt);
int lib_parseipaddr(const char *ipaddr,uint8_t *dest);
int lib_atoi(const char *dest);
int lib_lookup(const cons_t *list,char *str);
int lib_setoptions(const cons_t *list,char *str,unsigned int *flags);
int lib_xtoi(const char *dest);
uint64_t lib_xtoq(const char *dest);



#ifndef _LIB_NO_MACROS_
#define strcpy(d,s) lib_strcpy(d,s)
#define strncpy(d,s,c) lib_strncpy(d,s,c)
#define xstrncpy(d,s,c) lib_xstrncpy(d,s,c)
#define strlen(s) lib_strlen(s)
#define strchr(s,c) lib_strchr(s,c)
#define strrchr(s,c) lib_strrchr(s,c)
#define strdup(s) lib_strdup(s)
#define strcmp(d,s) lib_strcmp(d,s)
#define strncmp(d,s,c) lib_strncmp(d,s,c)
#define strcmpi(d,s) lib_strcmpi(d,s)
#define memcmp(d,s,c) lib_memcmp(d,s,c)
#define memset(d,s,c) lib_memset(d,s,c)
#define memcpy(d,s,c) lib_memcpy(d,s,c)
#define bcopy(s,d,c) lib_memcpy(d,s,c)
#define bzero(d,c) lib_memset(d,0,c)
#define strupr(s) lib_strupr(s)
#define toupper(c) lib_toupper(c)
#define strcat(d,s) lib_strcat(d,s)
#define gettoken(s) lib_gettoken(s)
#define strnchr(d,ch,cnt) lib_strnchr(d,ch,cnt)
#define atoi(d) lib_atoi(d)
#define xtoi(d) lib_xtoi(d)
#define xtoq(d) lib_xtoq(d)
#define parseipaddr(i,d) lib_parseipaddr(i,d)
#define lookup(x,y) lib_lookup(x,y)
#define setoptions(x,y,z) lib_setoptions(x,y,z)
#endif

void
qsort(void *bot, size_t nmemb, size_t size, int (*compar)(const void *,const void *));
