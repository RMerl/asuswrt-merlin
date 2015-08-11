/*
 * srtp_driver.c
 * 
 * a test driver for libSRTP
 *
 * David A. McGrew
 * Cisco Systems, Inc.
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


#include <string.h>   /* for memcpy()          */
#include <time.h>     /* for clock()           */
#include <stdlib.h>   /* for malloc(), free()  */
#include <stdio.h>    /* for print(), fflush() */
#include "getopt_s.h" /* for local getopt()    */

#include "srtp_priv.h"

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#elif defined HAVE_WINSOCK2_H
# include <winsock2.h>
#endif

#define PRINT_REFERENCE_PACKET 1

err_status_t
srtp_validate(void);

err_status_t
srtp_create_big_policy(srtp_policy_t **list);

err_status_t
srtp_test_remove_stream(void);

double
srtp_bits_per_second(int msg_len_octets, const srtp_policy_t *policy);

double
srtp_rejections_per_second(int msg_len_octets, const srtp_policy_t *policy);

void
srtp_do_timing(const srtp_policy_t *policy);

void
srtp_do_rejection_timing(const srtp_policy_t *policy);

err_status_t
srtp_test(const srtp_policy_t *policy);

err_status_t
srtcp_test(const srtp_policy_t *policy);

err_status_t
srtp_session_print_policy(srtp_t srtp);

err_status_t
srtp_print_policy(const srtp_policy_t *policy); 

char *
srtp_packet_to_string(srtp_hdr_t *hdr, int packet_len);

double
mips_estimate(int num_trials, int *ignore);

extern uint8_t test_key[30];

void
usage(char *prog_name) {
  printf("usage: %s [ -t ][ -c ][ -v ][-d <debug_module> ]* [ -l ]\n"
         "  -t         run timing test\n"
	 "  -r         run rejection timing test\n"
         "  -c         run codec timing test\n"
         "  -v         run validation tests\n"
         "  -d <mod>   turn on debugging module <mod>\n"
         "  -l         list debugging modules\n", prog_name);
  exit(1);
}

/*
 * The policy_array is a null-terminated array of policy structs. it
 * is declared at the end of this file
 */

extern const srtp_policy_t *policy_array[];


/* the wildcard_policy is declared below; it has a wildcard ssrc */

extern const srtp_policy_t wildcard_policy;

/*
 * mod_driver debug module - debugging module for this test driver
 *
 * we use the crypto_kernel debugging system in this driver, which 
 * makes the interface uniform and increases portability
 */ 

debug_module_t mod_driver = {
  0,                  /* debugging is off by default */
  "driver"            /* printable name for module   */
};

