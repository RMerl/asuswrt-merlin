/*
 * $Id: radiusclient.h,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 1995,1996,1997,1998 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#ifndef RADIUSCLIENT_H
#define RADIUSCLIENT_H

#include	<sys/types.h>
#include	<stdio.h>
#include	<time.h>
#include "pppd.h"

#ifndef _UINT4_T
/* This works for all machines that Linux runs on... */
typedef unsigned int UINT4;
typedef int          INT4;
#endif

#define AUTH_VECTOR_LEN		16
#define AUTH_PASS_LEN		(3 * 16) /* multiple of 16 */
#define AUTH_ID_LEN		64
#define AUTH_STRING_LEN		128	 /* maximum of 253 */

#define	BUFFER_LEN		8192

#define NAME_LENGTH		32
#define	GETSTR_LENGTH		128	/* must be bigger than AUTH_PASS_LEN */

/* codes for radius_buildreq, radius_getport, etc. */
#define AUTH			0
#define ACCT			1

/* defines for config.c */

#define SERVER_MAX 8

#define AUTH_LOCAL_FST	(1<<0)
#define AUTH_RADIUS_FST (1<<1)
#define AUTH_LOCAL_SND  (1<<2)
#define AUTH_RADIUS_SND (1<<3)

typedef struct server {
	int max;
	char *name[SERVER_MAX];
	unsigned short port[SERVER_MAX];
} SERVER;

typedef struct pw_auth_hdr
{
	u_char          code;
	u_char          id;
	u_short         length;
	u_char          vector[AUTH_VECTOR_LEN];
	u_char          data[2];
} AUTH_HDR;

#define AUTH_HDR_LEN			20
#define MAX_SECRET_LENGTH		(3 * 16) /* MUST be multiple of 16 */
#define CHAP_VALUE_LENGTH		16

#define PW_AUTH_UDP_PORT		1812
#define PW_ACCT_UDP_PORT		1813

#define PW_TYPE_STRING			0
#define PW_TYPE_INTEGER			1
#define PW_TYPE_IPADDR			2
#define PW_TYPE_DATE			3

/* standard RADIUS codes */

#define	PW_ACCESS_REQUEST		1
#define	PW_ACCESS_ACCEPT		2
#define	PW_ACCESS_REJECT		3
#define	PW_ACCOUNTING_REQUEST		4
#define	PW_ACCOUNTING_RESPONSE		5
#define	PW_ACCOUNTING_STATUS		6
#define	PW_PASSWORD_REQUEST		7
#define	PW_PASSWORD_ACK			8
#define	PW_PASSWORD_REJECT		9
#define	PW_ACCOUNTING_MESSAGE		10
#define	PW_ACCESS_CHALLENGE		11
#define	PW_STATUS_SERVER		12
#define	PW_STATUS_CLIENT		13


/* standard RADIUS attribute-value pairs */

#define	PW_USER_NAME			1	/* string */
#define	PW_USER_PASSWORD		2	/* string */
#define	PW_CHAP_PASSWORD		3	/* string */
#define	PW_NAS_IP_ADDRESS		4	/* ipaddr */
#define	PW_NAS_PORT			5	/* integer */
#define	PW_SERVICE_TYPE			6	/* integer */
#define	PW_FRAMED_PROTOCOL		7	/* integer */
#define	PW_FRAMED_IP_ADDRESS		8	/* ipaddr */
#define	PW_FRAMED_IP_NETMASK		9	/* ipaddr */
#define	PW_FRAMED_ROUTING		10	/* integer */
#define	PW_FILTER_ID		        11	/* string */
#define	PW_FRAMED_MTU			12	/* integer */
#define	PW_FRAMED_COMPRESSION		13	/* integer */
#define	PW_LOGIN_IP_HOST		14	/* ipaddr */
#define	PW_LOGIN_SERVICE		15	/* integer */
#define	PW_LOGIN_PORT			16	/* integer */
#define	PW_OLD_PASSWORD			17	/* string */ /* deprecated */
#define	PW_REPLY_MESSAGE		18	/* string */
#define	PW_LOGIN_CALLBACK_NUMBER	19	/* string */
#define	PW_FRAMED_CALLBACK_ID		20	/* string */
#define	PW_EXPIRATION			21	/* date */ /* deprecated */
#define	PW_FRAMED_ROUTE			22	/* string */
#define	PW_FRAMED_IPX_NETWORK		23	/* integer */
#define	PW_STATE			24	/* string */
#define	PW_CLASS			25	/* string */
#define	PW_VENDOR_SPECIFIC		26	/* string */
#define	PW_SESSION_TIMEOUT		27	/* integer */
#define	PW_IDLE_TIMEOUT			28	/* integer */
#define	PW_TERMINATION_ACTION		29	/* integer */
#define	PW_CALLED_STATION_ID            30      /* string */
#define	PW_CALLING_STATION_ID           31      /* string */
#define	PW_NAS_IDENTIFIER		32	/* string */
#define	PW_PROXY_STATE			33	/* string */
#define	PW_LOGIN_LAT_SERVICE		34	/* string */
#define	PW_LOGIN_LAT_NODE		35	/* string */
#define	PW_LOGIN_LAT_GROUP		36	/* string */
#define	PW_FRAMED_APPLETALK_LINK	37	/* integer */
#define	PW_FRAMED_APPLETALK_NETWORK	38	/* integer */
#define	PW_FRAMED_APPLETALK_ZONE	39	/* string */
#define	PW_CHAP_CHALLENGE               60      /* string */
#define	PW_NAS_PORT_TYPE                61      /* integer */
#define	PW_PORT_LIMIT                   62      /* integer */
#define PW_LOGIN_LAT_PORT               63      /* string */

