/* ans.c - Interface for text2atm and atm2text to ANS */

/* Written 1996-2000 by Werner Almesberger, EPFL-LRC/ICA */


/*
 * This stuff is a temporary hack to avoid using gethostbyname_nsap and such
 * without doing the "full upgrade" to getaddrinfo/getnameinfo. This also
 * serves as an exercise for me to get all the details right before I propose
 * a patch that would eventually end up in libc (and that should therefore be
 * as stable as possible).
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>

#include "atm.h"
#include "atmres.h"


#define MAX_ANSWER 2048
#define MAX_NAME   1024

#define MAX_LINE		2048	/* in /etc/e164_cc */
#define E164_CC_DEFAULT_LEN	   2
#define E164_CC_FILE		"/etc/e164_cc"

#define GET16(pos) (((pos)[0] << 8) | (pos)[1])


static int ans(const char *text,int wanted,void *result,int res_len)
{
    unsigned char answer[MAX_ANSWER];
    unsigned char name[MAX_NAME];
    unsigned char *pos,*data,*found;
    int answer_len,name_len,data_len,found_len;
    int questions,answers;

    found_len = 0; /* gcc wants it */
    if ((answer_len = res_search(text,C_IN,wanted,answer,MAX_ANSWER)) < 0)
	return TRY_OTHER;
    /*
     * Response header: id, flags, #queries, #answers, #authority,
     * #additional (all 16 bits)
     */
    pos = answer+12;
    if (answer[3] & 15) return TRY_OTHER; /* rcode != 0 */
    questions = GET16(answer+4);
    if (questions != 1) return TRY_OTHER; /* trouble ... */
    answers = GET16(answer+6);
    if (answers < 1) return TRY_OTHER;
    /*
     * Query: name, type (16), class (16)
     */
    if ((name_len = dn_expand(answer,answer+answer_len,pos,name,MAX_NAME)) < 0)
	return TRY_OTHER;
    pos += name_len;
    if (GET16(pos) != wanted || GET16(pos+2) != C_IN) return TRY_OTHER;
    pos += 4;
    /*
     * Iterate over answers until we find something we like, giving priority
     * to ATMA_AESA (until signaling is fixed to work with E.164 too)
     */
    found = NULL;
    while (answers--) {
	/*
	 * RR: name, type (16), class (16), TTL (32), resource_len (16),
	 * resource_data ...
	 */
	if ((name_len = dn_expand(answer,answer+answer_len,pos,name,MAX_NAME))
	  < 0) return TRY_OTHER;
	pos += name_len;
	data_len = GET16(pos+8);
	data = pos+10;
	pos = data+data_len;
	if (GET16(data-10) != wanted || GET16(data-8) != C_IN || !--data_len)
	    continue;
	switch (wanted) {
            case T_NSAP:
                data_len++;
                if (data_len != ATM_ESA_LEN) continue;
                memcpy(((struct sockaddr_atmsvc *) result)->
                  sas_addr.prv,data,ATM_ESA_LEN);
                return 0;
	    case T_ATMA:
		switch (*data++) {
		    case ATMA_AESA:
			if (data_len != ATM_ESA_LEN) continue;
			memcpy(((struct sockaddr_atmsvc *) result)->
			  sas_addr.prv,data,ATM_ESA_LEN);
			return 0;
		    case ATMA_E164:
			if (data_len > ATM_E164_LEN) continue;
			if (!found) {
			    found = data;
			    found_len = data_len;
			}
			break;
		    default:
			continue;
		}
	    case T_PTR:
		    if (dn_expand(answer,answer+answer_len,data,result,
		      res_len) < 0) return FATAL;
		    return 0;
		default:
		    continue;
	}
    }
    if (!found) return TRY_OTHER;
    memcpy(((struct sockaddr_atmsvc *) result)->sas_addr.pub,found,
      found_len);
    ((struct sockaddr_atmsvc *) result)->sas_addr.pub[found_len] = 0;
    return 0;
}


int ans_byname(const char *text,struct sockaddr_atmsvc *addr,int length,
  int flags)
{
    if (!(flags & T2A_SVC) || length != sizeof(*addr)) return TRY_OTHER; 
    memset(addr,0,sizeof(*addr));
    addr->sas_family = AF_ATMSVC;
    if (!ans(text,T_ATMA,addr,length)) return 0;
    return ans(text,T_NSAP,addr,length);
}