int
main (int argc, char *argv[]) {
  char q;
  unsigned do_timing_test    = 0;
  unsigned do_rejection_test = 0;
  unsigned do_codec_timing   = 0;
  unsigned do_validation     = 0;
  unsigned do_list_mods      = 0;
  err_status_t status;

  /* 
   * verify that the compiler has interpreted the header data
   * structure srtp_hdr_t correctly
   */
  if (sizeof(srtp_hdr_t) != 12) {
     printf("error: srtp_hdr_t has incorrect size"
	    "(size is %ld bytes, expected 12)\n", 
	    sizeof(srtp_hdr_t));
    exit(1);
  }

  /* initialize srtp library */
  status = srtp_init();
  if (status) {
    printf("error: srtp init failed with error code %d\n", status);
    exit(1);
  }

  /*  load srtp_driver debug module */
  status = crypto_kernel_load_debug_module(&mod_driver);
    if (status) {
    printf("error: load of srtp_driver debug module failed "
           "with error code %d\n", status);
    exit(1);   
  }

  /* process input arguments */
  while (1) {
    q = getopt_s(argc, argv, "trcvld:");
    if (q == -1) 
      break;
    switch (q) {
    case 't':
      do_timing_test = 1;
      break;
    case 'r':
      do_rejection_test = 1;
      break;
    case 'c':
      do_codec_timing = 1;
      break;
    case 'v':
      do_validation = 1;
      break;
    case 'l':
      do_list_mods = 1;
      break;
    case 'd':
      status = crypto_kernel_set_debug_module(optarg_s, 1);
      if (status) {
        printf("error: set debug module (%s) failed\n", optarg_s);
        exit(1);
      }  
      break;
    default:
      usage(argv[0]);
    }    
  }

  if (!do_validation && !do_timing_test && !do_codec_timing 
      && !do_list_mods && !do_rejection_test)
    usage(argv[0]);

  if (do_list_mods) {
    status = crypto_kernel_list_debug_modules();
    if (status) {
      printf("error: list of debug modules failed\n");
      exit(1);
    }
  }
  
  if (do_validation) {
    const srtp_policy_t **policy = policy_array;
    srtp_policy_t *big_policy;

    /* loop over policy array, testing srtp and srtcp for each policy */
    while (*policy != NULL) {
      printf("testing srtp_protect and srtp_unprotect\n");
      if (srtp_test(*policy) == err_status_ok)
	printf("passed\n\n");
      else {
	printf("failed\n");
	exit(1);
      }
      printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp\n");
      if (srtcp_test(*policy) == err_status_ok)
	printf("passed\n\n");
      else {
	printf("failed\n");
	exit(1);
      }
      policy++;
    }

    /* create a big policy list and run tests on it */
    status = srtp_create_big_policy(&big_policy);
    if (status) {
      printf("unexpected failure with error code %d\n", status);
      exit(1);
    }
    printf("testing srtp_protect and srtp_unprotect with big policy\n");
    if (srtp_test(big_policy) == err_status_ok)
      printf("passed\n\n");
    else {
      printf("failed\n");
      exit(1);
    }

    /* run test on wildcard policy */
    printf("testing srtp_protect and srtp_unprotect on "
	   "wildcard ssrc policy\n");
    if (srtp_test(&wildcard_policy) == err_status_ok)
      printf("passed\n\n");
    else {
      printf("failed\n");
      exit(1);
    }   

    /*
     * run validation test against the reference packets - note 
     * that this test only covers the default policy
     */
    printf("testing srtp_protect and srtp_unprotect against "
	   "reference packets\n");
    if (srtp_validate() == err_status_ok) 
      printf("passed\n\n");
    else {
      printf("failed\n");
       exit(1); 
    }

    /*
     * test the function srtp_remove_stream()
     */
    printf("testing srtp_remove_stream()...");
    if (srtp_test_remove_stream() == err_status_ok)
      printf("passed\n");
    else {
      printf("failed\n");
      exit(1);
    }
  }
  
  if (do_timing_test) {
    const srtp_policy_t **policy = policy_array;
    
    /* loop over policies, run timing test for each */
    while (*policy != NULL) {
      srtp_print_policy(*policy);
      srtp_do_timing(*policy);
      policy++;
    }
  }

  if (do_rejection_test) {
    const srtp_policy_t **policy = policy_array;
    
    /* loop over policies, run rejection timing test for each */
    while (*policy != NULL) {
      srtp_print_policy(*policy);
      srtp_do_rejection_timing(*policy);
      policy++;
    }
  }
  
  if (do_codec_timing) {
    srtp_policy_t policy;
    int ignore;
    double mips = mips_estimate(1000000000, &ignore);

    crypto_policy_set_rtp_default(&policy.rtp);
    crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type  = ssrc_specific;
    policy.ssrc.value = 0xdecafbad;
    policy.key  = test_key;
    policy.next = NULL;

    printf("mips estimate: %e\n", mips);

    printf("testing srtp processing time for voice codecs:\n");
    printf("codec\t\tlength (octets)\t\tsrtp instructions/second\n");
    printf("G.711\t\t%d\t\t\t%e\n", 80, 
           (double) mips * (80 * 8) / 
	   srtp_bits_per_second(80, &policy) / .01 );
    printf("G.711\t\t%d\t\t\t%e\n", 160, 
           (double) mips * (160 * 8) / 
	   srtp_bits_per_second(160, &policy) / .02);
    printf("G.726-32\t%d\t\t\t%e\n", 40, 
           (double) mips * (40 * 8) / 
	   srtp_bits_per_second(40, &policy) / .01 );
    printf("G.726-32\t%d\t\t\t%e\n", 80, 
           (double) mips * (80 * 8) / 
	   srtp_bits_per_second(80, &policy) / .02);
    printf("G.729\t\t%d\t\t\t%e\n", 10, 
           (double) mips * (10 * 8) / 
	   srtp_bits_per_second(10, &policy) / .01 );
    printf("G.729\t\t%d\t\t\t%e\n", 20, 
           (double) mips * (20 * 8) /
	   srtp_bits_per_second(20, &policy) / .02 );
    printf("Wideband\t%d\t\t\t%e\n", 320, 
           (double) mips * (320 * 8) /
	   srtp_bits_per_second(320, &policy) / .01 );
    printf("Wideband\t%d\t\t\t%e\n", 640, 
           (double) mips * (640 * 8) /
	   srtp_bits_per_second(640, &policy) / .02 );
  }

  return 0;  
}



/*
 * srtp_create_test_packet(len, ssrc) returns a pointer to a
 * (malloced) example RTP packet whose data field has the length given
 * by pkt_octet_len and the SSRC value ssrc.  The total length of the
 * packet is twelve octets longer, since the header is at the
 * beginning.  There is room at the end of the packet for a trailer,
 * and the four octets following the packet are filled with 0xff
 * values to enable testing for overwrites.
 *
 * note that the location of the test packet can (and should) be
 * deallocated with the free() call once it is no longer needed.
 */

srtp_hdr_t *
srtp_create_test_packet(int pkt_octet_len, uint32_t ssrc) {
  int i;
  uint8_t *buffer;
  srtp_hdr_t *hdr;
  int bytes_in_hdr = 12;

  /* allocate memory for test packet */
  hdr = (srtp_hdr_t*) malloc(pkt_octet_len + bytes_in_hdr
	       + SRTP_MAX_TRAILER_LEN + 4);
  if (!hdr)
    return NULL;
  
  hdr->version = 2;              /* RTP version two     */
  hdr->p    = 0;                 /* no padding needed   */
  hdr->x    = 0;                 /* no header extension */
  hdr->cc   = 0;                 /* no CSRCs            */
  hdr->m    = 0;                 /* marker bit          */
  hdr->pt   = 0xf;               /* payload type        */
  hdr->seq  = htons(0x1234);     /* sequence number     */
  hdr->ts   = htonl(0xdecafbad); /* timestamp           */
  hdr->ssrc = htonl(ssrc);       /* synch. source       */

  buffer = (uint8_t *)hdr;
  buffer += bytes_in_hdr;

  /* set RTP data to 0xab */
  for (i=0; i < pkt_octet_len; i++)
    *buffer++ = 0xab;

  /* set post-data value to 0xffff to enable overrun checking */
  for (i=0; i < SRTP_MAX_TRAILER_LEN+4; i++)
    *buffer++ = 0xff;

  return hdr;
}