/* Vendor RADIUS attribute-value pairs */
#define PW_MS_CHAP_CHALLENGE		11	/* string */
#define PW_MS_CHAP_RESPONSE		1	/* string */
#define PW_MS_CHAP2_RESPONSE		25	/* string */
#define PW_MS_CHAP2_SUCCESS		26	/* string */
#define PW_MS_MPPE_ENCRYPTION_POLICY	7	/* string */
#define PW_MS_MPPE_ENCRYPTION_TYPE	8	/* string */
#define PW_MS_MPPE_ENCRYPTION_TYPES PW_MS_MPPE_ENCRYPTION_TYPE
#define PW_MS_CHAP_MPPE_KEYS		12	/* string */
#define PW_MS_MPPE_SEND_KEY		16	/* string */
#define PW_MS_MPPE_RECV_KEY		17	/* string */

/*	Accounting */

#define	PW_ACCT_STATUS_TYPE		40	/* integer */
#define	PW_ACCT_DELAY_TIME		41	/* integer */
#define	PW_ACCT_INPUT_OCTETS		42	/* integer */
#define	PW_ACCT_OUTPUT_OCTETS		43	/* integer */
#define	PW_ACCT_SESSION_ID		44	/* string */
#define	PW_ACCT_AUTHENTIC		45	/* integer */
#define	PW_ACCT_SESSION_TIME		46	/* integer */
#define	PW_ACCT_INPUT_PACKETS		47	/* integer */
#define	PW_ACCT_OUTPUT_PACKETS		48	/* integer */
#define PW_ACCT_TERMINATE_CAUSE		49	/* integer */
#define PW_ACCT_MULTI_SESSION_ID	50	/* string */
#define PW_ACCT_LINK_COUNT		51	/* integer */

/* From RFC 2869 */
#define PW_ACCT_INTERIM_INTERVAL        85	/* integer */

/*	Merit Experimental Extensions */

#define PW_USER_ID                      222     /* string */
#define PW_USER_REALM                   223     /* string */


/*      Session limits */
#define PW_SESSION_OCTETS_LIMIT		227    /* integer */
#define PW_OCTETS_DIRECTION		228    /* integer */

/*	Integer Translations */

/*	SERVICE TYPES	*/

#define	PW_LOGIN			1
#define	PW_FRAMED			2
#define	PW_CALLBACK_LOGIN		3
#define	PW_CALLBACK_FRAMED		4
#define	PW_OUTBOUND			5
#define	PW_ADMINISTRATIVE		6
#define PW_NAS_PROMPT                   7
#define PW_AUTHENTICATE_ONLY		8
#define PW_CALLBACK_NAS_PROMPT          9

/*	FRAMED PROTOCOLS	*/

#define	PW_PPP				1
#define	PW_SLIP				2
#define PW_ARA                          3
#define PW_GANDALF                      4
#define PW_XYLOGICS                     5

/*	FRAMED ROUTING VALUES	*/

