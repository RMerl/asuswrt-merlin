/* ============================================================================
 * Copyright (C) 1998-2001 Angus Mackay. All rights reserved; 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================
 */

/*
 * ez-ipupdate
 *
 * a very simple dynDNS client for the ez-ip dynamic dns service 
 * (http://www.ez-ip.net).
 *
 * why this program when something like:
 *   curl -u user:pass http://www.ez-ip.net/members/update/?key=val&...
 * would do the trick? because there are nicer clients for other OSes and
 * I don't like to see UNIX get the short end of the stick.
 *
 * tested under Linux and Solaris.
 * 
 */
//2007.03.14 Yau add
#ifdef ASUS_DDNS
#include "asus_ddns.h"
#include <bcmnvram.h>
#endif  // ASUS_DDNS

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef EMBED
// you man very well need to edit this, don't worry though, email is only sent
// if bad things happend and it has to exit when in daemon mode.
#define SEND_EMAIL_CMD "mail"
#endif

#define EZIP_DEFAULT_SERVER "www.EZ-IP.Net"
#define EZIP_DEFAULT_PORT "80"
#define EZIP_REQUEST "/members/update/"

#define NOIP_DEFAULT_SERVER "dynupdate.no-ip.com"
#define NOIP_DEFAULT_PORT "80"
#define NOIP_REQUEST "/nic/update"

#define PGPOW_DEFAULT_SERVER "www.penguinpowered.com"
#define PGPOW_DEFAULT_PORT "2345"
#define PGPOW_REQUEST "update"
#define PGPOW_VERSION "1.0"

#define DHS_DEFAULT_SERVER "members.dhs.org"
#define DHS_DEFAULT_PORT "80"
#define DHS_REQUEST "/nic/hosts"
#define DHS_SUCKY_TIMEOUT 5 //60

#define DNSOMATIC_DEFAULT_SERVER "updates.dnsomatic.com"
#define DNSOMATIC_DEFAULT_PORT "80"
#define DNSOMATIC_REQUEST "/nic/update"

#define DYNDNS_DEFAULT_SERVER "members.dyndns.org"
#define DYNDNS_DEFAULT_PORT "80"
#define DYNDNS_REQUEST "/nic/update"
#define DYNDNS_STAT_REQUEST "/nic/update"
#define DYNDNS_MAX_INTERVAL (25*24*3600)

#define QDNS_DEFAULT_SERVER "members.3322.org"
#define QDNS_DEFAULT_PORT "80"
#define QDNS_REQUEST "/dyndns/update"
#define QDNS_STAT_REQUEST "/dyndns/update"

#define ODS_DEFAULT_SERVER "update.ods.org"
#define ODS_DEFAULT_PORT "7070"
#define ODS_REQUEST "update"

#define TZO_DEFAULT_SERVER "cgi.tzo.com"
#define TZO_DEFAULT_PORT "80"
#define TZO_REQUEST "/webclient/signedon.html"

#define GNUDIP_DEFAULT_SERVER ""
#define GNUDIP_DEFAULT_PORT "3495"
#define GNUDIP_REQUEST "0"

#define EASYDNS_DEFAULT_SERVER "members.easydns.com"
#define EASYDNS_DEFAULT_PORT "80"
#define EASYDNS_REQUEST "/dyn/ez-ipupdate.php"

#define EASYDNS_PARTNER_DEFAULT_SERVER "api.easydns.com"
#define EASYDNS_PARTNER_DEFAULT_PORT "80"
#define EASYDNS_PARTNER_REQUEST "/dyn/ez-ipupdate.php"

#define JUSTL_DEFAULT_SERVER "www.justlinux.com"
#define JUSTL_DEFAULT_PORT "80"
#define JUSTL_REQUEST "/bin/controlpanel/dyndns/jlc.pl"
#define JUSTL_VERSION "2.0"

#define DYNS_DEFAULT_SERVER "www.dyns.cx"
#define DYNS_DEFAULT_PORT "80"
#define DYNS_REQUEST "/postscript.php"

#define HN_DEFAULT_SERVER "dup.hn.org"
#define HN_DEFAULT_PORT "80"
#define HN_REQUEST "/vanity/update"

#define ZONEEDIT_DEFAULT_SERVER "dynamic.zoneedit.com"
#define ZONEEDIT_DEFAULT_PORT "80"
#define ZONEEDIT_REQUEST "/auth/dynamic.html"

#define HEIPV6TB_DEFAULT_SERVER "ipv4.tunnelbroker.net"
#define HEIPV6TB_DEFAULT_PORT "80"
#define HEIPV6TB_REQUEST "/ipv4_end.php"

//Andy Chiu, 2015/04/02, add for SelfHost.de
#define SELFHOST_DEFAULT_SERVER	"carol.selfhost.de"
#define SELFHOST_DEFAULT_PORT	"80"
#define SELFHOST_REQUEST	"/nic/update"

#define DEFAULT_TIMEOUT 15 //120
#define DEFAULT_UPDATE_PERIOD 120
#define DEFAULT_RESOLV_PERIOD 30

#include "namecheap.h"

#ifdef DEBUG
#  define BUFFER_SIZE (16*1024)
#else
#  define BUFFER_SIZE (4*1024-1)
#endif

#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <netinet/in.h>
#if HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
#if HAVE_ERRNO_H
#  include <errno.h>
#endif
#include <netdb.h>
#include <sys/socket.h>
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SIGNAL_H
#  include <signal.h>
#endif
#if HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#if HAVE_SYSLOG_H
#  include <syslog.h>
#endif
#if HAVE_STDARG_H
#  include <stdarg.h>
#endif
#include <error.h>
#if HAVE_PWD_H && HAVE_GRP_H
#  include <pwd.h>
#  include <grp.h>
#endif
#if defined(HAVE_FORK) && !defined(HAVE_VFORK)
#  define vfork fork
#endif
#if USE_MD5
#  include <md5.h>
#  define MD5_DIGEST_BYTES (16)
#endif


#if __linux__ || __SVR4 || __OpenBSD__ || __FreeBSD__ || __NetBSD__
#  define IF_LOOKUP 1
#  include <sys/ioctl.h>
#  include <net/if.h>
#  ifdef HAVE_SYS_SOCKIO_H
#    include <sys/sockio.h>
#  endif
#endif

#include <dprintf.h>
#include <conf_file.h>
#include <cache_file.h>
#include <pid_file.h>

#if !defined(__GNUC__) && !defined(HAVE_SNPRINTF)
#error "get gcc, fix this code, or find yourself a snprintf!"
#else
#  if HAVE_SNPRINTF
#    define  snprintf(x, y, z...) snprintf(x, y, ## z)
#  else
#    define  snprintf(x, y, z...) sprintf(x, ## z)
#  endif
#endif
#if HAVE_VSNPRINTF
#  define  vsnprintf(x, y, z...) vsnprintf(x, y, ## z)
#else
#  define  vsnprintf(x, y, z...) vsprintf(x, ## z)
#endif

#ifndef HAVE_HERROR
#  define herror(x) show_message("%s: error\n", x)
#endif

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))
#define N_STR(x) (x == NULL ? "(null)" : x)

#ifndef OS
#  define OS "unknown"
#endif

// the min period for checking the interface
#define MIN_UPDATE_PERIOD 10
// the min/max time to wait if we fail to update
#define MIN_WAIT_PERIOD 300
#define MAX_WAIT_PERIOD (2*3600)
// the min time that max-period can be set to
#define MIN_MAXINTERVAL (24*3600)
// the max time we will wait if the server tells us to
#define MAX_WAITRESPONSE_WAIT (24*3600)
#define MAX_MESSAGE_LEN 256
#define ARGLENGTH 32
#define BLOCK_FILE "/var/run/ez-ipupdate.block"
#define PID_FILE "/var/run/ez-ipupdate.pid"

/**************************************************/

struct service_t
{
  char *title;
  char *names[3];
  void (*init)(void);
  int (*update_entry)(void);
  int (*check_info)(void);
  char **fields_used;
  char *default_server;
  char *default_port;
  char *default_request;
};

enum {
  UPDATERES_OK = 0,
  UPDATERES_ERROR,
  UPDATERES_SHUTDOWN,
  UPDATERES_AUTHFAIL,
};

/**************************************************/

char *program_name = NULL;
char *cache_file = NULL;
char *config_file = NULL;
char *server = NULL;
char *port = NULL;
char user[256];
char auth[512];
char user_name[128];
char password[128];
char *address = NULL;
char *request = NULL;
char *request_over_ride = NULL;
int wildcard = 0;
char *mx = NULL;
char *url = NULL;
char *host = NULL;
char *cloak_title = NULL;
char *interface = NULL;
int ntrys = 1;
int update_period = DEFAULT_UPDATE_PERIOD;
int resolv_period = DEFAULT_RESOLV_PERIOD;
struct timeval timeout;
int max_interval = 0;
int service_set = 0;
char *post_update_cmd = NULL;
char *post_update_cmd_arg = NULL;
int connection_type = 1;
time_t last_update = 0;
#ifdef SEND_EMAIL_CMD
char *notify_email = NULL;
#endif
char *pid_file = NULL;
char *partner = NULL;
char update_entry_buf[BUFFER_SIZE+1];
char update_entry_putbuf[BUFFER_SIZE+1];

//2007.03.14 Yau add
#ifdef ASUS_DDNS
volatile int client_sockfd;
#else   // !ASUS_DDNS
static volatile int client_sockfd;
#endif  // ASUS_DDNS
static volatile int last_sig = 0;

/* service objects for various services */

// this one is for when people don't configure a default service at build time
int NULL_check_info(void);
static char *NULL_fields_used[] = { NULL };

int EZIP_update_entry(void);
int EZIP_check_info(void);
static char *EZIP_fields_used[] = { "server", "user", "address", "wildcard", "mx", "url", "host", NULL };

int NOIP_update_entry(void);
int NOIP_check_info(void);
static char *NOIP_fields_used[] = { "server", "user", "host", "address", "wildcard", "mx", "offline", NULL };

#ifdef CONFIG_PGP
int PGPOW_update_entry(void);
int PGPOW_check_info(void);
static char *PGPOW_fields_used[] = { "server", "host", NULL };
#endif

int DHS_update_entry(void);
int DHS_check_info(void);
static char *DHS_fields_used[] = { "server", "user", "address", "wildcard", "mx", "url", "host", NULL };

void DYNDNS_init(void);
int DYNDNS_update_entry(void);
int DYNDNS_check_info(void);
static char *DYNDNS_fields_used[] = { "server", "user", "address", "wildcard", "mx", "host", NULL };
static char *DYNDNS_STAT_fields_used[] = { "server", "user", "address", "wildcard", "mx", "host", NULL };

#ifdef CONFIG_ODS
int ODS_update_entry(void);
int ODS_check_info(void);
static char *ODS_fields_used[] = { "server", "host", "address", NULL };
#endif

int TZO_update_entry(void);
int TZO_check_info(void);
static char *TZO_fields_used[] = { "server", "user", "address", "host", "connection-type", NULL };

int EASYDNS_update_entry(void);
int EASYDNS_check_info(void);
static char *EASYDNS_fields_used[] = { "server", "user", "address", "wildcard", "mx", "host", NULL };

int EASYDNS_PARTNER_update_entry(void);
int EASYDNS_PARTNER_check_info(void);
static char *EASYDNS_PARTNER_fields_used[] = { "server", "partner", "user", "address", "wildcard", "host", NULL };

#ifdef USE_MD5
#ifdef GNUDIP
int GNUDIP_update_entry(void);
int GNUDIP_check_info(void);
static char *GNUDIP_fields_used[] = { "server", "user", "host", "address", NULL };
#endif
#endif

#ifdef CONFIG_PGP
int JUSTL_update_entry(void);
int JUSTL_check_info(void);
static char *JUSTL_fields_used[] = { "server", "user", "host", NULL };
#endif

int DYNS_update_entry(void);
int DYNS_check_info(void);
static char *DYNS_fields_used[] = { "server", "user", "host", NULL };

#ifdef CONFIG_HN
int HN_update_entry(void);
int HN_check_info(void);
static char *HN_fields_used[] = { "server", "user", "address", NULL };
#endif

int ZONEEDIT_update_entry(void);
int ZONEEDIT_check_info(void);
static char *ZONEEDIT_fields_used[] = { "server", "user", "address", "mx", "host", NULL };

#ifdef USE_MD5
int HEIPV6TB_update_entry(void);
int HEIPV6TB_check_info(void);
static char *HEIPV6TB_fields_used[] = { "server", "user", "address", "host", NULL };
#endif

