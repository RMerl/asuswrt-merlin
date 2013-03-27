/* 
   Stubs for SSL support when no SSL library has been configured
   Copyright (C) 2002-2006, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#include "config.h"

#include <stdlib.h> /* for NULL */

#include "ne_ssl.h"
#include "ne_session.h"

char *ne_ssl_readable_dname(const ne_ssl_dname *dn)
{
    return NULL;
}

ne_ssl_certificate *ne_ssl_cert_read(const char *filename)
{
    return NULL;
}

int ne_ssl_cert_cmp(const ne_ssl_certificate *c1, const ne_ssl_certificate *c2)
{
    return 1;
}

const ne_ssl_certificate *ne_ssl_cert_signedby(const ne_ssl_certificate *cert)
{ 
    return NULL;
}

const ne_ssl_dname *ne_ssl_cert_issuer(const ne_ssl_certificate *cert)
{
    return NULL;
}

const ne_ssl_dname *ne_ssl_cert_subject(const ne_ssl_certificate *cert)
{
    return NULL;
}

void ne_ssl_cert_free(ne_ssl_certificate *cert) {}

ne_ssl_client_cert *ne_ssl_clicert_read(const char *filename)
{
    return NULL;
}

const ne_ssl_certificate *ne_ssl_clicert_owner(const ne_ssl_client_cert *ccert)
{
    return NULL;
}

int ne_ssl_clicert_encrypted(const ne_ssl_client_cert *ccert)
{
    return -1;
}

int ne_ssl_clicert_decrypt(ne_ssl_client_cert *ccert, const char *password)
{
    return -1;
}

void ne_ssl_clicert_free(ne_ssl_client_cert *ccert) {}

void ne_ssl_trust_default_ca(ne_session *sess) {}

ne_ssl_context *ne_ssl_context_create(int mode)
{
    return NULL;
}

void ne_ssl_context_trustcert(ne_ssl_context *ctx, const ne_ssl_certificate *cert)
{}

int ne_ssl_context_set_verify(ne_ssl_context *ctx, 
                              int required,
                              const char *ca_names,
                              const char *verify_cas)
{
    return -1;
}

void ne_ssl_context_set_flag(ne_ssl_context *ctx, int flag, int value) {}

void ne_ssl_context_destroy(ne_ssl_context *ctx) {}

int ne_ssl_cert_digest(const ne_ssl_certificate *cert, char digest[60])
{
    return -1;
}

void ne_ssl_cert_validity_time(const ne_ssl_certificate *cert,
                               time_t *from, time_t *until) {}

const char *ne_ssl_cert_identity(const ne_ssl_certificate *cert)
{
    return NULL;
}


const char *ne_ssl_clicert_name(const ne_ssl_client_cert *ccert)
{
    return NULL;
}

int ne_ssl_dname_cmp(const ne_ssl_dname *dn1, const ne_ssl_dname *dn2)
{
    return -1;
}

int ne_ssl_cert_write(const ne_ssl_certificate *cert, const char *filename)
{
    return -1;
}

char *ne_ssl_cert_export(const ne_ssl_certificate *cert)
{
    return NULL;
}

ne_ssl_certificate *ne_ssl_cert_import(const char *data)
{
    return NULL;
}

void ne_ssl_set_clicert(ne_session *sess, const ne_ssl_client_cert *cc) 
{}
