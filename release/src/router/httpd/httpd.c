/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/* milli_httpd - pretty small HTTP server
** A combination of
** micro_httpd - really small HTTP server
** and
** mini_httpd - small HTTP server
**
** Copyright ?1999,2000 by Jef Poskanzer <jef@acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
//#include <sys/stat.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <ctype.h>

typedef unsigned int __u32;   // 1225 ham

#include <httpd.h>
//2008.08 magic{
#include <bcmnvram.h>	//2008.08 magic
#include <arpa/inet.h>	//2008.08 magic

#include <error.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <shared.h>
#include <shutils.h>

#define ETCPHYRD	14
#define SIOCGETCPHYRD   0x89FE
//#include "etioctl.h"

#ifdef RTCONFIG_HTTPS
#include <syslog.h>
#include <mssl.h>
#include <shutils.h>
#include <sys/file.h>
#define SERVER_PORT_SSL	443
#endif
#include "bcmnvram_f.h"

/* A multi-family sockaddr. */
typedef union {
    struct sockaddr sa;
    struct sockaddr_in sa_in;
} usockaddr;

#include "queue.h"
#define MAX_CONN_ACCEPT 64
#define MAX_CONN_TIMEOUT 60

#ifdef RTCONFIG_IFTTT
#define IFTTTUSERAGENT	"asusrouter-Windows-IFTTT-1.0"
#define GETIFTTTCGI	"get_IFTTTPincode.cgi"
#endif

typedef struct conn_item {
	TAILQ_ENTRY(conn_item) entry;
	int fd;
	usockaddr usa;
} conn_item_t;

typedef struct conn_list {
	TAILQ_HEAD(, conn_item) head;
	int count;
} conn_list_t;

/* Globals. */
static FILE *conn_fp;
static char auth_userid[AUTH_MAX];
static char auth_passwd[AUTH_MAX];
static char auth_realm[AUTH_MAX];
char host_name[64];
char referer_host[64];
char current_page_name[128];
char user_agent[1024];
char gen_token[32]={0};
char last_fail_token[32]={0};

#ifdef TRANSLATE_ON_FLY
char Accept_Language[16];

struct language_table language_tables[] = {
	{"br", "BR"},
	{"pt-BR", "BR"},
	{"zh-cn", "CN"},
	{"zh-Hans-CN", "CN"},
	{"cs", "CZ"},
	{"cs-cz", "CZ"},
	{"da", "DA"},
	{"da-DK", "DA"},
	{"de", "DE"},
	{"de-at", "DE"},
	{"de-ch", "DE"},
	{"de-de", "DE"},
	{"de-li", "DE"},
	{"de-lu", "DE"},
	{"en", "EN"},
	{"en-us", "EN"},
	{"es", "ES"},
	{"es-ec", "ES"},
	{"es-py", "ES"},
	{"es-pa", "ES"},
	{"es-ni", "ES"},
	{"es-gt", "ES"},
	{"es-do", "ES"},
	{"es-es", "ES"},
	{"es-hn", "ES"},
	{"es-ve", "ES"},
	{"es-pr", "ES"},
	{"es-ar", "ES"},
	{"es-bo", "ES"},
	{"es-us", "ES"},
	{"es-co", "ES"},
	{"es-cr", "ES"},
	{"es-uy", "ES"},
	{"es-pe", "ES"},
	{"es-cl", "ES"},
	{"es-mx", "ES"},
	{"es-sv", "ES"},
	{"fi", "FI"},
	{"fi-FI", "FI"},
	{"fr", "FR"},
	{"fr-fr", "FR"},
	{"fr-BE", "FR"},
	{"fr-CA", "FR"},
	{"fr-CH", "FR"},
	{"fr-MC", "FR"},
	{"fr-LU", "FR"},
	{"hu-hu", "HU"},
	{"hu", "HU"},
	{"it", "IT"},
	{"it-it", "IT"},
	{"it-ch", "IT"},
	{"ja", "JP"},
	{"ja-JP", "JP"},
	{"ko", "KR"},
	{"ko-kr", "KR"},
	{"ms", "MS"},
	{"ms-MY", "MS"},
	{"ms-BN", "MS"},
	{"nl", "NL"},
	{"nl-NL", "NL"},
	{"no", "NO"},
	{"nb", "NO"},
	{"nn", "NO"},
	{"nb-NO", "NO"},
	{"nn-NO", "NO"},
	{"pl-pl", "PL"},
	{"pl", "PL"},
	{"ru", "RU"},
	{"ru-ru", "RU"},
	{"ro", "RO"},
	{"ro-ro", "RO"},
	{"sl", "SL"},
	{"sl-SI", "SL"},
	{"sv", "SV"},
	{"sv-FI", "SV"},
	{"sv-SE", "SV"},
	{"th", "TH"},
	{"th-TH", "TH"},
	{"th-TH-TH", "TH"},
	{"tr", "TR"},
	{"tr-TR", "TR"},
	{"zh", "TW"},
	{"zh-tw", "TW"},
	{"zh-Hant-TW", "TW"},
	{"zh-hk", "TW"},
	{"uk", "UK"},
	{NULL, NULL}
};

#endif //TRANSLATE_ON_FLY

/* Forwards. */
static int initialize_listen_socket(usockaddr* usa, const char *ifname);
static int auth_check( char* dirname, char* authorization, char* url, char* file, char* cookies, int fromapp_flag);
int check_noauth_referrer(char* referer, int fromapp_flag);
char *get_referrer(char *referer);
char *generate_token(void);
static void send_error( int status, char* title, char* extra_header, char* text );
//#ifdef RTCONFIG_CLOUDSYNC
static void send_page( int status, char* title, char* extra_header, char* text , int fromapp);
//#endif
static void send_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp);
static void send_token_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp);
static void handle_request(void);
void send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time);
void __send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time);
int check_user_agent(char* user_agent);
#ifdef RTCONFIG_IFTTT
void add_ifttt_flag(void);
#endif

/* added by Joey */
//2008.08 magic{
//int redirect = 1;
int redirect = 0;
int change_passwd = 0;
int reget_passwd = 0;
int x_Setting = 0;
int skip_auth = 0;
char url[128];
int http_port = SERVER_PORT;
char *http_ifname = NULL;
time_t login_dt=0;
char login_url[128];
int login_error_status = 0;
char cloud_file[256];

/* Added by Joey for handle one people at the same time */
unsigned int login_ip=0; // the logined ip
time_t login_timestamp=0; // the timestamp of the logined ip
time_t login_timestamp_tmp=0; // the timestamp of the current session.
time_t last_login_timestamp=0; // the timestamp of the current session.
unsigned int login_ip_tmp=0; // the ip of the current session.
unsigned int login_try=0;
unsigned int last_login_ip = 0;	// the last logined ip 2008.08 magic
unsigned int MAX_login;
int lock_flag = 0;

// 2008.08 magic {
time_t request_timestamp = 0;
time_t turn_off_auth_timestamp = 0;
int temp_turn_off_auth = 0;	// for QISxxx.htm pages

/* Const vars */
const int int_1 = 1;

void http_login(unsigned int ip, char *url);
void http_login_timeout(unsigned int ip, char *cookies, int fromapp_flag);
void http_logout(unsigned int ip, char *cookies, int fromapp_flag);
int http_login_check(void);
asus_token_t* search_token_in_list(char* token, asus_token_t **prev);

#if 0
static int check_if_inviteCode(const char *dirpath){
	return 1;
}
#endif
void sethost(char *host)
{
	char *cp;

	if(!host) return;

	memset(host_name, 0, sizeof(host_name));
	strncpy(host_name, host, sizeof(host_name)-1);

	cp = host_name;
	for ( cp = cp + 7; *cp && *cp != '\r' && *cp != '\n'; cp++ );
	*cp = '\0';
}

char *gethost(void)
{
	if(strlen(host_name)) {
		return host_name;
	}
	else return(nvram_safe_get("lan_ipaddr"));
}

#include <sys/sysinfo.h>
long uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);

	return info.uptime;
}

static int
initialize_listen_socket(usockaddr* usa, const char *ifname)
{
	struct ifreq ifr;
	int fd;

	memset(usa, 0, sizeof(usockaddr));
	usa->sa.sa_family = AF_INET;
	usa->sa_in.sin_addr.s_addr = htonl(INADDR_ANY);
	usa->sa_in.sin_port = htons(http_port);

	fd = socket(usa->sa.sa_family, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	if (ifname) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
			perror("ioctl");
			goto error;
		}
		usa->sa_in.sin_addr.s_addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &int_1, sizeof(int_1)) < 0) {
		perror("setsockopt");
		goto error;
	}
	if (bind(fd, &usa->sa, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		goto error;
	}
	if (listen(fd, 1024) < 0) {
		perror( "listen" );
		goto error;
	}

	return fd;

error:
	close(fd);
	return -1;
}