#define	PW_NONE				0
#define	PW_BROADCAST			1
#define	PW_LISTEN			2
#define	PW_BROADCAST_LISTEN		3

/*	FRAMED COMPRESSION TYPES	*/

#define	PW_VAN_JACOBSON_TCP_IP		1
#define	PW_IPX_HEADER_COMPRESSION	2

/*	LOGIN SERVICES	*/

#define PW_TELNET                       0
#define PW_RLOGIN                       1
#define PW_TCP_CLEAR                    2
#define PW_PORTMASTER                   3
#define PW_LAT                          4
#define PW_X25_PAD                      5
#define PW_X25_T3POS                    6

/*	TERMINATION ACTIONS	*/

#define	PW_DEFAULT			0
#define	PW_RADIUS_REQUEST		1

/*	PROHIBIT PROTOCOL  */

#define PW_DUMB		0	/* 1 and 2 are defined in FRAMED PROTOCOLS */
#define PW_AUTH_ONLY	3
#define PW_ALL		255

/*	ACCOUNTING STATUS TYPES    */

#define PW_STATUS_START		1
#define PW_STATUS_STOP		2
#define PW_STATUS_ALIVE		3
#define PW_STATUS_MODEM_START	4
#define PW_STATUS_MODEM_STOP	5
#define PW_STATUS_CANCEL	6
#define PW_ACCOUNTING_ON	7
#define PW_ACCOUNTING_OFF	8

/*      ACCOUNTING TERMINATION CAUSES   */

#define PW_USER_REQUEST         1
#define PW_LOST_CARRIER         2
#define PW_LOST_SERVICE         3
#define PW_ACCT_IDLE_TIMEOUT    4
#define PW_ACCT_SESSION_TIMEOUT 5
#define PW_ADMIN_RESET          6
#define PW_ADMIN_REBOOT         7
#define PW_PORT_ERROR           8
#define PW_NAS_ERROR            9
#define PW_NAS_REQUEST          10
#define PW_NAS_REBOOT           11
#define PW_PORT_UNNEEDED        12
#define PW_PORT_PREEMPTED       13
#define PW_PORT_SUSPENDED       14
#define PW_SERVICE_UNAVAILABLE  15
#define PW_CALLBACK             16
#define PW_USER_ERROR           17
#define PW_HOST_REQUEST         18

/*     NAS PORT TYPES    */

#define PW_ASYNC		0
#define PW_SYNC			1
#define PW_ISDN_SYNC		2
#define PW_ISDN_SYNC_V120	3
#define PW_ISDN_SYNC_V110	4
#define PW_VIRTUAL		5

/*	   AUTHENTIC TYPES */
#define PW_RADIUS	1
#define PW_LOCAL	2
#define PW_REMOTE	3

/*    Session-Octets-Limit    */
#define PW_OCTETS_DIRECTION_SUM	0
#define PW_OCTETS_DIRECTION_IN	1
#define PW_OCTETS_DIRECTION_OUT	2
#define PW_OCTETS_DIRECTION_MAX	3


/* Vendor codes */
#define VENDOR_NONE     (-1)
#define VENDOR_MICROSOFT	311

/* Server data structures */

typedef struct dict_attr
{
	char              name[NAME_LENGTH + 1];	/* attribute name */
	int               value;			/* attribute index */
	int               type;				/* string, int, etc. */
	int               vendorcode;                   /* vendor code */
	struct dict_attr *next;
} DICT_ATTR;

typedef struct dict_value
{
	char               attrname[NAME_LENGTH +1];
	char               name[NAME_LENGTH + 1];
	int                value;
	struct dict_value *next;
} DICT_VALUE;

typedef struct vendor_dict
{
    char vendorname[NAME_LENGTH + 1];
    int vendorcode;
    DICT_ATTR *attributes;
    struct vendor_dict *next;
} VENDOR_DICT;

typedef struct value_pair
{
	char               name[NAME_LENGTH + 1];
	int                attribute;
	int                vendorcode;
	int                type;
	UINT4              lvalue;
	u_char             strvalue[AUTH_STRING_LEN + 1];
	struct value_pair *next;
} VALUE_PAIR;

/* don't change this, as it has to be the same as in the Merit radiusd code */
#define MGMT_POLL_SECRET	"Hardlyasecret"

/*	Define return codes from "SendServer" utility */

#define BADRESP_RC	-2
#define ERROR_RC	-1
#define OK_RC		0
#define TIMEOUT_RC	1

