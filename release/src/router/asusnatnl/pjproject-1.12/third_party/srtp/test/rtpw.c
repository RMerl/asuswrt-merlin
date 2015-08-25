/*
 * rtpw.c
 *
 * rtp word sender/receiver
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 *
 * This app is a simple RTP application intended only for testing
 * libsrtp.  It reads one word at a time from /usr/dict/words (or
 * whatever file is specified as DICT_FILE), and sends one word out
 * each USEC_RATE microseconds.  Secure RTP protections can be
 * applied.  See the usage() function for more details.
 *
 */

/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "datatypes.h"
#include "getopt_s.h"       /* for local getopt()  */

#include <stdio.h>          /* for printf, fprintf */
#include <stdlib.h>         /* for atoi()          */
#include <errno.h>
#include <unistd.h>         /* for close()         */

#include <string.h>         /* for strncpy()       */
#include <time.h>	    /* for usleep()        */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#elif defined HAVE_WINSOCK2_H
# include <winsock2.h>
# include <ws2tcpip.h>
# define RTPW_USE_WINSOCK2	1
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#include "srtp.h"           
#include "rtp.h"

#ifdef RTPW_USE_WINSOCK2
# define DICT_FILE        "words.txt"
#else
# define DICT_FILE        "/usr/share/dict/words"
#endif
#define USEC_RATE        (5e5)
#define MAX_WORD_LEN     128  
#define ADDR_IS_MULTICAST(a) IN_MULTICAST(htonl(a))
#define MAX_KEY_LEN      64
#define MASTER_KEY_LEN   30


#ifndef HAVE_USLEEP
# ifdef HAVE_WINDOWS_H
#  define usleep(us)	Sleep((us)/1000)
# else
#  define usleep(us)	sleep((us)/1000000)
# endif
#endif


/*
 * the function usage() prints an error message describing how this
 * program should be called, then calls exit()
 */

void
usage(char *prog_name);

/*
 * leave_group(...) de-registers from a multicast group
 */

void
leave_group(int sock, struct ip_mreq mreq, char *name);


/*
 * program_type distinguishes the [s]rtp sender and receiver cases
 */

typedef enum { sender, receiver, unknown } program_type;

int
main (int argc, char *argv[]) {
  char *dictfile = DICT_FILE;
  FILE *dict;
  char word[MAX_WORD_LEN];
  int sock, ret;
  struct in_addr rcvr_addr;
  struct sockaddr_in name;
  struct ip_mreq mreq;
#if BEW
  struct sockaddr_in local;
#endif 
  program_type prog_type = unknown;
  sec_serv_t sec_servs = sec_serv_none;
  unsigned char ttl = 5;
  int c;
  char *input_key = NULL;
  char *address = NULL;
  char key[MAX_KEY_LEN];
  unsigned short port = 0;
  rtp_sender_t snd;
  srtp_policy_t policy;
  err_status_t status;
  int len;
  int do_list_mods = 0;
  uint32_t ssrc = 0xdeadbeef; /* ssrc value hardcoded for now */
#ifdef RTPW_USE_WINSOCK2
  WORD wVersionRequested = MAKEWORD(2, 0);
  WSADATA wsaData;

  ret = WSAStartup(wVersionRequested, &wsaData);
  if (ret != 0) {
    fprintf(stderr, "error: WSAStartup() failed: %d\n", ret);
    exit(1);
  }
#endif

  /* initialize srtp library */
  status = srtp_init();
  if (status) {
    printf("error: srtp initialization failed with error code %d\n", status);
    exit(1);
  }

  /* check args */
  while (1) {
    c = getopt_s(argc, argv, "k:rsaeld:");
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'k':
      input_key = optarg_s;
      break;
    case 'e':
      sec_servs |= sec_serv_conf;
      break;
    case 'a':
      sec_servs |= sec_serv_auth;
      break;
    case 'r':
      prog_type = receiver;
      break;
    case 's':
      prog_type = sender;
      break;
    case 'd':
      status = crypto_kernel_set_debug_module(optarg_s, 1);
      if (status) {
        printf("error: set debug module (%s) failed\n", optarg_s);
        exit(1);
      }
      break;
    case 'l':
      do_list_mods = 1;
      break;
    default:
      usage(argv[0]);
    }
  }

  if (prog_type == unknown) {
    if (do_list_mods) {
      status = crypto_kernel_list_debug_modules();
      if (status) {
	printf("error: list of debug modules failed\n");
	exit(1);
      }
      return 0;
    } else {
      printf("error: neither sender [-s] nor receiver [-r] specified\n");
      usage(argv[0]);
    }
  }
   
  if ((sec_servs && !input_key) || (!sec_servs && input_key)) {
    /* 
     * a key must be provided if and only if security services have
     * been requested 
     */
    usage(argv[0]);
  }
    
  if (argc != optind_s + 2) {
    /* wrong number of arguments */
    usage(argv[0]);
  }

  /* get address from arg */
  address = argv[optind_s++];

  /* get port from arg */
  port = atoi(argv[optind_s++]);

  /* set address */