void 
send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time)
{
	char inviteCode[512]={0};
	//char url_tmp[64]={0};
	char *cp, *file_var=NULL;

	if(url == NULL)
		strncpy(login_url, "index.asp", sizeof(login_url));
	else
		strncpy(login_url, url, sizeof(login_url));

	login_dt = lock_time;

	login_error_status = error_status;

	if(fromapp_flag == 0){
		if(strncmp(login_url, "cloud_sync.asp", strlen(login_url))==0){
			if(file != NULL){
				cp = strstr(file,"flag=");
				if(cp != (char*) 0){
					file_var = &cp[5];
					memset(cloud_file, 0, sizeof(cloud_file));
					strncpy(cloud_file, file_var, sizeof(cloud_file));
				}
			}
		}
		snprintf(inviteCode, sizeof(inviteCode), "<script>top.location.href='/Main_Login.asp';</script>");
	}else{
		snprintf(inviteCode, sizeof(inviteCode), "\"error_status\":\"%d\"", error_status);
	}
	send_page( 200, "OK", (char*) 0, inviteCode, fromapp_flag);
}

void
__send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time)
{
	login_try++;
	last_login_timestamp = login_timestamp_tmp;

	send_login_page(fromapp_flag, error_status, url, file, lock_time);
}

char
*get_referrer(char *referer)
{
	char *auth_referer=NULL;
	char *cp1=NULL, *cp2=NULL, *location_cp=NULL, *location_cp1=NULL;

	if(strstr(referer,"\r") != (char*) 0)
		location_cp1 = strtok(referer, "\r");
	else
		location_cp1 = referer;

	location_cp = strstr(location_cp1,"//");
	if(location_cp != (char*) 0){
		cp1 = &location_cp[2];
		if(strstr(cp1,"/") != (char*) 0){
			cp2 = strtok(cp1, "/");
			auth_referer = cp2;
		}else
			auth_referer = cp1;
	}else
		auth_referer = location_cp1;

	return auth_referer;
}

/* That code is now closed source.  Until we get binary blobs for
   all model, reuse the original code. */

#ifdef MISSING_HOOKS
int
check_noauth_referrer(char* referer, int fromapp_flag)
{
	char *auth_referer=NULL;

	if(fromapp_flag != 0)
		return 0;

	if(!referer || !strlen(host_name)){
		return NOREFERER;
	}else{
		auth_referer = get_referrer(referer);
	}

	if(!strcmp(host_name, auth_referer))
		return 0;
	else
		return REFERERFAIL;
}

int
referer_check(char* referer, int fromapp_flag)
{
	char *auth_referer=NULL;
	const int d_len = strlen(DUT_DOMAIN_NAME);
	int port = 0;
	int referer_from_https = 0;
	int referer_host_check = 0;

	if(fromapp_flag != 0)
		return 0;
	if(!referer){
		return NOREFERER;
	}else{
		auth_referer = get_referrer(referer);
	}

	if(referer_host[0] == 0){
		return WEB_NOREFERER;
	}

	if(!strcmp(host_name, auth_referer)) referer_host_check = 1;

	if (*(auth_referer + d_len) == ':' && (port = atoi(auth_referer + d_len + 1)) > 0 && port < 65536)
		referer_from_https = 1;

	if (((strlen(auth_referer) == d_len) || (*(auth_referer + d_len) == ':' && atoi(auth_referer + d_len + 1) > 0))
	   && strncmp(DUT_DOMAIN_NAME, auth_referer, d_len)==0){
		if(referer_from_https)
			snprintf(auth_referer,sizeof(referer_host),"%s:%d",nvram_safe_get("lan_ipaddr"), port);
		else
			snprintf(auth_referer,sizeof(referer_host),"%s",nvram_safe_get("lan_ipaddr"));
	}

	/* form based referer info? */
	if(referer_host_check && (strlen(auth_referer) == strlen(referer_host)) && strncmp( auth_referer, referer_host, strlen(referer_host) ) == 0){
		//_dprintf("asus token referer_check: the right user and password\n");
		return 0;
	}else{
		//_dprintf("asus token referer_check: the wrong user and password\n");
		return REFERERFAIL;
	}
	return REFERERFAIL;
}

#endif	// temporary code


#define	HEAD_HTTP_LOGIN	"HTTP login"	// copy from push_log/push_log.h

static int
auth_check( char* dirname, char* authorization, char* url, char* file, char* cookies, int fromapp_flag)
{
	struct in_addr temp_ip_addr;
	char *temp_ip_str;
	time_t dt;
	char asustoken[32];
	char *cp=NULL, *location_cp;

	memset(asustoken,0,sizeof(asustoken));

	login_timestamp_tmp = uptime();
	dt = login_timestamp_tmp - last_login_timestamp;
	if(last_login_timestamp != 0 && dt > 60){
		login_try = 0;
		last_login_timestamp = 0;
		lock_flag = 0;
	}
	if (MAX_login <= DEFAULT_LOGIN_MAX_NUM)
		MAX_login = DEFAULT_LOGIN_MAX_NUM;
	if(login_try >= MAX_login){
		lock_flag = 1;
		temp_ip_addr.s_addr = login_ip_tmp;
		temp_ip_str = inet_ntoa(temp_ip_addr);

		if(login_try%MAX_login == 0){
			check_token_timeout_in_list();
			logmessage(HEAD_HTTP_LOGIN, "Detect abnormal logins at %d times. The newest one was from %s in auth check.", login_try, temp_ip_str);
		}

//#ifdef LOGIN_LOCK
		__send_login_page(fromapp_flag, LOGINLOCK, url, NULL, dt);
		return LOGINLOCK;
//#endif
	}

	/* Is this directory unprotected? */
	if ( !strlen(auth_passwd) ){
		/* Yes, let the request go through. */
		return 0;
	}

	if(!cookies){
		send_login_page(fromapp_flag, NOTOKEN, url, file, 0);
		return NOTOKEN;
	}else{
		location_cp = strstr(cookies,"asus_token");
		if(location_cp != NULL){		
			cp = &location_cp[11];
			cp += strspn( cp, " \t" );
			snprintf(asustoken, sizeof(asustoken), "%s", cp);
		}else{
			send_login_page(fromapp_flag, NOTOKEN, url, file, 0);
			return NOTOKEN;
		}
	}
	/* form based authorization info? */

	if(search_token_in_list(asustoken, NULL) != NULL){
		//_dprintf("asus token auth_check: the right user and password\n");
#ifdef RTCONFIG_IFTTT
		if(strncmp(url, GETIFTTTCGI, strlen(GETIFTTTCGI))==0) add_ifttt_flag();
#endif
		login_try = 0;
		last_login_timestamp = 0;
		lock_flag = 0;
		return 0;
	}else{
		//_dprintf("asus token auth_check: the wrong user and password\n");
		if(!strcmp(last_fail_token, asustoken))
			send_login_page(fromapp_flag, AUTHFAIL, url, file, dt);
		else{
			strlcpy(last_fail_token, asustoken, sizeof(last_fail_token));
			__send_login_page(fromapp_flag, AUTHFAIL, url, file, dt);
		}
		return AUTHFAIL;
	}

	send_login_page(fromapp_flag, AUTHFAIL, url, file, 0);
	return AUTHFAIL;
}

char *generate_token(void){

	int a=0, b=0, c=0, d=0;
	//char create_token[32]={0};

	memset(gen_token,0,sizeof(gen_token));
	srand (time(NULL));
	a=rand();
	b=rand();
	c=rand();
	d=rand();
	snprintf(gen_token, sizeof(gen_token),"%d%d%d%d", a, b, c, d);

	return gen_token;
}

static void
send_error( int status, char* title, char* extra_header, char* text )
{
	send_headers( status, title, extra_header, "text/html", 0);
	(void) fprintf( conn_fp, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n<BODY BGCOLOR=\"#cc9999\"><H4>%d %s</H4>\n", status, title, status, title );
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "</BODY></HTML>\n" );
	(void) fflush( conn_fp );
}

//#ifdef RTCONFIG_CLOUDSYNC
static void
send_page( int status, char* title, char* extra_header, char* text , int fromapp){
    if(fromapp == 0){
	send_headers( status, title, extra_header, "text/html", fromapp);
	(void) fprintf( conn_fp, "<HTML><HEAD>");
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "</HEAD></HTML>\n" );
    }else{
	send_headers( status, title, extra_header, "application/json;charset=UTF-8", fromapp );
	(void) fprintf( conn_fp, "{\n");
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "}\n" );	
    }
    (void) fflush( conn_fp );
}
//#endif