void
srtp_do_timing(const srtp_policy_t *policy) {
  int len;

  /*
   * note: the output of this function is formatted so that it
   * can be used in gnuplot.  '#' indicates a comment, and "\r\n"
   * terminates a record
   */
  
  printf("# testing srtp throughput:\r\n");
  printf("# mesg length (octets)\tthroughput (megabits per second)\r\n");
  
  for (len=16; len <= 2048; len *= 2)
    printf("%d\t\t\t%f\r\n", len, 
	   srtp_bits_per_second(len, policy) / 1.0E6);
  
  /* these extra linefeeds let gnuplot know that a dataset is done */
  printf("\r\n\r\n");  

}

void
srtp_do_rejection_timing(const srtp_policy_t *policy) {
  int len;

  /*
   * note: the output of this function is formatted so that it
   * can be used in gnuplot.  '#' indicates a comment, and "\r\n"
   * terminates a record
   */
  
  printf("# testing srtp rejection throughput:\r\n");
  printf("# mesg length (octets)\trejections per second\r\n");
  
  for (len=8; len <= 2048; len *= 2)
    printf("%d\t\t\t%e\r\n", len, srtp_rejections_per_second(len, policy));
  
  /* these extra linefeeds let gnuplot know that a dataset is done */
  printf("\r\n\r\n");  

}


#define MAX_MSG_LEN 1024

double
srtp_bits_per_second(int msg_len_octets, const srtp_policy_t *policy) {
  srtp_t srtp;
  srtp_hdr_t *mesg;  
  int i;
  clock_t timer;
  int num_trials = 100000;
  int len;
  uint32_t ssrc;
  err_status_t status;

  /*
   * allocate and initialize an srtp session
   */
  status = srtp_create(&srtp, policy);
  if (status) {
    printf("error: srtp_create() failed with error code %d\n", status);
    exit(1);
  }

  /*
   * if the ssrc is unspecified, use a predetermined one
   */
  if (policy->ssrc.type != ssrc_specific) {
    ssrc = 0xdeadbeef;
  } else {
    ssrc = policy->ssrc.value;
  }

  /*
   * create a test packet
   */
  mesg = srtp_create_test_packet(msg_len_octets, ssrc);
  if (mesg == NULL)
    return 0.0;   /* indicate failure by returning zero */
  
  timer = clock();
  for (i=0; i < num_trials; i++) {
    err_status_t status;
    len = msg_len_octets + 12;  /* add in rtp header length */
    
    /* srtp protect message */
    status = srtp_protect(srtp, mesg, &len);
    if (status) {
      printf("error: srtp_protect() failed with error code %d\n", status);
      exit(1);
    }

    /* increment message number */
    mesg->seq = htons(ntohs(mesg->seq) + 1);

  }
  timer = clock() - timer;

  free(mesg);
  
  return (double) (msg_len_octets) * 8 *
                  num_trials * CLOCKS_PER_SEC / timer;   
}

double
srtp_rejections_per_second(int msg_len_octets, const srtp_policy_t *policy) {
  srtp_ctx_t *srtp;
  srtp_hdr_t *mesg; 
  int i;
  int len;
  clock_t timer;
  int num_trials = 1000000;
  uint32_t ssrc = policy->ssrc.value;
  err_status_t status;

  /*
   * allocate and initialize an srtp session
   */
  status = srtp_create(&srtp, policy);
  if (status) {
    printf("error: srtp_create() failed with error code %d\n", status);
    exit(1);
  } 

  mesg = srtp_create_test_packet(msg_len_octets, ssrc);
  if (mesg == NULL)
    return 0.0;  /* indicate failure by returning zero */
  
  len = msg_len_octets;  
  srtp_protect(srtp, (srtp_hdr_t *)mesg, &len);
  
  timer = clock();
  for (i=0; i < num_trials; i++) {
    len = msg_len_octets;
    srtp_unprotect(srtp, (srtp_hdr_t *)mesg, &len);
  }
  timer = clock() - timer;

  free(mesg);
  
  return (double) num_trials * CLOCKS_PER_SEC / timer;   
}


void
err_check(err_status_t s) {
  if (s == err_status_ok) 
    return;
  else
    fprintf(stderr, "error: unexpected srtp failure (code %d)\n", s);
  exit (1);
}

