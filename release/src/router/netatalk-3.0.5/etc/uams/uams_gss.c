/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 1999 Adrian Sun (asun@u.washington.edu)
 * Copyright (c) 2003 The Reed Institute
 * Copyright (c) 2004 Bjoern Fernhomberg
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/uam.h>
#include <atalk/util.h>
#include <atalk/compat.h>

/* Kerberos includes */
#ifdef HAVE_GSSAPI_GSSAPI_H
#include <gssapi/gssapi.h>
#else
#include <gssapi.h>
#endif // HAVE_GSSAPI_GSSAPI_H

#define LOG_UAMS(log_level, ...) \
    LOG(log_level, logtype_uams, __VA_ARGS__)

#define LOG_LOGINCONT(log_level, ...) \
    LOG_UAMS(log_level, "FPLoginCont: " __VA_ARGS__)

static void log_status(char *s,
                       OM_uint32 major_status,
                       OM_uint32 minor_status)
{
    gss_buffer_desc msg = GSS_C_EMPTY_BUFFER;
    OM_uint32 min_status, maj_status;
    OM_uint32 maj_ctx = 0, min_ctx = 0;

    while (1) {
        maj_status = gss_display_status( &min_status, major_status,
                                         GSS_C_GSS_CODE, GSS_C_NULL_OID,
                                         &maj_ctx, &msg );
        LOG_UAMS(log_error, "%s %.*s (error %s)",
                 s, msg.length, msg.value, strerror(errno));
        gss_release_buffer(&min_status, &msg);

        if (!maj_ctx)
            break;
    }

    while (1) {
        maj_status = gss_display_status( &min_status, minor_status,
                                         GSS_C_MECH_CODE, GSS_C_NULL_OID,
                                         &min_ctx, &msg );
        LOG_UAMS(log_error, "%s %.*s (error %s)",
                 s, msg.length, msg.value, strerror(errno));
        gss_release_buffer(&min_status, &msg);

        if (!min_ctx)
            break;
    }
}

static void log_ctx_flags(OM_uint32 flags)
{
    if (flags & GSS_C_DELEG_FLAG)
        LOG_LOGINCONT(log_debug, "context flag: GSS_C_DELEG_FLAG");
    if (flags & GSS_C_MUTUAL_FLAG)
        LOG_LOGINCONT(log_debug, "context flag: GSS_C_MUTUAL_FLAG");
    if (flags & GSS_C_REPLAY_FLAG)
        LOG_LOGINCONT(log_debug, "context flag: GSS_C_REPLAY_FLAG");
    if (flags & GSS_C_SEQUENCE_FLAG)
        LOG_LOGINCONT(log_debug, "context flag: GSS_C_SEQUENCE_FLAG");
    if (flags & GSS_C_CONF_FLAG)
        LOG_LOGINCONT(log_debug, "context flag: GSS_C_CONF_FLAG");
    if (flags & GSS_C_INTEG_FLAG)
        LOG_LOGINCONT(log_debug, "context flag: GSS_C_INTEG_FLAG");
}

static void log_service_name(gss_ctx_id_t context)
{
    OM_uint32 major_status = 0, minor_status = 0;
    gss_name_t service_name;
    gss_buffer_desc service_name_buffer;

    major_status = gss_inquire_context(&minor_status,
                                       context,
                                       NULL,
                                       &service_name,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);
    if (major_status != GSS_S_COMPLETE) {
        log_status("gss_inquire_context", major_status, minor_status);
        return;
    }

    major_status = gss_display_name(&minor_status,
                                    service_name,
                                    &service_name_buffer,
                                    NULL);
    if (major_status == GSS_S_COMPLETE) {
        LOG_LOGINCONT(log_debug,
                      "service principal is `%s'",
                      service_name_buffer.value);

        gss_release_buffer(&minor_status, &service_name_buffer);
    } else
        log_status("gss_display_name", major_status, minor_status);

    gss_release_name(&minor_status, &service_name);
}