struct service_t services[] = {
  { "NULL",
    { "null", "NULL", 0, },
    NULL,
    NULL,
    NULL_check_info,
    NULL_fields_used,
    "",
    "",
    ""
  },
  { "ez-ip",
    { "ezip", "ez-ip", 0, },
    NULL,
    EZIP_update_entry,
    EZIP_check_info,
    EZIP_fields_used,
    EZIP_DEFAULT_SERVER,
    EZIP_DEFAULT_PORT,
    EZIP_REQUEST
  },
  { "no-ip",
    { "noip", "no-ip", 0, },
    NULL,
    NOIP_update_entry,
    NOIP_check_info,
    NOIP_fields_used,
    NOIP_DEFAULT_SERVER,
    NOIP_DEFAULT_PORT,
    NOIP_REQUEST
  },
#ifdef CONFIG_PGP
  { "justlinux v1.0 (penguinpowered)",
    { "pgpow", "penguinpowered", 0, },
    NULL,
    PGPOW_update_entry,
    PGPOW_check_info,
    PGPOW_fields_used,
    PGPOW_DEFAULT_SERVER,
    PGPOW_DEFAULT_PORT,
    PGPOW_REQUEST
  },
#endif
#ifdef CONFIG_DHS
  { "dhs",
    { "dhs", 0, 0, },
    NULL,
    DHS_update_entry,
    DHS_check_info,
    DHS_fields_used,
    DHS_DEFAULT_SERVER,
    DHS_DEFAULT_PORT,
    DHS_REQUEST
  },
#endif
  { "dyndns",
    { "dyndns", 0, 0, },
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_fields_used,
    DYNDNS_DEFAULT_SERVER,
    DYNDNS_DEFAULT_PORT,
    DYNDNS_REQUEST
  },
  { "dyndns-static",
    { "dyndns-static", "dyndns-stat", "statdns", },
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_STAT_fields_used,
    DYNDNS_DEFAULT_SERVER,
    DYNDNS_DEFAULT_PORT,
    DYNDNS_STAT_REQUEST
  },
  { "dyndns-custom",
    { "dyndns-custom", "mydyndns", 0 },
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_STAT_fields_used,
    DYNDNS_DEFAULT_SERVER,
    DYNDNS_DEFAULT_PORT,
    DYNDNS_REQUEST
  },
  { "qdns",
    {  "qdns dynamic" },
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_fields_used,
    QDNS_DEFAULT_SERVER,
    QDNS_DEFAULT_PORT,
    QDNS_REQUEST
  },
  //Andy Chiu, 2015/04/02, add for SelfHost.de
  { "selfhost",
    {  "selfhost", 0, 0 },
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_fields_used,
    SELFHOST_DEFAULT_SERVER,
    SELFHOST_DEFAULT_PORT,
    SELFHOST_REQUEST
  },
  { "qdns-static",
    {"qdns-static"},
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_STAT_fields_used,
    QDNS_DEFAULT_SERVER,
    QDNS_DEFAULT_PORT,
    QDNS_STAT_REQUEST
  },
#ifdef CONFIG_ODS
  { "ods",
    { "ods", 0, 0, },
    NULL,
    ODS_update_entry,
    ODS_check_info,
    ODS_fields_used,
    ODS_DEFAULT_SERVER,
    ODS_DEFAULT_PORT,
    ODS_REQUEST
  },
#endif
  { "tzo",
    { "tzo", 0, 0, },
    NULL,
    TZO_update_entry,
    TZO_check_info,
    TZO_fields_used,
    TZO_DEFAULT_SERVER,
    TZO_DEFAULT_PORT,
    TZO_REQUEST
  },
  { "easydns",
    { "easydns", 0, 0, },
    NULL,
    EASYDNS_update_entry,
    EASYDNS_check_info,
    EASYDNS_fields_used,
    EASYDNS_DEFAULT_SERVER,
    EASYDNS_DEFAULT_PORT,
    EASYDNS_REQUEST
  },
  { "easydns-partner",
    { "easydns-partner", 0, 0, },
    NULL,
    EASYDNS_PARTNER_update_entry,
    EASYDNS_PARTNER_check_info,
    EASYDNS_PARTNER_fields_used,
    EASYDNS_PARTNER_DEFAULT_SERVER,
    EASYDNS_PARTNER_DEFAULT_PORT,
    EASYDNS_PARTNER_REQUEST
  },
#ifdef USE_MD5
#ifdef GNUDIP
  { "gnudip",
    { "gnudip", 0, 0, },
    NULL,
    GNUDIP_update_entry,
    GNUDIP_check_info,
    GNUDIP_fields_used,
    GNUDIP_DEFAULT_SERVER,
    GNUDIP_DEFAULT_PORT,
    GNUDIP_REQUEST
  },
#endif
#endif

#ifdef CONFIG_PGP
  { "justlinux v2.0 (penguinpowered)",
    { "justlinux", 0, 0, },
    NULL,
    JUSTL_update_entry,
    JUSTL_check_info,
    JUSTL_fields_used,
    JUSTL_DEFAULT_SERVER,
    JUSTL_DEFAULT_PORT,
    JUSTL_REQUEST
  },
#endif
  { "dyns",
    { "dyns", 0, 0, },
    NULL,
    DYNS_update_entry,
    DYNS_check_info,
    DYNS_fields_used,
    DYNS_DEFAULT_SERVER,
    DYNS_DEFAULT_PORT,
    DYNS_REQUEST
  },
#ifdef CONFIG_HN
  { "hammer node",
    { "hn", 0, 0, },
    NULL,
    HN_update_entry,
    HN_check_info,
    HN_fields_used,
    HN_DEFAULT_SERVER,
    HN_DEFAULT_PORT,
    HN_REQUEST
  },
#endif
  { "zoneedit",
    { "zoneedit", 0, 0, },
    NULL,
    ZONEEDIT_update_entry,
    ZONEEDIT_check_info,
    ZONEEDIT_fields_used,
    ZONEEDIT_DEFAULT_SERVER,
    ZONEEDIT_DEFAULT_PORT,
    ZONEEDIT_REQUEST
  },
#ifdef USE_MD5
  { "heipv6tb",
    { "heipv6tb", 0, 0, },
    NULL,
    HEIPV6TB_update_entry,
    HEIPV6TB_check_info,
    HEIPV6TB_fields_used,
    HEIPV6TB_DEFAULT_SERVER,
    HEIPV6TB_DEFAULT_PORT,
    HEIPV6TB_REQUEST
  },
#endif
  { "dnsomatic",
    { "dnsomatic", 0, 0, },
    DYNDNS_init,
    DYNDNS_update_entry,
    DYNDNS_check_info,
    DYNDNS_fields_used,
    DNSOMATIC_DEFAULT_SERVER,
    DNSOMATIC_DEFAULT_PORT,
    DNSOMATIC_REQUEST
  },
  { "namecheap",
    { "namecheap", 0, 0, },
    NULL,
    NC_update_entry,
    NC_check_info,
    NC_fields_used,
    NC_DEFAULT_SERVER,
    NC_DEFAULT_PORT,
    NC_REQUEST
  },
};

struct service_t *service = NULL;

int options;

#define OPT_DEBUG       0x0001
#define OPT_DAEMON      0x0004
#define OPT_QUIET       0x0008
#define OPT_FOREGROUND  0x0010
#define OPT_OFFLINE     0x0020
#define OPT_ONCE		0x0040

enum { 
  CMD__start = 1,
  CMD_service_type,
  CMD_server,
  CMD_request,
  CMD_user,
  CMD_address,
  CMD_wildcard,
  CMD_mx,
  CMD_max_interval,
  CMD_url,
  CMD_host,
  CMD_cloak_title,
  CMD_interface,
  CMD_retrys,
  CMD_resolv_period,
  CMD_period,
  CMD_daemon,
  CMD_debug,
  CMD_execute,
  CMD_foreground,
  CMD_quiet,
  CMD_timeout,
  CMD_run_as_user,
  CMD_run_as_euser,
  CMD_connection_type,
  CMD_cache_file,
#ifdef SEND_EMAIL_CMD
  CMD_notify_email,
#endif
  CMD_pid_file,
  CMD_offline,
  CMD_partner,
  CMD_once,
//2007.03.14 Yau add
#ifdef ASUS_DDNS
  CMD_asus,
#endif  // ASUS_DDNS
  CMD__end
};

int conf_handler(struct conf_cmd *cmd, char *arg);
static struct conf_cmd conf_commands[] = {
  { CMD_address,         "address",         CONF_NEED_ARG, 1, conf_handler, "%s=<ip address>" },
  { CMD_cache_file,      "cache-file",      CONF_NEED_ARG, 1, conf_handler, "%s=<cache file>" },
  { CMD_cloak_title,     "cloak-title",     CONF_NEED_ARG, 1, conf_handler, "%s=<title>" },
  { CMD_daemon,          "daemon",          CONF_NO_ARG,   1, conf_handler, "%s=<command>" },
  { CMD_execute,         "execute",         CONF_NEED_ARG, 1, conf_handler, "%s=<shell command>" },
  { CMD_debug,           "debug",           CONF_NO_ARG,   1, conf_handler, "%s" },
  { CMD_foreground,      "foreground",      CONF_NO_ARG,   1, conf_handler, "%s" },
  { CMD_pid_file,        "pid-file",        CONF_NEED_ARG, 1, conf_handler, "%s=<file>" },
  { CMD_host,            "host",            CONF_NEED_ARG, 1, conf_handler, "%s=<host>" },
  { CMD_interface,       "interface",       CONF_NEED_ARG, 1, conf_handler, "%s=<interface>" },
  { CMD_mx,              "mx",              CONF_NEED_ARG, 1, conf_handler, "%s=<mail exchanger>" },
  { CMD_max_interval,    "max-interval",    CONF_NEED_ARG, 1, conf_handler, "%s=<number of seconds between updates>" },
#ifdef SEND_EMAIL_CMD
  { CMD_notify_email,    "notify-email",    CONF_NEED_ARG, 1, conf_handler, "%s=<address to email if bad things happen>" },
#endif
  { CMD_offline,         "offline",         CONF_NO_ARG,   1, conf_handler, "%s" },
  { CMD_retrys,          "retrys",          CONF_NEED_ARG, 1, conf_handler, "%s=<number of trys>" },
  { CMD_server,          "server",          CONF_NEED_ARG, 1, conf_handler, "%s=<server name>" },
  { CMD_service_type,    "service-type",    CONF_NEED_ARG, 1, conf_handler, "%s=<service type>" },
  { CMD_timeout,         "timeout",         CONF_NEED_ARG, 1, conf_handler, "%s=<sec.millisec>" },
  { CMD_resolv_period,   "resolv-period",   CONF_NEED_ARG, 1, conf_handler, "%s=<time between failed resolve attempts>" },
  { CMD_period,          "period",          CONF_NEED_ARG, 1, conf_handler, "%s=<time between update attempts>" },
  { CMD_url,             "url",             CONF_NEED_ARG, 1, conf_handler, "%s=<url>" },
  { CMD_user,            "user",            CONF_NEED_ARG, 1, conf_handler, "%s=<user name>[:password]" },
  { CMD_run_as_user,     "run-as-user",     CONF_NEED_ARG, 1, conf_handler, "%s=<user>" },
  { CMD_run_as_euser,    "run-as-euser",    CONF_NEED_ARG, 1, conf_handler, "%s=<user> (this is not secure)" },
  { CMD_wildcard,        "wildcard",        CONF_NO_ARG,   1, conf_handler, "%s" },
  { CMD_quiet,           "quiet",           CONF_NO_ARG,   1, conf_handler, "%s" },
  { CMD_connection_type, "connection-type", CONF_NEED_ARG, 1, conf_handler, "%s=<connection type>" },
  { CMD_request,         "request",         CONF_NEED_ARG, 1, conf_handler, "%s=<request uri>" },
  { CMD_partner,         "partner",         CONF_NEED_ARG, 1, conf_handler, "%s=<easydns partner>" },
  { 0, 0, 0, 0, 0 }
};

/**************************************************/

void print_usage( void );
void print_version( void );
void parse_args( int argc, char **argv );
int do_connect(int *sock, char *host, char *port);
void base64Encode(char *intext, char *output);
int main( int argc, char **argv );
void warn_fields(char **okay_fields);
static int is_in_list(char *needle, char **haystack);

/**************************************************/

void print_usage( void )
{
  int i;
  int width;

  fprintf(stdout, "usage: ");
  fprintf(stdout, "%s [options] \n\n", program_name);
  fprintf(stdout, " Options are:\n");
  fprintf(stdout, "  -a, --address <ip address>\tstring to send as your ip address\n");
  fprintf(stdout, "  -b, --cache-file <file>\tfile to use for caching the ipaddress\n");
  fprintf(stdout, "  -c, --config-file <file>\tconfiguration file, almost all arguments can be\n");
  fprintf(stdout, "\t\t\t\tgiven with: <name>[=<value>]\n\t\t\t\tto see a list of possible config commands\n");
  fprintf(stdout, "\t\t\t\ttry \"echo help | %s -c -\"\n", program_name);
  fprintf(stdout, "  -d, --daemon\t\t\trun as a daemon periodicly updating if \n\t\t\t\tnecessary\n");
#ifdef DEBUG
  fprintf(stdout, "  -D, --debug\t\t\tturn on debuggin\n");
#endif
  fprintf(stdout, "  -e, --execute <command>\tshell command to execute after a successful\n\t\t\t\tupdate\n");
  fprintf(stdout, "  -f, --foreground\t\twhen running as a daemon run in the foreground\n");
  fprintf(stdout, "  -F, --pidfile <file>\t\tuse <file> as a pid file\n");
  fprintf(stdout, "  -g, --request-uri <uri>\tURI to send updates to\n");
  fprintf(stdout, "  -h, --host <host>\t\tstring to send as host parameter\n");
  fprintf(stdout, "  -i, --interface <iface>\twhich interface to use\n");
  fprintf(stdout, "  -L, --cloak_title <host>\tsome stupid thing for DHS only\n");
  fprintf(stdout, "  -m, --mx <mail exchange>\tstring to send as your mail exchange\n");
  fprintf(stdout, "  -M, --max-interval <# of sec>\tmax time in between updates\n");
#ifdef SEND_EMAIL_CMD
  fprintf(stdout, "  -N, --notify-email <email>\taddress to send mail to if bad things happen\n");
#endif
  fprintf(stdout, "  -o, --offline\t\t\tset to off line mode\n");
  fprintf(stdout, "  -p, --resolv-period <sec>\tperiod to check IP if it can't be resolved\n");
  fprintf(stdout, "  -P, --period <# of sec>\tperiod to check IP in daemon \n\t\t\t\tmode (default: 1800 seconds)\n");
  fprintf(stdout, "  -q, --quiet \t\t\tbe quiet\n");
  fprintf(stdout, "  -r, --retrys <num>\t\tnumber of trys (default: 1)\n");
  fprintf(stdout, "  -R, --run-as-user <user>\tchange to <user> for running, be ware\n\t\t\t\tthat this can cause problems with handeling\n\t\t\t\tSIGHUP properly if that user can't read the\n\t\t\t\tconfig file. also it can't write it's pid file \n\t\t\t\tto a root directory\n");
  fprintf(stdout, "  -Q, --run-as-euser <user>\tchange to effective <user> for running, \n\t\t\t\tthis is NOT secure but it does solve the \n\t\t\t\tproblems with run-as-user and config files and \n\t\t\t\tpid files.\n");
  fprintf(stdout, "  -s, --server <server[:port]>\tthe server to connect to\n");
  fprintf(stdout, "  -S, --service-type <server>\tthe type of service that you are using\n");
  width = fprintf(stdout, "\t\t\t\ttry one of: ") + 4*7;
  for(i=0; i<ARRAY_LEN(services); i++)
  {
    if(width > 60) { width = fprintf(stdout, "\n\t\t\t\t") -1 + 4*7; }
    width += fprintf(stdout, "%s ", services[i].names[0]);
  }
  fprintf(stdout, "\n");
  fprintf(stdout, "  -t, --timeout <sec.millisec>\tthe amount of time to wait on I/O\n");
  fprintf(stdout, "  -T, --connection-type <num>\tnumber sent to TZO as your connection \n\t\t\t\ttype (default: 1)\n");
  fprintf(stdout, "  -U, --url <url>\t\tstring to send as the url parameter\n");
  fprintf(stdout, "  -u, --user <user[:passwd]>\tuser ID and password, if either is left blank \n\t\t\t\tthey will be prompted for\n");
  fprintf(stdout, "  -w, --wildcard\t\tset your domain to have a wildcard alias\n");
  fprintf(stdout, "  -z, --partner <partner>\tspecify easyDNS partner (for easydns-partner \n\t\t\t\tservices)\n");
  fprintf(stdout, "  -1, --once\tin daemon mode, update once then exit\n");
  fprintf(stdout, "      --help\t\t\tdisplay this help and exit\n");
  fprintf(stdout, "      --version\t\t\toutput version information and exit\n");
  fprintf(stdout, "      --credits\t\t\tprint the credits and exit\n");
#ifndef EMBED
  fprintf(stdout, "      --signalhelp\t\tprint help about signals\n");
#endif
  fprintf(stdout, "\n");
}

