/* test.h */

#ifndef CyaSSL_TEST_H
#define CyaSSL_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "types.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <process.h>
    #ifdef TEST_IPV6            /* don't require newer SDK for IPV4 */
	    #include <ws2tcpip.h>
        #include <wspiapi.h>
    #endif
    #define SOCKET_T int
#else
    #include <string.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <pthread.h>
    #ifdef NON_BLOCKING
        #include <fcntl.h>
    #endif
    #ifdef TEST_IPV6
        #include <netdb.h>
    #endif
    #define SOCKET_T unsigned int
#endif /* _WIN32 */

#ifdef _MSC_VER
    /* disable conversion warning */
    /* 4996 warning to use MS extensions e.g., strcpy_s instead of strncpy */
    #pragma warning(disable:4244 4996)
#endif

#if defined(__MACH__) || defined(_WIN32)
    #ifndef _SOCKLEN_T
        typedef int socklen_t;
    #endif
#endif


/* HPUX doesn't use socklent_t for third parameter to accept */
#if !defined(__hpux__)
    typedef socklen_t* ACCEPT_THIRD_T;
#else
    typedef int*       ACCEPT_THIRD_T;
#endif


#ifdef _WIN32
    #define CloseSocket(s) closesocket(s)
    #define StartTCP() { WSADATA wsd; WSAStartup(0x0002, &wsd); }
#else
    #define CloseSocket(s) close(s)
    #define StartTCP() 
#endif


#ifdef SINGLE_THREADED
    typedef unsigned int  THREAD_RETURN;
    typedef void*         THREAD_TYPE;
    #define CYASSL_API
#else
    #ifndef _POSIX_THREADS
        typedef unsigned int  THREAD_RETURN;
        typedef HANDLE        THREAD_TYPE;
        #define CYASSL_API __stdcall
    #else
        typedef void*         THREAD_RETURN;
        typedef pthread_t     THREAD_TYPE;
        #define CYASSL_API 
    #endif
#endif


#ifdef TEST_IPV6
    typedef struct sockaddr_in6 SOCKADDR_IN_T;
    #define AF_INET_V    AF_INET6
#else
    typedef struct sockaddr_in  SOCKADDR_IN_T;
    #define AF_INET_V    AF_INET
#endif
   

#ifndef NO_MAIN_DRIVER
    const char* caCert = "../../certs/ca-cert.pem";
    const char* svrCert = "../../certs/server-cert.pem";
    const char* svrKey  = "../../certs/server-key.pem";
    const char* cliCert = "../../certs/client-cert.pem";
    const char* cliKey  = "../../certs/client-key.pem";
#else
    static const char* caCert = "../certs/ca-cert.pem";
    static const char* svrCert = "../certs/server-cert.pem";
    static const char* svrKey  = "../certs/server-key.pem";
    static const char* cliCert = "../certs/client-cert.pem";
    static const char* cliKey  = "../certs/client-key.pem";
#endif


typedef struct tcp_ready {
    int ready;              /* predicate */
#ifdef _POSIX_THREADS
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
#endif
} tcp_ready;    


void InitTcpReady();
void FreeTcpReady();


typedef struct func_args {
    int    argc;
    char** argv;
    int    return_code;
    tcp_ready* signal;
} func_args;


typedef THREAD_RETURN CYASSL_API THREAD_FUNC(void*);

void start_thread(THREAD_FUNC, func_args*, THREAD_TYPE*);
void join_thread(THREAD_TYPE);

/* yaSSL */
static const char* const yasslIP   = "127.0.0.1";
static const word16      yasslPort = 11111;


static INLINE void err_sys(const char* msg)
{
    printf("yassl error: %s\n", msg);
    exit(EXIT_FAILURE);
}


#ifdef OPENSSL_EXTRA

static int PasswordCallBack(char* passwd, int sz, int rw, void* userdata)
{
    strncpy(passwd, "yassl123", sz);
    return 8;
}

#endif