static int get_client_username(char *username,
                               int ulen,
                               gss_name_t *client_name)
{
    OM_uint32 major_status = 0, minor_status = 0;
    gss_buffer_desc client_name_buffer;
    char *p;
    int ret = 0;

    /*
     * To extract the unix username, use gss_display_name on client_name.
     * We do rely on gss_display_name returning a zero terminated string.
     * The username returned contains the realm and possibly an instance.
     * We only want the username for afpd, so we have to strip those from
     * the username before copying it to afpd's buffer.
     */

    major_status = gss_display_name(&minor_status,
                                    *client_name,
                                    &client_name_buffer,
                                    NULL);
    if (major_status != GSS_S_COMPLETE) {
        log_status("gss_display_name", major_status, minor_status);
        return 1;
    }

    LOG_LOGINCONT(log_debug,
                  "user principal is `%s'",
                  client_name_buffer.value);

    /* chop off realm */
    p = strchr(client_name_buffer.value, '@');
    if (p)
        *p = 0;
    /* FIXME: chop off instance? */
    p = strchr(client_name_buffer.value, '/');
    if (p)
        *p = 0;

    /* check if this username fits into afpd's username buffer */
    size_t cnblen = strlen(client_name_buffer.value);
    if (cnblen >= ulen) {
        /* The username is too long for afpd's buffer, bail out */
        LOG_LOGINCONT(log_info,
                      "username `%s' too long (%d)",
                      client_name_buffer.value, cnblen);
        ret = 1;
    } else {
        /* copy stripped username to afpd's buffer */
        strlcpy(username, client_name_buffer.value, ulen);
    }

    gss_release_buffer(&minor_status, &client_name_buffer);

    return ret;
}

/* wrap afpd's sessionkey */
static int wrap_sessionkey(gss_ctx_id_t context, struct session_info *sinfo)
{
    OM_uint32 major_status = 0, minor_status = 0;
    gss_buffer_desc sesskey_buff, wrap_buff;
    int ret = 0;

    /*
     * gss_wrap afpd's session_key.
     * This is needed fo OS X 10.3 clients. They request this information
     * with type 8 (kGetKerberosSessionKey) on FPGetSession.
     * See AFP 3.1 specs, page 77.
     */
    sesskey_buff.value = sinfo->sessionkey;
    sesskey_buff.length = sinfo->sessionkey_len;

    /* gss_wrap the session key with the default mechanism.
       Require both confidentiality and integrity services */
    major_status = gss_wrap(&minor_status,
                            context,
                            true,
                            GSS_C_QOP_DEFAULT,
                            &sesskey_buff,
                            NULL,
                            &wrap_buff);

    if (major_status != GSS_S_COMPLETE) {
        log_status("gss_wrap", major_status, minor_status);
        return 1;
    }

    /* store the wrapped session key in afpd's session_info struct */
    if (NULL == (sinfo->cryptedkey = malloc(wrap_buff.length))) {
        LOG_UAMS(log_error,
                 "wrap_sessionkey: out of memory tyring to allocate %u bytes",
                 wrap_buff.length);
        ret = 1;
    } else {
        /* cryptedkey is binary data */
        memcpy(sinfo->cryptedkey, wrap_buff.value, wrap_buff.length);
        sinfo->cryptedkey_len = wrap_buff.length;
    }

    /* we're done with buffer, release */
    gss_release_buffer(&minor_status, &wrap_buff);

    return ret;
}

static int accept_sec_context(gss_ctx_id_t *context,
                              gss_buffer_desc *ticket_buffer,
                              gss_name_t *client_name,
                              gss_buffer_desc *authenticator_buff)
{
    OM_uint32 major_status = 0, minor_status = 0, flags = 0;

    /* Initialize autheticator buffer. */
    authenticator_buff->length = 0;
    authenticator_buff->value = NULL;

    LOG_LOGINCONT(log_debug,
                  "accepting context (ticketlen: %u)",
                  ticket_buffer->length);

    /*
     * Try to accept the secondary context using the token in ticket_buffer.
     * We don't care about the principals or mechanisms used, nor for the time.
     * We don't act as a proxy either.
     */
    major_status = gss_accept_sec_context(&minor_status,
                                          context,
                                          GSS_C_NO_CREDENTIAL,
                                          ticket_buffer,
                                          GSS_C_NO_CHANNEL_BINDINGS,
                                          client_name,
                                          NULL,
                                          authenticator_buff,
                                          &flags,
                                          NULL,
                                          NULL);

    if (major_status != GSS_S_COMPLETE) {
        log_status("gss_accept_sec_context", major_status, minor_status);
        return 1;
    }

    log_ctx_flags(flags);
    return 0;
}