#ifdef HAVE_INET_ATON
  if (0 == inet_aton(address, &rcvr_addr)) {
    fprintf(stderr, "%s: cannot parse IP v4 address %s\n", argv[0], address);
    exit(1);
  }
  if (rcvr_addr.s_addr == INADDR_NONE) {
    fprintf(stderr, "%s: address error", argv[0]);
    exit(1);
  }
#else
  rcvr_addr.s_addr = inet_addr(address);
  if (0xffffffff == rcvr_addr.s_addr) {
    fprintf(stderr, "%s: cannot parse IP v4 address %s\n", argv[0], address);
    exit(1);
  }
#endif

  /* open socket */
  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    int err;
#ifdef RTPW_USE_WINSOCK2
    err = WSAGetLastError();
#else
    err = errno;
#endif
    fprintf(stderr, "%s: couldn't open socket: %d\n", argv[0], err);
    exit(1);
  }

  name.sin_addr   = rcvr_addr;    
  name.sin_family = PF_INET;
  name.sin_port   = htons(port);
 
  if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
    if (prog_type == sender) {
      ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 
  	               sizeof(ttl));
      if (ret < 0) {
	fprintf(stderr, "%s: Failed to set TTL for multicast group", argv[0]);
	perror("");
	exit(1);
      }
    }

    mreq.imr_multiaddr.s_addr = rcvr_addr.s_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq,
		     sizeof(mreq));
    if (ret < 0) {
      fprintf(stderr, "%s: Failed to join multicast group", argv[0]);
      perror("");
      exit(1);
    }
  }

  /* report security services selected on the command line */
  printf("security services: ");
  if (sec_servs & sec_serv_conf)
    printf("confidentiality ");
  if (sec_servs & sec_serv_auth)
    printf("message authentication");
  if (sec_servs == sec_serv_none)
    printf("none");
  printf("\n");
  
  /* set up the srtp policy and master key */    
  if (sec_servs) {
    /* 
     * create policy structure, using the default mechanisms but 
     * with only the security services requested on the command line,
     * using the right SSRC value
     */
    switch (sec_servs) {
    case sec_serv_conf_and_auth:
      crypto_policy_set_rtp_default(&policy.rtp);
      crypto_policy_set_rtcp_default(&policy.rtcp);
      break;
    case sec_serv_conf:
      crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
      crypto_policy_set_rtcp_default(&policy.rtcp);      
      break;
    case sec_serv_auth:
      crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
      crypto_policy_set_rtcp_default(&policy.rtcp);
      break;
    default:
      printf("error: unknown security service requested\n");
      return -1;
    } 
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = ssrc;
    policy.key  = (uint8_t *) key;
    policy.next = NULL;
    policy.rtp.sec_serv = sec_servs;
    policy.rtcp.sec_serv = sec_serv_none;  /* we don't do RTCP anyway */

    /*
     * read key from hexadecimal on command line into an octet string
     */
    len = hex_string_to_octet_string(key, input_key, MASTER_KEY_LEN*2);
    
    /* check that hex string is the right length */
    if (len < MASTER_KEY_LEN*2) {
      fprintf(stderr, 
	      "error: too few digits in key/salt "
	      "(should be %d hexadecimal digits, found %d)\n",
	      MASTER_KEY_LEN*2, len);
      exit(1);    
    } 
    if (strlen(input_key) > MASTER_KEY_LEN*2) {
      fprintf(stderr, 
	      "error: too many digits in key/salt "
	      "(should be %d hexadecimal digits, found %u)\n",
	      MASTER_KEY_LEN*2, (unsigned)strlen(input_key));
      exit(1);    
    }
    
    printf("set master key/salt to %s/", octet_string_hex_string(key, 16));
    printf("%s\n", octet_string_hex_string(key+16, 14));
  
  } else {
    /*
     * we're not providing security services, so set the policy to the
     * null policy
     *
     * Note that this policy does not conform to the SRTP
     * specification, since RTCP authentication is required.  However,
     * the effect of this policy is to turn off SRTP, so that this
     * application is now a vanilla-flavored RTP application.
     */
    policy.key                 = (uint8_t *)key;
    policy.ssrc.type           = ssrc_specific;
    policy.ssrc.value          = ssrc;
    policy.rtp.cipher_type     = NULL_CIPHER;
    policy.rtp.cipher_key_len  = 0; 
    policy.rtp.auth_type       = NULL_AUTH;
    policy.rtp.auth_key_len    = 0;
    policy.rtp.auth_tag_len    = 0;
    policy.rtp.sec_serv        = sec_serv_none;   
    policy.rtcp.cipher_type    = NULL_CIPHER;
    policy.rtcp.cipher_key_len = 0; 
    policy.rtcp.auth_type      = NULL_AUTH;
    policy.rtcp.auth_key_len   = 0;
    policy.rtcp.auth_tag_len   = 0;
    policy.rtcp.sec_serv       = sec_serv_none;   
    policy.next                = NULL;
  }

  if (prog_type == sender) {

#if BEW
    /* bind to local socket (to match crypto policy, if need be) */
    memset(&local, 0, sizeof(struct sockaddr_in));
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(port);
    ret = bind(sock, (struct sockaddr *) &local, sizeof(struct sockaddr_in));
    if (ret < 0) {
      fprintf(stderr, "%s: bind failed\n", argv[0]);
      perror("");
      exit(1); 
    }
#endif /* BEW */

    /* initialize sender's rtp and srtp contexts */
    snd = rtp_sender_alloc();
    if (snd == NULL) {
      fprintf(stderr, "error: malloc() failed\n");
      exit(1);
    }
    rtp_sender_init(snd, sock, name, ssrc); 
    status = rtp_sender_init_srtp(snd, &policy);
    if (status) {
      fprintf(stderr, 
	      "error: srtp_create() failed with code %d\n", 
	      status);
      exit(1);
    }
 
    /* open dictionary */
    dict = fopen (dictfile, "r");
    if (dict == NULL) {
      fprintf(stderr, "%s: couldn't open file %s\n", argv[0], dictfile);
      if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
  	leave_group(sock, mreq, argv[0]);
      }
      exit(1);
    }
          
    /* read words from dictionary, then send them off */
    while (fgets(word, MAX_WORD_LEN, dict) != NULL) { 
      len = strlen(word) + 1;  /* plus one for null */
      
      if (len > MAX_WORD_LEN) 
	printf("error: word %s too large to send\n", word);
      else {
	rtp_sendto(snd, word, len);
        printf("sending word: %s", word);
      }
      usleep(USEC_RATE);
    }
    
  } else  { /* prog_type == receiver */
    rtp_receiver_t rcvr;
        
    if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
      close(sock);
      fprintf(stderr, "%s: socket bind error\n", argv[0]);
      perror(NULL);
      if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
    	leave_group(sock, mreq, argv[0]);
      }
      exit(1);
    }

    rcvr = rtp_receiver_alloc();
    if (rcvr == NULL) {
      fprintf(stderr, "error: malloc() failed\n");
      exit(1);
    }
    rtp_receiver_init(rcvr, sock, name, ssrc);
    status = rtp_receiver_init_srtp(rcvr, &policy);
    if (status) {
      fprintf(stderr, 
	      "error: srtp_create() failed with code %d\n", 
	      status);
      exit(1);
    }

    /* get next word and loop */
    while (1) {
      len = MAX_WORD_LEN;
      if (rtp_recvfrom(rcvr, word, &len) > -1)
	printf("\tword: %s", word);
    }
      
  } 

  if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
    leave_group(sock, mreq, argv[0]);
  }

#ifdef RTPW_USE_WINSOCK2
  WSACleanup();
#endif

  return 0;
}


void
usage(char *string) {

  printf("usage: %s [-d <debug>]* [-k <key> [-a][-e]] "
	 "[-s | -r] dest_ip dest_port\n"
	 "or     %s -l\n"
	 "where  -a use message authentication\n"
	 "       -e use encryption\n"
	 "       -k <key>  sets the srtp master key\n"
	 "       -s act as rtp sender\n"
	 "       -r act as rtp receiver\n"
	 "       -l list debug modules\n"
	 "       -d <debug> turn on debugging for module <debug>\n",
	 string, string);
  exit(1);
  
}


void
leave_group(int sock, struct ip_mreq mreq, char *name) {
  int ret;

  ret = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&mreq,
		   sizeof(mreq));
  if (ret < 0) {
	fprintf(stderr, "%s: Failed to leave multicast group", name);
	perror("");
  }
}