static INLINE void showPeer(SSL* ssl)
{
#ifdef OPENSSL_EXTRA

    SSL_CIPHER* cipher;
    X509*       peer = SSL_get_peer_certificate(ssl);
    if (peer) {
        char* issuer  = X509_NAME_oneline(X509_get_issuer_name(peer), 0, 0);
        char* subject = X509_NAME_oneline(X509_get_subject_name(peer), 0, 0);
        
        printf("peer's cert info:\n issuer : %s\n subject: %s\n", issuer,
                                                                  subject);
        XFREE(subject, 0);
        XFREE(issuer, 0);
    }
    else
        printf("peer has no cert!\n");
    printf("SSL version is %s\n", SSL_get_version(ssl));

    cipher = SSL_get_current_cipher(ssl);
    printf("SSL cipher suite is %s\n", SSL_CIPHER_get_name(cipher));
#endif

#ifdef SESSION_CERTS
    {
        X509_CHAIN* chain = CyaSSL_get_peer_chain(ssl);
        int         count = CyaSSL_get_chain_count(chain);
        int i;

        for (i = 0; i < count; i++) {
            int length;
            unsigned char buffer[3072];

            CyaSSL_get_chain_cert_pem(chain,i,buffer, sizeof(buffer), &length);
            buffer[length] = 0;
            printf("cert %d has length %d data = \n%s\n", i, length, buffer);
        }
    }
#endif

}


static INLINE void tcp_socket(SOCKET_T* sockfd, SOCKADDR_IN_T* addr,
                              const char* peer, word16 port)
{
#ifndef TEST_IPV6
    const char* host = peer;

    /* peer could be in human readable form */
    if (peer != INADDR_ANY && isalpha(peer[0])) {
        struct hostent* entry = gethostbyname(peer);

        if (entry) {
            struct sockaddr_in tmp;
            memset(&tmp, 0, sizeof(struct sockaddr_in));
            memcpy(&tmp.sin_addr.s_addr, entry->h_addr_list[0],
                   entry->h_length);
            host = inet_ntoa(tmp.sin_addr);
        }
        else
            err_sys("no entry for host");
    }
#endif

#ifdef CYASSL_DTLS
    *sockfd = socket(AF_INET_V, SOCK_DGRAM, 0);
#else
    *sockfd = socket(AF_INET_V, SOCK_STREAM, 0);
#endif
    memset(addr, 0, sizeof(SOCKADDR_IN_T));

#ifndef TEST_IPV6
    addr->sin_family = AF_INET_V;
    addr->sin_port = htons(port);
    if (host == INADDR_ANY)
        addr->sin_addr.s_addr = INADDR_ANY;
    else
        addr->sin_addr.s_addr = inet_addr(host);
#else
    addr->sin6_family = AF_INET_V;
    addr->sin6_port = htons(port);
    addr->sin6_addr = in6addr_loopback;
#endif

#ifndef _WIN32
#ifdef SO_NOSIGPIPE
    {
        int       on = 1;
        socklen_t len = sizeof(on);
        int       res = setsockopt(*sockfd, SOL_SOCKET, SO_NOSIGPIPE, &on, len);
        if (res < 0)
            err_sys("setsockopt SO_NOSIGPIPE failed\n");
    }
#endif

#if defined(TCP_NODELAY) && !defined(CYASSL_DTLS)
    {
        int       on = 1;
        socklen_t len = sizeof(on);
        int       res = setsockopt(*sockfd, IPPROTO_TCP, TCP_NODELAY, &on, len);
        if (res < 0)
            err_sys("setsockopt TCP_NODELAY failed\n");
    }
#endif
#endif  /* _WIN32 */
}


