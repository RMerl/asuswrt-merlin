/* client.c */
#include "openssl/ssl.h"
#include "../test.h"


/*
#define TEST_RESUME 
*/


#ifdef CYASSL_CALLBACKS
    int handShakeCB(HandShakeInfo*);
    int timeoutCB(TimeoutInfo*);
    Timeval timeout;
#endif

#if defined(NON_BLOCKING) || defined(CYASSL_CALLBACKS)
    void NonBlockingSSL_Connect(SSL* ssl)
    {
#ifndef CYASSL_CALLBACKS
        int ret = SSL_connect(ssl);
#else
        int ret = CyaSSL_connect_ex(ssl, handShakeCB, timeoutCB, timeout);
#endif
        int error = SSL_get_error(ssl, 0);
        while (ret != SSL_SUCCESS && (error == SSL_ERROR_WANT_READ ||
                                      error == SSL_ERROR_WANT_WRITE)) {
            if (error == SSL_ERROR_WANT_READ)
                printf("... client would read block\n");
            else
                printf("... client would write block\n");
            #ifdef _WIN32
                Sleep(100);
            #else
                sleep(1);
            #endif
            #ifndef CYASSL_CALLBACKS
                ret = SSL_connect(ssl);
            #else
                ret = CyaSSL_connect_ex(ssl, handShakeCB, timeoutCB, timeout);
            #endif
            error = SSL_get_error(ssl, 0);
        }
        if (ret != SSL_SUCCESS)
            err_sys("SSL_connect failed");
    }
#endif


void client_test(void* args)
{
    SOCKET_T sockfd = 0;

    SSL_METHOD*  method  = 0;
    SSL_CTX*     ctx     = 0;
    SSL*         ssl     = 0;
    
#ifdef TEST_RESUME
    SSL*         sslResume = 0;
    SSL_SESSION* session = 0;
    char         resumeMsg[] = "resuming cyassl!";
    int          resumeSz    = sizeof(resumeMsg);
#endif

    char msg[] = "hello cyassl!";
    char reply[1024];
    int  input;
    int  msgSz = sizeof(msg);

    int     argc = ((func_args*)args)->argc;
    char**  argv = ((func_args*)args)->argv;

    ((func_args*)args)->return_code = -1; /* error state */

#if defined(CYASSL_DTLS)
    method  = DTLSv1_client_method();
#elif  !defined(NO_TLS)
    method  = TLSv1_client_method();
#else
    method  = SSLv3_client_method();
#endif
    ctx     = SSL_CTX_new(method);

#ifndef NO_PSK
    SSL_CTX_set_psk_client_callback(ctx, my_psk_client_cb);
#endif

#ifdef OPENSSL_EXTRA
    SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

    if (SSL_CTX_load_verify_locations(ctx, caCert, 0) != SSL_SUCCESS)
        err_sys("can't load ca file");

    if (argc == 3) {
        /*  ./client server securePort  */
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);  /* TODO: add ca cert */
                    /* this is just to allow easy testing of other servers */
        tcp_connect(&sockfd, argv[1], (short)atoi(argv[2]));
    }
    else if (argc == 1) {
        /* ./client          // plain mode */
        /* for client cert authentication if server requests */
        if (SSL_CTX_use_certificate_file(ctx, cliCert, SSL_FILETYPE_PEM)
                != SSL_SUCCESS)
            err_sys("can't load client cert file");

        if (SSL_CTX_use_PrivateKey_file(ctx, cliKey, SSL_FILETYPE_PEM)
                != SSL_SUCCESS)
            err_sys("can't load client key file");

        tcp_connect(&sockfd, yasslIP, yasslPort);
    }
    else if (argc == 2) {
        /* time passed in number of connects give average */
        int times = atoi(argv[1]);
        int i = 0;

        double start = current_time(), avg;

        for (i = 0; i < times; i++) {
            tcp_connect(&sockfd, yasslIP, yasslPort);
            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, sockfd);
            if (SSL_connect(ssl) != SSL_SUCCESS)
                err_sys("SSL_connect failed");

            SSL_shutdown(ssl);
            SSL_free(ssl);
            CloseSocket(sockfd);
        }
        avg = current_time() - start;
        avg /= times;
        avg *= 1000;    /* milliseconds */  
        printf("SSL_connect avg took:%6.3f milliseconds\n", avg);

        SSL_CTX_free(ctx);
        ((func_args*)args)->return_code = 0;
        return;
    }
    else
        err_sys("usage: ./client server securePort");

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);

