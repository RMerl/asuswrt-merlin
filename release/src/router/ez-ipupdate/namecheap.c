#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "namecheap.h"

/* @See: http://www.namecheap.com/support/knowledgebase/article.aspx/29/11/how-to-use-the-browser-to-dynamically-update-hosts-ip */

/********************
 * BEGIN from ez-ipupdate.c
 * NOTE: The following should be moved into its own header, as it is also used by asus_ddns.c.
 *       However, that would make merges with Asus' releases more difficult.
 */
#ifdef DEBUG
#  define BUFFER_SIZE (16*1024)
#else
#  define BUFFER_SIZE (4*1024-1)
#endif

#define OPT_DAEMON      0x0004
#define OPT_QUIET       0x0008

#define CMD_interface 13

enum {
    UPDATERES_OK = 0,
    UPDATERES_ERROR,
    UPDATERES_SHUTDOWN,
    UPDATERES_AUTHFAIL,
};

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

extern char *server;
extern char *port;
extern char user[256];
extern char auth[512];
extern char user_name[128];
extern char password[128];
extern char *address;
extern char *request;
extern char *host;
extern char *interface;
extern int options;
extern char update_entry_buf[BUFFER_SIZE + 1];
extern volatile int client_sockfd;
extern struct service_t *service;

extern void warn_fields(char **okay_fields);
extern int do_connect(int *sock, char *host, char *port);
extern void show_message(char *fmt, ...);
extern void output(void *buf);
extern int read_input(char *buf, int len);
extern void get_input(char *prompt, char *buf, int buflen);
extern int option_handler(int id, char *optarg);
/* END from ez-ipupdate.c
 ********************/

const char *NC_fields_used[] = { "server", "user", "host", "address", NULL };

int NC_check_info(void)
{
    char buf[BUFSIZ + 1];
    
    if (!interface && !address)
    {
        if (options & OPT_DAEMON)
        {
            show_message("you must provide either an interface or an address\n");
            return -1;
        }
        if (interface)
        {
            free(interface);
        }
        get_input("interface", buf, sizeof(buf));
        option_handler(CMD_interface, buf);
    }

    warn_fields(service->fields_used);

    return 0;
}

int NC_update_entry(void)
{
    char *buf = update_entry_buf;
    char *bp;
    int bytes;
    int btot;
    int ret;
    int retval = UPDATERES_OK;

    buf[BUFFER_SIZE] = '\0';

    if (do_connect((int*)&client_sockfd, server, port))
    {
        if (!(options & OPT_QUIET))
        {
            show_message("error connecting to %s:%s\n", server, port);
        }
        return UPDATERES_ERROR;
    }

    snprintf(buf, BUFFER_SIZE, "GET %s?", request);
    output(buf);
    snprintf(buf, BUFFER_SIZE, "host=%s", host);
    output(buf);
    snprintf(buf, BUFFER_SIZE, "&domain=%s", user_name);
    output(buf);
    snprintf(buf, BUFFER_SIZE, "&password=%s", password);
    output(buf);
    if (address)
    {
        snprintf(buf, BUFFER_SIZE, "&ip=%s", address);
        output(buf);
    }
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
    while ((bytes = read_input(bp, BUFFER_SIZE - btot)) > 0)
    {
        bp += bytes;
        btot += bytes;
    }
    close(client_sockfd);
    buf[btot] = '\0';

    if (sscanf(buf, " HTTP/1.%*c %3d", &ret) != 1)
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
        if (strstr(buf, "<ErrCount>0"))
        {
            if (!(options & OPT_QUIET))
            {
                show_message("request successful\n");
            }
        }
        else
        {
            show_message("error processing request\n");
            if (!(options & OPT_QUIET))
            {
                show_message("server output: %s\n", buf);
            }
            return UPDATERES_ERROR;
        }
        break;

    case 401:
        if (!(options & OPT_QUIET))
        {
            show_message("authentication failure\n");
        }
        retval = UPDATERES_AUTHFAIL;
        break;

    default:
        if (!(options & OPT_QUIET))
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

    return retval;
}
