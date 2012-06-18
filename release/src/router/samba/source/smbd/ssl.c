/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SSLeay utility functions
   Copyright (C) Christian Starkjohann <cs@obdev.at> 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* 
 * since includes.h pulls in config.h which is were WITH_SSL will be 
 * defined, we want to include includes.h before testing for WITH_SSL
 * RJS 26-Jan-1999
 */

#include "includes.h"

#ifdef WITH_SSL  /* should always be defined if this module is compiled */

#include <ssl.h>
#include <err.h>

BOOL            sslEnabled;
SSL             *ssl = NULL;
int             sslFd = -1;
static SSL_CTX  *sslContext = NULL;
extern int      DEBUGLEVEL;

static int  ssl_verify_cb(int ok, X509_STORE_CTX *ctx)
{
char    buffer[256];

    X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert),
            buffer, sizeof(buffer));
    if(ok){
        DEBUG(0, ("SSL: Certificate OK: %s\n", buffer));
    }else{
        switch (ctx->error){
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
            DEBUG(0, ("SSL: Cert error: CA not known: %s\n", buffer));
            break;
        case X509_V_ERR_CERT_NOT_YET_VALID:
            DEBUG(0, ("SSL: Cert error: Cert not yet valid: %s\n", buffer));
            break;
        case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
            DEBUG(0, ("SSL: Cert error: illegal \'not before\' field: %s\n",
                    buffer));
            break;
        case X509_V_ERR_CERT_HAS_EXPIRED:
            DEBUG(0, ("SSL: Cert error: Cert expired: %s\n", buffer));
            break;
        case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
            DEBUG(0, ("SSL: Cert error: invalid \'not after\' field: %s\n",
                    buffer));
            break;
        default:
            DEBUG(0, ("SSL: Cert error: unknown error %d in %s\n", ctx->error,
                    buffer));
            break;
        }
    }
    return ok;
}

static RSA  *ssl_temp_rsa_cb(SSL *ssl, int export)
{
static RSA  *rsa = NULL;
    
    if(rsa == NULL)
        rsa = RSA_generate_key(512, RSA_F4, NULL, NULL);
    return rsa;
}

/* This is called before we fork. It should ask the user for the pass phrase
 * if necessary. Error output can still go to stderr because the process
 * has a terminal.
 */
int sslutil_init(int isServer)
{
int     err;
char    *certfile, *keyfile, *ciphers, *cacertDir, *cacertFile;

    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    switch(lp_ssl_version()){
        case SMB_SSL_V2:    sslContext = SSL_CTX_new(SSLv2_method());   break;
        case SMB_SSL_V3:    sslContext = SSL_CTX_new(SSLv3_method());   break;
        default:
        case SMB_SSL_V23:   sslContext = SSL_CTX_new(SSLv23_method());  break;
        case SMB_SSL_TLS1:  sslContext = SSL_CTX_new(TLSv1_method());   break;
    }
    if(sslContext == NULL){
        err = ERR_get_error();
        fprintf(stderr, "SSL: Error allocating context: %s\n",
                ERR_error_string(err, NULL));
        exit(1);
    }
    if(lp_ssl_compatibility()){
        SSL_CTX_set_options(sslContext, SSL_OP_ALL);
    }
    certfile = isServer ? lp_ssl_cert() : lp_ssl_client_cert();
    if((certfile == NULL || *certfile == 0) && isServer){
        fprintf(stderr, "SSL: No cert file specified in config file!\n");
        fprintf(stderr, "The server MUST have a certificate!\n");
        exit(1);
    }
    keyfile = isServer ? lp_ssl_privkey() : lp_ssl_client_privkey();
    if(keyfile == NULL || *keyfile == 0)
        keyfile = certfile;
    if(certfile != NULL && *certfile != 0){
        if(!SSL_CTX_use_certificate_file(sslContext, certfile, SSL_FILETYPE_PEM)){
            err = ERR_get_error();
            fprintf(stderr, "SSL: error reading certificate from file %s: %s\n",
                    certfile, ERR_error_string(err, NULL));
            exit(1);
        }
        if(!SSL_CTX_use_PrivateKey_file(sslContext, keyfile, SSL_FILETYPE_PEM)){
            err = ERR_get_error();
            fprintf(stderr, "SSL: error reading private key from file %s: %s\n",
                    keyfile, ERR_error_string(err, NULL));
            exit(1);
        }
        if(!SSL_CTX_check_private_key(sslContext)){
            err = ERR_get_error();
            fprintf(stderr, "SSL: Private key does not match public key in cert!\n");
            exit(1);
        }
    }
    cacertDir = lp_ssl_cacertdir();
    cacertFile = lp_ssl_cacertfile();
    if(cacertDir != NULL && *cacertDir == 0)
        cacertDir = NULL;
    if(cacertFile != NULL && *cacertFile == 0)
        cacertFile = NULL;
    if(!SSL_CTX_load_verify_locations(sslContext, cacertFile, cacertDir)){
        err = ERR_get_error();
        fprintf(stderr, "SSL: Error error setting CA cert locations: %s\n",
                ERR_error_string(err, NULL));
        fprintf(stderr, "trying default locations.\n");
        cacertFile = cacertDir = NULL;
        if(!SSL_CTX_set_default_verify_paths(sslContext)){
            err = ERR_get_error();
            fprintf(stderr, "SSL: Error error setting default CA cert location: %s\n",
                    ERR_error_string(err, NULL));
            exit(1);
        }
    }
    SSL_CTX_set_tmp_rsa_callback(sslContext, ssl_temp_rsa_cb);
    if((ciphers = lp_ssl_ciphers()) != NULL && *ciphers != 0)
        SSL_CTX_set_cipher_list(sslContext, ciphers);
    if((isServer && lp_ssl_reqClientCert()) || (!isServer && lp_ssl_reqServerCert())){
        SSL_CTX_set_verify(sslContext,
            SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, ssl_verify_cb);
    }else{
        SSL_CTX_set_verify(sslContext, SSL_VERIFY_NONE, ssl_verify_cb);
    }
#if 1 /* don't know what this is good for, but s_server in SSLeay does it, too */
    if(isServer){
        SSL_CTX_set_client_CA_list(sslContext, SSL_load_client_CA_file(certfile));
    }
#endif
    return 0;
}