#ifdef NON_BLOCKING
    tcp_set_nonblocking(&sockfd);
    NonBlockingSSL_Connect(ssl);
#else
    #ifndef CYASSL_CALLBACKS
        if (SSL_connect(ssl) != SSL_SUCCESS) { /* see note at top of README */
            int  err = SSL_get_error(ssl, 0);
            char buffer[80];
            printf("err = %d, %s\n", err, ERR_error_string(err, buffer));
            err_sys("SSL_connect failed");/* if you're getting an error here  */
        }
    #else
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;
        NonBlockingSSL_Connect(ssl);  /* will keep retrying on timeout */
    #endif
#endif
    showPeer(ssl);
    
    if (argc == 3) {
        printf("SSL connect ok, sending GET...\n");
        strncpy(msg, "GET\r\n", 6);
        msgSz = 6;
    }
    if (SSL_write(ssl, msg, msgSz) != msgSz)
        err_sys("SSL_write failed");

    input = SSL_read(ssl, reply, sizeof(reply));
    if (input > 0) {
        reply[input] = 0;
        printf("Server response: %s\n", reply);
    }
   
#ifdef TEST_RESUME
    #ifdef CYASSL_DTLS
        strncpy(msg, "break", 6);
        msgSz = (int)strlen(msg);
        /* try to send session close */
        SSL_write(ssl, msg, msgSz);
    #endif
    session   = SSL_get_session(ssl);
    sslResume = SSL_new(ctx);
#endif

    SSL_shutdown(ssl);
    SSL_free(ssl);
    CloseSocket(sockfd);

#ifdef TEST_RESUME
    #ifdef CYASSL_DTLS
        #ifdef _WIN32
            Sleep(500);
        #else
            sleep(1);
        #endif
    #endif
    if (argc == 3)
        tcp_connect(&sockfd, argv[1], (short)atoi(argv[2]));
    else
        tcp_connect(&sockfd, yasslIP, yasslPort);
    SSL_set_fd(sslResume, sockfd);
    SSL_set_session(sslResume, session);
   
    showPeer(sslResume); 
    if (SSL_connect(sslResume) != SSL_SUCCESS) err_sys("SSL resume failed");

#ifdef OPENSSL_EXTRA
    if (SSL_session_reused(sslResume))
        printf("reused session id\n");
    else
        printf("didn't reuse session id!!!\n");
#endif
  
    if (SSL_write(sslResume, resumeMsg, resumeSz) != resumeSz)
        err_sys("SSL_write failed");

    input = SSL_read(sslResume, reply, sizeof(reply));
    if (input > 0) {
        reply[input] = 0;
        printf("Server resume response: %s\n", reply);
    }

    /* try to send session break */
    SSL_write(sslResume, msg, msgSz); 

    SSL_shutdown(sslResume);
    SSL_free(sslResume);
#endif /* TEST_RESUME */

    SSL_CTX_free(ctx);
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
#ifdef DEBUG_CYASSL
        CyaSSL_Debugging_ON();
#endif
        client_test(&args);
        FreeCyaSSL();

        return args.return_code;
    }

#endif /* NO_MAIN_DRIVER */


#ifdef NO_FILESYSTEM

    void test_buffer(SSL_CTX* ctx)
    {
        /* test buffer load */
        long  sz = 0;
        byte  buff[4096];
        FILE* file = fopen(caCert, "rb");
        fseek(file, 0, SEEK_END);
        sz = ftell(file);
        rewind(file);
        fread(buff, sizeof(buff), 1, file);
   
        if (CyaSSL_CTX_load_verify_buffer(ctx, buff, sz) != SSL_SUCCESS)
            err_sys("can't load buffer ca file");
    }

#endif /* NO_FILESYSTEM */



#ifdef CYASSL_CALLBACKS

    int handShakeCB(HandShakeInfo* info)
    {

        return 0;
    }


    int timeoutCB(TimeoutInfo* info)
    {

        return 0;
    }

#endif