void print_version( void )
{
  fprintf(stdout, "%s: - %s - $Id: ez-ipupdate.c,v 1.1.1.1 2008/07/21 09:17:41 james26_jang Exp $\n", program_name, VERSION);
}

void print_credits( void )
{
  fprintf( stdout, "AUTHORS / CONTRIBUTORS\n"
      "  Angus Mackay <amackay@gusnet.cx>\n"
      "  Jeremy Bopp <jbopp@mail.utexas.edu>\n"
      "  Mark Jeftovic <markjr@easydns.com>\n"
      "  Stefaan Ponnet <webmaster@dyns.cx>\n"
      "  Colin Viebrock <colin@easydns.com>\n"
      "  Tim Brown <timb@machine.org.uk>\n"
      "\n" );
}

#ifndef EMBED
void print_signalhelp( void )
{
  fprintf(stdout, "\nsignals are only really used when in daemon mode.\n\n");
  fprintf(stdout, "signals: \n");
  fprintf(stdout, "  HUP\t\tcauses it to re-read its config file\n");
  fprintf(stdout, "  TERM\t\twake up and possibly perform an update\n");
  fprintf(stdout, "  QUIT\t\tshutdown\n");
  fprintf(stdout, "\n");
}
#endif

#if HAVE_SIGNAL_H
RETSIGTYPE sigint_handler(int sig)
{
  char message[] = "interupted.\n";
  close(client_sockfd);
  write(2, message, sizeof(message)-1);

#if HAVE_GETPID
  if(pid_file)
  {
    pid_file_delete(pid_file);
  }
#endif

  exit(1);
}
RETSIGTYPE generic_sig_handler(int sig)
{
  last_sig = sig;
}
#endif

int get_duration(char *str)
{
  char *multchar;
  int mult;
  char save;
  int duration;

  for(multchar=str; *multchar != '\0'; multchar++);
  if(multchar != str) { multchar--; }
  if(*multchar == '\0' || isdigit(*multchar)) { mult = 1; multchar++; }
  else if(*multchar == 'M') { mult = 60; }
  else if(*multchar == 'H') { mult = 60*60; }
  else if(*multchar == 'd') { mult = 60*60*24; }
  else if(*multchar == 'w') { mult = 60*60*24*7; }
  else if(*multchar == 'f') { mult = 60*60*24*7*2; }
  else if(*multchar == 'm') { mult = 60*60*24*30; }
  else if(*multchar == 'y') { mult = 60*60*24*365; }
  else
  {
    show_message("invalid multiplier: %c\n", *multchar);
#ifndef EMBED
    fprintf(stderr, "valid multipliers:\n");
    fprintf(stderr, "  %c -> %s (%d)\n", 'M', "Minute",    60);
    fprintf(stderr, "  %c -> %s (%d)\n", 'H', "Hour",      60*60);
    fprintf(stderr, "  %c -> %s (%d)\n", 'd', "day",       60*60*24);
    fprintf(stderr, "  %c -> %s (%d)\n", 'w', "week",      60*60*24*7);
    fprintf(stderr, "  %c -> %s (%d)\n", 'f', "fortnight", 60*60*24*7*2);
    fprintf(stderr, "  %c -> %s (%d)\n", 'm', "month",     60*60*24*30);
    fprintf(stderr, "  %c -> %s (%d)\n", 'y', "year",      60*60*24*365);
#else
	fprintf(stderr, "valid multipliers: MHdwfmy\n");
#endif
    exit(1);
  }
  save = *multchar;
  *multchar = '\0';
  duration = strtol(str, NULL, 0) * mult;
  *multchar = save;

  return(duration);
}

const char* format_time(int seconds)
{
  static char buf[16];

  snprintf(buf, sizeof(buf), "%d:%02d:%02d", seconds/(3600),
          (seconds%(3600))/(60), (seconds%60));
  return(buf);
}

/*
 * like "chomp" in perl, take off trailing newline chars
 */
char *chomp(char *buf)
{
  char *p;

  for(p=buf; *p != '\0'; p++);
  if(p != buf) { p--; }
  while(p>=buf && (*p == '\n' || *p == '\r'))
  {
    *p-- = '\0';
  }

  return(buf);
}

void get_input(char *prompt, char *buf, int buflen)
{
  printf("%s: ", prompt);
  fflush(stdout);
  *buf = '\0';
  fgets(buf, buflen-1, stdin);
  chomp(buf);
}

/*
 * show_message
 *
 * if we are running in daemon mode then log to syslog, if not just output to
 * stderr.
 *
 */
#ifdef REMOVE
void show_message(char *fmt, ...)
{
  va_list args; 
  va_start(args, fmt);

  if((options & OPT_DAEMON) && !(options & OPT_FOREGROUND))
  {
    char buf[MAX_MESSAGE_LEN];

#if defined(HAVE_VSPRINTF) || defined(HAVE_VSNPRINTF)
    vsnprintf(buf, sizeof(buf), fmt, args);
#else
    sprintf(buf, "message incomplete because your OS sucks: %s\n", fmt);
#endif

    syslog(LOG_NOTICE, "%s", buf);
  }
  else
  {
#ifdef HAVE_VFPRINTF
    vfprintf(stderr, fmt, args);
#else
    fprintf(stderr, "message incomplete because your OS sucks: %s\n", fmt);
#endif
  }

  va_end(args);
}
#else
/*
 * show_message
 *
 */
void show_message(char *fmt, ...)
{
  va_list args;
  char buf[512];

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  openlog("ddns_update", 0, 0);
  syslog(0, "%s", buf);
  closelog();
  va_end(args);
}
#endif

/*
 * returns true if the string passed in is an internet address in dotted quad
 * notation.
 */
int is_dotted_quad(char *addr)
{
  int q[4];
  char *p;
  int i;

  if(sscanf(addr, "%d.%d.%d.%d", &(q[0]), &(q[1]), &(q[2]), &(q[3])) != 4)
  {
    return(0);
  }
  
  if(q[0] > 255 || q[0] < 0 ||
      q[1] > 255 || q[1] < 0 ||
      q[2] > 255 || q[2] < 0 ||
      q[3] > 255 || q[3] < 0)
  {
    return(0);
  }

  /* we know there are 3 dots */
  p = addr;
  if(p != NULL) { p = strchr(p, '.'); p++; }
  if(p != NULL) { p = strchr(p, '.'); p++; }
  if(p != NULL) { p = strchr(p, '.'); }
  for(i=0; *p != '\0' && i<4; i++, p++);
  if(*p != '\0')
  {
    return(0);
  }

  return(1);
}

void parse_service(char *str)
{
  int i;
  int width;

  for(i=0; i<ARRAY_LEN(services); i++)
  {
    int j;
    for(j=0; j<ARRAY_LEN(services[i].names) && services[i].names[j] != NULL; j++)
    {
      if(strcmp(services[i].names[j], str) == 0)
      {
        service = &(services[i]);
        return;
      }
    }
  }
  show_message("unknown service type: %s\n", str);
  fprintf(stderr, "try one of: \n");
  width = fprintf(stderr, "  ");
  for(i=0; i<ARRAY_LEN(services); i++)
  {
    if(width > 60) { width = fprintf(stderr, "\n  ") -1; }
    width += fprintf(stderr, "%s ", services[i].names[0]);
  }
  fprintf(stderr, "\n");
  exit(1);
}

int option_handler(int id, char *optarg)
{
#if HAVE_PWD_H && HAVE_GRP_H
  struct passwd *pw;
#endif
  char *tmp;
  int i;

  switch(id)
  {
    case CMD_address:
      if(address) { free(address); }
      address = strdup(optarg);
      dprintf((stderr, "address: %s\n", address));
      break;

//2007.03.14 Yau add
#ifdef ASUS_DDNS
        case CMD_asus:
                i = atoi (optarg);
                if (i >= 0 && i <= 2)   {
                        g_asus_ddns_mode = i;
                        dprintf((stderr, "register domain mode %d\n", g_asus_ddns_mode));
                }
                break;
#endif  // ASUS_DDNS

    case CMD_daemon:
      options |= OPT_DAEMON;
      dprintf((stderr, "daemon mode\n"));
#if HAVE_GETPID
      if(!pid_file) pid_file = strdup(PID_FILE);
#endif
      break;

    case CMD_debug:
#ifdef DEBUG
      options |= OPT_DEBUG;
      dprintf((stderr, "debugging on\n"));
#else
      show_message("debugging was not enabled at compile time\n");
#endif
      break;

    case CMD_execute:
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT)
      if(post_update_cmd) { free(post_update_cmd); }
      post_update_cmd = malloc(strlen(optarg) + 1 + ARGLENGTH + 1);
      post_update_cmd_arg = post_update_cmd + strlen(optarg) + 1;
      sprintf(post_update_cmd, "%s ", optarg);
      dprintf((stderr, "post_update_cmd: %s\n", post_update_cmd));
#else
      show_message("command execution not enabled at compile time\n");
      exit(1);
#endif
      break;

    case CMD_foreground:
      options |= OPT_FOREGROUND;
      dprintf((stderr, "fork()ing off\n"));
      break;

    case CMD_pid_file:
#if HAVE_GETPID
      if(pid_file) { free(pid_file); }
      pid_file = strdup(optarg);
      dprintf((stderr, "pid file: %s\n", pid_file));
#else
      show_message("pid file support not enabled at compile time\n");
#endif
      break;

    case CMD_host:
      if(host) { free(host); }
      host = strdup(optarg);
      dprintf((stderr, "host: %s\n", host));
      break;

    case CMD_interface:
#ifdef IF_LOOKUP
      if(interface) { free(interface); }
      interface = strdup(optarg);
      dprintf((stderr, "interface: %s\n", interface));
#else
      show_message("interface lookup not enabled at compile time\n");
      exit(1);
#endif
      break;

    case CMD_mx:
      if(mx) { free(mx); }
      mx = strdup(optarg);
      dprintf((stderr, "mx: %s\n", mx));
      break;

    case CMD_max_interval:
      max_interval = get_duration(optarg);
      if(max_interval < MIN_MAXINTERVAL)
      {
        show_message("WARNING: max-interval of %d is too short, using %d\n",
            max_interval, MIN_MAXINTERVAL);
        max_interval = MIN_MAXINTERVAL;
      }
      dprintf((stderr, "max_interval: %d\n", max_interval));
      break;

#ifdef SEND_EMAIL_CMD
    case CMD_notify_email:
      if(notify_email) { free(notify_email); }
      notify_email = strdup(optarg);
      dprintf((stderr, "notify_email: %s\n", notify_email));
      break;
#endif

    case CMD_offline:
      options |= OPT_OFFLINE;
      dprintf((stderr, "offline mode\n"));
      break;

    case CMD_period:
      update_period = get_duration(optarg);
      if(update_period < MIN_UPDATE_PERIOD)
      {
        show_message("WARNING: period of %d is too short, using %d\n",
            update_period, MIN_UPDATE_PERIOD);
        update_period = MIN_UPDATE_PERIOD;
      }
      dprintf((stderr, "update_period: %d\n", update_period));
      break;

    case CMD_resolv_period:
      resolv_period = get_duration(optarg);
      if(resolv_period < 1)
      {
        show_message("WARNING: period of %d is too short, using %d\n",
            resolv_period, 1);
        resolv_period = 1;
      }
      dprintf((stderr, "resolv_period: %d\n", resolv_period));
      break;

    case CMD_quiet:
      options |= OPT_QUIET;
      dprintf((stderr, "quiet mode\n"));
      break;

    case CMD_retrys:
      ntrys = atoi(optarg);
      dprintf((stderr, "ntrys: %d\n", ntrys));
      break;

    case CMD_server:
      if(server) { free(server); }
      server = strdup(optarg);
      tmp = strchr(server, ':');
      if(tmp)
      {
        *tmp++ = '\0';
        if(port) { free(port); }
        port = strdup(tmp);
      }
      dprintf((stderr, "server: %s\n", server));
      dprintf((stderr, "port: %s\n", port));
      break;

    case CMD_request:
      if(request_over_ride) { free(request_over_ride); }
      request_over_ride = strdup(optarg);
      dprintf((stderr, "request_over_ride: %s\n", request_over_ride));
      break;

    case CMD_partner:
      if(partner) { free(partner); }
      partner = strdup(optarg);
      dprintf((stderr, "easyDNS partner: %s\n", partner));
      break;

    case CMD_service_type:
      parse_service(optarg);
      service_set = 1;
      dprintf((stderr, "service_type: %s\n", service->title));
      dprintf((stderr, "service->name: %s\n", service->names[0]));
      break;

    case CMD_user:
      strncpy(user, optarg, sizeof(user));
      user[sizeof(user)-1] = '\0';
      dprintf((stderr, "user: %s\n", user));
      tmp = strchr(optarg, ':');
      if(tmp)
      {
        tmp++;
        while(*tmp) { *tmp++ = '*'; }
      }
      break;

    case CMD_run_as_user:
#if HAVE_PWD_H && HAVE_GRP_H
      if((pw=getpwnam(optarg)) == NULL)
      {
        i = atoi(optarg);
      }
      else
      {
        if(setgid(pw->pw_gid) != 0)
        {
          show_message("error changing group id\n");
        }
        dprintf((stderr, "GID now %d\n", pw->pw_gid));
        i = pw->pw_uid;
      }
      if(setuid(i) != 0)
      {
        show_message("error changing user id\n");
      }
      dprintf((stderr, "UID now %d\n", i));
#else
      show_message("option \"daemon-user\" not supported on this system\n");
#endif
      break;

    case CMD_run_as_euser:
#if HAVE_PWD_H && HAVE_GRP_H && HAVE_SETEUID && HAVE_SETEGID
      if((pw=getpwnam(optarg)) == NULL)
      {
        i = atoi(optarg);
      }
      else
      {
        if(setegid(pw->pw_gid) != 0)
        {
          show_message("error changing group id\n");
        }
        dprintf((stderr, "GID now %d\n", pw->pw_gid));
        i = pw->pw_uid;
      }
      if(seteuid(i) != 0)
      {
        show_message("error changing user id\n");
      }
      dprintf((stderr, "UID now %d\n", i));
#else
      show_message("option \"daemon-euser\" not supported on this system\n");
#endif
      break;

    case CMD_url:
      if(url) { free(url); }
      url = strdup(optarg);
      dprintf((stderr, "url: %s\n", url));
      break;

    case CMD_wildcard:
      wildcard = 1;
      dprintf((stderr, "wildcard: %d\n", wildcard));
      break;

    case CMD_cloak_title:
      if(cloak_title) { free(cloak_title); }
      cloak_title = strdup(optarg);
      dprintf((stderr, "cloak_title: %s\n", cloak_title));
      break;

    case CMD_timeout:
      timeout.tv_sec = atoi(optarg);
      timeout.tv_usec = (atof(optarg) - timeout.tv_sec) * 1000000L;
      dprintf((stderr, "timeout: %ld.%06ld\n", timeout.tv_sec, timeout.tv_usec));
      break;

    case CMD_connection_type:
      connection_type = atoi(optarg);
      dprintf((stderr, "connection_type: %d\n", connection_type));
      break;

    case CMD_cache_file:
      if(cache_file) { free(cache_file); }
      cache_file = strdup(optarg);
      dprintf((stderr, "cache_file: %s\n", cache_file));
      break;

    case CMD_once:
      options |= OPT_ONCE;
      dprintf((stderr, "update once only\n"));
      break;

    default:
      dprintf((stderr, "case not handled: %d\n", id));
      break;
  }

  return 0;
}