err_status_t
srtp_test(const srtp_policy_t *policy) {
  int i;
  srtp_t srtp_sender;
  srtp_t srtp_rcvr;
  err_status_t status = err_status_ok;
  srtp_hdr_t *hdr, *hdr2;
  uint8_t hdr_enc[64];
  uint8_t *pkt_end;
  int msg_len_octets, msg_len_enc;
  int len;
  int tag_length = policy->rtp.auth_tag_len; 
  uint32_t ssrc;
  srtp_policy_t *rcvr_policy;

  err_check(srtp_create(&srtp_sender, policy));

  /* print out policy */
  err_check(srtp_session_print_policy(srtp_sender)); 

  /*
   * initialize data buffer, using the ssrc in the policy unless that
   * value is a wildcard, in which case we'll just use an arbitrary
   * one
   */
  if (policy->ssrc.type != ssrc_specific)
    ssrc = 0xdecafbad;
  else
    ssrc = policy->ssrc.value;
  msg_len_octets = 28;
  hdr = srtp_create_test_packet(msg_len_octets, ssrc);

  if (hdr == NULL)
    return err_status_alloc_fail;
  hdr2 = srtp_create_test_packet(msg_len_octets, ssrc);
  if (hdr2 == NULL) {
    free(hdr);
    return err_status_alloc_fail;
  }

  /* set message length */
  len = msg_len_octets;

  debug_print(mod_driver, "before protection:\n%s", 	      
	      srtp_packet_to_string(hdr, len));

#if PRINT_REFERENCE_PACKET
  debug_print(mod_driver, "reference packet before protection:\n%s", 	      
	      octet_string_hex_string((uint8_t *)hdr, len));
#endif
  err_check(srtp_protect(srtp_sender, hdr, &len));

  debug_print(mod_driver, "after protection:\n%s", 	      
	      srtp_packet_to_string(hdr, len));
#if PRINT_REFERENCE_PACKET
  debug_print(mod_driver, "after protection:\n%s", 	      
	      octet_string_hex_string((uint8_t *)hdr, len));
#endif

  /* save protected message and length */
  memcpy(hdr_enc, hdr, len);
  msg_len_enc = len;

  /* 
   * check for overrun of the srtp_protect() function
   *
   * The packet is followed by a value of 0xfffff; if the value of the
   * data following the packet is different, then we know that the
   * protect function is overwriting the end of the packet.
   */
  pkt_end = (uint8_t *)hdr + sizeof(srtp_hdr_t) 
    + msg_len_octets + tag_length;
  for (i = 0; i < 4; i++)
    if (pkt_end[i] != 0xff) {
      fprintf(stdout, "overwrite in srtp_protect() function "
              "(expected %x, found %x in trailing octet %d)\n",
              0xff, ((uint8_t *)hdr)[i], i);
      free(hdr);
      free(hdr2);
      return err_status_algo_fail;
    }  

  /*
   * if the policy includes confidentiality, check that ciphertext is
   * different than plaintext
   * 
   * Note that this check will give false negatives, with some small
   * probability, especially if the packets are short.  For that
   * reason, we skip this check if the plaintext is less than four
   * octets long.
   */
  if ((policy->rtp.sec_serv & sec_serv_conf) && (msg_len_octets >= 4)) {
    printf("testing that ciphertext is distinct from plaintext...");
    status = err_status_algo_fail;
    for (i=12; i < msg_len_octets+12; i++)
      if (((uint8_t *)hdr)[i] != ((uint8_t *)hdr2)[i]) {
	status = err_status_ok;
      }
    if (status) {
      printf("failed\n");
      free(hdr);
      free(hdr2);
      return status;
    }
    printf("passed\n");
  }
  
  /*
   * if the policy uses a 'wildcard' ssrc, then we need to make a copy
   * of the policy that changes the direction to inbound
   *
   * we always copy the policy into the rcvr_policy, since otherwise
   * the compiler would fret about the constness of the policy
   */
  rcvr_policy = (srtp_policy_t*) malloc(sizeof(srtp_policy_t));
  if (rcvr_policy == NULL)
    return err_status_alloc_fail;
  memcpy(rcvr_policy, policy, sizeof(srtp_policy_t));
  if (policy->ssrc.type == ssrc_any_outbound) {
    rcvr_policy->ssrc.type = ssrc_any_inbound;       
  } 

  err_check(srtp_create(&srtp_rcvr, rcvr_policy));
   
  err_check(srtp_unprotect(srtp_rcvr, hdr, &len));

  debug_print(mod_driver, "after unprotection:\n%s", 	      
	      srtp_packet_to_string(hdr, len));

  /* verify that the unprotected packet matches the origial one */
  for (i=0; i < msg_len_octets; i++)
    if (((uint8_t *)hdr)[i] != ((uint8_t *)hdr2)[i]) {
      fprintf(stdout, "mismatch at octet %d\n", i);
      status = err_status_algo_fail;
    }
  if (status) {
    free(hdr);
    free(hdr2);
    return status;
  }

  /* 
   * if the policy includes authentication, then test for false positives
   */  
  if (policy->rtp.sec_serv & sec_serv_auth) {
    char *data = ((char *)hdr) + 12;
    
    printf("testing for false positives in replay check...");

    /* set message length */
    len = msg_len_enc;

    /* unprotect a second time - should fail with a replay error */
    status = srtp_unprotect(srtp_rcvr, hdr_enc, &len);
    if (status != err_status_replay_fail) {
      printf("failed with error code %d\n", status);
      free(hdr); 
      free(hdr2);
      return status;
    } else {
      printf("passed\n");
    }

    printf("testing for false positives in auth check...");

    /* increment sequence number in header */
    hdr->seq++; 

    /* set message length */
    len = msg_len_octets;

    /* apply protection */
    err_check(srtp_protect(srtp_sender, hdr, &len));
    
    /* flip bits in packet */
    data[0] ^= 0xff;

    /* unprotect, and check for authentication failure */
    status = srtp_unprotect(srtp_rcvr, hdr, &len);
    if (status != err_status_auth_fail) {
      printf("failed\n");
      free(hdr); 
      free(hdr2);
      return status;
    } else {
      printf("passed\n");
    }
            
  }

  err_check(srtp_dealloc(srtp_sender));
  err_check(srtp_dealloc(srtp_rcvr));

  free(hdr);
  free(hdr2);
  return err_status_ok;
}


