/* echoclient.c  */

#include "openssl/ssl.h"
#include "../test.h"


void echoclient_test(void* args)
{
    SOCKET_T sockfd = 0;

    FILE* fin  = stdin;
    FILE* fout = stdout;

    int inCreated  = 0;
    int outCreated = 0;

    char send[1024];
    char reply[1024];

    SSL_METHOD* method = 0;
    SSL_CTX*    ctx    = 0;
    SSL*        ssl    = 0;

    int sendSz;
    int argc    = 0;
    char** argv = 0;

    ((func_args*)args)->return_code = -1; /* error state */
    argc = ((func_args*)args)->argc;
    argv = ((func_args*)args)->argv;

    if (argc >= 2) {
        fin  = fopen(argv[1], "r"); 
        inCreated = 1;
    }
    if (argc >= 3) {
        fout = fopen(argv[2], "w");
        outCreated = 1;
    }

    if (!fin)  err_sys("can't open input file");
    if (!fout) err_sys("can't open output file");

    tcp_connect(&sockfd, yasslIP, yasslPort);

#if defined(CYASSL_DTLS)
    method  = DTLSv1_client_method();
#elif  !defined(NO_TLS)
    method = TLSv1_client_method();
#else
    method = SSLv3_client_method();
#endif
    ctx    = SSL_CTX_new(method);

    if (SSL_CTX_load_verify_locations(ctx, caCert, 0) != SSL_SUCCESS)
        err_sys("can't load ca file");

#ifdef OPENSSL_EXTRA
    SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif
    ssl = SSL_new(ctx);

    SSL_set_fd(ssl, sockfd);
#if defined(_WIN32) && defined(CYASSL_DTLS) && defined(NO_MAIN_DRIVER)
    /* let echoserver bind first, TODO: add Windows signal like pthreads does */
    Sleep(100);
#endif
    if (SSL_connect(ssl) != SSL_SUCCESS) err_sys("SSL_connect failed");

    while (fgets(send, sizeof(send), fin)) {

        sendSz = (int)strlen(send) + 1;

        if (SSL_write(ssl, send, sendSz) != sendSz)
            err_sys("SSL_write failed");

        if (strncmp(send, "quit", 4) == 0) {
            fputs("sending server shutdown command: quit!\n", fout);
            break;
        }

        if (strncmp(send, "break", 4) == 0) {
            fputs("sending server session close: break!\n", fout);
            break;
        }

        if (SSL_read(ssl, reply, sizeof(reply)) > 0) 
            fputs(reply, fout);
    }

#ifdef CYASSL_DTLS
    strncpy(send, "break", 6);
    sendSz = (int)strlen(send);
    /* try to tell server done */
    SSL_write(ssl, send, sendSz);
#else
    SSL_shutdown(ssl);
#endif

    SSL_free(ssl);
    SSL_CTX_free(ctx);

    fflush(fout);
    if (inCreated)  fclose(fin);
    if (outCreated) fclose(fout);

    CloseSocket(sockfd);
    ((func_args*)args)->return_code = 0; 
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
        echoclient_test(&args);
        FreeCyaSSL();

        return args.return_code;
    }

#endif /* NO_MAIN_DRIVER */