static void
send_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp)
{
    time_t now;
    char timebuf[100];
    (void) fprintf( conn_fp, "%s %d %s\r\n", PROTOCOL, status, title );
    (void) fprintf( conn_fp, "Server: %s\r\n", SERVER_NAME );
    (void) fprintf( conn_fp, "x-frame-options: SAMEORIGIN\r\n");
    (void) fprintf( conn_fp, "x-xss-protection: 1; mode=block\r\n");

    if (fromapp != 0){
	(void) fprintf( conn_fp, "Cache-Control: no-store\r\n");	
	(void) fprintf( conn_fp, "Pragma: no-cache\r\n");
	if(fromapp == FROM_DUTUtil){
		(void) fprintf( conn_fp, "AiHOMEAPILevel: %d\r\n", EXTEND_AIHOME_API_LEVEL );
		(void) fprintf( conn_fp, "Httpd_AiHome_Ver: %d\r\n", EXTEND_HTTPD_AIHOME_VER );
		(void) fprintf( conn_fp, "Model_Name: %s\r\n", get_productid() );
	}else if(fromapp == FROM_ASSIA){
		(void) fprintf( conn_fp, "ASSIA_API_Level: %d\r\n", EXTEND_ASSIA_API_LEVEL );
	}
    }
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    (void) fprintf( conn_fp, "Date: %s\r\n", timebuf );
    if ( extra_header != (char*) 0 )
	(void) fprintf( conn_fp, "%s\r\n", extra_header );
    if ( mime_type != (char*) 0 ){
	if(fromapp != 0)
		(void) fprintf( conn_fp, "Content-Type: %s\r\n", "application/json;charset=UTF-8" );		
	else
		(void) fprintf( conn_fp, "Content-Type: %s\r\n", mime_type );
    }

    (void) fprintf( conn_fp, "Connection: close\r\n" );
    (void) fprintf( conn_fp, "\r\n" );
}

static void
send_token_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp)
{
    time_t now;
    char timebuf[100];
    char asus_token[32]={0};
    memset(asus_token,0,sizeof(asus_token));

    if(nvram_match("x_Setting", "0") && strcmp( gen_token, "") != 0){
        strlcpy(asus_token, gen_token, sizeof(asus_token));
    }else{
	strlcpy(asus_token, generate_token(), sizeof(asus_token));
        add_asus_token(asus_token);
    }

    (void) fprintf( conn_fp, "%s %d %s\r\n", PROTOCOL, status, title );
    (void) fprintf( conn_fp, "Server: %s\r\n", SERVER_NAME );
    if(fromapp == FROM_DUTUtil){
	(void) fprintf( conn_fp, "AiHOMEAPILevel: %d\r\n", EXTEND_AIHOME_API_LEVEL );
	(void) fprintf( conn_fp, "Httpd_AiHome_Ver: %d\r\n", EXTEND_HTTPD_AIHOME_VER );
	(void) fprintf( conn_fp, "Model_Name: %s\r\n", get_productid() );
    }else if(fromapp == FROM_ASSIA){
	(void) fprintf( conn_fp, "ASSIA_API_Level: %d\r\n", EXTEND_ASSIA_API_LEVEL );
    }
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    (void) fprintf( conn_fp, "Date: %s\r\n", timebuf );
    if ( extra_header != (char*) 0 )
	(void) fprintf( conn_fp, "%s\r\n", extra_header );
    if ( mime_type != (char*) 0 )
	(void) fprintf( conn_fp, "Content-Type: %s\r\n", mime_type );

	(void) fprintf( conn_fp, "Set-Cookie: asus_token=%s; HttpOnly;\r\n",asus_token );

    (void) fprintf( conn_fp, "Connection: close\r\n" );
    (void) fprintf( conn_fp, "\r\n" );
}

/* Simple shell-style filename matcher.  Only does ? * and **, and multiple
** patterns separated by |.  Returns 1 or 0.
*/
int
match( const char* pattern, const char* string )
    {
    const char* or;

    for (;;)
	{
	or = strchr( pattern, '|' );
	if ( or == (char*) 0 )
	    return match_one( pattern, strlen( pattern ), string );
	if ( match_one( pattern, or - pattern, string ) )
	    return 1;
	pattern = or + 1;
	}
    }


int
match_one( const char* pattern, int patternlen, const char* string )
    {
    const char* p;

    for ( p = pattern; p - pattern < patternlen; ++p, ++string )
	{
	if ( *p == '?' && *string != '\0' )
	    continue;
	if ( *p == '*' )
	    {
	    int i, pl;
	    ++p;
	    if ( *p == '*' )
		{
		/* Double-wildcard matches anything. */
		++p;
		i = strlen( string );
		}
	    else
		/* Single-wildcard matches anything but slash. */
		i = strcspn( string, "/" );
	    pl = patternlen - ( p - pattern );
	    for ( ; i >= 0; --i )
		if ( match_one( p, pl, &(string[i]) ) )
		    return 1;
	    return 0;
	    }
	if ( *p != *string )
	    return 0;
	}
    if ( *string == '\0' )
	return 1;
    return 0;
}

int web_write(const char *buffer, int len, FILE *stream)
{
	int n = len;
	int r = 0;

	while (n > 0) {
		r = fwrite(buffer, 1, n, stream);
		if (( r == 0) && (errno != EINTR)) return -1;
		buffer += r;
		n -= r;
	}
	return r;
}

int check_user_agent(char* user_agent){

	int fromapp = 0;

	if(user_agent != NULL){
		char *cp1=NULL, *app_router=NULL, *app_platform=NULL, *app_framework=NULL, *app_verison=NULL;
		cp1 = strdup(user_agent);

		vstrsep(cp1, "-", &app_router, &app_platform, &app_framework, &app_verison);

		if(app_router != NULL && app_framework != NULL && strcmp( app_router, "asusrouter") == 0){
				fromapp=FROM_ASUSROUTER;
			if(strcmp( app_framework, "DUTUtil") == 0)
				fromapp=FROM_DUTUtil;
			else if(strcmp( app_framework, "ASSIA") == 0)
				fromapp=FROM_ASSIA;
			else if(strcmp( app_framework, "IFTTT") == 0)
				fromapp=FROM_IFTTT;
		}
		if(cp1) free(cp1);
	}
	return fromapp;
}

#ifdef RTCONFIG_IFTTT
void add_ifttt_flag(void){

	memset(user_agent, 0, sizeof(user_agent));
	sprintf(user_agent, "%s",IFTTTUSERAGENT);
	return;
}
#endif

#if 0
void
do_file(char *path, FILE *stream)
{
	FILE *fp;
	char buf[1024];
	int nr;

	if (!(fp = fopen(path, "r")))
		return;
	while ((nr = fread(buf, 1, sizeof(buf), fp)) > 0) {
		web_write(buf, nr, stream);
	}
	fclose(fp);
}
#else
int do_fwrite(const char *buffer, int len, FILE *stream)
{
	int n = len;
	int r = 0;

	while (n > 0) {
		r = fwrite(buffer, 1, n, stream);
		if ((r == 0) && (errno != EINTR)) return -1;
		buffer += r;
		n -= r;
	}

	return r;
}

void do_file(char *path, FILE *stream)
{
	FILE *fp;
	char buf[1024];
	int nr;

	if ((fp = fopen(path, "r")) != NULL) {
		while ((nr = fread(buf, 1, sizeof(buf), fp)) > 0)
			do_fwrite(buf, nr, stream);
		fclose(fp);
	}
}

#endif

void set_referer_host(void)
{
	const int d_len = strlen(DUT_DOMAIN_NAME);
	int port = 0;
	int referer_from_https = 0;

	memset(referer_host, 0, sizeof(referer_host));
	if (*(host_name + d_len) == ':' && (port = atoi(host_name + d_len + 1)) > 0 && port < 65536){
		referer_from_https = 1;
	}
	if (((strlen(host_name) == d_len) || (*(host_name + d_len) == ':' && atoi(host_name + d_len + 1) > 0))
	   && strncmp(DUT_DOMAIN_NAME, host_name, d_len)==0){
		if(referer_from_https)
			snprintf(referer_host,sizeof(referer_host),"%s:%d",nvram_safe_get("lan_ipaddr"), port);
		else
			snprintf(referer_host,sizeof(referer_host),"%s",nvram_safe_get("lan_ipaddr"));
	}
	else
		snprintf(referer_host,sizeof(host_name),"%s",host_name);
}

int is_firsttime(void);

time_t detect_timestamp, detect_timestamp_old, signal_timestamp;
char detect_timestampstr[32];


#define APPLYAPPSTR 	"applyapp.cgi"
#define GETAPPSTR 	"getapp"
#define APPGETCGI 	"appGet.cgi"

#ifdef RTCONFIG_ROG
#define APPLYROGSTR     "api.asp"
#endif


