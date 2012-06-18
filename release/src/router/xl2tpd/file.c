/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * File format handling
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "l2tp.h"

struct lns *lnslist;
struct lac *laclist;
struct lns *deflns;
struct lac *deflac;
struct global gconfig;
char filerr[STRLEN];

int parse_config (FILE *);
struct keyword words[];

int init_config ()
{
    FILE *f;
    int returnedValue;

    gconfig.port = UDP_LISTEN_PORT;
    gconfig.listenaddr = htonl(INADDR_ANY); /* Default is to bind (listen) to all interfaces */
    gconfig.debug_avp = 0;
    gconfig.debug_network = 0;
    gconfig.packet_dump = 0;
    gconfig.debug_tunnel = 0;
    gconfig.debug_state = 0;
    lnslist = NULL;
    laclist = NULL;
    deflac = (struct lac *) calloc (1, sizeof (struct lac));

    f = fopen (gconfig.configfile, "r");
    if (!f) 
    {
        f = fopen (gconfig.altconfigfile, "r");
        if (f)
        {
	     l2tp_log (LOG_WARNING, "%s: Using old style config files %s and %s\n",
		__FUNCTION__, gconfig.altconfigfile, gconfig.altauthfile);
            strncpy (gconfig.authfile, gconfig.altauthfile, 
            	sizeof (gconfig.authfile));
        }
        else
        {
            l2tp_log (LOG_CRIT, "%s: Unable to open config file %s or %s\n",
                 __FUNCTION__, gconfig.configfile, gconfig.altconfigfile);
            return -1;
        }

    }
    returnedValue = parse_config (f);
    fclose (f);
    return (returnedValue);
    filerr[0] = 0;
}

struct lns *new_lns ()
{
    struct lns *tmp;
    tmp = (struct lns *) calloc (1, sizeof (struct lns));
    if (!tmp)
    {
        l2tp_log (LOG_CRIT, "%s: Unable to allocate memory for new LNS\n",
             __FUNCTION__);
        return NULL;
    }
    tmp->next = NULL;
    tmp->exclusive = 0;
    tmp->localaddr = 0;
    tmp->tun_rws = DEFAULT_RWS_SIZE;
    tmp->call_rws = DEFAULT_RWS_SIZE;
    tmp->rxspeed = DEFAULT_RX_BPS;
    tmp->txspeed = DEFAULT_TX_BPS;
    tmp->hbit = 0;
    tmp->lbit = 0;
    tmp->authpeer = 0;
    tmp->authself = -1;
    tmp->authname[0] = 0;
    tmp->peername[0] = 0;
    tmp->hostname[0] = 0;
    tmp->entname[0] = 0;
    tmp->range = NULL;
    tmp->assign_ip = 1;                /* default to 'yes' */
    tmp->lacs = NULL;
    tmp->passwdauth = 0;
    tmp->pap_require = 0;
    tmp->pap_refuse = 0;
    tmp->chap_require = 0;
    tmp->chap_refuse = 0;
    tmp->idle = 0;
    tmp->pridns = 0;
    tmp->secdns = 0;
    tmp->priwins = 0;
    tmp->secwins = 0;
    tmp->proxyarp = 0;
    tmp->proxyauth = 0;
    tmp->challenge = 0;
    tmp->debug = 0;
    tmp->pppoptfile[0] = 0;
    tmp->t = NULL;
    return tmp;
}

struct lac *new_lac ()
{
    struct lac *tmp;
    tmp = (struct lac *) calloc (1, sizeof (struct lac));
    if (!tmp)
    {
        l2tp_log (LOG_CRIT, "%s: Unable to allocate memory for lac entry!\n",
             __FUNCTION__);
        return NULL;
    }
    tmp->next = NULL;
    tmp->rsched = NULL;
    tmp->localaddr = 0;
    tmp->remoteaddr = 0;
    tmp->lns = 0;
    tmp->tun_rws = DEFAULT_RWS_SIZE;
    tmp->call_rws = DEFAULT_RWS_SIZE;
    tmp->hbit = 0;
    tmp->lbit = 0;
    tmp->authpeer = 0;
    tmp->authself = -1;
    tmp->authname[0] = 0;
    tmp->peername[0] = 0;
    tmp->hostname[0] = 0;
    tmp->entname[0] = 0;
    tmp->pap_require = 0;
    tmp->pap_refuse = 0;
    tmp->chap_require = 0;
    tmp->chap_refuse = 0;
    tmp->t = NULL;
    tmp->redial = 0;
    tmp->rtries = 0;
    tmp->rmax = 0;
    tmp->challenge = 0;
    tmp->autodial = 0;
    tmp->rtimeout = 30;
    tmp->active = 0;
    tmp->debug = 0;
    tmp->pppoptfile[0] = 0;
    tmp->defaultroute = 0;
    return tmp;
}