typedef struct send_data /* Used to pass information to sendserver() function */
{
	u_char          code;		/* RADIUS packet code */
	u_char          seq_nbr;	/* Packet sequence number */
	char           *server;		/* Name/addrress of RADIUS server */
	int             svc_port;	/* RADIUS protocol destination port */
	int             timeout;	/* Session timeout in seconds */
	int		retries;
	VALUE_PAIR     *send_pairs;     /* More a/v pairs to send */
	VALUE_PAIR     *receive_pairs;  /* Where to place received a/v pairs */
} SEND_DATA;

typedef struct request_info
{
	char		secret[MAX_SECRET_LENGTH + 1];
	u_char		request_vector[AUTH_VECTOR_LEN];
} REQUEST_INFO;

#ifndef MIN
#define MIN(a, b)     ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)     ((a) > (b) ? (a) : (b))
#endif

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

typedef struct env
{
	int maxsize, size;
	char **env;
} ENV;

#define ENV_SIZE	128

/*	Function prototypes	*/

/*	avpair.c		*/

VALUE_PAIR *rc_avpair_add __P((VALUE_PAIR **, int, void *, int, int));
int rc_avpair_assign __P((VALUE_PAIR *, void *, int));
VALUE_PAIR *rc_avpair_new __P((int, void *, int, int));
VALUE_PAIR *rc_avpair_gen __P((AUTH_HDR *));
VALUE_PAIR *rc_avpair_get __P((VALUE_PAIR *, UINT4));
VALUE_PAIR *rc_avpair_copy __P((VALUE_PAIR *));
void rc_avpair_insert __P((VALUE_PAIR **, VALUE_PAIR *, VALUE_PAIR *));
void rc_avpair_free __P((VALUE_PAIR *));
int rc_avpair_parse __P((char *, VALUE_PAIR **));
int rc_avpair_tostr __P((VALUE_PAIR *, char *, int, char *, int));
VALUE_PAIR *rc_avpair_readin __P((FILE *));

/*	buildreq.c		*/

void rc_buildreq __P((SEND_DATA *, int, char *, unsigned short, int, int));
unsigned char rc_get_seqnbr __P((void));
int rc_auth __P((UINT4, VALUE_PAIR *, VALUE_PAIR **, char *, REQUEST_INFO *));
int rc_auth_using_server __P((SERVER *, UINT4, VALUE_PAIR *, VALUE_PAIR **,
			      char *, REQUEST_INFO *));
int rc_auth_proxy __P((VALUE_PAIR *, VALUE_PAIR **, char *));
int rc_acct __P((UINT4, VALUE_PAIR *));
int rc_acct_using_server __P((SERVER *, UINT4, VALUE_PAIR *));
int rc_acct_proxy __P((VALUE_PAIR *));
int rc_check __P((char *, unsigned short, char *));

/*	clientid.c		*/

int rc_read_mapfile __P((char *));
UINT4 rc_map2id __P((char *));

/*	config.c		*/

int rc_read_config __P((char *));
char *rc_conf_str __P((char *));
int rc_conf_int __P((char *));
SERVER *rc_conf_srv __P((char *));
int rc_find_server __P((char *, UINT4 *, char *));

/*	dict.c			*/

int rc_read_dictionary __P((char *));
DICT_ATTR *rc_dict_getattr __P((int, int));
DICT_ATTR *rc_dict_findattr __P((char *));
DICT_VALUE *rc_dict_findval __P((char *));
DICT_VALUE * rc_dict_getval __P((UINT4, char *));
VENDOR_DICT * rc_dict_findvendor __P((char *));
VENDOR_DICT * rc_dict_getvendor __P((int));

/*	ip_util.c		*/

UINT4 rc_get_ipaddr __P((char *));
int rc_good_ipaddr __P((char *));
const char *rc_ip_hostname __P((UINT4));
UINT4 rc_own_ipaddress __P((void));


/*	sendserver.c		*/

int rc_send_server __P((SEND_DATA *, char *, REQUEST_INFO *));

/*	util.c			*/

void rc_str2tm __P((char *, struct tm *));
char *rc_mksid __P((void));
void rc_mdelay __P((int));

/* md5.c			*/

void rc_md5_calc __P((unsigned char *, unsigned char *, unsigned int));

#endif /* RADIUSCLIENT_H */