int conf_handler(struct conf_cmd *cmd, char *arg)
{
  return(option_handler(cmd->id, arg));
}


#ifdef HAVE_GETOPT_LONG
#  define xgetopt( x1, x2, x3, x4, x5 ) getopt_long( x1, x2, x3, x4, x5 )
#else
#  define xgetopt( x1, x2, x3, x4, x5 ) getopt( x1, x2, x3 )
#endif

void parse_args( int argc, char **argv )
{
#ifdef HAVE_GETOPT_LONG
  struct option long_options[] = {
//2007.03.14 Yau add
#ifdef ASUS_DDNS
      {"asus",                required_argument,      0, 'A'},
#endif  // ASUS_DDNS
      {"address",         required_argument,      0, 'a'},
      {"cache-file",      required_argument,      0, 'b'},
      {"config_file",     required_argument,      0, 'c'},
      {"config-file",     required_argument,      0, 'c'},
      {"daemon",          no_argument,            0, 'd'},
      {"debug",           no_argument,            0, 'D'},
      {"execute",         required_argument,      0, 'e'},
      {"foreground",      no_argument,            0, 'f'},
      {"pid-file",        required_argument,      0, 'F'},
      {"host",            required_argument,      0, 'h'},
      {"interface",       required_argument,      0, 'i'},
      {"cloak_title",     required_argument,      0, 'L'},
      {"mx",              required_argument,      0, 'm'},
      {"max-interval",    required_argument,      0, 'M'},
#ifdef SEND_EMAIL_CMD
      {"notify-email",    required_argument,      0, 'N'},
#endif
      {"resolv-period",   required_argument,      0, 'p'},
      {"period",          required_argument,      0, 'P'},
      {"quiet",           no_argument,            0, 'q'},
      {"retrys",          required_argument,      0, 'r'},
      {"run-as-user",     required_argument,      0, 'R'},
      {"run-as-euser",    required_argument,      0, 'Q'},
      {"server",          required_argument,      0, 's'},
      {"service-type",    required_argument,      0, 'S'},
      {"timeout",         required_argument,      0, 't'},
      {"connection-type", required_argument,      0, 'T'},
      {"url",             required_argument,      0, 'U'},
      {"user",            required_argument,      0, 'u'},
      {"wildcard",        no_argument,            0, 'w'},
      {"help",            no_argument,            0, 'H'},
      {"version",         no_argument,            0, 'V'},
      {"credits",         no_argument,            0, 'C'},
#ifndef EMBED
      {"signalhelp",      no_argument,            0, 'Z'},
#endif
      {"once",            no_argument,            0, '1'},
      {0,0,0,0}
  };
#else
#  define long_options NULL
#endif
  int opt;

//2007.03.14 Yau add
#ifdef ASUS_DDNS
  while((opt=xgetopt(argc, argv, "A:a:b:c:dDe:fF:h:i:L:m:M:N:o:p:P:qr:R:s:S:t:T:U:u:wHVCZ", long_options, NULL)) != -1)
#else   // !ASUS_DDNS
  while((opt=xgetopt(argc, argv, "a:b:c:dDe:fF:h:i:L:m:M:N:o:p:P:qr:R:s:S:t:T:U:u:wHVCZ", long_options, NULL)) != -1)
#endif  // ASUS_DDNS
  {
    switch (opt)
    {
      case 'a':
        option_handler(CMD_address, optarg);
        break;

//2007.03.14 Yau add
#ifdef ASUS_DDNS
      case 'A':
        option_handler(CMD_asus, optarg);
        break;
#endif  // ASUS_DDNS

      case 'b':
        option_handler(CMD_cache_file, optarg);
        break;

      case 'c':
        if(config_file) { free(config_file); }
        config_file = strdup(optarg);
        dprintf((stderr, "config_file: %s\n", config_file));
        if(config_file)
        {
          if(parse_conf_file(config_file, conf_commands) != 0)
          {
            show_message("error parsing config file \"%s\"\n", config_file);
            exit(1);
          }
        }
        break;

      case 'd':
        option_handler(CMD_daemon, optarg);
        break;

      case 'D':
        option_handler(CMD_debug, optarg);
        break;

      case 'e':
        option_handler(CMD_execute, optarg);
        break;

      case 'f':
        option_handler(CMD_foreground, optarg);
        break;

      case 'F':
        option_handler(CMD_pid_file, optarg);
        break;

      case 'g':
        option_handler(CMD_request, optarg);
        break;

      case 'h':
        option_handler(CMD_host, optarg);
        break;

      case 'i':
        option_handler(CMD_interface, optarg);
        break;

      case 'L':
        option_handler(CMD_cloak_title, optarg);
        break;

      case 'm':
        option_handler(CMD_mx, optarg);
        break;

      case 'M':
        option_handler(CMD_max_interval, optarg);
        break;

#ifdef SEND_EMAIL_CMD
      case 'N':
        option_handler(CMD_notify_email, optarg);
        break;
#endif

      case 'o':
        option_handler(CMD_offline, optarg);
        break;

      case 'p':
        option_handler(CMD_resolv_period, optarg);
        break;

      case 'P':
        option_handler(CMD_period, optarg);
        break;

      case 'q':
        option_handler(CMD_quiet, optarg);
        break;

      case 'Q':
        option_handler(CMD_run_as_euser, optarg);
        break;

      case 'r':
        option_handler(CMD_retrys, optarg);
        break;

      case 'R':
        option_handler(CMD_run_as_user, optarg);
        break;

      case 's':
        option_handler(CMD_server, optarg);
        break;

      case 'S':
        option_handler(CMD_service_type, optarg);
        break;

      case 't':
        option_handler(CMD_timeout, optarg);
        break;

      case 'T':
        option_handler(CMD_connection_type, optarg);
        break;

      case 'u':
        option_handler(CMD_user, optarg);
        break;

      case 'U':
        option_handler(CMD_url, optarg);
        break;

      case 'w':
        option_handler(CMD_wildcard, optarg);
        break;

      case 'H':
        print_usage();
        exit(0);
        break;

      case 'V':
        print_version();
        exit(0);
        break;

      case 'C':
        print_credits();
        exit(0);
        break;

#ifndef EMBED
      case 'Z':
        print_signalhelp();
        exit(0);
        break;
#endif

      case 'z':
        option_handler(CMD_partner, optarg);
        break;

      case '1':
        option_handler(CMD_once, optarg);
        break;

      default:
#ifdef HAVE_GETOPT_LONG
        fprintf(stderr, "Try `%s --help' for more information\n", argv[0]);
#else
        fprintf(stderr, "Try `%s -H' for more information\n", argv[0]);
        fprintf(stderr, "warning: this program was compilied without getopt_long\n");
        fprintf(stderr, "         as such all long options will not work!\n");
#endif
        exit(1);
        break;
    }
  }
}

/*
 * do_connect
 *
 * connect a socket and return the file descriptor
 *
 */
