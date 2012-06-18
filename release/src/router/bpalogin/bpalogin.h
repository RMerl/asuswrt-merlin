/*
**    BPALogin - lightweight portable BIDS2 login client
**    Copyright (c) 2001-3 Shane Hyde, and others.
** 
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
** 
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
** 
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
*/ 

/**
 * Changes:
 * 
 * 2001-12-05:  wdrose     Added errno.h to list of UNIX-only includes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <arpa/inet.h>
#endif

#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include "get_time.h"

#ifdef _WIN32
#include <winsock.h>
#endif

/*
**  Win32 & BeOS use a closesocket call, and unices use a close call, this define fixes this up
*/
#ifndef _WIN32
int closesocket(int);
//#define closesocket close
#endif


#ifdef _WIN32
#define NOSYSLOG
#define HAS_VNSPRINTF

#define sleep(x) Sleep((x)*1000)
#endif

/*
** This is needed for compiling with EMX under OS/2
*/
#ifdef __EMX__
  #define strcasecmp stricmp
#endif


#define TRUE 1
#define FALSE 0

#define LOGIN_SOFTWARE "bpalogin"
#define LOGIN_VERSION  1

#define MAXUSERNAME 25
#define MAXPASSWORD 25
#define MAXAUTHSERVER 80
#define MAXAUTHDOMAIN 80
#define MAXLOGINPROG 256
#define MAXCONFFILE 256
#define MAXLOCALADDRESS 32
#define MAXDDNSCONFFILE 256

#define DEFAULT_DEBUG         1
#define DEFAULT_AUTHSERVER    "sm-server"
#define DEFAULT_AUTHDOMAIN    ""
#define DEFAULT_AUTHPORT      5050
#define DEFAULT_CONFFILE      "/usr/local/etc/bpalogin.conf"
/*
** state engine codes
*/
#define STATE_NEED_PROTOCOL 0
#define STATE_SEND_PROT_REQ 1
#define STATE_AWAIT_NEG_RESP 2
#define STATE_SEND_LOGIN_REQ 3
#define STATE_AWAIT_LOGIN_AUTH_RESP 4
#define STATE_SEND_LOGIN_AUTH_REQ 5
#define STATE_AWAIT_LOGIN_RESP 6
#define STATE_SEND_LOGOUT_REQ 7
#define STATE_AWAIT_LOGOUT_AUTH_RESP 8
#define STATE_SEND_LOGOUT_AUTH_REQ 9
#define STATE_AWAIT_LOGOUT_RESP 10
#define STATE_IDLE_LOGIN 11
#define STATE_RECEIVED_STATUS_REQ 12
#define STATE_RECEIVED_RESTART_REQ 13
#define STATE_IDLE_LOGOFF 14

/*
** message type codes
*/
#define T_MSG_MIN 1
#define T_MSG_PROTOCOL_NEG_REQ 1
#define T_MSG_PROTOCOL_NEG_RESP 2
#define T_MSG_LOGIN_REQ 3
#define T_MSG_LOGIN_AUTH_REQ 4
#define T_MSG_LOGIN_RESP 5
#define T_MSG_LOGOUT_REQ 6
#define T_MSG_LOGOUT_AUTH_RESP 7
#define T_MSG_LOGOUT_RESP 8
#define T_MSG_AUTH_RESP 9
#define T_MSG_AUTH_REQ 10
#define T_MSG_STATUS_REQ 11
#define T_MSG_STATUS_RESP 12
#define T_MSG_RESTART_REQ 13
#define T_MSG_RESTART_RESP 14
#define T_MSG_MAX 14

/*
** message parameter codes
*/
#define T_PARAM_MIN 1
#define T_PARAM_PROTOCOL_LIST 1
#define T_PARAM_PROTOCOL_SELECT 2
#define T_PARAM_CLIENT_VERSION 3
#define T_PARAM_OS_IDENTITY 4
#define T_PARAM_OS_VERSION 5
#define T_PARAM_REASON_CODE 6
#define T_PARAM_USERNAME 7
#define T_PARAM_REQ_PORT 8
#define T_PARAM_RESPONSE_TEXT 9
#define T_PARAM_STATUS_CODE 10
#define T_PARAM_AUTH_CREDENTIALS 11
#define T_PARAM_NONCE 12
#define T_PARAM_SEQNUM 13
#define T_PARAM_HASH_METHOD 14
#define T_PARAM_LOGIN_SERVICE_PORT 15
#define T_PARAM_LOGOUT_SERVICE_PORT 16
#define T_PARAM_STATUS_SERVICE_PORT 17
#define T_PARAM_SUSPEND_IND 18
#define T_PARAM_STATUS_AUTH 19
#define T_PARAM_RESTART_AUTH 20
#define T_PARAM_TIMESTAMP 21
#define T_PARAM_TSMLIST 22
#define T_PARAM_LOGIN_PARAM_HASH 23
#define T_PARAM_LOGIN_SERVER_HOST 24
#define T_PARAM_MAX 24

/*
** login reason codes
*/
#define T_LOGIN_REASON_CODE_NORMAL 0
#define T_LOGIN_REASON_CODE_REAUTH 1