static int do_gss_auth(void *obj,
                       char *ibuf, size_t ibuflen,
                       char *rbuf, int *rbuflen,
                       char *username, size_t ulen,
                       struct session_info *sinfo )
{
    OM_uint32 status = 0;
    gss_name_t client_name;
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_buffer_desc ticket_buffer, authenticator_buff;
    int ret = 0;

    /*
     * Try to accept the secondary context, using the ticket/token the
     * client sent us. Ticket is stored at current ibuf position.
     * Don't try to release ticket_buffer later, it points into ibuf!
     */
    ticket_buffer.length = ibuflen;
    ticket_buffer.value = ibuf;

    if ((ret = accept_sec_context(&context,
                                  &ticket_buffer,
                                  &client_name,
                                  &authenticator_buff)))
        return ret;
    log_service_name(context);

    /* We succesfully acquired the secondary context, now get the
       username for afpd and gss_wrap the sessionkey */
    if ((ret = get_client_username(username, ulen, &client_name)))
        goto cleanup_client_name;

    if ((ret = wrap_sessionkey(context, sinfo)))
        goto cleanup_client_name;

    /* Authenticated, construct the reply using:
     * authenticator length (uint16_t)
     * authenticator
     */
    /* copy the authenticator length into the reply buffer */
    uint16_t auth_len = htons(authenticator_buff.length);
    memcpy(rbuf, &auth_len, sizeof(auth_len));
    *rbuflen += sizeof(auth_len);
    rbuf += sizeof(auth_len);

    /* copy the authenticator value into the reply buffer */
    memcpy(rbuf, authenticator_buff.value, authenticator_buff.length);
    *rbuflen += authenticator_buff.length;

cleanup_client_name:
    gss_release_name(&status, &client_name);
    gss_release_buffer(&status, &authenticator_buff);
    gss_delete_sec_context(&status, &context, NULL);

    return ret;
}

/* -------------------------- */

/*
 * For the gss uam, this function only needs to return a two-byte
 * login-session id. None of the data provided by the client up to this
 * point is trustworthy as we'll have a signed ticket to parse in logincont.
 */
static int gss_login(void *obj,
                     struct passwd **uam_pwd,
                     char *ibuf, size_t ibuflen,
                     char *rbuf, size_t *rbuflen)
{
    *rbuflen = 0;

    /* The reply contains a two-byte ID value - note
     * that Apple's implementation seems to always return 1 as well
     */
    uint16_t temp16 = htons(1);
    memcpy(rbuf, &temp16, sizeof(temp16));
    *rbuflen += sizeof(temp16);

    return AFPERR_AUTHCONT;
}