err_status_t
srtcp_test(const srtp_policy_t *policy) {
  int i;
  srtp_t srtcp_sender;
  srtp_t srtcp_rcvr;
  err_status_t status = err_status_ok;
  srtp_hdr_t *hdr, *hdr2;
  uint8_t hdr_enc[64];
  uint8_t *pkt_end;
  int msg_len_octets, msg_len_enc;
  int len;
  int tag_length = policy->rtp.auth_tag_len; 
  uint32_t ssrc;
  srtp_policy_t *rcvr_policy;

  err_check(srtp_create(&srtcp_sender, policy));

  /* print out policy */
  err_check(srtp_session_print_policy(srtcp_sender)); 

  /*
   * initialize data buffer, using the ssrc in the policy unless that
   * value is a wildcard, in which case we'll just use an arbitrary
   * one
   */
  if (policy->ssrc.type != ssrc_specific)
    ssrc = 0xdecafbad;
  else
    ssrc = policy->ssrc.value;
  msg_len_octets = 28;
  hdr = srtp_create_test_packet(msg_len_octets, ssrc);

  if (hdr == NULL)
    return err_status_alloc_fail;
  hdr2 = srtp_create_test_packet(msg_len_octets, ssrc);
  if (hdr2 == NULL) {
    free(hdr);
    return err_status_alloc_fail;
  }

  /* set message length */
  len = msg_len_octets;

  debug_print(mod_driver, "before protection:\n%s", 	      
	      srtp_packet_to_string(hdr, len));

#if PRINT_REFERENCE_PACKET
  debug_print(mod_driver, "reference packet before protection:\n%s", 	      
	      octet_string_hex_string((uint8_t *)hdr, len));
#endif
  err_check(srtp_protect_rtcp(srtcp_sender, hdr, &len));

  debug_print(mod_driver, "after protection:\n%s", 	      
	      srtp_packet_to_string(hdr, len));
#if PRINT_REFERENCE_PACKET
  debug_print(mod_driver, "after protection:\n%s", 	      
	      octet_string_hex_string((uint8_t *)hdr, len));
#endif

  /* save protected message and length */
  memcpy(hdr_enc, hdr, len);
  msg_len_enc = len;

  /* 
   * check for overrun of the srtp_protect() function
   *
   * The packet is followed by a value of 0xfffff; if the value of the
   * data following the packet is different, then we know that the
   * protect function is overwriting the end of the packet.
   */
  pkt_end = (uint8_t *)hdr + sizeof(srtp_hdr_t) 
    + msg_len_octets + tag_length;
  for (i = 0; i < 4; i++)
    if (pkt_end[i] != 0xff) {
      fprintf(stdout, "overwrite in srtp_protect_rtcp() function "
              "(expected %x, found %x in trailing octet %d)\n",
              0xff, ((uint8_t *)hdr)[i], i);
      free(hdr);
      free(hdr2);
      return err_status_algo_fail;
    }  

  /*
   * if the policy includes confidentiality, check that ciphertext is
   * different than plaintext
   * 
   * Note that this check will give false negatives, with some small
   * probability, especially if the packets are short.  For that
   * reason, we skip this check if the plaintext is less than four
   * octets long.
   */
  if ((policy->rtp.sec_serv & sec_serv_conf) && (msg_len_octets >= 4)) {
    printf("testing that ciphertext is distinct from plaintext...");
    status = err_status_algo_fail;
    for (i=12; i < msg_len_octets+12; i++)
      if (((uint8_t *)hdr)[i] != ((uint8_t *)hdr2)[i]) {
	status = err_status_ok;
      }
    if (status) {
      printf("failed\n");
      free(hdr);
      free(hdr2);
      return status;
    }
    printf("passed\n");
  }
  
  /*
   * if the policy uses a 'wildcard' ssrc, then we need to make a copy
   * of the policy that changes the direction to inbound
   *
   * we always copy the policy into the rcvr_policy, since otherwise
   * the compiler would fret about the constness of the policy
   */
  rcvr_policy = (srtp_policy_t*) malloc(sizeof(srtp_policy_t));
  if (rcvr_policy == NULL)
    return err_status_alloc_fail;
  memcpy(rcvr_policy, policy, sizeof(srtp_policy_t));
  if (policy->ssrc.type == ssrc_any_outbound) {
    rcvr_policy->ssrc.type = ssrc_any_inbound;       
  } 

  err_check(srtp_create(&srtcp_rcvr, rcvr_policy));
   
  err_check(srtp_unprotect_rtcp(srtcp_rcvr, hdr, &len));

  debug_print(mod_driver, "after unprotection:\n%s", 	      
	      srtp_packet_to_string(hdr, len));

  /* verify that the unprotected packet matches the origial one */
  for (i=0; i < msg_len_octets; i++)
    if (((uint8_t *)hdr)[i] != ((uint8_t *)hdr2)[i]) {
      fprintf(stdout, "mismatch at octet %d\n", i);
      status = err_status_algo_fail;
    }
  if (status) {
    free(hdr);
    free(hdr2);
    return status;
  }

  /* 
   * if the policy includes authentication, then test for false positives
   */  
  if (policy->rtp.sec_serv & sec_serv_auth) {
    char *data = ((char *)hdr) + 12;
    
    printf("testing for false positives in replay check...");

    /* set message length */
    len = msg_len_enc;

    /* unprotect a second time - should fail with a replay error */
    status = srtp_unprotect_rtcp(srtcp_rcvr, hdr_enc, &len);
    if (status != err_status_replay_fail) {
      printf("failed with error code %d\n", status);
      free(hdr); 
      free(hdr2);
      return status;
    } else {
      printf("passed\n");
    }

    printf("testing for false positives in auth check...");

    /* increment sequence number in header */
    hdr->seq++; 

    /* set message length */
    len = msg_len_octets;

    /* apply protection */
    err_check(srtp_protect_rtcp(srtcp_sender, hdr, &len));
    
    /* flip bits in packet */
    data[0] ^= 0xff;

    /* unprotect, and check for authentication failure */
    status = srtp_unprotect_rtcp(srtcp_rcvr, hdr, &len);
    if (status != err_status_auth_fail) {
      printf("failed\n");
      free(hdr); 
      free(hdr2);
      return status;
    } else {
      printf("passed\n");
    }
            
  }

  err_check(srtp_dealloc(srtcp_sender));
  err_check(srtp_dealloc(srtcp_rcvr));

  free(hdr);
  free(hdr2);
  return err_status_ok;
}