int do_connect(int *sock, char *host, char *port)
{
  struct sockaddr_in address;
  int len;
  int result;
  struct hostent *hostinfo;
  struct servent *servinfo;

  // set up the socket
  if((*sock=socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    if(!(options & OPT_QUIET))
    {
      perror("socket");
    }
    return(-1);
  }
  address.sin_family = AF_INET;

  // get the host address
  hostinfo = gethostbyname(host);
  if(!hostinfo)
  {
    if(!(options & OPT_QUIET))
    {
      herror("gethostbyname");
    }
    close(*sock);
    return(-1);
  }
  address.sin_addr = *(struct in_addr *)*hostinfo -> h_addr_list;

  // get the host port
  servinfo = getservbyname(port, "tcp");
  if(servinfo)
  {
    address.sin_port = servinfo -> s_port;
  }
  else
  {
    address.sin_port = htons(atoi(port));
  }

  // connect the socket
  len = sizeof(address);
  if((result=connect(*sock, (struct sockaddr *)&address, len)) == -1) 
  {
    if(!(options & OPT_QUIET))
    {
      perror("connect");
    }
    close(*sock);
    return(-1);
  }
  // print out some info
  if(!(options & OPT_QUIET))
  {
    show_message(
        "connected to %s (%s) on port %d.\n",
        host,
        inet_ntoa(address.sin_addr),
        ntohs(address.sin_port));
  }

  return 0;
}

static char table64[]=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void base64Encode(char *intext, char *output)
{
  unsigned char ibuf[3];
  unsigned char obuf[4];
  int i;
  int inputparts;

  while(*intext) {
    for (i = inputparts = 0; i < 3; i++) { 
      if(*intext) {
        inputparts++;
        ibuf[i] = *intext;
        intext++;
      }
      else
        ibuf[i] = 0;
    }

    obuf [0] = (ibuf [0] & 0xFC) >> 2;
    obuf [1] = ((ibuf [0] & 0x03) << 4) | ((ibuf [1] & 0xF0) >> 4);
    obuf [2] = ((ibuf [1] & 0x0F) << 2) | ((ibuf [2] & 0xC0) >> 6);
    obuf [3] = ibuf [2] & 0x3F;

    switch(inputparts) {
      case 1: /* only one byte read */
        sprintf(output, "%c%c==", 
            table64[obuf[0]],
            table64[obuf[1]]);
        break;
      case 2: /* two bytes read */
        sprintf(output, "%c%c%c=", 
            table64[obuf[0]],
            table64[obuf[1]],
            table64[obuf[2]]);
        break;
      default:
        sprintf(output, "%c%c%c%c", 
            table64[obuf[0]],
            table64[obuf[1]],
            table64[obuf[2]],
            table64[obuf[3]] );
        break;
    }
    output += 4;
  }
  *output=0;
}

#if IF_LOOKUP 
#  if !defined(HAVE_INET_ATON) 
#    if defined(HAVE_INET_ADDR)
int inet_aton(const char *cp, struct in_addr *inp)
{
  (*inp).s_addr = inet_addr(cp);
}
#  else
#    error "sorry, can't compile with IF_LOOKUP and no inet_aton"
#  endif
#endif
#endif

void output(void *buf)
{
  fd_set writefds;
  int max_fd;
  struct timeval tv;
  int ret;

  dprintf((stderr, "I say: %s\n", (char *)buf));

  // set up our fdset and timeout
  FD_ZERO(&writefds);
  FD_SET(client_sockfd, &writefds);
  max_fd = client_sockfd;
  memcpy(&tv, &timeout, sizeof(struct timeval));

  ret = select(max_fd + 1, NULL, &writefds, NULL, &tv);
  dprintf((stderr, "ret: %d\n", ret));

  if(ret == -1)
  {
    dprintf((stderr, "select: %s\n", error_string));
  }
  else if(ret == 0)
  {
    show_message("output timeout\n");
  }
  else
  {
    /* if we woke up on client_sockfd do the data passing */
    if(FD_ISSET(client_sockfd, &writefds))
    {
      if(send(client_sockfd, buf, strlen(buf), 0) == -1)
      {
        show_message("error send()ing request: %s\n", error_string);
      }
    }
    else
    {
      dprintf((stderr, "error: case not handled."));
    }
  }
}

int read_input(char *buf, int len)
{
  fd_set readfds;
  int max_fd;
  struct timeval tv;
  int ret;
  int bread = -1;

  // set up our fdset and timeout
  FD_ZERO(&readfds);
  FD_SET(client_sockfd, &readfds);
  max_fd = client_sockfd;
  memcpy(&tv, &timeout, sizeof(struct timeval));

  ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
  dprintf((stderr, "read_input ret: %d\n", ret));
  fprintf(stderr, "read_input ret: %d\n", ret);
  if(ret == -1)
  {
    dprintf((stderr, "select: %s\n", error_string));
  }
  else if(ret == 0)
  {
    show_message("Server timeout\n");
    nvram_set ("ddns_return_code", "Time-out");
    nvram_set ("ddns_return_code_chk", "Time-out");
    fprintf(stderr,"read_input: server timeout!\n");
  }
  else
  {
    /* if we woke up on client_sockfd do the data passing */
    if(FD_ISSET(client_sockfd, &readfds))
    {
      bread = recv(client_sockfd, buf, len-1, 0);
      dprintf((stderr, "bread: %d\n", bread));
      buf[bread] = '\0';
      dprintf((stderr, "got: %s\n", buf));
      if(bread == -1)
      {
        show_message("error recv()ing reply: %s\n", error_string);
      }
    }
    else
    {
      dprintf((stderr, "error: case not handled."));
    }
  }

  return(bread);
}

int get_if_addr(int sock, char *name, struct sockaddr_in *sin)
{
#ifdef IF_LOOKUP
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  strcpy(ifr.ifr_name, name);
  ifr.ifr_addr.sa_family = AF_INET;
  if(ioctl(sock, SIOCGIFADDR, &ifr) < 0)
  { 
    perror("ioctl(SIOCGIFADDR)"); 
    memset(sin, 0, sizeof(struct sockaddr_in));
    dprintf((stderr, "%s: %s\n", name, "unknown interface"));
    return -1;
  }

  if(ifr.ifr_addr.sa_family == AF_INET)
  {
    memcpy(sin, &(ifr.ifr_addr), sizeof(struct sockaddr_in));
    dprintf((stderr, "%s: %s\n", name, inet_ntoa(sin->sin_addr)));
    return 0;
  }
  else
  {
    memset(sin, 0, sizeof(struct sockaddr_in));
    dprintf((stderr, "%s: %s\n", name, "could not resolve interface"));
    return -1;
  }
  return -1;
#else
  return -1;
#endif
}

#ifdef CONFIG_PGP
static int PGPOW_read_response(char *buf)
{
  int bytes; 

  bytes = read_input(buf, BUFFER_SIZE);
  if(bytes < 1)
  {
    close(client_sockfd);
    return(-1);
  }
  buf[bytes] = '\0';

  dprintf((stderr, "server says: %s\n", buf));
  
  if(strncmp("OK", buf, 2) != 0)
  {
    return(1);
  }
  else
  {
    return(0);
  }
}
#endif

static int ODS_read_response(char *buf, int len)
{
  int bytes = 0; 
  int bread = 0;
  char* p = buf;
  int max_iter = 32;

  for(; bytes < len && max_iter > 0; max_iter--)
  {
    bread = read_input(p, len-bytes);
    bytes += bread;
    if(bytes < 1)
    {
      close(client_sockfd);
      return(-1);
    }
    if(strstr(buf, "\r\n") > 0)
    {
      break;
    }

    dprintf((stderr, "server says: %s\n", p));
    p += bread;
  }
  dprintf((stderr, "server said: %s\n", buf));
  
  return(atoi(buf));
}

int NULL_check_info(void)
{
  char buf[64];

  if(options & OPT_DAEMON)
  {
    show_message("no compile time default service was set therefor you must "
        "specify a service type.\n");

    return(-1);
  }
  get_input("service", buf, sizeof(buf));
  option_handler(CMD_service_type, buf);

  return(0);
}

int EZIP_check_info(void)
{
  warn_fields(service->fields_used);

  return 0;
}

int EZIP_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?mode=update&", request);
  output(buf);
  if(address)
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "ipaddress", address);
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "wildcard", wildcard ? "yes" : "no");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "mx", mx);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "url", url);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "host", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(!(options & OPT_QUIET))
      {
        show_message("request successful\n");
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}