static void
handle_request(void)
{
	char line[10000], *cur;
	char *method, *path, *protocol, *authorization, *boundary, *alang, *cookies, *referer, *useragent;
	char *cp;
	char *file;
	int len;
	struct mime_handler *handler;
	struct except_mime_handler *exhandler;
	struct mime_referer *doreferer;
	int mime_exception, do_referer, login_state = -1;
	int fromapp=0;
	int cl = 0, flags;
	int auth_result = 1;
	int referer_result = 1;
#ifdef RTCONFIG_FINDASUS
	int i, isDeviceDiscovery=0;
	char id_local[32],prouduct_id[32];
#endif

	/* Initialize the request variables. */
	authorization = boundary = cookies = referer = useragent = NULL;
	host_name[0] = 0;
	bzero( line, sizeof line );

	/* Parse the first line of the request. */
	if ( fgets( line, sizeof(line), conn_fp ) == (char*) 0 ) {
		send_error( 400, "Bad Request", (char*) 0, "No request found." );
		return;
	}

	method = path = line;
	strsep(&path, " ");
	//while (*path == ' ') path++;
	while (path && *path == ' ') path++;	// oleg patch
	protocol = path;
	strsep(&protocol, " ");
	//while (*protocol == ' ') protocol++;
	while (protocol && *protocol == ' ') protocol++;    // oleg pat
	cp = protocol;
	strsep(&cp, " ");
	if ( !method || !path || !protocol ) {
		send_error( 400, "Bad Request", (char*) 0, "Can't parse request." );
		return;
	}
	cur = protocol + strlen(protocol) + 1;

#ifdef TRANSLATE_ON_FLY
	memset(Accept_Language, 0, sizeof(Accept_Language));
#endif

	/* Parse the rest of the request headers. */
	while ( fgets( cur, line + sizeof(line) - cur, conn_fp ) != (char*) 0 )
	{
		//_dprintf("handle_request:cur = %s\n",cur);
		if ( strcmp( cur, "\n" ) == 0 || strcmp( cur, "\r\n" ) == 0 ) {
			break;
		}
#ifdef TRANSLATE_ON_FLY
		else if ( strncasecmp( cur, "Accept-Language:",16) ==0) {
			char *p;
			struct language_table *pLang;
			char lang_buf[256];
			memset(lang_buf, 0, sizeof(lang_buf));
			alang = &cur[16];
			strncpy(lang_buf, alang, sizeof(lang_buf)-1);
			p = lang_buf;
			while (p != NULL)
			{
				p = strtok (p, "\r\n ,;");
				if (p == NULL)  break;
				//2008.11 magic{
				int i, len=strlen(p);

				for (i=0;i<len;++i)
					if (isupper(p[i])) {
						p[i]=tolower(p[i]);
					}

				//2008.11 magic}
				for (pLang = language_tables; pLang->Lang != NULL; ++pLang)
				{
					if (strcasecmp(p, pLang->Lang)==0)
					{
						char dictname[32];

						if (!check_lang_support(pLang->Target_Lang))
							continue;
						snprintf(dictname,sizeof(dictname),"%s.dict", pLang->Target_Lang);
						if(!check_if_file_exist(dictname))
						{
							//_dprintf("language(%s) is not supported!!\n", pLang->Target_Lang);
							continue;
						}
						snprintf(Accept_Language,sizeof(Accept_Language),"%s",pLang->Target_Lang);
						if (is_firsttime() && nvram_match("ui_Setting", "0")) {
							_dprintf("%s", Accept_Language);
							nvram_set("ui_Setting", "1");
							nvram_set("preferred_lang", Accept_Language);
							

#if defined(RTCONFIG_TCODE)
							if (find_word(nvram_safe_get("rc_support"), "tcode") && nvram_get("territory_code")){
								if (!strncmp(nvram_get("territory_code"), "CN", 2))
									nvram_set("preferred_lang", "CN");
							}
#endif
						#ifdef RTCONFIG_DSL_TCLINUX
							if(!strcmp(Accept_Language, "CZ") || !strcmp(Accept_Language, "DE")) {
								int do_restart = 0;
								if( nvram_match("dslx_annex", "4")
									&& nvram_match("dsltmp_adslsyncsts", "down")
								){
									_dprintf("DSL: auto switch to annex b/j\n");
									nvram_set("dslx_annex", "6");
									do_restart = 1;
								}
								if(!strcmp(Accept_Language, "DE")
									&& nvram_match("dslx_vdsl_profile", "0")) {
									_dprintf("DSL: auto switch to 17a multi mode\n");
									nvram_set("dslx_vdsl_profile", "1");
									do_restart = 1;
								}
								if (do_restart)
									notify_rc("restart_dsl_setting");
							}
						#endif
						}

						break;
					}
				}

				if (Accept_Language[0] != 0) {
					break;
				}
				p+=strlen(p)+1;
			}

			if (Accept_Language[0] == 0) {
				// If all language setting of user's browser are not supported, use English.
				//printf ("Auto detect language failed. Use English.\n");
				strcpy (Accept_Language, "EN");

				// 2008.10 magic {
				if (is_firsttime())
					nvram_set("preferred_lang", "EN");
				// 2008.10 magic }
			}
		}
#endif
		else if ( strncasecmp( cur, "Authorization:", 14 ) == 0 )
		{
			cp = &cur[14];
			cp += strspn( cp, " \t" );
			authorization = cp;
			cur = cp + strlen(cp) + 1;
		}
		else if ( strncasecmp( cur, "User-Agent:", 11 ) == 0 )
		{
			cp = &cur[11];
			cp += strspn( cp, " \t" );
			useragent = cp;
			cur = cp + strlen(cp) + 1;
		}
		else if ( strncasecmp( cur, "Cookie:", 7 ) == 0 )
		{
			cp = &cur[7];
			cp += strspn( cp, " \t" );
			cookies = cp;
			cur = cp + strlen(cp) + 1;
		}
		else if ( strncasecmp( cur, "Referer:", 8 ) == 0 )
		{
			cp = &cur[8];
			cp += strspn( cp, " \t" );
			referer = cp;
			cur = cp + strlen(cp) + 1;
			//_dprintf("httpd referer = %s\n", referer);
		}
		else if ( strncasecmp( cur, "Host:", 5 ) == 0 )
		{
			cp = &cur[5];
			cp += strspn( cp, " \t" );
			sethost(cp);
			cur = cp + strlen(cp) + 1;
#ifdef RTCONFIG_FINDASUS
			sprintf(prouduct_id, "%s",get_productid());
			for(i = 0 ; i < strlen(prouduct_id) ; i++ ){
				prouduct_id[i] = tolower(prouduct_id[i]) ;
			}
			sprintf(id_local, "%s.local",prouduct_id);
			if(!strncmp(cp, "findasus", 8) || !strncmp(cp, id_local,strlen(id_local)))
				isDeviceDiscovery = 1;
			else
				isDeviceDiscovery = 0;
#endif
		}
		else if (strncasecmp( cur, "Content-Length:", 15 ) == 0) {
			cp = &cur[15];
			cp += strspn( cp, " \t" );
			cl = strtoul( cp, NULL, 0 );
		}
		else if ((cp = strstr( cur, "boundary=" ))) {
			boundary = &cp[9];
			for ( cp = cp + 9; *cp && *cp != '\r' && *cp != '\n'; cp++ );
			*cp = '\0';
			cur = ++cp;
		}
	}

	if ( strcasecmp( method, "get" ) != 0 && strcasecmp(method, "post") != 0 && strcasecmp(method, "head") != 0 ) {
		send_error( 501, "Not Implemented", (char*) 0, "That method is not implemented." );
		return;
	}

	if ( path[0] != '/' ) {
		send_error( 400, "Bad Request", (char*) 0, "Bad filename." );
		return;
	}
	file = &(path[1]);
	len = strlen( file );
	if ( file[0] == '/' || strcmp( file, ".." ) == 0 || strncmp( file, "../", 3 ) == 0 || strstr( file, "/../" ) != (char*) 0 || strcmp( &(file[len-3]), "/.." ) == 0 ) {
		send_error( 400, "Bad Request", (char*) 0, "Illegal filename." );
		return;
	}

//2008.08 magic{
	if (file[0] == '\0' || file[len-1] == '/'){
		if (is_firsttime()
#ifdef RTCONFIG_FINDASUS
		    && !isDeviceDiscovery
#endif
		   )
			//file = "QIS_wizard.htm";
			file = "QIS_default.cgi";
#ifdef RTCONFIG_FINDASUS
		else if(isDeviceDiscovery == 1)
			file = "find_device.asp";
#endif
		else
			file = "index.asp";
	}

// 2007.11 James. {
	char *query;
	int file_len;

	memset(url, 0, sizeof(url));
	if ((query = index(file, '?')) != NULL) {
		file_len = strlen(file)-strlen(query);

		if(file_len > sizeof(url))
			file_len = sizeof(url);

		strncpy(url, file, file_len);
	}
	else
	{
		strncpy(url, file, sizeof(url)-1);
	}
// 2007.11 James. }

	memset(current_page_name, 0, sizeof(current_page_name));
	strlcpy(current_page_name, url, sizeof(current_page_name));

	if(strncmp(url, APPLYAPPSTR, strlen(APPLYAPPSTR))==0 
#ifdef RTCONFIG_ROG
		|| strncmp(url, APPLYROGSTR, strlen(APPLYROGSTR))==0
#endif
	)
		fromapp=1;
	else if(strncmp(url, GETAPPSTR, strlen(GETAPPSTR))==0)  {
		fromapp=1;
		strcpy(url, url+strlen(GETAPPSTR));
		file += strlen(GETAPPSTR);
	}

	memset(user_agent, 0, sizeof(user_agent));
	if(useragent != NULL)
		strncpy(user_agent, useragent, sizeof(user_agent)-1);
	else
		strcpy(user_agent, "");

	fromapp = check_user_agent(useragent);

	//printf("httpd url: %s file: %s\n", url, file);
	//_dprintf("httpd url: %s file: %s\n", url, file);

	mime_exception = 0;
	do_referer = 0;

	if(!fromapp) {
		if(lock_flag == 1){
			login_timestamp_tmp = uptime();
			login_dt = login_timestamp_tmp - last_login_timestamp;
			if(last_login_timestamp != 0 && login_dt > 60){
				login_try = 0;
				last_login_timestamp = 0;
				lock_flag = 0;
				login_error_status = 0;
			}else{
				if((strncmp(file, "Main_Login.asp", 14)==0 && login_error_status == 7)|| strstr(url, ".png")){
				}else{
					send_login_page(fromapp, LOGINLOCK, url, NULL, login_dt);
					return;
				}
			}
		}
		http_login_timeout(login_ip_tmp, cookies, fromapp);	// 2008.07 James.
		login_state = http_login_check();
		// for each page, mime_exception is defined to do exception handler

		// check exception first
		for (exhandler = &except_mime_handlers[0]; exhandler->pattern; exhandler++) {
			if(match(exhandler->pattern, url))
			{
				mime_exception = exhandler->flag;
				break;
			}
		}

		// check doreferer first
		for (doreferer = &mime_referers[0]; doreferer->pattern; doreferer++) {
			if(match(doreferer->pattern, url))
			{
				do_referer = doreferer->flag;
				break;
			}
		}

		x_Setting = nvram_get_int("x_Setting");
	}
	else { // Jerry5 fix AiCloud login issue. 20120815
		x_Setting = nvram_get_int("x_Setting");
		//skip_auth = 0;
	}
	for (handler = &mime_handlers[0]; handler->pattern; handler++) {
		if (match(handler->pattern, url))
		{
			nvram_set("httpd_handle_request", url);
			nvram_set_int("httpd_handle_request_fromapp", fromapp);
			if(login_state==3 && !fromapp) { // few pages can be shown even someone else login
				if(!(mime_exception&MIME_EXCEPTION_MAINPAGE || (strncmp(file, "Main_Login.asp", 14)==0 && login_error_status == 9) || ((!handler->auth) && strncmp(file, "Main_Login.asp", 14) != 0))) {
					if(strcasecmp(method, "post") == 0 && handler->input)   //response post request
						while (cl--) (void)fgetc(conn_fp);

					send_login_page(fromapp, NOLOGIN, NULL, NULL, 0);
					return;
				}
			}
			if (handler->auth) {
				if ((mime_exception&MIME_EXCEPTION_NOAUTH_FIRST)&&!x_Setting) {
					//skip_auth=1;
				}
				else if((mime_exception&MIME_EXCEPTION_NOAUTH_ALL)) {
				}
				else {
					if(do_referer&CHECK_REFERER){
						referer_result = referer_check(referer, fromapp);
						if(referer_result != 0){
							if(strcasecmp(method, "post") == 0 && handler->input)   //response post request
								while (cl--) (void)fgetc(conn_fp);

							send_login_page(fromapp, referer_result, NULL, NULL, 0);
							//if(!fromapp) http_logout(login_ip_tmp, cookies);
							return;
						}
					}
					handler->auth(auth_userid, auth_passwd, auth_realm);
					auth_result = auth_check(auth_realm, authorization, url, file, cookies, fromapp);
					if (auth_result != 0)
					{
						if(strcasecmp(method, "post") == 0 && handler->input)   //response post request
							while (cl--) (void)fgetc(conn_fp);

						send_login_page(fromapp, auth_result, NULL, NULL, 0);
						return;
					}
				}

				if(!fromapp) {
					if (	!strstr(url, "QIS_")
							&& !strstr(url, ".js")
							&& !strstr(url, ".css")
							&& !strstr(url, ".gif")
							&& !strstr(url, ".png"))
						http_login(login_ip_tmp, url);
				}
			}else{
				if(fromapp == 0 && (do_referer&CHECK_REFERER)){
					referer_result = check_noauth_referrer(referer, fromapp);
					if(referer_result != 0){
						if(strcasecmp(method, "post") == 0){
							if (handler->input) {
								handler->input(file, conn_fp, cl, boundary);
							}
						}
						send_login_page(fromapp, referer_result, NULL, NULL, 0);
						//if(!fromapp) http_logout(login_ip_tmp, cookies);
						return;
					}
				}
			}

			if(!strcmp(file, "Logout.asp")){
				http_logout(login_ip_tmp, cookies, fromapp);
				send_login_page(fromapp, ISLOGOUT, NULL, NULL, 0);
				return;
			}
			if (strcasecmp(method, "post") == 0 && !handler->input) {
				send_error(501, "Not Implemented", NULL, "That method is not implemented.");
				return;
			}
// 2007.11 James. for QISxxx.htm pages }
			if (handler->input) {
				handler->input(file, conn_fp, cl, boundary);
#if defined(linux)
				if ((flags = fcntl(fileno(conn_fp), F_GETFL)) != -1 &&
						fcntl(fileno(conn_fp), F_SETFL, flags | O_NONBLOCK) != -1) {
					/* Read up to two more characters */
					if (fgetc(conn_fp) != EOF)
						(void)fgetc(conn_fp);

					fcntl(fileno(conn_fp), F_SETFL, flags);
				}
#elif defined(vxworks)
				flags = 1;
				if (ioctl(fileno(conn_fp), FIONBIO, (int) &flags) != -1) {
					/* Read up to two more characters */
					if (fgetc(conn_fp) != EOF)
						(void)fgetc(conn_fp);
					flags = 0;
					ioctl(fileno(conn_fp), FIONBIO, (int) &flags);
				}
#endif
			}
			if(!strstr(file, ".cgi") && !strstr(file, "syslog.txt") && !(strstr(file,"uploadIconFile.tar")) && !(strstr(file,"backup_jffs.tar")) && !(strstr(file,"networkmap.tar")) && !(strstr(file,".CFG")) && !(strstr(file,".log")) && !check_if_file_exist(file)
#ifdef RTCONFIG_USB_MODEM
					&& !strstr(file, "modemlog.txt")
#endif
#ifdef RTCONFIG_DSL_TCLINUX
					&& !strstr(file, "TCC.log")
#endif
					){
				send_error( 404, "Not Found", (char*) 0, "File not found." );
				return;
			} 
			if(strncmp(url, "QIS_default.cgi", strlen(url))==0 && nvram_match("x_Setting", "0")){

				if(!fromapp) set_referer_host();
				send_token_headers( 200, "Ok", handler->extra_header, handler->mime_type, fromapp);

			}else if(strncmp(url, "login.cgi", strlen(url))!=0){
				send_headers( 200, "Ok", handler->extra_header, handler->mime_type, fromapp);
			}

			if (strcasecmp(method, "head") != 0 && handler->output) {
				handler->output(file, conn_fp);
			}

			break;
		}
	}

	if (!handler->pattern){
		if(strlen(file) > 50 && !(strstr(file, "findasus")) && !(strstr(file, "acme-challenge"))){
			char inviteCode[512];
			memset(cloud_file, 0, sizeof(cloud_file));
			if(!check_xss_blacklist(file, 0))
				strlcpy(cloud_file, file, sizeof(cloud_file));

			snprintf(inviteCode, sizeof(inviteCode), "<meta http-equiv=\"refresh\" content=\"0; url=cloud_sync.asp?flag=%s\">\r\n", cloud_file);
			send_page( 200, "OK", (char*) 0, inviteCode, 0);
		}
		else
			send_error( 404, "Not Found", (char*) 0, "File not found." );
	}
	nvram_unset("httpd_handle_request");
	nvram_unset("httpd_handle_request_fromapp");
}

