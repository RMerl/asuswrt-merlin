#ifndef RT_NAMES_H_
#define RT_NAMES_H_ 1

#include <asm/types.h>

char* rtnl_rtprot_n2a(int id, char *buf, int len);
char* rtnl_rtscope_n2a(int id, char *buf, int len);
char* rtnl_rttable_n2a(int id, char *buf, int len);
char* rtnl_rtrealm_n2a(int id, char *buf, int len);
char* rtnl_dsfield_n2a(int id, char *buf, int len);
int rtnl_rtprot_a2n(__u32 *id, char *arg);
int rtnl_rtscope_a2n(__u32 *id, char *arg);
int rtnl_rttable_a2n(__u32 *id, char *arg);
int rtnl_rtrealm_a2n(__u32 *id, char *arg);
int rtnl_dsfield_a2n(__u32 *id, char *arg);

const char *inet_proto_n2a(int proto, char *buf, int len);
int inet_proto_a2n(char *buf);


const char * ll_type_n2a(int type, char *buf, int len);

const char *ll_addr_n2a(unsigned char *addr, int alen, int type, char *buf, int blen);
int ll_addr_a2n(char *lladdr, int len, char *arg);

const char * ll_proto_n2a(unsigned short id, char *buf, int len);
int ll_proto_a2n(unsigned short *id, char *buf);


#endif