int NOIP_check_info(void)
{
  char buf[BUFSIZ+1];

  if ((host == NULL) || (*host == '\0'))
  {
    if (options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if (interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int NOIP_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?", request);
  output(buf);

  snprintf(buf, BUFFER_SIZE, "%s=%s", "hostname", host);
  output(buf);

  if (address)
  {
    snprintf(buf, BUFFER_SIZE, "&%s=%s", "myip", address);
    output(buf);
  }

  if (options & OPT_OFFLINE)
  {
    snprintf(buf, BUFFER_SIZE, "&%s=%s", "offline", "yes");
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, "&%s=%s", "wildcard", wildcard ? "yes" : "no");
  output(buf);

  snprintf(buf, BUFFER_SIZE, "&%s=%s", "mx", mx);
  output(buf);

  //snprintf(buf, BUFFER_SIZE, "&%s=%s", "url", url);
  //output(buf);

  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);

  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);

  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);

  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);

  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while ((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }

  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(!(options & OPT_QUIET))
      {
        show_message("request successful\n");
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}

void DYNDNS_init(void)
{

  if(options & OPT_DAEMON)
  {
    if(!(max_interval > 0))
    {
      max_interval = DYNDNS_MAX_INTERVAL;
    }
  }
}

int DYNDNS_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(address != NULL && !is_dotted_quad(address))
  {
    show_message("the IP address \"%s\" is invalid\n", address);
    return(-1);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int DYNDNS_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;
  int retval = UPDATERES_OK; 

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?", request);
  output(buf);

  if(is_in_list("dyndns-static", service->names))
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "system", "statdns");
    output(buf);
  }
  else if(is_in_list("dyndns-custom", service->names))
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "system", "custom");
    output(buf);
  }

  snprintf(buf, BUFFER_SIZE, "%s=%s&", "hostname", host);
  output(buf);
  if(address != NULL)
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "myip", address);
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "wildcard", wildcard ? "ON" : "OFF");
  output(buf);
  if(mx != NULL && *mx != '\0')
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "mx", mx);
    output(buf);
  }
  //snprintf(buf, BUFFER_SIZE, "%s=%s&", "backmx", "NO");
  //output(buf);
  if(options & OPT_OFFLINE)
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "offline", "yes");
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  //show_message("server output: %s\n", buf);//Yaudbg

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      retval = UPDATERES_ERROR;
      break;

    case 200:
      if(strstr(buf, "\ngood ") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
      }
      else
      {
        if(strstr(buf, "\nnohost") != NULL)
        {
          show_message("invalid hostname: %s\n", host);
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\nnotfqdn") != NULL)
        {
          show_message("malformed hostname: %s\n", host);
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\n!yours") != NULL)
        {
          show_message("host \"%s\" is not under your control\n", host);
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\nabuse") != NULL)
        {
          show_message("host \"%s\" has been blocked for abuse\n", host);
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\nnochg") != NULL)
        {
          show_message("%s says that your IP address has not changed since the last update\n", server);
          // lets say that this counts as a successful update
          // but we'll roll back the last update time to max_interval/2
          if(max_interval > 0)
          {
            last_update = time(NULL) - max_interval/2;
          }
          retval = UPDATERES_OK;
        }
        else if(strstr(buf, "\nbadauth") != NULL)
        {
          show_message("authentication failure\n");
          retval = UPDATERES_AUTHFAIL;
        }
        else if(strstr(buf, "\nbadsys") != NULL)
        {
          show_message("invalid system parameter\n");
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\nbadagent") != NULL)
        {
          show_message("this useragent has been blocked\n");
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\nnumhost") != NULL)
        {
          show_message("Too many or too few hosts found\n");
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\ndnserr") != NULL)
        {
          char *p = strstr(buf, "\ndnserr");
          show_message("dyndns internal error, please report this number to "
              "their support people: %s\n", N_STR(p));
          retval = UPDATERES_ERROR;
        }
        else if(strstr(buf, "\n911") != NULL)
        {
          show_message("Ahhhh! call 911!\n");
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\n999") != NULL)
        {
          show_message("Ahhhh! call 999!\n");
          retval = UPDATERES_SHUTDOWN;
        }
        else if(strstr(buf, "\n!donator") != NULL)
        {
          show_message("a feature requested is only available to donators, please donate.\n", host);
          retval = UPDATERES_OK;
        }
        // this one should be last as it is a stupid string to signify waits
        // with as it is so short
        else if(strstr(buf, "\nw") != NULL)
        {
          int howlong = 0;
          char *p = strstr(buf, "\nw");
          char reason[256];
          char mult;

          // get time and reason
          if(strlen(p) >= 2)
          {
            sscanf(p, "%d%c %255[^\r\n]", &howlong, &mult, reason);
            if(mult == 'h')
            {
              howlong *= 3600;
            }
            else if(mult == 'm')
            {
              howlong *= 60;
            }
            if(howlong > MAX_WAITRESPONSE_WAIT)
            {
              howlong = MAX_WAITRESPONSE_WAIT;
            };
          }
          else
          {
            sprintf(reason, "problem parsing reason for wait response");
          }

          show_message("Wait response received, waiting for %s before next update.\n",
              format_time(howlong));
          show_message("Wait response reason: %d\n", N_STR(reason));
          sleep(howlong);
          retval = UPDATERES_ERROR;
        }
        else
        {
          show_message("error processing request\n");
          if(!(options & OPT_QUIET))
          {
            show_message("server output: %s\n", buf);
          }
          retval = UPDATERES_ERROR;
        }
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      retval = UPDATERES_AUTHFAIL;
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      retval = UPDATERES_ERROR;
      break;
  }

  return(retval);
}

#ifdef CONFIG_PGP
int PGPOW_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int PGPOW_update_entry(void)
{
  char *buf = update_entry_buf;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  /* read server message */
  if(PGPOW_read_response(buf) != 0)
  {
    show_message("strange server response, are you connecting to the right server?\n");
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send version command */
  snprintf(buf, BUFFER_SIZE, "VER %s [%s-%s %s (%s)]\015\012", PGPOW_VERSION,
      "ez-update", VERSION, OS, "by Angus Mackay");
  output(buf);

  if(PGPOW_read_response(buf) != 0)
  {
    if(strncmp("ERR", buf, 3) == 0)
    {
      show_message("error talking to server: %s\n", &(buf[3]));
    }
    else
    {
      show_message("error talking to server:\n\t%s\n", buf);
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send user command */
  snprintf(buf, BUFFER_SIZE, "USER %s\015\012", user_name);
  output(buf);

  if(PGPOW_read_response(buf) != 0)
  {
    if(strncmp("ERR", buf, 3) == 0)
    {
      show_message("error talking to server: %s\n", &(buf[3]));
    }
    else
    {
      show_message("error talking to server:\n\t%s\n", buf);
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send pass command */
  snprintf(buf, BUFFER_SIZE, "PASS %s\015\012", password);
  output(buf);

  if(PGPOW_read_response(buf) != 0)
  {
    if(strncmp("ERR", buf, 3) == 0)
    {
      show_message("error talking to server: %s\n", &(buf[3]));
    }
    else
    {
      show_message("error talking to server:\n\t%s\n", buf);
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send host command */
  snprintf(buf, BUFFER_SIZE, "HOST %s\015\012", host);
  output(buf);

  if(PGPOW_read_response(buf) != 0)
  {
    if(strncmp("ERR", buf, 3) == 0)
    {
      show_message("error talking to server: %s\n", &(buf[3]));
    }
    else
    {
      show_message("error talking to server:\n\t%s\n", buf);
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send oper command */
  snprintf(buf, BUFFER_SIZE, "OPER %s\015\012", request);
  output(buf);

  if(PGPOW_read_response(buf) != 0)
  {
    if(strncmp("ERR", buf, 3) == 0)
    {
      show_message("error talking to server: %s\n", &(buf[3]));
    }
    else
    {
      show_message("error talking to server:\n\t%s\n", buf);
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  if(strcmp("update", request) == 0)
  {
    /* send ip command */
    snprintf(buf, BUFFER_SIZE, "IP %s\015\012", address);
    output(buf);

    if(PGPOW_read_response(buf) != 0)
    {
      if(strncmp("ERR", buf, 3) == 0)
      {
        show_message("error talking to server: %s\n", &(buf[3]));
      }
      else
      {
        show_message("error talking to server:\n\t%s\n", buf);
      }
      close(client_sockfd);
      return(UPDATERES_ERROR);
    }
  }

  /* send done command */
  snprintf(buf, BUFFER_SIZE, "DONE\015\012");
  output(buf);

  if(PGPOW_read_response(buf) != 0)
  {
    if(strncmp("ERR", buf, 3) == 0)
    {
      show_message("error talking to server: %s\n", &(buf[3]));
    }
    else
    {
      show_message("error talking to server:\n\t%s\n", buf);
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  if(!(options & OPT_QUIET))
  {
    show_message("request successful\n");
  }

  close(client_sockfd);
  return(UPDATERES_OK);
}
#endif

#ifdef CONFIG_DHS
int DHS_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

/*
 * grrrrr, it seems that dhs.org requires us to use POST
 * also DHS doesn't update both the mx record and the address at the same
 * time, this service really stinks. go with justlinix.com (penguinpowered)
 * instead, the only advantage is short host names.
 */
int DHS_update_entry(void)
{
  char *buf = update_entry_buf;
  char *putbuf = update_entry_putbuf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;
  char *domain = NULL;
  char *hostname = NULL;
  char *p;
  int limit;
  int retval = UPDATERES_OK;

  buf[BUFFER_SIZE] = '\0';
  putbuf[BUFFER_SIZE] = '\0';

  /* parse apart the domain and hostname */
  hostname = strdup(host);
  if((p=strchr(hostname, '.')) == NULL)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error parsing hostname from host %s\n", host);
    }
    return(UPDATERES_ERROR);
  }
  *p = '\0';
  p++;
  if(*p == '\0')
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error parsing domain from host %s\n", host);
    }
    return(UPDATERES_ERROR);
  }
  domain = strdup(p);

  dprintf((stderr, "hostname: %s, domain: %s\n", hostname, domain));

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "POST %s HTTP/1.0\015\012", request);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);

  p = putbuf;
  *p = '\0';
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "hostscmd=edit&hostscmdstage=2&type=4&");
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "%s=%s&", "updatetype", "Online");
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "%s=%s&", "ip", address);
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "%s=%s&", "mx", mx);
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "%s=%s&", "offline_url", url);
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  if(cloak_title)
  {
    snprintf(p, limit, "%s=%s&", "cloak", "Y");
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s&", "cloak_title", cloak_title);
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
  }
  else
  {
    snprintf(p, limit, "%s=%s&", "cloak_title", "");
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
  }
  snprintf(p, limit, "%s=%s&", "submit", "Update");
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "%s=%s&", "domain", domain);
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);
  snprintf(p, limit, "%s=%s", "hostname", hostname);
  p += strlen(p);
  limit = BUFFER_SIZE - 1 - strlen(buf);

  snprintf(buf, BUFFER_SIZE, "Content-length: %d\015\012", strlen(putbuf));
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  output(putbuf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      retval = UPDATERES_ERROR;
      break;

    case 200:
      if(!(options & OPT_QUIET))
      {
        show_message("request successful\n");
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      retval = UPDATERES_AUTHFAIL;
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      retval = UPDATERES_ERROR;
      break;
  }

  // this stupid service requires us to do seperate request if we want to 
  // update the mail exchanger (mx). grrrrrr
  if(*mx != '\0')
  {
    // okay, dhs's service is incredibly stupid and will not work with two
    // requests right after each other. I could care less that this is ugly,
    // I personally will NEVER use dhs, it is laughable.
    sleep(DHS_SUCKY_TIMEOUT < timeout.tv_sec ? DHS_SUCKY_TIMEOUT : timeout.tv_sec);

    if(do_connect((int*)&client_sockfd, server, port) != 0)
    {
      if(!(options & OPT_QUIET))
      {
        show_message("error connecting to %s:%s\n", server, port);
      }
      return(UPDATERES_ERROR);
    }

    snprintf(buf, BUFFER_SIZE, "POST %s HTTP/1.0\015\012", request);
    output(buf);
    snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
    output(buf);
    snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
        "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
    output(buf);
    snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
    output(buf);

    p = putbuf;
    *p = '\0';
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "hostscmd=edit&hostscmdstage=2&type=4&");
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s&", "updatetype", "Update+Mail+Exchanger");
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s&", "ip", address);
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s&", "mx", mx);
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s&", "offline_url", url);
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    if(cloak_title)
    {
      snprintf(p, limit, "%s=%s&", "cloak", "Y");
      p += strlen(p);
      limit = BUFFER_SIZE - 1 - strlen(buf);
      snprintf(p, limit, "%s=%s&", "cloak_title", cloak_title);
      p += strlen(p);
      limit = BUFFER_SIZE - 1 - strlen(buf);
    }
    else
    {
      snprintf(p, limit, "%s=%s&", "cloak_title", "");
      p += strlen(p);
      limit = BUFFER_SIZE - 1 - strlen(buf);
    }
    snprintf(p, limit, "%s=%s&", "submit", "Update");
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s&", "domain", domain);
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);
    snprintf(p, limit, "%s=%s", "hostname", hostname);
    p += strlen(p);
    limit = BUFFER_SIZE - 1 - strlen(buf);

    snprintf(buf, BUFFER_SIZE, "Content-length: %d\015\012", strlen(putbuf));
    output(buf);
    snprintf(buf, BUFFER_SIZE, "\015\012");
    output(buf);

    output(putbuf);
    snprintf(buf, BUFFER_SIZE, "\015\012");
    output(buf);

    bp = buf;
    bytes = 0;
    btot = 0;
    while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
    {
      bp += bytes;
      btot += bytes;
      dprintf((stderr, "btot: %d\n", btot));
    }
    close(client_sockfd);
    buf[btot] = '\0';

    dprintf((stderr, "server output: %s\n", buf));

    if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
    {
      ret = -1;
    }

    switch(ret)
    {
      case -1:
        if(!(options & OPT_QUIET))
        {
          show_message("strange server response, are you connecting to the right server?\n");
        }
        retval = UPDATERES_ERROR;
        break;

      case 200:
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
        break;

      case 401:
        if(!(options & OPT_QUIET))
        {
          show_message("authentication failure\n");
        }
        retval = UPDATERES_AUTHFAIL;
        break;

      default:
        if(!(options & OPT_QUIET))
        {
          // reuse the auth buffer
          *auth = '\0';
          sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
          show_message("unknown return code: %d\n", ret);
          show_message("server response: %s\n", auth);
        }
        retval = UPDATERES_ERROR;
        break;
    }
  }

  return(retval);
}
#endif

#ifdef CONFIG_ODS
int ODS_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(address) { free(address); }
    address = strdup("");
  }

  warn_fields(service->fields_used);

  return 0;
}

int ODS_update_entry(void)
{
  char *buf = update_entry_buf;
  int response;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  /* read server message */
  if(ODS_read_response(buf, BUFFER_SIZE) != 100)
  {
    show_message("strange server response, are you connecting to the right server?\n");
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send login command */
  snprintf(buf, BUFFER_SIZE, "LOGIN %s %s\012", user_name, password);
  output(buf);

  response = ODS_read_response(buf, BUFFER_SIZE);
  if(!(response == 225 || response == 226))
  {
    if(strlen(buf) > 4)
    {
      show_message("error talking to server: %s\n", &(buf[4]));
    }
    else
    {
      show_message("error talking to server\n");
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send delete command */
  snprintf(buf, BUFFER_SIZE, "DELRR %s A\012", host);
  output(buf);

  if(ODS_read_response(buf, BUFFER_SIZE) != 901)
  {
    if(strlen(buf) > 4)
    {
      show_message("error talking to server: %s\n", &(buf[4]));
    }
    else
    {
      show_message("error talking to server\n");
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  /* send address command */
  snprintf(buf, BUFFER_SIZE, "ADDRR %s A %s\012", host, 
                *address == '\0' ? "CONNIP" :  address);
  output(buf);

  response = ODS_read_response(buf, BUFFER_SIZE);
  if(!(response == 795 || response == 796))
  {
    if(strlen(buf) > 4)
    {
      show_message("error talking to server: %s\n", &(buf[4]));
    }
    else
    {
      show_message("error talking to server\n");
    }
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }

  if(!(options & OPT_QUIET))
  {
    show_message("request successful\n");
  }

  close(client_sockfd);
  return(UPDATERES_OK);
}
#endif

int TZO_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int TZO_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?", request);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "TZOName", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "Email", user_name);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "TZOKey", password);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "IPAddress", address);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(!(options & OPT_QUIET))
      {
        show_message("request successful\n");
      }
      break;

    case 302:
      // There is no neat way to determine the exact error other than to
      // parse the Location part of the mime header to find where we're
      // being redirected.
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        bp = strstr(buf, "Location: ");
        if((bp < strstr(buf, "\r\n\r\n")) && (sscanf(bp, "Location: http://%*[^/]%255[^\r\n]", auth) == 1))
        {
          bp = strrchr(auth, '/') + 1;
        }
        else
        {
          bp = "";
        }
        fprintf(stderr, "location: %s\n", bp);

        if(!(strncmp(bp, "domainmismatch.htm", strlen(bp)) && strncmp(bp, "invname.htm", strlen(bp))))
        {
          show_message("invalid host name\n");
        }
        else if(!strncmp(bp, "invkey.htm", strlen(bp)))
        {
          show_message("invalid password(tzo key)\n");
        }
        else if(!(strncmp(bp, "emailmismatch.htm", strlen(bp)) && strncmp(bp, "invemail.htm", strlen(bp))))
        {
          show_message("invalid user name(email address)\n");
        }
        else
        {
          show_message("unknown error\n");
        }
      }
      return(UPDATERES_ERROR);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}

int EASYDNS_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int EASYDNS_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?action=edit&", request);
  output(buf);
  if(address != NULL && *address != '\0')
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "myip", address);
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "wildcard", wildcard ? "ON" : "OFF");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "mx", mx);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "backmx", *mx == '\0' ? "NO" : "YES");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "host_id", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(strstr(buf, "NOERROR\n") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
      }
      else
      {
        show_message("error processing request\n");
        if(!(options & OPT_QUIET))
        {
          show_message("server output: %s\n", buf);
        }
        return(UPDATERES_ERROR);
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}

int EASYDNS_PARTNER_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if((partner == NULL) || (*partner == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(partner) { free(partner); }
    get_input("easyDNS partner", buf, sizeof(buf));
    partner = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int EASYDNS_PARTNER_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?action=edit&", request);
  output(buf);
  if(address != NULL && *address != '\0')
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "myip", address);
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "partner", partner);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "wildcard", wildcard ? "ON" : "OFF");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s", "hostname", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(strstr(buf, "OK\n") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
      }
      else
      {
        show_message("error processing request\n");
        if(!(options & OPT_QUIET))
        {
          show_message("server output: %s\n", buf);
        }
        return(UPDATERES_ERROR);
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    case 403:
      if(!(options & OPT_QUIET))
      {
        show_message("updating too frequently\n");
      }
      show_message("sleeping for %s\n", format_time(MAX_WAITRESPONSE_WAIT));
      sleep(MAX_WAITRESPONSE_WAIT);
      return(UPDATERES_ERROR);
      break;

    case 404:
      if(!(options & OPT_QUIET))
      {
        show_message("no dynamic service for this host/domain\n");
      }
      return(UPDATERES_SHUTDOWN);
      break;

    case 405:
      if(!(options & OPT_QUIET))
      {
        show_message("partner not supported\n");
      }
      return(UPDATERES_SHUTDOWN);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}


#ifdef USE_MD5
#ifdef GNUDIP
int GNUDIP_check_info(void)
{
  char buf[BUFSIZ+1];

  if((server == NULL) || (*server == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(server) { free(server); }
    get_input("server", buf, sizeof(buf));
    server = strdup(buf);
  }

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if((address) && (strcmp(address, "0.0.0.0") != 0))
  {
    if(!(options & OPT_QUIET))
    {
      show_message("warning: for GNUDIP the \"address\" parpameter is only used if set to \"0.0.0.0\" thus making an offline request.\n");
    }
  }

  warn_fields(service->fields_used);

  return 0;
}

int GNUDIP_update_entry(void)
{
  char *buf = update_entry_buf;
  unsigned char digestbuf[MD5_DIGEST_BYTES];
  char *p;
  int bytes;
  int ret;
  int i;
  char *domainname;
  char gnudip_request[2]; 

  // send an offline request if address 0.0.0.0 is used
  // otherwise, we ignore the address and send an update request
  gnudip_request[0] = strcmp(address, "0.0.0.0") == 0 ? '1' : '0';
  gnudip_request[1] = '\0';

  // find domainname
  for(p=host; *p != '\0' && *p != '.'; p++);
  if(*p != '\0') { p++; }
  if(*p == '\0')
  {
    return(UPDATERES_ERROR);
  }
  domainname = p;

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  if((bytes=read_input(buf, BUFFER_SIZE)) <= 0)
  {
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }
  buf[bytes] = '\0';
  dprintf((stderr, "bytes: %d\n", bytes));
  dprintf((stderr, "server output: %s\n", buf));

  // buf holds the shared secret
  chomp(buf);

  // use the auth buffer
  md5_buffer(password, strlen(password), digestbuf);
  for(i=0, p=auth; i<MD5_DIGEST_BYTES; i++, p+=2)
  {
    sprintf(p, "%02x", digestbuf[i]);
  }
  strncat(auth, ".", 255-strlen(auth));
  strncat(auth, buf, 255-strlen(auth));
  dprintf((stderr, "auth: %s\n", auth));
  md5_buffer(auth, strlen(auth), digestbuf);
  for(i=0, p=buf; i<MD5_DIGEST_BYTES; i++, p+=2)
  {
    sprintf(p, "%02x", digestbuf[i]);
  }
  strcpy(auth, buf);

  dprintf((stderr, "auth: %s\n", auth));

  snprintf(buf, BUFFER_SIZE, "%s:%s:%s:%s\n", user_name, auth, domainname,
      gnudip_request);
  output(buf);

  bytes = 0;
  if((bytes=read_input(buf, BUFFER_SIZE)) <= 0)
  {
    close(client_sockfd);
    return(UPDATERES_ERROR);
  }
  buf[bytes] = '\0';

  dprintf((stderr, "bytes: %d\n", bytes));
  dprintf((stderr, "server output: %s\n", buf));

  close(client_sockfd);

  if(sscanf(buf, "%d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 0:
      if(!(options & OPT_QUIET))
      {
        show_message("update request successful\n");
      }
      break;

    case 1:
      if(!(options & OPT_QUIET))
      {
        show_message("invalid login attempt\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 2:
      if(!(options & OPT_QUIET))
      {
        show_message("offline request successful\n");
      }
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        show_message("unknown return code: %d\n", ret);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}
#endif
#endif

#ifdef CONFIG_PGP
int JUSTL_check_info(void)
{
  char buf[BUFSIZ+1];

  if(host == NULL)
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int JUSTL_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?direct=1&", request);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "username", user_name);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "password", password);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "host", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "ip", address);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(strstr(buf, " set ") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
      }
      else
      {
        show_message("error processing request\n");
        if(!(options & OPT_QUIET))
        {
          show_message("server output: %s\n", buf);
        }
        return(UPDATERES_ERROR);
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}
#endif

int DYNS_check_info(void)
{
  char buf[BUFSIZ+1];

  if(host == NULL)
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  if(interface == NULL && address == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int DYNS_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?", request);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "username", user_name);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "password", password);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "host", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s", "ip", address);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(strstr(buf, "200 Host") != NULL ||
          strstr(buf, "200 host") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
      }
      else if(strstr(buf, "400 Bad Request") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("bad request\n");
        }
      }
      else if(strstr(buf, "401 User") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("authentication failure (username/password)\n");
        }
      }
      else if(strstr(buf, "405 Hostname") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("authentication failure (hostname not found)\n");
        }
      }

      else
      {
        show_message("error processing request\n");
        if(!(options & OPT_QUIET))
        {
          show_message("server output: %s\n", buf);
        }
        return(UPDATERES_ERROR);
      }

      break;

    case 405:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}

#ifdef CONFIG_HN
int HN_check_info(void)
{
  warn_fields(service->fields_used);

  return 0;
}

int HN_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?ver=%d&", request, 1);
  output(buf);
  if(address)
  {
    snprintf(buf, BUFFER_SIZE, "%s=%s&", "IP", address);
    output(buf);
  }
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    char *p;

    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      ret = -1;
      if((p=strstr(buf, "DDNS_Response_")) != NULL)
      {
         sscanf(p, "DDNS_Response_%*code=%3d", &ret);
      }

      /*
       * 101 - Successfully Updated
       * 201 - Failure because previous update occured
       *       less than 300 seconds ago
       * 202 - Failure because of server error
       * 203 - Failure because account is frozen (by admin)
       * 204 - Failure because account is locked (by user)
       */
      switch(ret)
      {
        case -1:
          if(!(options & OPT_QUIET))
          {
            show_message("strange server response, are you connecting to the right server?\n");
          }
          return(UPDATERES_ERROR);
          break;

        case 101:
          if(!(options & OPT_QUIET))
          {
            show_message("request successful\n");
          }
          break;

        case 201:
          show_message("Last update was less than %d seconds ago.\n", 300);
          return(UPDATERES_ERROR);
          break;

        case 202:
          show_message("Server error.\n");
          return(UPDATERES_ERROR);
          break;

        case 203:
          show_message("Failure because account is frozen (by admin).\n");
          return(UPDATERES_SHUTDOWN);
          break;

        case 204:
          show_message("Failure because account is locked (by user).\n");
          return(UPDATERES_SHUTDOWN);
          break;

        default:
          if(!(options & OPT_QUIET))
          {
            show_message("unknown return code: %d\n", ret);
            show_message("server response: %s\n", buf);
          }
          return(UPDATERES_ERROR);
          break;
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}
#endif

int ZONEEDIT_check_info(void)
{
  char buf[BUFSIZ+1];

  if((host == NULL) || (*host == '\0'))
  {
    if(options & OPT_DAEMON)
    {
      return(-1);
    }
    if(host) { free(host); }
    get_input("host", buf, sizeof(buf));
    host = strdup(buf);
  }

  warn_fields(service->fields_used);

  return 0;
}

int ZONEEDIT_update_entry(void)
{
  char *buf = update_entry_buf;
  char *bp = buf;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?", request);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "%s=%s&", "host", host);
  output(buf);
  if (address && *address) {
      snprintf(buf, BUFFER_SIZE, "%s=%s&", "dnsto", address);
      output(buf);
  }
  if (address && *mx && *mx != '0') {
      snprintf(buf, BUFFER_SIZE, "%s=%s&", "type", "a,mx");
      output(buf);
  }
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s (%s)\015\012", 
      "zoneedit", VERSION, OS, "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Authorization: Basic %s\015\012", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(strstr(buf, "<SUCCESS") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful\n");
        }
      }
      else
      {
        show_message("error processing request\n");
        if(!(options & OPT_QUIET))
        {
          show_message("server output: %s\n", buf);
        }
        return(UPDATERES_ERROR);
      }
      break;

    case 401:
      if(!(options & OPT_QUIET))
      {
        show_message("authentication failure\n");
      }
      return(UPDATERES_AUTHFAIL);
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}

#ifdef USE_MD5
int HEIPV6TB_check_info(void)
{
  char buf[BUFSIZ+1];

  if(interface == NULL)
  {
    if(options & OPT_DAEMON)
    {
      show_message("you must provide either an interface or an address\n");
      return(-1);
    }
    if(interface) { free(interface); }
    get_input("interface", buf, sizeof(buf));
    option_handler(CMD_interface, buf);
  }

  if((host == NULL) || (*host == '\0'))
  {
    show_message("you must provide global tunnel id in 'host' param\n");
    return(-1);
  }

  warn_fields(service->fields_used);

  return 0;
}

int HEIPV6TB_update_entry(void)
{
  unsigned char digestbuf[MD5_DIGEST_BYTES];
  char *buf = update_entry_buf;
  char *bp;
  int bytes;
  int btot;
  int ret;

  buf[BUFFER_SIZE] = '\0';

  if(do_connect((int*)&client_sockfd, server, port) != 0)
  {
    if(!(options & OPT_QUIET))
    {
      show_message("error connecting to %s:%s\n", server, port);
    }
    return(UPDATERES_ERROR);
  }

  // use the auth buffer
  md5_buffer(password, strlen(password), digestbuf);
  for(bytes = 0, bp = auth; bytes < MD5_DIGEST_BYTES; bytes++)
  {
    bp += sprintf(bp, "%02x", digestbuf[bytes]);
  }

  snprintf(buf, BUFFER_SIZE, "GET %s?", request);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "ip=%s&", (address && *address) ? address : "AUTO");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "apikey=%s&", user_name);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "pass=%s&", auth);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "tid=%s&", host);
  output(buf);
  snprintf(buf, BUFFER_SIZE, " HTTP/1.0\015\012");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "User-Agent: %s-%s %s [%s] (%s)\015\012", 
      "ez-update", VERSION, OS, (options & OPT_DAEMON) ? "daemon" : "", "by Angus Mackay");
  output(buf);
  snprintf(buf, BUFFER_SIZE, "Host: %s\015\012", server);
  output(buf);
  snprintf(buf, BUFFER_SIZE, "\015\012");
  output(buf);

  bp = buf;
  bytes = 0;
  btot = 0;
  while((bytes=read_input(bp, BUFFER_SIZE-btot)) > 0)
  {
    bp += bytes;
    btot += bytes;
    dprintf((stderr, "btot: %d\n", btot));
  }
  close(client_sockfd);
  buf[btot] = '\0';

  dprintf((stderr, "server output: %s\n", buf));

  if(sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
  {
    ret = -1;
  }

  switch(ret)
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        show_message("strange server response, are you connecting to the right server?\n");
      }
      return(UPDATERES_ERROR);
      break;

    case 200:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        if ((bp = strstr(buf, "+OK:")) != NULL)
          sscanf(bp, "+OK%255[^\r\n]", auth);
        else if ((bp = strstr(buf, "-ERROR:")) != NULL)
          sscanf(bp, "-ERROR%255[^\r\n]", auth);
      }
      if(strstr(buf, "+OK:") != NULL ||
         strstr(buf, "endpoint updated") != NULL ||
         strstr(buf, "tunnel is already") != NULL)
      {
        if(!(options & OPT_QUIET))
        {
          show_message("request successful%s\n", auth);
        }
      }
      else
      {
        if(!(options & OPT_QUIET))
        {
          show_message("bad request%s\n", auth);
        }
        return(UPDATERES_ERROR);
      }
      break;

    default:
      if(!(options & OPT_QUIET))
      {
        // reuse the auth buffer
        *auth = '\0';
        sscanf(buf, " HTTP/1.%*c %*3d %255[^\r\n]", auth);
        show_message("unknown return code: %d\n", ret);
        show_message("server response: %s\n", auth);
      }
      return(UPDATERES_ERROR);
      break;
  }

  return(UPDATERES_OK);
}
#endif

