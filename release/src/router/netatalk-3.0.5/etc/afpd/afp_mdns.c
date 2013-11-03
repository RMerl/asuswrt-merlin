/*
 * Author:   Lee Essen <lee.essen@nowonline.co.uk>
 * Based on: avahi support from Daniel S. Haischt <me@daniel.stefan.haischt.name>
 * Purpose:  mdns based Zeroconf support
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MDNS

#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <poll.h>

#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/dsi.h>
#include <atalk/unicode.h>
#include <atalk/netatalk_conf.h>

#include "afp_zeroconf.h"
#include "afp_mdns.h"

/*
 * We'll store all the DNSServiceRef's here so that we can
 * deallocate them later
 */
static DNSServiceRef   *svc_refs = NULL;
static int             svc_ref_count = 0;
static pthread_t       poller;

/*
 * Its easier to use asprintf to set the TXT record values
 */
#define TXTRecordPrintf(rec, key, args...) {            \
        char *str;                                      \
        asprintf(&str, args);                           \
        TXTRecordSetValue(rec, key, strlen(str), str);  \
        free(str);                                      \
    }
#define TXTRecordKeyPrintf(rec, k, var, args...) {      \
        char *key, *str;                                \
        asprintf(&key, k, var);                         \
        asprintf(&str, args);                           \
        TXTRecordSetValue(rec, key, strlen(str), str);  \
        free(str); free(key);                           \
    }

static struct pollfd *fds;

/*
 * This is the thread that polls the filehandles
 */
static void *polling_thread(void *arg) {
    // First we loop through getting the filehandles and adding them to our poll, we
    // need to allocate our pollfd's
    DNSServiceErrorType error;
    fds = calloc(svc_ref_count, sizeof(struct pollfd));
    assert(fds);

    for(int i=0; i < svc_ref_count; i++) {
        int fd = DNSServiceRefSockFD(svc_refs[i]);
        fds[i].fd = fd;
        fds[i].events = POLLIN;
    }

    // Now we can poll and process the results...
    while(poll(fds, svc_ref_count, -1) > 0) {
        for(int i=0; i < svc_ref_count; i++) {
            if(fds[i].revents & POLLIN) {
                error = DNSServiceProcessResult(svc_refs[i]);
            }
        }
    }
    return(NULL);
}

/*
 * This is the callback for the service register function ... actually there isn't a lot
 * we can do if we get problems, so we don't really need to do anything other than report
 * the issue.
 */
static void RegisterReply(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
                          const char *name, const char *regtype, const char *domain, void *context)
{
    if (errorCode != kDNSServiceErr_NoError) {
        LOG(log_error, logtype_afpd, "Failed to register mDNS service: %s%s%s: code=%d",
            name, regtype, domain, errorCode);
    }
}

/*
 * This function unregisters anything we have already
 * registered and frees associated memory
 */
static void unregister_stuff() {
    pthread_cancel(poller);    

    for (int i = 0; i < svc_ref_count; i++)
        close(fds[i].fd);
    free(fds);
    fds = NULL;

    if(svc_refs) {
        for(int i=0; i < svc_ref_count; i++) {
            DNSServiceRefDeallocate(svc_refs[i]);
        }
        free(svc_refs);
        svc_refs = NULL;
        svc_ref_count = 0;
    }
}

/*
 * This function tries to register the AFP DNS
 * SRV service type.
 */