int sslutil_accept(int fd)
{
int     err;

    if(ssl != NULL){
        DEBUG(0, ("SSL: internal error: more than one SSL connection (server)\n"));
        return -1;
    }
    if((ssl = SSL_new(sslContext)) == NULL){
        err = ERR_get_error();
        DEBUG(0, ("SSL: Error allocating handle: %s\n",
            ERR_error_string(err, NULL)));
        return -1;
    }
    SSL_set_fd(ssl, fd);
    sslFd = fd;
    if(SSL_accept(ssl) <= 0){
        err = ERR_get_error();
        DEBUG(0, ("SSL: Error accepting on socket: %s\n",
            ERR_error_string(err, NULL)));
        return -1;
    }
    DEBUG(0, ("SSL: negotiated cipher: %s\n", SSL_get_cipher(ssl)));
    return 0;
}

int sslutil_fd_is_ssl(int fd)
{
    return fd == sslFd;
}

int sslutil_connect(int fd)
{
int     err;

    if(ssl != NULL){
        DEBUG(0, ("SSL: internal error: more than one SSL connection (client)\n"));
        return -1;
    }
    if((ssl = SSL_new(sslContext)) == NULL){
        err = ERR_get_error();
        DEBUG(0, ("SSL: Error allocating handle: %s\n",
            ERR_error_string(err, NULL)));
        return -1;
    }
    SSL_set_fd(ssl, fd);
    sslFd = fd;
    if(SSL_connect(ssl) <= 0){
        err = ERR_get_error();
        DEBUG(0, ("SSL: Error conencting socket: %s\n",
            ERR_error_string(err, NULL)));
        return -1;
    }
    DEBUG(0, ("SSL: negotiated cipher: %s\n", SSL_get_cipher(ssl)));
    return 0;
}

int sslutil_disconnect(int fd)
{
    if(fd == sslFd && ssl != NULL){
        SSL_free(ssl);
        ssl = NULL;
        sslFd = -1;
    }
    return 0;
}

int sslutil_negotiate_ssl(int fd, int msg_type)
{
unsigned char   buf[5] = {0x83, 0, 0, 1, 0x81};
char            *reqHosts, *resignHosts;

    reqHosts = lp_ssl_hosts();
    resignHosts = lp_ssl_hosts_resign();
    if(!allow_access(resignHosts, reqHosts, client_name(fd), client_addr(fd))){
        sslEnabled = False;
        return 0;
    }
    if(msg_type != 0x81){ /* first packet must be a session request */
        DEBUG( 0, ( "Client %s did not use session setup; access denied\n",
                     client_addr(fd) ) );
        send_smb(fd, (char *)buf);
        return -1;
    }
    buf[4] = 0x8e;  /* negative session response: use SSL */
    send_smb(fd, (char *)buf);
    if(sslutil_accept(fd) != 0){
        DEBUG( 0, ( "Client %s failed SSL negotiation!\n", client_addr(fd) ) );
        return -1;
    }
    return 1;
}

#else /* WITH_SSL */
 void ssl_dummy(void);
 void ssl_dummy(void) {;} /* So some compilers don't complain. */
#endif  /* WITH_SSL */