/*
** logout reason codes
*/
#define T_LOGOUT_REASON_CODE_USER_INITIATED 0
#define T_LOGOUT_REASON_CODE_APP_SHUTDOWN 1
#define T_LOGOUT_REASON_CODE_OS_SHUTDOWN 2
#define T_LOGOUT_REASON_CODE_UNKNOWN 3

/*
** client status transaction codes
*/
#define T_STATUS_TRANSACTION_OK 0

/*
** restart reasons
*/
#define T_RESTART_ADMIN 0

/*
** auth responses
*/
#define T_AUTH_NOHASH 0
#define T_AUTH_MD5_HASH 1

/*
** protocol types
*/
#define T_PROTOCOL_CHAL 1

/*
** status return codes
*/
#define T_STATUS_SUCCESS 0
#define T_STATUS_USERNAME_NOT_FOUND 1
#define T_STATUS_INCORRECT_PASSWORD 2
#define T_STATUS_ACCOUNT_DISABLED 3
#define T_STATUS_USER_DISABLED 4
#define T_STATUS_LOGIN_SUCCESSFUL_ALREADY_LOGGED_IN 100
#define T_STATUS_LOGIN_RETRY_LIMIT 101
#define T_STATUS_LOGIN_SUCCESSFUL_SWVER 102
#define T_STATUS_LOGIN_FAIL_SW 103
#define T_STATUS_LOGOUT_SUCCESSFUL_ALREADY_DISCONNECTED 200
#define T_STATUS_LOGOUT_AUTH_RETRY_LIMIT 201
#define T_STATUS_LOGIN_SUCCESS_SWVER 300
#define T_STATUS_LOGIN_FAIL_SWVER 301
#define T_STATUS_LOGIN_FAIL_INV_PROT 302
#define T_STATUS_LOGIN_UNKNOWN 500
#define T_STATUS_FAIL_USERNAME_VALIDATE 501
#define T_STATUS_FAIL_PASSWORD_VALIDATE 502

typedef unsigned short INT2;
typedef unsigned int INT4;

struct transaction
{
    char data[1512];
    int length;
};

/*
**  This structure holds all information necessary to connect/disconnect
*/
struct session
{
    /*
    **  Control paramters
    */
    char username[MAXUSERNAME];
    char password[MAXPASSWORD];
    char authserver[MAXAUTHSERVER];
    char authdomain[MAXAUTHDOMAIN];
    unsigned short authport;
    char connectedprog[MAXLOGINPROG];
    char disconnectedprog[MAXLOGINPROG];
    void * pUserData;
    int shutdown;
    char localaddress[MAXLOCALADDRESS];
    unsigned short localport;
    int minheartbeat, maxheartbeat;

    /*
    **  Callback functions
    */
    void (*debug)(int,char *,...);
    void (*critical)(char *);
    void (*noncritical)(char *,...);
    void (*onconnected)(int listenport);
    void (*ondisconnected)(int reason);

    /*
    **  Internal data
    */
    INT4 sessionid;
    INT2 listenport;
    struct sockaddr_in authhost;
    char osname[80];
    char osrelease[80];
    int listensock;
    struct sockaddr_in localaddr;
    struct sockaddr_in localipaddress;

    INT2 protocol;
    INT2 loginserviceport;
    char loginserverhost[128];
    INT2 hashmethod;
    char nonce[17];
    INT2 retcode;
    INT2 logoutport;
    INT2 statusport;
    char tsmlist[512];
    char tsmlist_s[512][20];
    struct sockaddr_in tsmlist_in[20];
    int tsmcount;
    char resptext[512];
    INT4 timestamp;

    time_t lastheartbeat;
    int recenthb;
    INT4 sequence;
    struct sockaddr_in fromaddr;
};

/*
**  Prototypes
*/
int mainloop(struct session *);
int handle_heartbeats(struct session *);

void start_transaction(struct transaction * t,INT2 msgtype,INT4 sessionid);
void send_transaction(struct session *s,int socket,struct transaction * t);
INT2 receive_transaction(struct session *s,int socket,struct transaction * t);
INT2 receive_udp_transaction(struct session *s,int socket,struct transaction * t,struct sockaddr_in *addr);
void send_udp_transaction(struct session * s,struct transaction * t);

int  extract_valueINT2(struct session *s,struct transaction * t,INT2 parm,INT2 *v);
int  extract_valueINT4(struct session *s,struct transaction *,INT2,INT4 *);
int  extract_valuestring(struct session *s,struct transaction *,INT2,char *);

void add_field_string(struct session *s,struct transaction * t,INT2 fn,char * p);
void add_field_data(struct session *s,struct transaction * t,INT2 fn,char * p,int c);
void add_field_INT2(struct session *s,struct transaction * t,INT2 fn,INT2 v);
void add_field_INT4(struct session *s,struct transaction * t,INT2 fn,INT4 v);

int  login(struct session *);
int  logout(INT2,struct session *);

INT2 read_INT2(void *);
INT4 read_INT4(void *);

void socketerror(struct session *,const char *);

extern int test_connect_success;