static void register_stuff(const AFPObj *obj) {
    uint                                        port;
    const struct vol                *volume;
    DSI                                         *dsi;
    char                                        name[MAXINSTANCENAMELEN+1];
    DNSServiceErrorType         error;
    TXTRecordRef                        txt_adisk;
    TXTRecordRef                        txt_devinfo;
    char                                        tmpname[256];

    // If we had already registered, then we will unregister and re-register
    if(svc_refs) unregister_stuff();

    /* Register our service, prepare the TXT record */
    TXTRecordCreate(&txt_adisk, 0, NULL);
    TXTRecordPrintf(&txt_adisk, "sys", "waMa=0,adVF=0x100");

    /* Build AFP volumes list */
    int i = 0;

    for (volume = getvolumes(); volume; volume = volume->v_next) {

        if (convert_string(CH_UCS2, CH_UTF8_MAC, volume->v_u8mname, -1, tmpname, 255) <= 0) {
            LOG ( log_error, logtype_afpd, "Could not set Zeroconf volume name for TimeMachine");
            goto fail;
        }

        if (volume->v_flags & AFPVOL_TM) {
            if (volume->v_uuid) {
                LOG(log_info, logtype_afpd, "Registering volume '%s' with UUID: '%s' for TimeMachine",
                    volume->v_localname, volume->v_uuid);
                TXTRecordKeyPrintf(&txt_adisk, "dk%u", i++, "adVN=%s,adVF=0xa1,adVU=%s",
                                   tmpname, volume->v_uuid);
            } else {
                LOG(log_warning, logtype_afpd, "Registering volume '%s' for TimeMachine. But UUID is invalid.",
                    volume->v_localname);
                TXTRecordKeyPrintf(&txt_adisk, "dk%u", i++, "adVN=%s,adVF=0xa1", tmpname);
            }
        }
    }

    // Now we can count the configs so we know how many service
    // records to allocate
    for (dsi = obj->dsi; dsi; dsi = dsi->next) {
        svc_ref_count++;                    // AFP_DNS_SERVICE_TYPE
        if (i) svc_ref_count++;      // ADISK_SERVICE_TYPE
        if (obj->options.mimicmodel) svc_ref_count++;        // DEV_INFO_SERVICE_TYPE
    }

    // Allocate the memory to store our service refs
    svc_refs = calloc(svc_ref_count, sizeof(DNSServiceRef));
    assert(svc_ref);
    svc_ref_count = 0;

    /* AFP server */
    for (dsi = obj->dsi; dsi; dsi = dsi->next) {

        port = getip_port((struct sockaddr *)&dsi->server);

        if (convert_string(obj->options.unixcharset,
                           CH_UTF8,
                           obj->options.hostname,
                           -1,
                           name,
                           MAXINSTANCENAMELEN) <= 0) {
            LOG(log_error, logtype_afpd, "Could not set Zeroconf instance name");
            goto fail;
        }
        if ((dsi->bonjourname = strdup(name)) == NULL) {
            LOG(log_error, logtype_afpd, "Could not set Zeroconf instance name");
            goto fail;

        }
        LOG(log_info, logtype_afpd, "Registering server '%s' with Bonjour",
            dsi->bonjourname);

        error = DNSServiceRegister(&svc_refs[svc_ref_count++],
                                   0,               // no flags
                                   0,               // all network interfaces
                                   dsi->bonjourname,
                                   AFP_DNS_SERVICE_TYPE,
                                   "",            // default domains
                                   NULL,            // default host name
                                   htons(port),
                                   0,               // length of TXT
                                   NULL,            // no TXT
                                   RegisterReply,           // callback
                                   NULL);       // no context
        if(error != kDNSServiceErr_NoError) {
            LOG(log_error, logtype_afpd, "Failed to add service: %s, error=%d",
                AFP_DNS_SERVICE_TYPE, error);
            goto fail;
        }

        if(i) {
            error = DNSServiceRegister(&svc_refs[svc_ref_count++],
                                       0,               // no flags
                                       0,               // all network interfaces
                                       dsi->bonjourname,
                                       ADISK_SERVICE_TYPE,
                                       "",            // default domains
                                       NULL,            // default host name
                                       htons(port),
                                       TXTRecordGetLength(&txt_adisk),
                                       TXTRecordGetBytesPtr(&txt_adisk),
                                       RegisterReply,           // callback
                                       NULL);       // no context
            if(error != kDNSServiceErr_NoError) {
                LOG(log_error, logtype_afpd, "Failed to add service: %s, error=%d",
                    ADISK_SERVICE_TYPE, error);
                goto fail;
            }
        }

        if (obj->options.mimicmodel) {
            LOG(log_info, logtype_afpd, "Registering server '%s' with model '%s'",
                dsi->bonjourname, obj->options.mimicmodel);
            TXTRecordCreate(&txt_devinfo, 0, NULL);
            TXTRecordPrintf(&txt_devinfo, "model", obj->options.mimicmodel);
            error = DNSServiceRegister(&svc_refs[svc_ref_count++],
                                       0,               // no flags
                                       0,               // all network interfaces
                                       dsi->bonjourname,
                                       DEV_INFO_SERVICE_TYPE,
                                       "",            // default domains
                                       NULL,            // default host name
                                       /*
                                        * We would probably use port 0 zero, but we can't, from man DNSServiceRegister:
                                        *   "A value of 0 for a port is passed to register placeholder services.
                                        *    Place holder services are not found  when browsing, but other
                                        *    clients cannot register with the same name as the placeholder service."
                                        * We therefor use port 9 which is used by the adisk service type.
                                        */
                                       htons(9),
                                       TXTRecordGetLength(&txt_devinfo),
                                       TXTRecordGetBytesPtr(&txt_devinfo),
                                       RegisterReply,           // callback
                                       NULL);       // no context
            TXTRecordDeallocate(&txt_devinfo);
            if(error != kDNSServiceErr_NoError) {
                LOG(log_error, logtype_afpd, "Failed to add service: %s, error=%d",
                    DEV_INFO_SERVICE_TYPE, error);
                goto fail;
            }
        } /* if (config->obj.options.mimicmodel) */
    }   /* for config*/

        /*
         * Now we can create the thread that will poll for the results
         * and handle the calling of the callbacks
         */
    if(pthread_create(&poller, NULL, polling_thread, NULL) != 0) {
        LOG(log_error, logtype_afpd, "Unable to start mDNS polling thread");
        goto fail;
    }

fail:
    TXTRecordDeallocate(&txt_adisk);
    return;
}

/************************************************************************
 * Public funcions
 ************************************************************************/

/*
 * Tries to setup the Zeroconf thread and any
 * neccessary config setting.
 */
void md_zeroconf_register(const AFPObj *obj) {
    int error;

    register_stuff(obj);
    return;
}

/*
 * Tries to shutdown this loop impl.
 * Call this function from inside this thread.
 */
int md_zeroconf_unregister() {
    unregister_stuff();
    return 0;
}

#endif /* USE_MDNS */