int yesno (char *value)
{
    if (!strcasecmp (value, "yes") || !strcasecmp (value, "y") ||
        !strcasecmp (value, "true"))
        return 1;
    else if (!strcasecmp (value, "no") || !strcasecmp (value, "n") ||
             !strcasecmp (value, "false"))
        return 0;
    else
        return -1;
}

int set_boolean (char *word, char *value, int *ptr)
{
    int val;
#ifdef DEBUG_FILE
    l2tp_log (LOG_DEBUG, "set_%s: %s  flag to '%s'\n", word, word, value);
#endif /* ; */
    if ((val = yesno (value)) < 0)
    {
        snprintf (filerr, sizeof (filerr), "%s must be 'yes' or 'no'\n",
                  word);
        return -1;
    }
    *ptr = val;
    return 0;
}

int set_int (char *word, char *value, int *ptr)
{
    int val;
#ifdef DEBUG_FILE
    l2tp_log (LOG_DEBUG, "set_%s: %s  flag to '%s'\n", word, word, value);
#endif /* ; */
    if ((val = atoi (value)) < 0)
    {
        snprintf (filerr, sizeof (filerr), "%s must be a number\n", word);
        return -1;
    }
    *ptr = val;
    return 0;
}

int set_string (char *word, char *value, char *ptr, int len)
{
#ifdef DEBUG_FILE
    l2tp_log (LOG_DEBUG, "set_%s: %s  flag to '%s'\n", word, word, value);
#endif /* ; */
    strncpy (ptr, value, len);
    return 0;
}

