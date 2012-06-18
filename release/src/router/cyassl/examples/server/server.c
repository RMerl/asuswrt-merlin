/* server.c */
#include "openssl/ssl.h"
#include "../test.h"


#ifdef CYASSL_CALLBACKS
    int srvHandShakeCB(HandShakeInfo*);
    int srvTimeoutCB(TimeoutInfo*);
    Timeval srvTo;
#endif

#if defined(NON_BLOCKING) || defined(CYASSL_CALLBACKS)
    void NonBlockingSSL_Accept(SSL* ssl)
    {
    #ifndef CYASSL_CALLBACKS
        int ret = SSL_accept(ssl);
    #else
        int ret = CyaSSL_accept_ex(ssl, srvHandShakeCB, srvTimeoutCB, srvTo);
    #endif
        int error = SSL_get_error(ssl, 0);
        while (ret != SSL_SUCCESS && (error == SSL_ERROR_WANT_READ ||
                                      error == SSL_ERROR_WANT_WRITE)) {
            printf("... server would block\n");
            #ifdef _WIN32
                Sleep(1000);
            #else
                sleep(1);
            #endif
            #ifndef CYASSL_CALLBACKS
                ret = SSL_accept(ssl);
            #else
                ret = CyaSSL_accept_ex(ssl, srvHandShakeCB, srvTimeoutCB,srvTo);
            #endif
            error = SSL_get_error(ssl, 0);
        }
        if (ret != SSL_SUCCESS)
            err_sys("SSL_accept failed");
    }
#endif


THREAD_RETURN CYASSL_API server_test(void* args)
{
    SOCKET_T sockfd   = 0;
    int      clientfd = 0;

    SSL_METHOD* method = 0;
    SSL_CTX*    ctx    = 0;
    SSL*        ssl    = 0;

    char msg[] = "I hear you fa shizzle!";
    char input[1024];
    int  idx;
   
    ((func_args*)args)->return_code = -1; /* error state */
#if defined(CYASSL_DTLS)
    method  = DTLSv1_server_method();
#elif  !defined(NO_TLS)
    method = TLSv1_server_method();
#else
    method = SSLv3_server_method();
#endif
    ctx    = SSL_CTX_new(method);

#ifndef NO_PSK
    SSL_CTX_set_psk_server_callback(ctx, my_psk_server_cb);
    SSL_CTX_use_psk_identity_hint(ctx, "cyassl server");
#endif

#ifdef OPENSSL_EXTRA
    SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,0);

    if (SSL_CTX_load_verify_locations(ctx, caCert, 0) != SSL_SUCCESS)
        err_sys("can't load ca file");

    /* for client auth */
    if (SSL_CTX_load_verify_locations(ctx, cliCert, 0) != SSL_SUCCESS)
        err_sys("can't load ca file");

    if (SSL_CTX_use_certificate_file(ctx, svrCert, SSL_FILETYPE_PEM)
            != SSL_SUCCESS)
        err_sys("can't load server cert file");

    if (SSL_CTX_use_PrivateKey_file(ctx, svrKey, SSL_FILETYPE_PEM)
            != SSL_SUCCESS)
        err_sys("can't load server key file");

    ssl = SSL_new(ctx);
    tcp_accept(&sockfd, &clientfd, (func_args*)args);
#ifndef CYASSL_DTLS
    CloseSocket(sockfd);
#endif

    SSL_set_fd(ssl, clientfd);

#ifdef NON_BLOCKING
    tcp_set_nonblocking(&clientfd);
    NonBlockingSSL_Accept(ssl);
#else
    #ifndef CYASSL_CALLBACKS
        if (SSL_accept(ssl) != SSL_SUCCESS) {
            int err = SSL_get_error(ssl, 0);
            char buffer[80];
            printf("error = %d, %s\n", err, ERR_error_string(err, buffer));
            err_sys("SSL_accept failed");
        }
    #else
        NonBlockingSSL_Accept(ssl);
    #endif
#endif
    showPeer(ssl);

    idx = SSL_read(ssl, input, sizeof(input));
    if (idx > 0) {
        input[idx] = 0;
        printf("Client message: %s\n", input);
    }
    
    if (SSL_write(ssl, msg, sizeof(msg)) != sizeof(msg))
        err_sys("SSL_write failed");

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    
    CloseSocket(clientfd);
    ((func_args*)args)->return_code = 0;
    return 0;
}


/* so overall tests can pull in test function */
#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        StartTCP();

        args.argc = argc;
        args.argv = argv;

        InitCyaSSL();
        server_test(&args);
        FreeCyaSSL();

        return args.return_code;
    }

#endif /* NO_MAIN_DRIVER */


#ifdef CYASSL_CALLBACKS

    int srvHandShakeCB(HandShakeInfo* info)
    {

        return 0;
    }


    int srvTimeoutCB(TimeoutInfo* info)
    {

        return 0;
    }

#endif


