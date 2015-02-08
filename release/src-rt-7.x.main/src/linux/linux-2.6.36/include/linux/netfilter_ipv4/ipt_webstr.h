#ifndef _IPT_WEBSTR_H
#define _IPT_WEBSTR_H

#define BM_MAX_NLEN 256
#define BM_MAX_HLEN 1024

#define BLK_JAVA		0x01
#define BLK_ACTIVE		0x02
#define BLK_COOKIE		0x04
#define BLK_PROXY		0x08

typedef char *(*proc_ipt_search) (char *, char *, int, int);

struct ipt_webstr_info {
    char string[BM_MAX_NLEN];
    u_int16_t invert;
    u_int16_t len;
    u_int8_t type;
};

enum ipt_webstr_type
{
    IPT_WEBSTR_HOST,
    IPT_WEBSTR_URL,
    IPT_WEBSTR_CONTENT
};

#endif /* _IPT_WEBSTR_H */