int set_port (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
#ifdef DEBUG_FILE
        l2tp_log (LOG_DEBUG, "set_port: Setting global port number to %s\n",
             value);
#endif
        set_int (word, value, &(((struct global *) item)->port));
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_rtimeout (char *word, char *value, int context, void *item)
{
    if (atoi (value) < 1)
    {
        snprintf (filerr, sizeof (filerr),
                  "rtimeout value must be at least 1\n");
        return -1;
    }
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
#ifdef DEBUG_FILE
        l2tp_log (LOG_DEBUG, "set_rtimeout: Setting redial timeout to %s\n",
             value);
#endif
        set_int (word, value, &(((struct lac *) item)->rtimeout));
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_rws (char *word, char *value, int context, void *item)
{
    if (atoi (value) < -1)
    {
        snprintf (filerr, sizeof (filerr),
                  "receive window size must be at least -1\n");
        return -1;
    }
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (word[0] == 'c')
            set_int (word, value, &(((struct lac *) item)->call_rws));
        if (word[0] == 't')
        {
            set_int (word, value, &(((struct lac *) item)->tun_rws));
            if (((struct lac *) item)->tun_rws < 1)
            {
                snprintf (filerr, sizeof (filerr),
                          "receive window size for tunnels must be at least 1\n");
                return -1;
            }
        }
        break;
    case CONTEXT_LNS:
        if (word[0] == 'c')
            set_int (word, value, &(((struct lns *) item)->call_rws));
        if (word[0] == 't')
        {
            set_int (word, value, &(((struct lns *) item)->tun_rws));
            if (((struct lns *) item)->tun_rws < 1)
            {
                snprintf (filerr, sizeof (filerr),
                          "receive window size for tunnels must be at least 1\n");
                return -1;
            }
        }
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_speed (char *word, char *value, int context, void *item)
{
    if (atoi (value) < 1 )
    {
        snprintf (filerr, sizeof (filerr),
                  "bps must be greater than zero\n");
        return -1;
    }
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (word[0] == 't')
            set_int (word, value, &(((struct lac *) item)->txspeed));
        else if (word[0] == 'r')
            set_int (word, value, &(((struct lac *) item)->rxspeed));
        else
	{
            set_int (word, value, &(((struct lac *) item)->rxspeed));
            set_int (word, value, &(((struct lac *) item)->txspeed));
	}
        break;
    case CONTEXT_LNS:
        if (word[0] == 't')
            set_int (word, value, &(((struct lns *) item)->txspeed));
        else if (word[0] == 'r')
            set_int (word, value, &(((struct lns *) item)->rxspeed));
        else
	{
            set_int (word, value, &(((struct lns *) item)->rxspeed));
            set_int (word, value, &(((struct lns *) item)->txspeed));
	}
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_rmax (char *word, char *value, int context, void *item)
{
    if (atoi (value) < 1)
    {
        snprintf (filerr, sizeof (filerr), "rmax value must be at least 1\n");
        return -1;
    }
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
#ifdef DEBUG_FILE
        l2tp_log (LOG_DEBUG, "set_rmax: Setting max redials to %s\n", value);
#endif
        set_int (word, value, &(((struct lac *) item)->rmax));
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_authfile (char *word, char *value, int context, void *item)
{
    if (!strlen (value))
    {
        snprintf (filerr, sizeof (filerr),
                  "no filename specified for authentication\n");
        return -1;
    }
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
#ifdef DEBUG_FILE
        l2tp_log (LOG_DEBUG, "set_authfile: Setting global auth file to '%s'\n",
             value);
#endif /* ; */
        strncpy (((struct global *) item)->authfile, value,
                 sizeof (((struct global *)item)->authfile));
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_autodial (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->autodial)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_flow (char *word, char *value, int context, void *item)
{
    int v;
    set_boolean (word, value, &v);
    if (v < 0)
        return -1;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (v)
        {
            if (((struct lac *) item)->call_rws < 0)
                ((struct lac *) item)->call_rws = 0;
        }
        else
        {
            ((struct lac *) item)->call_rws = -1;
        }
        break;
    case CONTEXT_LNS:
        if (v)
        {
            if (((struct lns *) item)->call_rws < 0)
                ((struct lns *) item)->call_rws = 0;
        }
        else
        {
            ((struct lns *) item)->call_rws = -1;
        }
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_defaultroute (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->defaultroute)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_authname (char *word, char *value, int context, void *item)
{
    struct lac *l = (struct lac *) item;
    struct lns *n = (struct lns *) item;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        if (set_string (word, value, n->authname, sizeof (n->authname)))
            return -1;
        break;
    case CONTEXT_LAC:
        if (set_string (word, value, l->authname, sizeof (l->authname)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_hostname (char *word, char *value, int context, void *item)
{
    struct lac *l = (struct lac *) item;
    struct lns *n = (struct lns *) item;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        if (set_string (word, value, n->hostname, sizeof (n->hostname)))
            return -1;
        break;
    case CONTEXT_LAC:
        if (set_string (word, value, l->hostname, sizeof (l->hostname)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_passwdauth (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->passwdauth)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_hbit (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->hbit)))
            return -1;
        break;
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->hbit)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_challenge (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->challenge)))
            return -1;
        break;
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->challenge)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_lbit (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->lbit)))
            return -1;
        break;
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->lbit)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}


int set_debug (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->debug)))
            return -1;
        break;
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->debug)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_pppoptfile (char *word, char *value, int context, void *item)
{
    struct lac *l = (struct lac *) item;
    struct lns *n = (struct lns *) item;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        if (set_string (word, value, n->pppoptfile, sizeof (n->pppoptfile)))
            return -1;
        break;
    case CONTEXT_LAC:
        if (set_string (word, value, l->pppoptfile, sizeof (l->pppoptfile)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_papchap (char *word, char *value, int context, void *item)
{
    int result;
    char *c;
    struct lac *l = (struct lac *) item;
    struct lns *n = (struct lns *) item;
    if (set_boolean (word, value, &result))
        return -1;
    c = strchr (word, ' ');
    c++;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (c[0] == 'p')        /* PAP */
            if (word[2] == 'f')
                l->pap_refuse = result;
            else
                l->pap_require = result;
        else if (c[0] == 'a')   /* Authentication */
            if (word[2] == 'f')
                l->authself = !result;
            else
                l->authpeer = result;
        else /* CHAP */ if (word[2] == 'f')
            l->chap_refuse = result;
        else
            l->chap_require = result;
        break;
    case CONTEXT_LNS:
        if (c[0] == 'p')        /* PAP */
            if (word[2] == 'f')
                n->pap_refuse = result;
            else
                n->pap_require = result;
        else if (c[0] == 'a')   /* Authentication */
            if (word[2] == 'f')
                n->authself = !result;
            else
                n->authpeer = result;
        else /* CHAP */ if (word[2] == 'f')
            n->chap_refuse = result;
        else
            n->chap_require = result;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_redial (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        if (set_boolean (word, value, &(((struct lac *) item)->redial)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_accesscontrol (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->accesscontrol)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_userspace (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->forceuserspace)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_debugavp (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->debug_avp)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_debugnetwork (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->debug_network)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_debugpacket (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->packet_dump)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_debugtunnel (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->debug_tunnel)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_debugstate (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
        if (set_boolean
            (word, value, &(((struct global *) item)->debug_state)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_assignip (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->assign_ip)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

struct iprange *set_range (char *word, char *value, struct iprange *in)
{
    char *c, *d = NULL, *e = NULL;
    struct iprange *ipr, *p;
    struct hostent *hp;
	int count = 0;
    c = strchr (value, '-');
    if (c)
    {
        d = c + 1;
        *c = 0;
        while ((c >= value) && (*c < 33))
            *(c--) = 0;
        while (*d && (*d < 33))
            d++;
    }
    if (!strlen (value) || (c && !strlen (d)))
    {
        snprintf (filerr, sizeof (filerr),
                  "format is '%s <host or ip> - <host or ip>'\n", word);
        return NULL;
    }
    ipr = (struct iprange *) malloc (sizeof (struct iprange));
    ipr->next = NULL;
    hp = gethostbyname (value);
    if (!hp)
    {
        snprintf (filerr, sizeof (filerr), "Unknown host %s\n", value);
        free (ipr);
        return NULL;
    }
    bcopy (hp->h_addr, &ipr->start, sizeof (unsigned int));
    if (c)
    {
		e = d;
		while(*e != '\0') {
			if (*e++ == '.')
				count++;
		}
		if (count != 3) {
			char ip_hi[16];

			strcpy(ip_hi, value);
			e = strrchr(ip_hi, '.')+1;
			/* Copy the last field + null terminator */
			strcpy(e, d);
			d = ip_hi;
		}
        hp = gethostbyname (d);
        if (!hp)
        {
            snprintf (filerr, sizeof (filerr), "Unknown host %s\n", d);
            free (ipr);
            return NULL;
        }
        bcopy (hp->h_addr, &ipr->end, sizeof (unsigned int));
    }
    else
        ipr->end = ipr->start;
    if (ntohl (ipr->start) > ntohl (ipr->end))
    {
        snprintf (filerr, sizeof (filerr), "start is greater than end!\n");
        free (ipr);
        return NULL;
    }
    if (word[0] == 'n')
        ipr->sense = SENSE_DENY;
    else
        ipr->sense = SENSE_ALLOW;
    p = in;
    if (p)
    {
        while (p->next)
            p = p->next;
        p->next = ipr;
        return in;
    }
    else
        return ipr;
}

int set_iprange (char *word, char *value, int context, void *item)
{
    struct lns *lns = (struct lns *) item;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    lns->range = set_range (word, value, lns->range);
    if (!lns->range)
        return -1;
#ifdef DEBUG_FILE
    l2tp_log (LOG_DEBUG, "range start = %x, end = %x, sense=%ud\n",
         ntohl (lns->range->start), ntohl (lns->range->end), lns->range->sense);
#endif
    return 0;
}

int set_lac (char *word, char *value, int context, void *item)
{
    struct lns *lns = (struct lns *) item;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    lns->lacs = set_range (word, value, lns->lacs);
    if (!lns->lacs)
        return -1;
#ifdef DEBUG_FILE
    l2tp_log (LOG_DEBUG, "lac start = %x, end = %x, sense=%ud\n",
         ntohl (lns->lacs->start), ntohl (lns->lacs->end), lns->lacs->sense);
#endif
    return 0;
}

int set_exclusive (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LNS:
        if (set_boolean (word, value, &(((struct lns *) item)->exclusive)))
            return -1;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_ip (char *word, char *value, unsigned int *addr)
{
    struct hostent *hp;
    hp = gethostbyname (value);
    if (!hp)
    {
        snprintf (filerr, sizeof (filerr), "%s: host '%s' not found\n",
                  __FUNCTION__, value);
        return -1;
    }
    bcopy (hp->h_addr, addr, sizeof (unsigned int));
    return 0;
}

int set_listenaddr (char *word, char *value, int context, void *item)
{
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
#ifdef DEBUG_FILE
        l2tp_log (LOG_DEBUG, "set_listenaddr: Setting listen address to %s\n",
             value);
#endif
        if (set_ip (word, value, &(((struct global *) item)->listenaddr)))
		return -1;
	break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_localaddr (char *word, char *value, int context, void *item)
{
    struct lac *l;
    struct lns *n;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        l = (struct lac *) item;
        return set_ip (word, value, &(l->localaddr));
    case CONTEXT_LNS:
        n = (struct lns *) item;
        return set_ip (word, value, &(n->localaddr));
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_remoteaddr (char *word, char *value, int context, void *item)
{
    struct lac *l;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
        l = (struct lac *) item;
        return set_ip (word, value, &(l->remoteaddr));
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_lns (char *word, char *value, int context, void *item)
{
#if 0
    struct hostent *hp;
#endif
    struct lac *l;
    struct host *ipr, *pos;
    char *d;
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_LAC:
#ifdef DEBUG_FILE
        l2tp_log (LOG_DEBUG, "set_lns: setting LNS to '%s'\n", value);
#endif
        l = (struct lac *) item;
        d = strchr (value, ':');
        if (d)
        {
            d[0] = 0;
            d++;
        }
#if 0 
		// why would you want to lookup hostnames at this time? 
        hp = gethostbyname (value);
        if (!hp)
        {
            snprintf (filerr, sizeof (filerr), "no such host '%s'\n", value);
            return -1;
        }
#endif
        ipr = malloc (sizeof (struct host));
        ipr->next = NULL;
        pos = l->lns;
        if (!pos)
        {
            l->lns = ipr;
        }
        else
        {
            while (pos->next)
                pos = pos->next;
            pos->next = ipr;
        }
        strncpy (ipr->hostname, value, sizeof (ipr->hostname));
        if (d)
            ipr->port = atoi (d);
        else
            ipr->port = UDP_LISTEN_PORT;
        break;
    default:
        snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
                  word);
        return -1;
    }
    return 0;
}

int set_rand_sys ()
{
    l2tp_log(LOG_WARNING, "The \"rand()\" function call is not a very good source"
            "of randomness\n");
    rand_source = RAND_SYS;
    return 0;
}

int set_ipsec_saref (char *word, char *value, int context, void *item)
{
	struct global *g = ((struct global *) item);
    switch (context & ~CONTEXT_DEFAULT)
    {
    case CONTEXT_GLOBAL:
	    if (set_boolean
		(word, value, &(g->ipsecsaref)))
		    return -1;
	    if(g->ipsecsaref) {
		    l2tp_log(LOG_WARNING, "Enabling IPsec SAref processing for L2TP transport mode SAs\n");
	    }
	    if(g->forceuserspace != 1) {
		    l2tp_log(LOG_WARNING, "IPsec SAref does not work with L2TP kernel mode yet, enabling forceuserspace=yes\n");
	    }
	    break;
    default:
	    snprintf (filerr, sizeof (filerr), "'%s' not valid in this context\n",
		      word);
	    return -1;
    }
    return 0;
}

int set_rand_dev ()
{
    rand_source = RAND_DEV;
    return 0;
}

int set_rand_egd (char *value)
{
    l2tp_log(LOG_WARNING, "%s: not yet implemented!\n", __FUNCTION__);
    rand_source = RAND_EGD;
    return -1;
}

int set_rand_source (char *word, char *value, int context, void *item)
{
    time_t seconds;
    /*
     * We're going to go ahead and seed the rand() function with srand()
     * because even if we set the randomness source to dev or egd, they
     * can fall back to sys if they fail, so we want to make sure we at
     * least have *some* semblance of randomness available from the
     * rand() function
     */
    /*
     * This is a sucky random number seed...just the result from the
     * time() call...but...the user requested to use the rand()
     * function, which is a pretty sucky source of randomness
     * regardless...at least we can get a almost sorta decent seed.  If
     * you have any better suggestions for creating a seed...lemme know
     * :/
     */
    seconds = time(NULL);
    srand(seconds);
 
    if (context != CONTEXT_GLOBAL)
    {
        l2tp_log(LOG_WARNING, "%s: %s not valid in context %d\n",
                __FUNCTION__, word, context);
        return -1;
    }
    /* WORKING HERE */
    if (strlen(value) == 0)
    {
        snprintf(filerr, sizeof (filerr), "no randomness source specified\n");
        return -1;
    }
    if (strncmp(value, "egd", 3) == 0)
    {
        return set_rand_egd(value);
    }
    else if (strncmp(value, "dev", 3) == 0)
    {
        return set_rand_dev();
    }
    else if (strncmp(value, "sys", 3) == 0)
    {
        return set_rand_sys();
    }
    else
    {
        l2tp_log(LOG_WARNING, "%s: %s is not a valid randomness source\n",
                __FUNCTION__, value);
        return -1;

    }
}

int parse_config (FILE * f)
{
    /* Read in the configuration file handed to us */
    /* FIXME: I should check for incompatible options */
    int context = 0;
    char buf[STRLEN];
    char *s, *d, *t;
    int linenum = 0;
    int def = 0;
    struct keyword *kw;
    void *data = NULL;
    struct lns *tl;
    struct lac *tc;
    while (!feof (f))
    {
        fgets (buf, sizeof (buf), f);
        if (feof (f))
            break;
        linenum++;
        s = buf;
        /* Strip comments */
        while (*s && *s != ';')
            s++;
        *s = 0;
        s = buf;
        if (!strlen (buf))
            continue;
        while ((*s < 33) && *s)
            s++;                /* Skip over beginning white space */
        t = s + strlen (s);
        while ((t >= s) && (*t < 33))
            *(t--) = 0;         /* Ditch trailing white space */
        if (!strlen (s))
            continue;
        if (s[0] == '[')
        {
            /* We've got a context description */
            if (!(t = strchr (s, ']')))
            {
                l2tp_log (LOG_CRIT, "parse_config: line %d: No closing bracket\n",
                     linenum);
                return -1;
            }
            t[0] = 0;
            s++;
            if ((d = strchr (s, ' ')))
            {
                /* There's a parameter */
                d[0] = 0;
                d++;
            }
            if (d && !strcasecmp (d, "default"))
                def = CONTEXT_DEFAULT;
            else
                def = 0;
            if (!strcasecmp (s, "global"))
            {
                context = CONTEXT_GLOBAL;
#ifdef DEBUG_FILE
                l2tp_log (LOG_DEBUG,
                     "parse_config: global context descriptor %s\n",
                     d ? d : "");
#endif
                data = &gconfig;
            }
            else if (!strcasecmp (s, "lns"))
            {
                context = CONTEXT_LNS;
                if (def)
                {
                    if (!deflns)
                    {
                        deflns = new_lns ();
                        strncpy (deflns->entname, "default",
                                 sizeof (deflns->entname));
                    }
                    data = deflns;
                    continue;
                }
                data = NULL;
                tl = lnslist;
                if (d)
                {
                    while (tl)
                    {
                        if (!strcasecmp (d, tl->entname))
                            break;
                        tl = tl->next;
                    }
                    if (tl)
                        data = tl;
                }
                if (!data)
                {
                    data = new_lns ();
                    if (!data)
                        return -1;
                    ((struct lns *) data)->next = lnslist;
                    lnslist = (struct lns *) data;
                }
                if (d)
                    strncpy (((struct lns *) data)->entname,
                             d, sizeof (((struct lns *) data)->entname));
#ifdef DEBUG_FILE
                l2tp_log (LOG_DEBUG, "parse_config: lns context descriptor %s\n",
                     d ? d : "");
#endif
            }
            else if (!strcasecmp (s, "lac"))
            {
                context = CONTEXT_LAC;
                if (def)
                {
                    if (!deflac)
                    {
                        deflac = new_lac ();
                        strncpy (deflac->entname, "default",
                                 sizeof (deflac->entname));
                    }
                    data = deflac;
                    continue;
                }
                data = NULL;
                tc = laclist;
                if (d)
                {
                    while (tc)
                    {
                        if (!strcasecmp (d, tc->entname))
                            break;
                        tc = tc->next;
                    }
                    if (tc)
                        data = tc;
                }
                if (!data)
                {
                    data = new_lac ();
                    if (!data)
                        return -1;
                    ((struct lac *) data)->next = laclist;
                    laclist = (struct lac *) data;
                }
                if (d)
                    strncpy (((struct lac *) data)->entname,
                             d, sizeof (((struct lac *) data)->entname));
#ifdef DEBUG_FILE
                l2tp_log (LOG_DEBUG, "parse_config: lac context descriptor %s\n",
                     d ? d : "");
#endif
            }
            else
            {
                l2tp_log (LOG_WARNING,
                     "parse_config: line %d: unknown context '%s'\n", linenum,
                     s);
                return -1;
            }
        }
        else
        {
            if (!context)
            {
                l2tp_log (LOG_WARNING,
                     "parse_config: line %d: data '%s' occurs with no context\n",
                     linenum, s);
                return -1;
            }
            if (!(t = strchr (s, '=')))
            {
                l2tp_log (LOG_WARNING, "parse_config: line %d: no '=' in data\n",
                     linenum);
                return -1;
            }
            d = t;
            d--;
            t++;
            while ((d >= s) && (*d < 33))
                d--;
            d++;
            *d = 0;
            while (*t && (*t < 33))
                t++;
#ifdef DEBUG_FILE
            l2tp_log (LOG_DEBUG, "parse_config: field is %s, value is %s\n", s, t);
#endif
            /* Okay, bit twidling is done.  Let's handle this */
            for (kw = words; kw->keyword; kw++)
            {
                if (!strcasecmp (s, kw->keyword))
                {
                    if (kw->handler (s, t, context | def, data))
                    {
                        l2tp_log (LOG_WARNING, "parse_config: line %d: %s", linenum,
                             filerr);
                        return -1;
                    }
                    break;
                }
            }
            if (!kw->keyword)
            {
                l2tp_log (LOG_CRIT, "parse_config: line %d: Unknown field '%s'\n",
                     linenum, s);
                return -1;
            }
        }
    }
    return 0;
}

struct keyword words[] = {
    {"listen-addr", &set_listenaddr},
    {"port", &set_port},
    {"rand source", &set_rand_source},
    {"auth file", &set_authfile},
    {"exclusive", &set_exclusive},
    {"autodial", &set_autodial},
    {"redial", &set_redial},
    {"redial timeout", &set_rtimeout},
    {"lns", &set_lns},
    {"max redials", &set_rmax},
    {"access control", &set_accesscontrol},
    {"force userspace", &set_userspace},
    {"ip range", &set_iprange},
    {"no ip range", &set_iprange},
    {"debug avp", &set_debugavp},
    {"debug network", &set_debugnetwork},
    {"debug packet", &set_debugpacket},
    {"debug tunnel", &set_debugtunnel},
    {"debug state", &set_debugstate},
    {"ipsec saref", &set_ipsec_saref},
    {"lac", &set_lac},
    {"no lac", &set_lac},
    {"assign ip", &set_assignip},
    {"local ip", &set_localaddr},
    {"remote ip", &set_remoteaddr},
    {"defaultroute", &set_defaultroute},
    {"length bit", &set_lbit},
    {"hidden bit", &set_hbit},
    {"require pap", &set_papchap},
    {"require chap", &set_papchap},
    {"require authentication", &set_papchap},
    {"require auth", &set_papchap},
    {"refuse pap", &set_papchap},
    {"refuse chap", &set_papchap},
    {"refuse authentication", &set_papchap},
    {"refuse auth", &set_papchap},
    {"unix authentication", &set_passwdauth},
    {"unix auth", &set_passwdauth},
    {"name", &set_authname},
    {"hostname", &set_hostname},
    {"ppp debug", &set_debug},
    {"pppoptfile", &set_pppoptfile},
    {"call rws", &set_rws},
    {"tunnel rws", &set_rws},
    {"flow bit", &set_flow},
    {"challenge", &set_challenge},
    {"tx bps", &set_speed},
    {"rx bps", &set_speed},
    {"bps", &set_speed},
    {NULL, NULL}
};