static int is_in_list(char *needle, char **haystack)
{
  char **p;
  int found = 0;

  for(p=haystack; *p != NULL; p++)
  {
    if(strcmp(needle, *p) == 0)
    {
      found = 1;
      break;
    }
  }

  return(found);
}

void warn_fields(char **okay_fields)
{
  if(wildcard != 0 && !is_in_list("wildcard", okay_fields))
  {
    show_message("warning: this service does not support the %s option\n",
        "wildcard");
  }
  if(!(mx == NULL || *mx == '\0') && !is_in_list("mx", okay_fields))
  {
    show_message("warning: this service does not support the %s option\n",
        "mx");
  }
  if(!(url == NULL || *url == '\0') && !is_in_list("url", okay_fields))
  {
    show_message("warning: this service does not support the %s option\n",
        "url");
  }
  if(!(cloak_title == NULL || *cloak_title == '\0') && !is_in_list("cloak_title", okay_fields))
  {
    show_message("warning: this service does not support the %s option\n",
        "cloak_title");
  }
  if(connection_type != 1 && !is_in_list("connection-type", okay_fields))
  {
    show_message("warning: this service does not support the %s option\n",
        "connection-type");
  }
}

int exec_cmd(char *cmd)
{
#if (defined(HAVE_WAITPID) || defined(HAVE_WAIT)) && (defined(HAVE_VFORK) || defined(HAVE_FORK))
  int kid;
  int exit_code;

  switch((kid=vfork()))
  {
    case -1:
      if(!(options & OPT_QUIET))
      {
        perror("fork");
      }
      return(-1);
      break;
    case 0:
      /* child */
      execl("/bin/sh", "sh", "-c", cmd, (char *)0);
      if(!(options & OPT_QUIET))
      {
        perror("exec");
      }
      exit(1);
      break;
    default:
      /* parent */
      dprintf((stderr, "forked kid: %d\n", kid));
      break;
  }

#  ifdef HAVE_WAITPID
  if(waitpid(kid, &exit_code, 0) != kid)
  {
    return(1);
  }
#  else
  if(wait(&exit_code) != kid)
  {
    return(1);
  }
#  endif
  exit_code = WEXITSTATUS(exit_code);

  return(exit_code);
#else
  return(-1);
#endif
}

void handle_sig(int sig)
{

  switch(sig)
  {
    case SIGHUP:
      if(config_file)
      {
        show_message("SIGHUP recieved, re-reading config file\n");
        if(parse_conf_file(config_file, conf_commands) != 0)
        {
          show_message("error parsing config file \"%s\"\n", config_file);
        }
      }
      break;
    case SIGTERM:
      /* 
       * this is used to wake up the client so that it will perform an update 
       */
      break;
    case SIGQUIT:
      show_message("received SIGQUIT, shutting down\n");

#if HAVE_SYSLOG_H
      closelog();
#endif

#if HAVE_GETPID
      if(pid_file)
      {
        pid_file_delete(pid_file);
      }
#endif

      exit(0);
    default:
      dprintf((stderr, "case not handled: %d\n", sig));
      break;
  }
}