static int encode_nsap(char *buf,const unsigned char *addr)
{
    static int fmt_dcc[] = { 2,12,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      4,2,0 };
    static int fmt_e164[] = { 2,12,1,1,1,1,1,1,1,1,16,2,0 };
    int *fmt;
    int pos,i,j;

    switch (*addr) {
	case ATM_AFI_DCC:
	case ATM_AFI_ICD:
	case ATM_AFI_LOCAL:
	case ATM_AFI_DCC_GROUP:
	case ATM_AFI_ICD_GROUP:
	case ATM_AFI_LOCAL_GROUP:
	    fmt = fmt_dcc;
	    break;
	case ATM_AFI_E164:
	case ATM_AFI_E164_GROUP:
	    fmt = fmt_e164;
	    break;
	default:
	    return TRY_OTHER;
    }
    pos = 2*ATM_ESA_LEN;
    for (i = 0; fmt[i]; i++) {
	pos -= fmt[i];
	for (j = 0; j < fmt[i]; j++)
	    sprintf(buf++,"%x",
	      (addr[(pos+j) >> 1] >> 4*(1-((pos+j) & 1))) & 0xf);
	*buf++ = '.';
    }
    strcpy(buf,"AESA.ATMA.INT.");
    return 0;
}


static int encode_nsap_new(char *buf,const unsigned char *addr)
{
    int i;
    int digit;

    for (i = 20; i; ) {
        i--;
        digit = addr[i] & 0x0F;
        *(buf++) = digit + (digit >= 10 ? '7' : '0');
        *(buf++) = '.';
        digit = ((unsigned char) (addr[i])) >> 4;
        *(buf++) = digit + (digit >= 10 ? '7' : '0');
        *(buf++) = '.';
    }
    strcpy (buf, "NSAP.INT.");
    return 0;
}


static int cc_len(int p0,int p1)
{
    static char *cc_table = NULL;
    FILE *file;
    char buffer[MAX_LINE];
    char *here;
    int cc;

    if (!cc_table) {
	if (!(cc_table = malloc(100))) {
	    perror("malloc");
	    return E164_CC_DEFAULT_LEN;
	}
	memset(cc_table,E164_CC_DEFAULT_LEN,100);
	if (!(file = fopen(E164_CC_FILE,"r")))
	    perror(E164_CC_FILE);
	else {
	    while (fgets(buffer,MAX_LINE,file)) {
		here = strchr(buffer,'#');
		if (here) *here = 0;
		if (sscanf(buffer,"%d",&cc) == 1) {
		    if (cc < 10) cc_table[cc] = 1;
		    else if (cc < 100) cc_table[cc] = 2;
			else cc_table[cc/10] = 3;
		}
	    }
	    fclose(file);
	}
    }
    if (cc_table[p0] == 1) return 1;
    return cc_table[p0*10+p1];
}


static int encode_e164(char *buf,const char *addr)
{
    const char *prefix,*here;

    prefix = addr+cc_len(addr[0]-48,addr[1]-48);
    here = strchr(addr,0);
    while (here > prefix) {
	*buf++ = *--here;
	*buf++ = '.';
    }
    while (here > addr) *buf++ = *addr++;
    strcpy(buf,".E164.ATMA.INT.");
    return 0;
}


int ans_byaddr(char *buffer,int length,const struct sockaddr_atmsvc *addr,
  int flags)
{
    char tmp[MAX_NAME]; /* could be smaller ... */
    int res;

    if (addr->sas_addr.prv) {
        res = encode_nsap(tmp,addr->sas_addr.prv);
        if (!res && !ans(tmp,T_PTR,buffer,length)) return 0;
	res = encode_nsap_new(tmp,addr->sas_addr.prv);
        if (res < 0) return res;
	return ans(tmp,T_PTR,buffer,length);
    } else {
        res = encode_e164(tmp,addr->sas_addr.pub);
        if (res < 0) return res;
        return ans(tmp,T_PTR,buffer,length);
    }
}