asus_token_t* search_token_in_list(char* token, asus_token_t **prev)
{
	asus_token_t *ptr = head;
	asus_token_t *tmp = NULL;
	int found = 0;
	char *cp=NULL;

	while(ptr != NULL)
	{
		if(!strncmp(token, ptr->token, 32))
		{
			found = 1;
			break;
		}
		else
		{
			tmp = ptr;
			ptr = ptr->next;
		}
    	}
    	if(found == 1)
    	{
        	if(prev)
	            *prev = tmp;
        	return ptr;
	}	
	else	
    	{
        	return NULL;
    	}
}

int delete_logout_from_list(char *cookies)
{
	asus_token_t *prev = NULL;
	asus_token_t *del = NULL;

	char asustoken[32];
	char *cp=NULL, *location_cp;

	memset(asustoken,0,sizeof(asustoken));

	//int fromapp_flag=0;

	if(!cookies || nvram_match("x_Setting", "0")){
		//send_login_page(fromapp_flag, NOTOKEN);
		return 0;
		
	}else{
		location_cp = strstr(cookies,"asus_token");
		if(location_cp != NULL){
			cp = &location_cp[11];
			cp += strspn( cp, " \t" );
			snprintf(asustoken, sizeof(asustoken), "%s", cp);
		}else{
			//send_login_page(fromapp_flag, NOTOKEN);
			return 0;
		}
	}

	del = search_token_in_list(asustoken,&prev);
	if(del == NULL)
	{
		return -1;
	}
	else
	{
		if(prev != NULL){
			prev->next = del->next;
		}
		if(del == curr)
		{
			curr = prev;
		}
		if(del == head)
		{
			head = del->next;
		}
	}

	free(del);
	del = NULL;

	return 0;
}

//2008 magic{
void http_login_cache(usockaddr *u) {
	struct in_addr temp_ip_addr;
	char *temp_ip_str;

	login_ip_tmp = (unsigned int)(u->sa_in.sin_addr.s_addr);
	temp_ip_addr.s_addr = login_ip_tmp;
	temp_ip_str = inet_ntoa(temp_ip_addr);
}