static INLINE void tcp_connect(SOCKET_T* sockfd, const char* ip, word16 port)
{
    SOCKADDR_IN_T addr;
    tcp_socket(sockfd, &addr, ip, port);

    if (connect(*sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
        err_sys("tcp connect failed");
}


static INLINE void tcp_listen(SOCKET_T* sockfd)
{
    SOCKADDR_IN_T addr;

    /* don't use INADDR_ANY by default, firewall may block, make user switch
       on */
#ifdef USE_ANY_ADDR
    tcp_socket(sockfd, &addr, INADDR_ANY, yasslPort);
#else
    tcp_socket(sockfd, &addr, yasslIP, yasslPort);
#endif

#ifndef _WIN32
    {
        int       on  = 1;
        socklen_t len = sizeof(on);
        setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &on, len);
    }
#endif

    if (bind(*sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
        err_sys("tcp bind failed");
#ifndef CYASSL_DTLS
    if (listen(*sockfd, 5) != 0)
        err_sys("tcp listen failed");
#endif
}


static INLINE int udp_read_connect(SOCKET_T sockfd)
{
    SOCKADDR_IN_T cliaddr;
    byte          b[1500];
    int           n;
    socklen_t     len = sizeof(cliaddr);

    n = recvfrom(sockfd, b, sizeof(b), MSG_PEEK, (struct sockaddr*)&cliaddr,
                 &len);
    if (n > 0) {
        if (connect(sockfd, (const struct sockaddr*)&cliaddr,
                    sizeof(cliaddr)) != 0)
            err_sys("udp connect failed");
    }
    else
        err_sys("recvfrom failed");

    return sockfd;
}

static INLINE void udp_accept(SOCKET_T* sockfd, int* clientfd, func_args* args)
{
    SOCKADDR_IN_T addr;

    tcp_socket(sockfd, &addr, yasslIP, yasslPort);


#ifndef _WIN32
    {
        int       on  = 1;
        socklen_t len = sizeof(on);
        setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &on, len);
    }
#endif

    if (bind(*sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
        err_sys("tcp bind failed");

#if defined(_POSIX_THREADS) && defined(NO_MAIN_DRIVER)
    /* signal ready to accept data */
    {
    tcp_ready* ready = args->signal;
    pthread_mutex_lock(&ready->mutex);
    ready->ready = 1;
    pthread_cond_signal(&ready->cond);
    pthread_mutex_unlock(&ready->mutex);
    }
#endif

    *clientfd = udp_read_connect(*sockfd);
}

static INLINE void tcp_accept(SOCKET_T* sockfd, int* clientfd, func_args* args)
{
    SOCKADDR_IN_T client;
    socklen_t client_len = sizeof(client);

    #ifdef CYASSL_DTLS
        udp_accept(sockfd, clientfd, args);
        return;
    #endif

    tcp_listen(sockfd);

#if defined(_POSIX_THREADS) && defined(NO_MAIN_DRIVER)
    /* signal ready to tcp_accept */
    {
    tcp_ready* ready = args->signal;
    pthread_mutex_lock(&ready->mutex);
    ready->ready = 1;
    pthread_cond_signal(&ready->cond);
    pthread_mutex_unlock(&ready->mutex);
    }
#endif

    *clientfd = accept(*sockfd, (struct sockaddr*)&client,
                      (ACCEPT_THIRD_T)&client_len);
    if (*clientfd == -1)
        err_sys("tcp accept failed");
}


static INLINE void tcp_set_nonblocking(SOCKET_T* sockfd)
{
#ifdef NON_BLOCKING
    #ifdef _WIN32
        unsigned long blocking = 1;
        int ret = ioctlsocket(*sockfd, FIONBIO, &blocking);
    #else
        int flags = fcntl(*sockfd, F_GETFL, 0);
        int ret = fcntl(*sockfd, F_SETFL, flags | O_NONBLOCK);
    #endif
#endif
}


#ifndef NO_PSK

static INLINE unsigned int my_psk_client_cb(SSL* ssl, const char* hint,
        char* identity, unsigned int id_max_len, unsigned char* key,
        unsigned int key_max_len)
{
    /* identity is OpenSSL testing default for openssl s_client, keep same */
    strncpy(identity, "Client_identity", id_max_len);


    /* test key in hex is 0x1a2b3c4d , in decimal 439,041,101 , we're using
       unsigned binary */
    key[0] = 26;
    key[1] = 43;
    key[2] = 60;
    key[3] = 77;

    return 4;   /* length of key in octets or 0 for error */
}


static INLINE unsigned int my_psk_server_cb(SSL* ssl, const char* identity,
        unsigned char* key, unsigned int key_max_len)
{
    /* identity is OpenSSL testing default for openssl s_client, keep same */
    if (strncmp(identity, "Client_identity", 15) != 0)
        return 0;

    /* test key in hex is 0x1a2b3c4d , in decimal 439,041,101 , we're using
       unsigned binary */
    key[0] = 26;
    key[1] = 43;
    key[2] = 60;
    key[3] = 77;

    return 4;   /* length of key in octets or 0 for error */
}

#endif /* NO_PSK */


#ifdef _WIN32

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    static INLINE double current_time()
    {
        static int init = 0;
        static LARGE_INTEGER freq;
    
        LARGE_INTEGER count;

        if (!init) {
            QueryPerformanceFrequency(&freq);
            init = 1;
        }

        QueryPerformanceCounter(&count);

        return (double)count.QuadPart / freq.QuadPart;
    }

#else

    #include <sys/time.h>

    static INLINE double current_time()
    {
        struct timeval tv;
        gettimeofday(&tv, 0);

        return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
    }

#endif /* _WIN32 */





#endif /* CyaSSL_TEST_H */