static int gss_logincont(void *obj,
                         struct passwd **uam_pwd,
                         char *ibuf, size_t ibuflen,
                         char *rbuf, size_t *rbuflen)
{
    struct passwd *pwd = NULL;
    uint16_t login_id;
    char *username;
    uint16_t ticket_len;
    char *p;
    int rblen;
    size_t userlen;
    struct session_info *sinfo;

    /* Apple's AFP 3.1 documentation specifies that this command
     * takes the following format:
     * pad (byte)
     * id returned in LoginExt response (uint16_t)
     * username (format unspecified)
     *   padded, when necessary, to end on an even boundary
     * ticket length (uint16_t)
     * ticket
     */

    /* Observation of AFP clients in the wild indicate that the actual
     * format of this request is as follows:
     * pad (byte) [consumed before login_ext is called]
     * ?? (byte) - always observed to be 0
     * id returned in LoginExt response (uint16_t)
     * username, encoding unspecified, null terminated C string,
     *   padded when the terminating null is an even numbered byte.
     *   The packet is formated such that the username begins on an
     *   odd numbered byte. Eg if the username is 3 characters and the
     *   terminating null makes 4, expect to pad the the result.
     *   The encoding of this string is unknown.
     * ticket length (uint16_t)
     * ticket
     */

    rblen = *rbuflen = 0;

    if (ibuflen < 1 +sizeof(login_id)) {
        LOG_LOGINCONT(log_info, "received incomplete packet");
        return AFPERR_PARAM;
    }
    ibuf++, ibuflen--; /* ?? */

    /* 2 byte ID from LoginExt -- always '00 01' in this implementation */
    memcpy(&login_id, ibuf, sizeof(login_id));
    ibuf += sizeof(login_id), ibuflen -= sizeof(login_id);
    login_id = ntohs(login_id);

    /* get the username buffer from apfd */
    if (uam_afpserver_option(obj, UAM_OPTION_USERNAME, &username, &userlen) < 0)
        return AFPERR_MISC;

    /* get the session_info structure from afpd. We need the session key */
    if (uam_afpserver_option(obj, UAM_OPTION_SESSIONINFO, &sinfo, NULL) < 0)
        return AFPERR_MISC;

    if (sinfo->sessionkey == NULL || sinfo->sessionkey_len == 0) {
        /* Should never happen. Most likely way too old afpd version */
        LOG_LOGINCONT(log_error, "internal error: afpd's sessionkey not set");
        return AFPERR_MISC;
    }

    /* We skip past the 'username' parameter because all that matters is the ticket */
    p = ibuf;
    while ( *ibuf && ibuflen ) { ibuf++, ibuflen--; }
    if (ibuflen < 4) {
        LOG_LOGINCONT(log_info, "user is %s, no ticket", p);
        return AFPERR_PARAM;
    }

    ibuf++, ibuflen--; /* null termination */

    if ((ibuf - p + 1) % 2) ibuf++, ibuflen--; /* deal with potential padding */

    LOG_LOGINCONT(log_debug, "client thinks user is %s", p);

    /* get the length of the ticket the client sends us */
    memcpy(&ticket_len, ibuf, sizeof(ticket_len));
    ibuf += sizeof(ticket_len); ibuflen -= sizeof(ticket_len);
    ticket_len = ntohs(ticket_len);

    /* a little bounds checking */
    if (ticket_len > ibuflen) {
        LOG_LOGINCONT(log_info,
                      "invalid ticket length (%u > %u)",
                      ticket_len, ibuflen);
        return AFPERR_PARAM;
    }

    /* now try to authenticate */
    if (do_gss_auth(obj, ibuf, ticket_len, rbuf, &rblen, username, userlen, sinfo)) {
        LOG_LOGINCONT(log_info, "do_gss_auth() failed" );
        *rbuflen = 0;
        return AFPERR_MISC;
    }

    /* We use the username we got back from the gssapi client_name.
       Should we compare this to the username the client sent in the clear?
       We know the character encoding of the cleartext username (UTF8), what
       encoding is the gssapi name in? */
    if ((pwd = uam_getname( obj, username, userlen )) == NULL) {
        LOG_LOGINCONT(log_info, "uam_getname() failed for %s", username);
        return AFPERR_NOTAUTH;
    }
    if (uam_checkuser(pwd) < 0) {
        LOG_LOGINCONT(log_info, "`%s'' not a valid user", username);
        return AFPERR_NOTAUTH;
    }

    *rbuflen = rblen;
    *uam_pwd = pwd;
    return AFP_OK;
}

/* logout */
static void gss_logout() {
}

static int gss_login_ext(void *obj,
                         char *uname,
                         struct passwd **uam_pwd,
                         char *ibuf, size_t ibuflen,
                         char *rbuf, size_t *rbuflen)
{
    return gss_login(obj, uam_pwd, ibuf, ibuflen, rbuf, rbuflen);
}

static int uam_setup(const char *path)
{
    return uam_register(UAM_SERVER_LOGIN_EXT, path, "Client Krb v2",
                        gss_login, gss_logincont, gss_logout, gss_login_ext);
}

static void uam_cleanup(void)
{
    uam_unregister(UAM_SERVER_LOGIN_EXT, "Client Krb v2");
}

UAM_MODULE_EXPORT struct uam_export uams_gss = {
    UAM_MODULE_SERVER,
    UAM_MODULE_VERSION,
    uam_setup,
    uam_cleanup
};