void http_login(unsigned int ip, char *url) {
	if(strncmp(url, "Main_Login.asp", strlen(url))==0)
		return;
	struct in_addr login_ip_addr;
	char *login_ip_str;
	char login_ipstr[32], login_timestampstr[32];

	if ((http_port != SERVER_PORT
/*	  && http_port != nvram_get_int("http_lanport")*/
#ifdef RTCONFIG_HTTPS
	  && http_port != SERVER_PORT_SSL
	  && http_port != nvram_get_int("https_lanport")
#endif
	    ) || ip == 0x100007f)
		return;

	login_ip = ip;
	last_login_ip = 0;

	login_ip_addr.s_addr = login_ip;
	login_ip_str = inet_ntoa(login_ip_addr);
	nvram_set("login_ip_str", login_ip_str);

	login_timestamp = uptime();

	memset(login_ipstr, 0, 32);
	sprintf(login_ipstr, "%u", login_ip);
	nvram_set("login_ip", login_ipstr);

	memset(login_timestampstr, 0, 32);
	sprintf(login_timestampstr, "%lu", login_timestamp);
	nvram_set("login_timestamp", login_timestampstr);
}

// 0: can not login, 1: can login, 2: loginer, 3: not loginer
int http_login_check(void)
{
	if ((http_port != SERVER_PORT
/*	  && http_port != nvram_get_int("http_lanport")*/
#ifdef RTCONFIG_HTTPS
	  && http_port != SERVER_PORT_SSL
	  && http_port != nvram_get_int("https_lanport")
#endif
	    ) || login_ip_tmp == 0x100007f)
		//return 1;
		return 0;	// 2008.01 James.

	if (login_ip == 0)
		return 1;
	else if (login_ip == login_ip_tmp)
		return 2;

	return 3;
}

void http_login_timeout(unsigned int ip, char *cookies, int fromapp_flag)
{
	time_t now, login_ts;

//	time(&now);
	now = uptime();
	login_ts = atol(nvram_safe_get("login_timestamp"));

// 2007.10 James. for really logout. {
	//if (login_ip!=ip && (unsigned long)(now-login_timestamp) > 60) //one minitues
	if ((login_ip != 0 && login_ip != ip) && ((unsigned long)(now-login_ts) > 60)) //one minitues
// 2007.10 James }
	{
		http_logout(login_ip, cookies, fromapp_flag);
	}
}

void http_logout(unsigned int ip, char *cookies, int fromapp_flag)
{
	if ((ip == login_ip || ip == 0 ) && fromapp_flag == 0) {
		last_login_ip = login_ip;
		login_ip = 0;
		login_timestamp = 0;

		nvram_set("login_ip", "");
		nvram_set("login_timestamp", "");
		memset(referer_host, 0, sizeof(referer_host));
		delete_logout_from_list(cookies);
// 2008.03 James. {
		if (change_passwd == 1) {
			change_passwd = 0;
			reget_passwd = 1;
		}
// 2008.03 James. }
	}else if(fromapp_flag != 0){
		delete_logout_from_list(cookies);
}
}
//2008 magic}
//

int is_auth(void)
{
	if (http_port==SERVER_PORT ||
/*	    http_port==nvram_get_int("http_lanport") ||*/
#ifdef RTCONFIG_HTTPS
	    http_port==SERVER_PORT_SSL ||
	    http_port==nvram_get_int("https_lanport") ||
#endif
		strcmp(nvram_get_x("PrinterStatus", "usb_webhttpcheck_x"), "1")==0) return 1;
	else return 0;
}

int is_firsttime(void)
{
	if (strcmp(nvram_get("x_Setting"), "1")==0)
		return 0;
	else
		return 1;
}

/* str_replace
* @param {char*} source
* @param {char*} find
* @param {char*} rep
* */
char *config_model_name(char *source, char *find,  char *rep){
   int find_L=strlen(find);
   int rep_L=strlen(rep);
   int length=strlen(source)+1;
   int gap=0;

   char *result = (char*)malloc(sizeof(char) * length);
   strcpy(result, source);

   char *former=source;
   char *location= strstr(former, find);

	/* stop searching when there is no finding string */
   while(location!=NULL){
       gap+=(location - former);
       result[gap]='\0';

       length+=(rep_L-find_L);
       result = (char*)realloc(result, length * sizeof(char));
       strcat(result, rep);
       gap+=rep_L;

       former=location+find_L;
       strcat(result, former);

       location= strstr(former, find);
   }
   return result;
}

#ifdef TRANSLATE_ON_FLY
/* Whether a language support should be enabled or not.
 * @lang:
 * @return:
 * 	0:	lang should not be supported.
 *     <0:	invalid parameter.
 *     >0:	lang can be supported.
 */
int check_lang_support(char *lang)
{
	int r = 1;

	if (!lang)
		return -1;

#if defined(RTCONFIG_TCODE)
	if (!find_word(nvram_safe_get("rc_support"), "tcode") || !nvram_get("territory_code"))
		return 1;
	if (!strncmp(nvram_get("territory_code"), "UK", 2) ||
	    !strncmp(nvram_get("territory_code"), "NE", 2)) {
		if (!strcmp(lang, "DA") || !strcmp(lang, "EN") ||
		    !strcmp(lang, "FI") || !strcmp(lang, "NO") ||
		    !strcmp(lang, "SV")) {
			r = 1;
		} else {
			r = 0;
		}
	} else {
		if (!strcmp(lang, "DA") || !strcmp(lang, "FI") ||
		    !strcmp(lang, "NO") || !strcmp(lang, "SV")) {
			r = 0;
		} else {
			r = 1;
		}
	}
#endif

	return r;
}

#ifdef RTCONFIG_AUTODICT
int
load_dictionary (char *lang, pkw_t pkw)
{
	char dfn[16];
	char dummy_buf[16];
	int dict_item_idx;
	char* tmp_ptr;
	int dict_item; // number of dict item, now get from text file
	char *q;
	FILE *dfp = NULL;
	int remain_dict;
	int dict_size = 0;
//	struct timeval tv1, tv2;
	const char *eng_dict = "EN.dict";
#ifndef RELOAD_DICT
	static char loaded_dict[12] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
#endif  // RELOAD_DICT
#ifdef RTCONFIG_DYN_DICT_NAME
	char *dyn_dict_buf;
	char *dyn_dict_buf_new;
#endif

//printf ("lang=%s\n", lang);

//	gettimeofday (&tv1, NULL);
	if (lang == NULL || (lang != NULL && strlen (lang) == 0)) {
		// if "lang" is invalid, use English as default
		snprintf (dfn, sizeof (dfn), eng_dict);
	} else {
		snprintf (dfn, sizeof (dfn), "%s.dict", lang);
	}

#ifndef RELOAD_DICT
//	printf ("loaded_dict (%s) v.s. dfn (%s)\n", loaded_dict, dfn);
	if (strcmp (dfn, loaded_dict) == 0) {
		return 1;
	}
	release_dictionary (pkw);
#endif  // RELOAD_DICT

	do {
//		 printf("Open (%s) dictionary file.\n", dfn);
//
// Now DICT files all use UTF-8, it is no longer a text file
// it need to use open as binary
//
		dfp = fopen (dfn, "rb");
		if (dfp != NULL)	{
#ifndef RELOAD_DICT
			snprintf (loaded_dict, sizeof (loaded_dict), "%s", dfn);
#endif  // RELOAD_DICT
			break;
		}

//		printf ("Open (%s) failure. errno %d (%s)\n", dfn, errno, strerror (errno));
		if (dfp == NULL && strcmp (dfn, eng_dict) == 0) {
			return 0;
		} else {
			// If we can't open specified language file, use English as default
			snprintf (dfn, sizeof (dfn), eng_dict);
		}
	} while (1);

	memset (pkw, 0, sizeof (kw_t));
	fseek (dfp, 0L, SEEK_END);
	dict_size = ftell (dfp) + 128;
	// skip BOM header length
	dict_size -= 3;
	printf ("dict_size %d\n", dict_size);

#ifdef RTCONFIG_DYN_DICT_NAME
	dyn_dict_buf = (char *) malloc(dict_size);
	fseek (dfp, 0L, SEEK_SET);
	// skip BOM
	fread (dummy_buf, 1, 3, dfp);
	// read to dict string buffer
	memset(dyn_dict_buf, 0, dict_size);
	fread (dyn_dict_buf, 1, dict_size, dfp);
	dyn_dict_buf_new = config_model_name(dyn_dict_buf, "ZVDYNMODELVZ", nvram_safe_get("productid"));

	free(dyn_dict_buf);

	dict_size = sizeof(char) * strlen(dyn_dict_buf_new);
	pkw->buf = (unsigned char *) (q = malloc (dict_size));
	strcpy(pkw->buf, dyn_dict_buf_new);
	free(dyn_dict_buf_new);
#else
	pkw->buf = (unsigned char *) (q = malloc (dict_size));

	fseek (dfp, 0L, SEEK_SET);
	// skip BOM
	fread (dummy_buf, 1, 3, dfp);
	// read to dict string buffer
	memset(pkw->buf, 0, dict_size);
	fread (pkw->buf, 1, dict_size, dfp);
#endif
	// calc how many dict item , dict_item
	remain_dict = dict_size;
	tmp_ptr = (char *) pkw->buf;
	dict_item = 0;
	while (remain_dict>0) {
		if (*tmp_ptr == 0x0a) {
			dict_item++;
			tmp_ptr++;
			remain_dict--;
		}
		else if (*tmp_ptr == 0) {
			break;
		}
		else {
			tmp_ptr++;
			remain_dict--;
		}
	}
	// allocate memory according dict_item
	pkw->idx = malloc (dict_item * sizeof(unsigned char*));

	printf ("dict_item %d\n", dict_item);

	// get all string start and put to pkw->idx
	remain_dict = dict_size;
	for (dict_item_idx = 0; dict_item_idx < dict_item; dict_item_idx++) {
		pkw->idx[dict_item_idx] = (unsigned char *) q;
		while (remain_dict>0) {
			if (*q == 0x0a) {
				*q=0;
				q++;
				remain_dict--;
				break;
			}
			if (*q == 0) {
				break;
			}
			q++;
			remain_dict--;
		}
	}

	pkw->len = dict_item;

	fclose (dfp);

	return 1;
}