err_status_t
srtp_session_print_policy(srtp_t srtp) {
  char *serv_descr[4] = {
    "none",
    "confidentiality",
    "authentication",
    "confidentiality and authentication"
  };
  char *direction[3] = {
    "unknown",
    "outbound",
    "inbound"
  };
  srtp_stream_t stream;

  /* sanity checking */
  if (srtp == NULL)
    return err_status_fail;

  /* if there's a template stream, print it out */
  if (srtp->stream_template != NULL) {
    stream = srtp->stream_template;
    printf("# SSRC:          any %s\r\n"
	   "# rtp cipher:    %s\r\n"
	   "# rtp auth:      %s\r\n"
	   "# rtp services:  %s\r\n" 
           "# rtcp cipher:   %s\r\n"
	   "# rtcp auth:     %s\r\n"
	   "# rtcp services: %s\r\n",
	   direction[stream->direction],
	   stream->rtp_cipher->type->description,
	   stream->rtp_auth->type->description,
	   serv_descr[stream->rtp_services],
	   stream->rtcp_cipher->type->description,
	   stream->rtcp_auth->type->description,
	   serv_descr[stream->rtcp_services]);
  }

  /* loop over streams in session, printing the policy of each */
  stream = srtp->stream_list;
  while (stream != NULL) {
    if (stream->rtp_services > sec_serv_conf_and_auth)
      return err_status_bad_param;
    
    printf("# SSRC:          0x%08x\r\n"
	   "# rtp cipher:    %s\r\n"
	   "# rtp auth:      %s\r\n"
	   "# rtp services:  %s\r\n" 
           "# rtcp cipher:   %s\r\n"
	   "# rtcp auth:     %s\r\n"
	   "# rtcp services: %s\r\n",
	   stream->ssrc,
	   stream->rtp_cipher->type->description,
	   stream->rtp_auth->type->description,
	   serv_descr[stream->rtp_services],
	   stream->rtcp_cipher->type->description,
	   stream->rtcp_auth->type->description,
	   serv_descr[stream->rtcp_services]);

    /* advance to next stream in the list */
    stream = stream->next;
  } 
  return err_status_ok;
}

err_status_t
srtp_print_policy(const srtp_policy_t *policy) {
  err_status_t status;
  srtp_t session;

  status = srtp_create(&session, policy);
  if (status)
    return status;
  status = srtp_session_print_policy(session);
  if (status)
    return status;
  status = srtp_dealloc(session);
  if (status)
    return status;
  return err_status_ok;
}

/* 
 * srtp_print_packet(...) is for debugging only 
 * it prints an RTP packet to the stdout
 *
 * note that this function is *not* threadsafe
 */

#include <stdio.h>

#define MTU 2048

char packet_string[MTU];

char *
srtp_packet_to_string(srtp_hdr_t *hdr, int pkt_octet_len) {
  int octets_in_rtp_header = 12;
  uint8_t *data = ((uint8_t *)hdr)+octets_in_rtp_header;
  int hex_len = pkt_octet_len-octets_in_rtp_header;

  /* sanity checking */
  if ((hdr == NULL) || (pkt_octet_len > MTU))
    return NULL;

  /* write packet into string */
  sprintf(packet_string, 
	  "(s)rtp packet: {\n"
	  "   version:\t%d\n" 
	  "   p:\t\t%d\n"     
	  "   x:\t\t%d\n"     
	  "   cc:\t\t%d\n"    
	  "   m:\t\t%d\n"     
	  "   pt:\t\t%x\n"    
	  "   seq:\t\t%x\n"   
	  "   ts:\t\t%x\n"    
	  "   ssrc:\t%x\n"    
	  "   data:\t%s\n"    
	  "} (%d octets in total)\n", 
	  hdr->version,  
	  hdr->p,	       
	  hdr->x,	       
	  hdr->cc,       
	  hdr->m,	       
	  hdr->pt,       
	  hdr->seq,      
	  hdr->ts,       
	  hdr->ssrc,      
  	  octet_string_hex_string(data, hex_len),
	  pkt_octet_len);

  return packet_string;
}

/*
 * mips_estimate() is a simple function to estimate the number of
 * instructions per second that the host can perform.  note that this
 * function can be grossly wrong; you may want to have a manual sanity
 * check of its output!
 *
 * the 'ignore' pointer is there to convince the compiler to not just
 * optimize away the function
 */