int main(int argc, char **argv)
{
  int ifresolve_warned = 0;
  int i;
  int retval = 1;
#ifdef IF_LOOKUP
  int sock = -1;
#endif

#if defined(DEBUG) && defined(__linux__) && !defined(EMBED)
  //mcheck(NULL);
#endif

  dprintf((stderr, "staring...\n"));

show_message("ez-ipupdate: starting...\n");

  program_name = argv[0];
  options = 0;
  *user = '\0';
  timeout.tv_sec = DEFAULT_TIMEOUT;
  timeout.tv_usec = 0;
  parse_service(DEF_SERVICE);


#if HAVE_SIGNAL_H
  // catch user interupts
  signal(SIGINT,  sigint_handler);
  signal(SIGHUP,  generic_sig_handler);
  signal(SIGTERM, generic_sig_handler);
  signal(SIGQUIT, generic_sig_handler);
#endif

  parse_args(argc, argv);

  if(!(options & OPT_QUIET) && !(options & OPT_DAEMON))
  {
    fprintf(stderr, "ez-ipupdate Version %s\nCopyright (C) 1998-2001 Angus Mackay.\n", VERSION);
  }

#if HAVE_SYSLOG_H
  if(options & OPT_DAEMON)
    openlog(program_name, LOG_PID, LOG_USER );
#endif


  dprintf((stderr, "options: 0x%04X\n", options));
  dprintf((stderr, "interface: %s\n", interface));
  dprintf((stderr, "ntrys: %d\n", ntrys));
  dprintf((stderr, "server: %s:%s\n", server, port));

  dprintf((stderr, "address: %s\n", address));
  dprintf((stderr, "wildcard: %d\n", wildcard));
  dprintf((stderr, "mx: %s\n", mx));
  dprintf((stderr, "auth: %s\n", auth));

  while(is_in_list("null", service->names))
  {
    if(service->check_info() != 0)
    {
      show_message("invalid data to perform requested action.\n");
      exit(1);
    }
  }

  if(server == NULL)
  {
    server = strdup(service->default_server);
  }
  if(port == NULL)
  {
    port = strdup(service->default_port);
  }

  *user_name = '\0';
  *password = '\0';

//2007.03.14 Yau add
#ifdef ASUS_DDNS
        if (g_asus_ddns_mode == 0)      {
#endif  // ASUS_DDNS

  if(*user != '\0')
  {
    sscanf(user, "%127[^:]:%127[^\n]", user_name, password);
    dprintf((stderr, "user_name: %s\n", user_name));
    dprintf((stderr, "password: %s\n", password));
  }
  if(*user_name == '\0')
  {
    get_input("user name", user_name, sizeof(user_name));
  }
  if(*password == '\0')
  {
    strncpy(password, getpass("password: "), sizeof(password));
  }
  sprintf(user, "%s:%s", user_name, password);

  base64Encode(user, auth);

//2007.03.14 Yau add
#ifdef ASUS_DDNS
        }
#endif  // ASUS_DDNS

  request = strdup(request_over_ride == NULL ? service->default_request : request_over_ride);
  dprintf((stderr, "request: %s\n", request));

  if(service->init != NULL)
  {
    service->init();
  }

  if(service->check_info() != 0)
  {
    show_message("invalid data to perform requested action.\n");
    exit(1);
  }

  if(mx == NULL) { mx = strdup(""); }
  if(url == NULL) { url = strdup(""); }

#ifdef IF_LOOKUP
  if(options & OPT_DAEMON)
  {
    sock = socket(AF_INET, SOCK_STREAM, 0);
  }
#endif

//2007.03.14 Yau add
#ifdef ASUS_DDNS
        if (g_asus_ddns_mode != 0)      {
                nvram_unset ("ddns_suggest_name");
                nvram_unset ("ddns_old_name");
                if (asus_private() == -1) {
			nvram_set ("ddns_return_code", "connect_fail");
			nvram_set ("ddns_return_code_chk", "connect_fail");
                        goto exit_main;
		}
        }
        if (g_asus_ddns_mode == 1)      {
              retval = asus_reg_domain ();
show_message("asus_reg_domain retval= %d\n", retval);
              goto asusddns_update;
        } else if (g_asus_ddns_mode == 2)       {
                // override update_entry() method
                service->update_entry = asus_update_entry;
        }
#endif  // ASUS_DDNS

  if(options & OPT_DAEMON)
  {
    int local_update_period = update_period;
#if IF_LOOKUP
    struct sockaddr_in sin;
    struct sockaddr_in sin2;

    if(interface == NULL) 
    { 
      show_message("invalid data to perform requested action.\n");
      show_message("you must provide an interface for daemon mode");
      exit(1);
    }

    /* background our selves */
    if(!(options & OPT_FOREGROUND))
    {
#  if HAVE_SYSLOG_H
      close(0);
      close(1);
      close(2);
#  endif
#  if HAVE_FORK
      if(fork() > 0) { exit(0); } /* parent */
#  endif
    }

#if HAVE_GETPID
    if(pid_file && pid_file_create(pid_file) != 0)
    {
      show_message("exiting...\n");
      exit(1);
    }
#endif

    show_message("ez-ipupdate Version %s, Copyright (C) 1998-2001 Angus Mackay.\n", 
        VERSION);
    show_message("%s started for interface %s host %s using server %s and service %s\n",
        program_name, N_STR(interface), N_STR(host), server, service->title);

    memset(&sin, 0, sizeof(sin));

    if(cache_file)
    {
      time_t ipdate;
      char *ipstr;

      if(read_cache_file(cache_file, &ipdate, &ipstr) == 0)
      {
        dprintf((stderr, "cache date: %ld\n", ipdate));
        dprintf((stderr, "cache IP: %s\n", ipstr ?: ""));

        if(ipstr && strchr(ipstr, '.'))
        {
          struct tm *ts;
          char timebuf[64];

          inet_aton(ipstr, &sin.sin_addr);
          last_update = ipdate;

          ts = localtime(&ipdate);
          strftime(timebuf, sizeof(timebuf), "%Y/%m/%d %H:%M", ts);
          show_message("got last update %s on %s from cache file\n", ipstr, timebuf);
        }
        if(ipstr) { free(ipstr); ipstr = NULL; }
      }
      else
      {
        show_message("error reading cache file \"%s\": %s\n", cache_file, 
            errno == 0 ? "malformed entry" : strerror(errno));
      }
    }

    for(;;)
    {
      if(options & OPT_ONCE)
      {
        int f;

        f = open(BLOCK_FILE, O_RDONLY);
        if(f >= 0)
        {
          close(f);
          show_message("update critically failed on a previous attempt\n");
          show_message("delete %s if the problem has been corrected\n",
              BLOCK_FILE);
          break;
		}
	  }

#if HAVE_SIGNAL_H
      /* check for signals */
      if(last_sig != 0)
      {
        handle_sig(last_sig);
        last_sig = 0;
      }
#endif
      if(get_if_addr(sock, interface, &sin2) == 0)
      {
        ifresolve_warned = 0;
        if(memcmp(&sin.sin_addr, &sin2.sin_addr, sizeof(struct in_addr)) != 0 || 
            (max_interval > 0 && time(NULL) - last_update > max_interval))
        {
          int updateres;

          // save this new ipaddr
          memcpy(&sin, &sin2, sizeof(sin));

          // update the address buffer
          if(address) { free(address); }
          address = strdup(inet_ntoa(sin.sin_addr));

          if((updateres=service->update_entry()) == UPDATERES_OK)
          {
            last_update = time(NULL);
            local_update_period = update_period;

            show_message("successful update for %s->%s (%s)\n",
                interface, inet_ntoa(sin.sin_addr), N_STR(host));

            if(post_update_cmd)
            {
              int res;

              if(post_update_cmd)
              {
                sprintf(post_update_cmd_arg, "%s", inet_ntoa(sin.sin_addr));

                if((res=exec_cmd(post_update_cmd)) != 0)
                {
                  if(res == -1)
                  {
                    show_message("(%s) error running post update command: %s\n",
                        N_STR(host), error_string);
                  }
                  else
                  {
                    show_message(
                        "(%s) error running post update command, command exit code: %d\n",
                        N_STR(host), res);
                  }
                }
              }
            }

            if(cache_file)
            {
              char ipbuf[64];

              snprintf(ipbuf, sizeof(ipbuf), "%s", inet_ntoa(sin.sin_addr));

              if(write_cache_file(cache_file, last_update, ipbuf) != 0)
              {
                show_message("unable to write cache file \"%s\": %s\n",
                    cache_file, error_string);
              }
            }

			if(options & OPT_ONCE)
				break;
          }
          else
          {
            show_message("failure to update %s->%s (%s)\n",
                interface, inet_ntoa(sin.sin_addr), N_STR(host));
            memset(&sin, 0, sizeof(sin));

            // double the time between attempts between each failure to update
            // this gets set back to the normal value the next time we get a
            // successful update
            if(local_update_period < MIN_WAIT_PERIOD)
            {
              local_update_period = MIN_WAIT_PERIOD;
            }
            else
            {
              local_update_period *= 2;
            }
            if(local_update_period > MAX_WAIT_PERIOD)
            {
              local_update_period = MAX_WAIT_PERIOD;
            }
            dprintf((stderr, "local_update_period: %d\n", local_update_period));

            dprintf((stderr, "updateres: %d\n", updateres));
            if(updateres == UPDATERES_SHUTDOWN)
            {
              show_message("shutting down updater for %s due to fatal error\n", 
                  N_STR(host));
#ifdef SEND_MEAIL_CMD
              if(notify_email && *notify_email != '\0')
              {
                char buf[1024];

                dprintf((stderr, "sending email to %s\n", notify_email));
                snprintf(buf, sizeof(buf), "echo \"ez-ipupdate shutting down"
                    " updater for %s due to fatal error.\" | %s %s", host,
                    SEND_EMAIL_CMD,
                    notify_email);
                system(buf);
              }
#endif
              if(options & OPT_ONCE)
              {
                int f;

                f = open(BLOCK_FILE, O_WRONLY|O_CREAT);
                if(f >= 0)
                  close(f);
              }
              break;
            }
          }
        }
		else if(options & OPT_ONCE)
			break;
        sleep(local_update_period);
      }
      else
      {
        if(!ifresolve_warned)
        {
          ifresolve_warned = 1;
          show_message("(%s) unable to resolve interface %s\n",
              N_STR(host), interface);
        }
        sleep(resolv_period);
      }
    }

#if HAVE_GETPID
    if(pid_file)
    {
      pid_file_delete(pid_file);
    }
#endif

#else
    show_message("sorry, this mode is only available on platforms that the ");
    show_message("IP address \ncan be determined. feel free to hack the code");
    show_message(" though.\n");
    exit(1);
#endif
  }
  else
  {
    int need_update = 1;

    if(cache_file)
    {
      time_t ipdate;
      char *ipstr;
      char ipbuf[64];

      if(read_cache_file(cache_file, &ipdate, &ipstr) != 0)
      {
        show_message( "error reading cache file \"%s\": %s\n", cache_file, 
            errno == 0 ? "malformed entry" : strerror(errno));
      }

      //show_message("cache date: %ld\n", ipdate);
      //show_message("cache IP %s\n", N_STR(ipstr));
      // check that the cache file contained something
      if(ipstr != NULL)
      {

        if(address == NULL || *address == '\0')
        {
#ifdef IF_LOOKUP
          struct sockaddr_in sin;
          int sock;

          sock = socket(AF_INET, SOCK_STREAM, 0);
          if(get_if_addr(sock, interface, &sin) != 0)
          {
            exit(1);
          }
          close(sock);
          snprintf(ipbuf, sizeof(ipbuf), "%s", inet_ntoa(sin.sin_addr));
#else
          show_message("interface lookup not enabled at compile time\n");
          exit(1);
#endif
        }
        else
        {
          snprintf(ipbuf, sizeof(ipbuf), "%s", address);
        }

        // check for a change in the IP
        if(strcmp(ipstr, ipbuf) == 0)
        {
	  if(nvram_match("ddns_server_x", "WWW.ASUS.COM")) { 
	    if(strcmp(nvram_safe_get("ddns_hostname_x"), nvram_safe_get("ddns_hostname_old")) == 0) {
              dprintf((stderr, "IP & hostname don't change, no need updating\n"));
              show_message("IP & hostname don't change, no need updating\n");
              need_update = 0;
	    }
	  }
	  else {
            dprintf((stderr, "cache IP doesn't need updating\n"));
	    show_message("cache IP doesn't need updating\n");
            need_update = 0;
	  }
        }

        // check the date
        if(max_interval > 0)
        {
          if(time(NULL) - ipdate > max_interval)
          {
            dprintf((stderr, "cache IP is passed max_interval of %d\n", max_interval));
            need_update = 1;
          }
        }
      }
      if(ipstr) { free(ipstr); ipstr = NULL; }
    }

    if(need_update)
    {
      int res;

      nvram_set ("ddns_return_code", "ddns_query");
      nvram_set ("ddns_return_code_chk", "ddns_query");

      if(address == NULL && interface != NULL)
      {
        struct sockaddr_in sin;
        int sock;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if(get_if_addr(sock, interface, &sin) == 0)
        {
          if(address) { free(address); }
          address = strdup(inet_ntoa(sin.sin_addr));
        }
        else
        {
          show_message("could not resolve ip address for %s.\n", interface);
          exit(1);
        }
        close(sock);
      }

      for(i=0; i<ntrys; i++)
      {
	retval = service->update_entry();
        if(retval == UPDATERES_OK)
          break;
        
        if(i+1 != ntrys) { sleep(10 + 10*i); }
      }

asusddns_update:
show_message("asusddns_update: %d\n", retval);
      // write cache file
      if(retval == 0 && cache_file)
      {
        char ipbuf[64];

        if(address == NULL || *address == '\0')
        {
#ifdef IF_LOOKUP
          struct sockaddr_in sin;
          int sock;

          sock = socket(AF_INET, SOCK_STREAM, 0);
          if(get_if_addr(sock, interface, &sin) != 0)
          {
            retval = 1;
            goto exit_main;
          }
          close(sock);
          snprintf(ipbuf, sizeof(ipbuf), "%s", inet_ntoa(sin.sin_addr));
#else
          show_message("interface lookup not enabled at compile time\n");
          retval = 1;
          goto exit_main;
#endif
        }
        else
        {
          snprintf(ipbuf, sizeof(ipbuf), "%s", address);
        }

        if(write_cache_file(cache_file, time(NULL), ipbuf) != 0)
        {
          show_message("unable to write cache file \"%s\": %s\n",
              cache_file, error_string);
          retval = 1;
          goto exit_main;
        }
      }
      if(retval == 0 && post_update_cmd)
      {
        if((res=exec_cmd(post_update_cmd)) != 0)
        {
          if(!(options & OPT_QUIET))
          {
            if(res == -1)
            {
              show_message("error running post update command: %s\n",
                  error_string);
            }
            else
            {
              show_message(
                  "error running post update command, command exit code: %d\n",
                  res);
            }
          }
        }
      }
      if(!nvram_match("ddns_server_x", "WWW.ASUS.COM")) {
	if(retval == 0){
          nvram_set("ddns_return_code", "200");
          nvram_set("ddns_return_code_chk", "200");
        }
	else if(retval == 3) {
	  nvram_set("ddns_return_code", "auth_fail");
	  nvram_set("ddns_return_code_chk", "auth_fail");
	}
	else {
          nvram_set("ddns_return_code", "unknown_error");
          nvram_set("ddns_return_code_chk", "unknown_error");
	}
      }
    }
    else
    {
      show_message("no update needed at this time\n");
    }
  }

  exit_main:

#ifdef IF_LOOKUP
  if(sock > 0) { close(sock); }
#endif

  if(address) { free(address); }
  if(cache_file) { free(cache_file); }
  if(config_file) { free(config_file); }
  if(host) { free(host); }
  if(interface) { free(interface); }
  if(mx) { free(mx); }
  if(port) { free(port); }
  if(request) { free(request); }
  if(request_over_ride) { free(request_over_ride); }
  if(server) { free(server); }
  if(url) { free(url); }
  if(partner) { free(partner); }

  dprintf((stderr, "done\n"));
  return(retval);
}