void
release_dictionary (pkw_t pkw)
{
	if (pkw == NULL)	{
		return;
	}

	//pkw->len = pkw->tlen = 0;
	pkw->len = 0;

	if (pkw->idx != NULL)   {
		free (pkw->idx);
		pkw->idx = NULL;
	}

	if (pkw->buf != NULL)   {
		free (pkw->buf);
		pkw->buf = NULL;
	}
}

char*
search_desc (pkw_t pkw, char *name)
{
	int i;
	char *ret = NULL;
	int dict_idx;
	char name_buf[128];

/*
	printf("search_desc:");
	printf(name);
	printf("\n");
*/

	if (pkw == NULL || (pkw != NULL && pkw->len <= 0)) {
		return NULL;
	}

	// remove equal
	memset(name_buf,0,sizeof(name_buf));
	// minus one for reserver one char for string zero char
	for (i = 0; i<sizeof(name_buf)-1; i++)  {
		if (*name == 0 || *name == '=') {
			break;
		}
		name_buf[i]=*name++;
	}

/*
	for (i = 0; i < pkw->len; ++i)  {
		char *p;
		p = pkw->idx[i];
		if (strncmp (name, p, strlen (name)) == 0) {
			ret = p + strlen (name);
			break;
		}
	}
*/

/*
	printf("search_desc:");
	printf(name_buf);
	printf("\n");
*/

	dict_idx = atoi(name_buf);
//	printf("%d , %d\n",dict_idx,pkw->len);
	if (dict_idx < pkw->len) {
		ret = (char *) pkw->idx[dict_idx];
	}
	else {
		ret = (char *) pkw->idx[0];
	}

/*
	printf("ret:");
	printf(ret);
	printf("\n");
*/

	return ret;
}
#else
int
load_dictionary (char *lang, pkw_t pkw)
{
	char dfn[16];
	char *p, *q;
	FILE *dfp = NULL;
	int dict_size = 0;
//	struct timeval tv1, tv2;
	const char *eng_dict = "EN.dict";
#ifndef RELOAD_DICT
	static char loaded_dict[12] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
#endif  // RELOAD_DICT

//	gettimeofday (&tv1, NULL);
	if (lang == NULL || (lang != NULL && strlen (lang) == 0)) {
		// if "lang" is invalid, use English as default
		snprintf (dfn, sizeof (dfn), eng_dict);
	} else {
		snprintf (dfn, sizeof (dfn), "%s.dict", lang);
	}

#ifndef RELOAD_DICT
//	printf ("loaded_dict (%s) v.s. dfn (%s)\n", loaded_dict, dfn);
	if (strcmp (dfn, loaded_dict) == 0) {
		return 1;
	}
	release_dictionary (pkw);
#endif  // RELOAD_DICT

	do {
//		 printf("Open (%s) dictionary file.\n", dfn);
		dfp = fopen (dfn, "r");
		if (dfp != NULL)	{
#ifndef RELOAD_DICT
			snprintf (loaded_dict, sizeof (loaded_dict), "%s", dfn);
#endif  // RELOAD_DICT
			break;
		}

//		printf ("Open (%s) failure. errno %d (%s)\n", dfn, errno, strerror (errno));
		if (dfp == NULL && strcmp (dfn, eng_dict) == 0) {
			return 0;
		} else {
			// If we can't open specified language file, use English as default
			snprintf (dfn, sizeof (dfn), eng_dict);
		}
	} while (1);

	memset (pkw, 0, sizeof (kw_t));
	fseek (dfp, 0L, SEEK_END);
	dict_size = ftell (dfp) + 128;
//	printf ("dict_size %d\n", dict_size);
	REALLOC_VECTOR (pkw->idx, pkw->len, pkw->tlen, sizeof (unsigned char*));
	pkw->buf = q = malloc (dict_size);

	fseek (dfp, 0L, SEEK_SET);
#if 0
	while (!feof (dfp)) {
		// if pkw->idx is not enough, add 32 item to pkw->idx
		REALLOC_VECTOR (pkw->idx, pkw->len, pkw->tlen, sizeof (unsigned char*));

		fscanf (dfp, "%[^\n]%*c", q);
		if ((p = strchr (q, '=')) != NULL) {
			pkw->idx[pkw->len] = q;
			pkw->len++;
			q = p + strlen (p);
			*q = '\0';
			q++;
		}
	}
#else
	while ((fscanf(dfp, "%[^\n]", q)) != EOF) {
		fgetc(dfp);

		// if pkw->idx is not enough, add 32 item to pkw->idx
		REALLOC_VECTOR (pkw->idx, pkw->len, pkw->tlen, sizeof (unsigned char*));

		if ((p = strchr (q, '=')) != NULL) {
			pkw->idx[pkw->len] = q;
			pkw->len++;
			q = p + strlen (p);
			*q = '\0';
			q++;
		}
	}

#endif

	fclose (dfp);
//	gettimeofday (&tv2, NULL);
//	printf("Load %d keywords spent %ldms\n", pkw->len, ((tv2.tv_sec * 1000000 + tv2.tv_usec) - (tv1.tv_sec * 1000000 + tv1.tv_usec)) / 1000);

	return 1;
}


void
release_dictionary (pkw_t pkw)
{
	if (pkw == NULL)	{
		return;
	}

	pkw->len = pkw->tlen = 0;

	if (pkw->idx != NULL)   {
		free (pkw->idx);
		pkw->idx = NULL;
	}

	if (pkw->buf != NULL)   {
		free (pkw->buf);
		pkw->buf = NULL;
	}
}

char*
search_desc (pkw_t pkw, char *name)
{
	int i;
	char *p, *ret = NULL;

	if (pkw == NULL || (pkw != NULL && pkw->len <= 0)) {
		return NULL;
	}
	for (i = 0; i < pkw->len; ++i)  {
		p = pkw->idx[i];
		if (strncmp (name, p, strlen (name)) == 0) {
			ret = p + strlen (name);
			break;
		}
	}

	return ret;
}
#endif
#endif //TRANSLATE_ON_FLY

void reapchild()	// 0527 add
{
	signal(SIGCHLD, reapchild);
	wait(NULL);
}

int do_ssl = 0; 	// use Global for HTTPS upgrade judgment in web.c
int ssl_stream_fd; 	// use Global for HTTPS stream fd in web.c
int main(int argc, char **argv)
{
	usockaddr usa;
	int listen_fd[2];
	socklen_t sz;
	char pidfile[32];
	fd_set active_rfds;
	conn_list_t pool;
	int i, c;
	//int do_ssl = 0;

	do_ssl = 0; // default

	// usage : httpd -s -p [port]
	while ((c = getopt(argc, argv, "sp:i:")) != -1) {
		switch (c) {
		case 's':
#ifdef RTCONFIG_HTTPS
			do_ssl = 1;
#endif
			break;
		case 'p':
			http_port = atoi(optarg);
			break;
		case 'i':
			http_ifname = optarg;
			break;
		default:
			fprintf(stderr, "ERROR: unknown option %c\n", c);
			break;
		}
	}

	/* -p might be specified before -s */
	if (http_port == 0) {
#ifdef RTCONFIG_HTTPS
		if (do_ssl) {
			http_port = SERVER_PORT_SSL;
		} else
#endif
		{
			http_port = SERVER_PORT;
		}
	}

	//websSetVer();
	//2008.08 magic
	nvram_unset("login_timestamp");
	nvram_unset("login_ip");
	nvram_unset("login_ip_str");
	MAX_login = nvram_get_int("login_max_num");
	if(MAX_login <= DEFAULT_LOGIN_MAX_NUM)
		MAX_login = DEFAULT_LOGIN_MAX_NUM;

	detect_timestamp_old = 0;
	detect_timestamp = 0;
	signal_timestamp = 0;

	/* Ignore broken pipes */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, reapchild);	// 0527 add

#ifdef RTCONFIG_HTTPS
	//if (do_ssl)
		start_ssl();