double
mips_estimate(int num_trials, int *ignore) {
  clock_t t;
  int i, sum;

  sum = 0;
  t = clock();
  for (i=0; i<num_trials; i++)
    sum += i;
  t = clock() - t;

/*   printf("%d\n", sum); */
  *ignore = sum;

  return (double) num_trials * CLOCKS_PER_SEC / t;
}


/*
 * srtp_validate() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the default SRTP policy.
 */


err_status_t
srtp_validate() {
  unsigned char test_key[30] = {
    0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
    0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
    0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
  };
  uint8_t srtp_plaintext_ref[28] = {
    0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad, 
    0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
    0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 
    0xab, 0xab, 0xab, 0xab
  };
  uint8_t srtp_plaintext[38] = {
    0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad, 
    0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
    0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 
    0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  uint8_t srtp_ciphertext[38] = {
    0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad, 
    0xca, 0xfe, 0xba, 0xbe, 0x4e, 0x55, 0xdc, 0x4c,
    0xe7, 0x99, 0x78, 0xd8, 0x8c, 0xa4, 0xd2, 0x15, 
    0x94, 0x9d, 0x24, 0x02, 0xb7, 0x8d, 0x6a, 0xcc,
    0x99, 0xea, 0x17, 0x9b, 0x8d, 0xbb
  };
  srtp_t srtp_snd, srtp_recv;
  err_status_t status;
  int len;
  srtp_policy_t policy;
  
  /*
   * create a session with a single stream using the default srtp
   * policy and with the SSRC value 0xcafebabe
   */
  crypto_policy_set_rtp_default(&policy.rtp);
  crypto_policy_set_rtcp_default(&policy.rtcp);
  policy.ssrc.type  = ssrc_specific;
  policy.ssrc.value = 0xcafebabe;
  policy.key  = test_key;
  policy.next = NULL;

  status = srtp_create(&srtp_snd, &policy);
  if (status)
    return status;
 
  /* 
   * protect plaintext, then compare with ciphertext 
   */
  len = 28;
  status = srtp_protect(srtp_snd, srtp_plaintext, &len);
  if (status || (len != 38))
    return err_status_fail;

  debug_print(mod_driver, "ciphertext:\n  %s", 	      
	      octet_string_hex_string(srtp_plaintext, len));
  debug_print(mod_driver, "ciphertext reference:\n  %s", 	      
	      octet_string_hex_string(srtp_ciphertext, len));

  if (octet_string_is_eq(srtp_plaintext, srtp_ciphertext, len))
    return err_status_fail;
  
  /*
   * create a receiver session context comparable to the one created
   * above - we need to do this so that the replay checking doesn't
   * complain
   */
  status = srtp_create(&srtp_recv, &policy);
  if (status)
    return status;

  /*
   * unprotect ciphertext, then compare with plaintext 
   */
  status = srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
  if (status || (len != 28))
    return status;
  
  if (octet_string_is_eq(srtp_ciphertext, srtp_plaintext_ref, len))
    return err_status_fail;

  return err_status_ok;
}


err_status_t
srtp_create_big_policy(srtp_policy_t **list) {
  extern const srtp_policy_t *policy_array[];
  srtp_policy_t *p, *tmp;
  int i = 0;
  uint32_t ssrc = 0;

  /* sanity checking */
  if ((list == NULL) || (policy_array[0] == NULL))
    return err_status_bad_param;

  /* 
   * loop over policy list, mallocing a new list and copying values
   * into it (and incrementing the SSRC value as we go along)
   */
  tmp = NULL;
  while (policy_array[i] != NULL) {
    p  = (srtp_policy_t*) malloc(sizeof(srtp_policy_t));
    if (p == NULL)
      return err_status_bad_param;
    memcpy(p, policy_array[i], sizeof(srtp_policy_t));
    p->ssrc.type = ssrc_specific;
    p->ssrc.value = ssrc++;
    p->next = tmp;
    tmp = p;
    i++;
  }
  *list = p;
 
  return err_status_ok;
}

err_status_t
srtp_test_remove_stream() { 
  err_status_t status;
  srtp_policy_t *policy_list;
  srtp_t session;
  srtp_stream_t stream;
  /* 
   * srtp_get_stream() is a libSRTP internal function that we declare
   * here so that we can use it to verify the correct operation of the
   * library
   */ 
  extern srtp_stream_t srtp_get_stream(srtp_t srtp, uint32_t ssrc);
  

  status = srtp_create_big_policy(&policy_list);
  if (status)
    return status;

  status = srtp_create(&session, policy_list);
  if (status)
    return status;

  /*
   * check for false positives by trying to remove a stream that's not
   * in the session
   */
  status = srtp_remove_stream(session, htonl(0xaaaaaaaa));
  if (status != err_status_no_ctx)
    return err_status_fail;
  
  /* 
   * check for false negatives by removing stream 0x1, then
   * searching for streams 0x0 and 0x2
   */
  status = srtp_remove_stream(session, htonl(0x1));
  if (status != err_status_ok)
    return err_status_fail;
  stream = srtp_get_stream(session, htonl(0x0));
  if (stream == NULL)
    return err_status_fail;
  stream = srtp_get_stream(session, htonl(0x2));
  if (stream == NULL)
    return err_status_fail;  

  return err_status_ok;  
}

/*
 * srtp policy definitions - these definitions are used above
 */

unsigned char test_key[30] = {
    0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
    0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
    0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
};


const srtp_policy_t default_policy = {
  { ssrc_any_outbound, 0 },  /* SSRC                           */
  {                      /* SRTP policy                    */                  
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    HMAC_SHA1,              /* authentication func type    */
    16,                     /* auth key length in octets   */
    10,                     /* auth tag length in octets   */
    sec_serv_conf_and_auth  /* security services flag      */
  },
  {                      /* SRTCP policy                   */
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    HMAC_SHA1,              /* authentication func type    */
    16,                     /* auth key length in octets   */
    10,                     /* auth tag length in octets   */
    sec_serv_conf_and_auth  /* security services flag      */
  },
  test_key,
  NULL
};

const srtp_policy_t aes_tmmh_policy = {
  { ssrc_any_outbound, 0 },     /* SSRC                        */
  { 
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    UST_TMMHv2,             /* authentication func type    */
    94,                     /* auth key length in octets   */
    4,                      /* auth tag length in octets   */
    sec_serv_conf_and_auth  /* security services flag      */
  },
  { 
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    UST_TMMHv2,             /* authentication func type    */
    94,                     /* auth key length in octets   */
    4,                      /* auth tag length in octets   */
    sec_serv_conf_and_auth  /* security services flag      */
  },
  test_key,
  NULL
};

const srtp_policy_t tmmh_only_policy = {
  { ssrc_any_outbound, 0 },     /* SSRC                        */
  {
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    UST_TMMHv2,             /* authentication func type    */
    94,                     /* auth key length in octets   */
    4,                      /* auth tag length in octets   */
    sec_serv_auth           /* security services flag      */
  },
  {
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    UST_TMMHv2,             /* authentication func type    */
    94,                     /* auth key length in octets   */
    4,                      /* auth tag length in octets   */
    sec_serv_auth           /* security services flag      */
  },
  test_key,
  NULL
};

const srtp_policy_t aes_only_policy = {
  { ssrc_any_outbound, 0 },     /* SSRC                        */ 
  {
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    NULL_AUTH,              /* authentication func type    */
    0,                      /* auth key length in octets   */
    0,                      /* auth tag length in octets   */
    sec_serv_conf           /* security services flag      */
  },
  {
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    NULL_AUTH,              /* authentication func type    */
    0,                      /* auth key length in octets   */
    0,                      /* auth tag length in octets   */
    sec_serv_conf           /* security services flag      */
  },
  test_key,
  NULL
};

const srtp_policy_t hmac_only_policy = {
  { ssrc_any_outbound, 0 },     /* SSRC                        */
  {
    NULL_CIPHER,            /* cipher type                 */
    0,                      /* cipher key length in octets */
    HMAC_SHA1,              /* authentication func type    */
    20,                     /* auth key length in octets   */
    4,                      /* auth tag length in octets   */
    sec_serv_auth           /* security services flag      */
  },  
  {
    NULL_CIPHER,            /* cipher type                 */
    0,                      /* cipher key length in octets */
    HMAC_SHA1,              /* authentication func type    */
    20,                     /* auth key length in octets   */
    4,                      /* auth tag length in octets   */
    sec_serv_auth           /* security services flag      */
  },
  test_key,
  NULL
};

const srtp_policy_t null_policy = {
  { ssrc_any_outbound, 0 },     /* SSRC                        */ 
  {
    NULL_CIPHER,            /* cipher type                 */
    0,                      /* cipher key length in octets */
    NULL_AUTH,              /* authentication func type    */
    0,                      /* auth key length in octets   */
    0,                      /* auth tag length in octets   */
    sec_serv_none           /* security services flag      */  
  },
  {
    NULL_CIPHER,            /* cipher type                 */
    0,                      /* cipher key length in octets */
    NULL_AUTH,              /* authentication func type    */
    0,                      /* auth key length in octets   */
    0,                      /* auth tag length in octets   */
    sec_serv_none           /* security services flag      */  
  },
  test_key,
  NULL
};


/*
 * an array of pointers to the policies listed above
 *
 * This array is used to test various aspects of libSRTP for
 * different cryptographic policies.  The order of the elements
 * matters - the timing test generates output that can be used
 * in a plot (see the gnuplot script file 'timing').  If you 
 * add to this list, you should do it at the end.
 */

#define USE_TMMH 0

const srtp_policy_t *
policy_array[] = {
  &hmac_only_policy,
#if USE_TMMH
  &tmmh_only_policy,
#endif
  &aes_only_policy,
#if USE_TMMH
  &aes_tmmh_policy,
#endif
  &default_policy,
  &null_policy,
  NULL
};

const srtp_policy_t wildcard_policy = {
  { ssrc_any_outbound, 0 }, /* SSRC                        */
  {                      /* SRTP policy                    */                  
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    HMAC_SHA1,              /* authentication func type    */
    16,                     /* auth key length in octets   */
    10,                     /* auth tag length in octets   */
    sec_serv_conf_and_auth  /* security services flag      */
  },
  {                      /* SRTCP policy                   */
    AES_128_ICM,            /* cipher type                 */
    30,                     /* cipher key length in octets */
    HMAC_SHA1,              /* authentication func type    */
    16,                     /* auth key length in octets   */
    10,                     /* auth tag length in octets   */
    sec_serv_conf_and_auth  /* security services flag      */
  },
  test_key,
  NULL
};