#endif

	/* Initialize listen socket */
	for (i = 0; i < ARRAY_SIZE(listen_fd); i++)
		listen_fd[i] = -1;
	if ((listen_fd[0] = initialize_listen_socket(&usa, http_ifname)) < 2) {
		fprintf(stderr, "can't bind to %s address\n", http_ifname ? : "any");
		exit(errno);
	}
	if ((http_ifname && strcmp(http_ifname, "lo") != 0) &&
	    (listen_fd[1] = initialize_listen_socket(&usa, "lo")) < 2) {
		fprintf(stderr, "can't bind to %s address\n", "loopback");
		/* allow fail if previous bind to interface was ok */
		/* exit(errno); */
	}

	FILE *pid_fp;
	if (http_port==SERVER_PORT)
		strcpy(pidfile, "/var/run/httpd.pid");
	else sprintf(pidfile, "/var/run/httpd-%d.pid", http_port);

	if (!(pid_fp = fopen(pidfile, "w"))) {
		perror(pidfile);
		return errno;
	}
	fprintf(pid_fp, "%d", getpid());
	fclose(pid_fp);

	/* Init connection pool */
	FD_ZERO(&active_rfds);
	TAILQ_INIT(&pool.head);
	pool.count = 0;

	/* Loop forever handling requests */
	for (;;) {
		int max_fd, count;
		struct timeval tv;
		fd_set rfds;
		conn_item_t *item, *next;

		memcpy(&rfds, &active_rfds, sizeof(rfds));
		max_fd = -1;
		if (pool.count < MAX_CONN_ACCEPT) {
			for (i = 0; i < ARRAY_SIZE(listen_fd); i++) {
				if (listen_fd[i] < 0)
					continue;
				FD_SET(listen_fd[i], &rfds);
				max_fd = max(listen_fd[i], max_fd);
			}
		}
		TAILQ_FOREACH(item, &pool.head, entry)
			max_fd = max(item->fd, max_fd);

		/* Wait for new connection or incoming request */
		tv.tv_sec = MAX_CONN_TIMEOUT;
		tv.tv_usec = 0;
		while ((count = select(max_fd + 1, &rfds, NULL, NULL, &tv)) < 0 && errno == EINTR)
			continue;
		if (count < 0) {
			perror("select");
			return errno;
		}

		/* Check and accept new connection */
		item = NULL;
		for (i = 0; count && i < ARRAY_SIZE(listen_fd); i++) {
			if (listen_fd[i] < 0 || !FD_ISSET(listen_fd[i], &rfds))
				continue;

			item = malloc(sizeof(*item));
			if (item == NULL) {
				perror("malloc");
				return errno;
			}
			sz = sizeof(item->usa);
			while ((item->fd = accept(listen_fd[i], &item->usa.sa, &sz)) < 0 && errno == EINTR)
				continue;
			if (item->fd < 0) {
				perror("accept");
				free(item);
				continue;
			}

			/* Set the KEEPALIVE option to cull dead connections */
			setsockopt(item->fd, SOL_SOCKET, SO_KEEPALIVE, &int_1, sizeof(int_1));

			/* Add to active connections */
			FD_SET(item->fd, &active_rfds);
			TAILQ_INSERT_TAIL(&pool.head, item, entry);
			pool.count++;
		}
		/* Continue waiting over again */
		if (count && item)
			continue;

		/* Check and process pending or expired requests */
		TAILQ_FOREACH_SAFE(item, &pool.head, entry, next) {
			if (count && !FD_ISSET(item->fd, &rfds))
				continue;

			/* Delete from active connections */
			FD_CLR(item->fd, &active_rfds);
			TAILQ_REMOVE(&pool.head, item, entry);
			pool.count--;

			/* Process request if any */
			if (count) {
#ifdef RTCONFIG_HTTPS
				if (do_ssl) {
					ssl_stream_fd = item->fd;
					if (!(conn_fp = ssl_server_fopen(item->fd))) {
						perror("fdopen(ssl)");
						goto skip;
					}
				} else
#endif
				if (!(conn_fp = fdopen(item->fd, "r+"))) {
					perror("fdopen");
					goto skip;
				}

				http_login_cache(&item->usa);
				handle_request();
				fflush(conn_fp);
#ifdef RTCONFIG_HTTPS
				if (!do_ssl)
#endif
				shutdown(item->fd, 2), item->fd = -1;
				fclose(conn_fp);

			skip:
				/* Skip the rest of */
				if (--count == 0)
					next = NULL;

			}

			/* Close timed out and/or still alive */
			if (item->fd >= 0) {
				shutdown(item->fd, 2);
				close(item->fd);
			}

			free(item);
		}
	}

	for (i = 0; i < ARRAY_SIZE(listen_fd); i++) {
		if (listen_fd[i] < 0)
			continue;
		shutdown(listen_fd[i], 2);
		close(listen_fd[i]);
	}

	return 0;
}

#ifdef RTCONFIG_HTTPS
void save_cert(void)
{
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	eval("cp", "-p", "/etc/cert.pem", "/etc/key.pem", "/jffs/ssl/");
#else
	if (eval("tar", "-C", "/", "-czf", "/tmp/cert.tgz", "etc/cert.pem", "etc/key.pem") == 0) {
		if (nvram_set_file("https_crt_file", "/tmp/cert.tgz", 8192)) {
			nvram_commit_x();
		}
	}
	unlink("/tmp/cert.tgz");
#endif

}

void erase_cert(void)
{
	unlink("/etc/cert.pem");
	unlink("/etc/key.pem");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	unlink(JFFSCERT);
	unlink(JFFSKEY);
#else
	nvram_unset("https_crt_file");
#endif
	//nvram_unset("https_crt_gen");
	nvram_set("https_crt_gen", "0");
}

void start_ssl(void)
{
	int lockfd;
	int ok=0;
	int retry;
	unsigned long long sn;
	int i;
#if !defined(RTCONFIG_JFFS2) && !defined(RTCONFIG_BRCM_NAND_JFFS2) && !defined(RTCONFIG_UBIFS)
        int save;

	save = nvram_match("https_crt_save", "1");
#endif

	lockfd = open("/var/lock/sslinit.lock", O_CREAT | O_RDWR, 0666);

	// Avoid collisions if another httpd instance is initializing SSL cert
	for ( i = 1; i < 5; i++ ) {
		if (flock(lockfd, LOCK_EX | LOCK_NB) < 0) {
			//logmessage("httpd", "Conflict, waiting %d", i);
			sleep(i*i);
		} else {
			i = 5;
		}
	}

	//fprintf(stderr,"[httpd] start_ssl running!!\n");
	//nvram_set("https_crt_gen", "1");

	if (nvram_match("https_crt_gen", "1")) {
		erase_cert();
	}

	retry = 1;

	while (1) {

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		if (f_exists(JFFSCERT) && f_exists(JFFSKEY)) {
			eval("cp", "-p", JFFSKEY, JFFSCERT, "/etc/");
			system("cat /etc/key.pem /etc/cert.pem > /etc/server.pem");
			ok = 1;
		}
#endif

		if ((!f_exists("/etc/cert.pem")) || (!f_exists("/etc/key.pem"))) {
			ok = 0;
#if !defined(RTCONFIG_JFFS2) && !defined(RTCONFIG_BRCM_NAND_JFFS2) && !defined(RTCONFIG_UBIFS)
			if (save) {
				fprintf(stderr, "Save SSL certificate...\n"); // tmp test
				if (nvram_get_file("https_crt_file", "/tmp/cert.tgz", 8192)) {
					if (eval("tar", "-xzf", "/tmp/cert.tgz", "-C", "/", "etc/cert.pem", "etc/key.pem") == 0){
						system("cat /etc/key.pem /etc/cert.pem > /etc/server.pem");
						ok = 1;
					}

					int save_intermediate_crt = nvram_match("https_intermediate_crt_save", "1");
					if(save_intermediate_crt){
						eval("tar", "-xzf", "/tmp/cert.tgz", "-C", "/", "etc/intermediate_cert.pem");
					}

					unlink("/tmp/cert.tgz");
				}
			}
#endif
			if (!ok) {
				erase_cert();
				logmessage("httpd", "Generating SSL certificate...");
				fprintf(stderr, "Generating SSL certificate...\n"); // tmp test
				// browsers seems to like this when the ip address moves...	-- zzz
				f_read("/dev/urandom", &sn, sizeof(sn));

				eval("gencert.sh", "web");
			}
		}

		if ((!ok)
#if !defined(RTCONFIG_JFFS2) && !defined(RTCONFIG_BRCM_NAND_JFFS2) && !defined(RTCONFIG_UBIFS)
		    && (save)
		    && (*nvram_safe_get("https_crt_file")) == 0
#endif
		    ){
			save_cert();
		}

		if (mssl_init("/etc/cert.pem", "/etc/key.pem")) {
			flock(lockfd, LOCK_UN);
			return;
		}

		logmessage("httpd", "Failed to initialize SSL, generating new key/cert.");
		erase_cert();

		if (!retry) {
			flock(lockfd, LOCK_UN);
			logmessage("httpd", "Unable to start in SSL mode, exiting!");
			exit(1);
		}
		retry = 0;
	}
}
#endif

