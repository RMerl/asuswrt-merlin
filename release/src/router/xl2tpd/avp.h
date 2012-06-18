/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Attribute Value Pair structures and
 * definitions
 */

#include "common.h"

struct avp_hdr
{
    _u16 length;
    _u16 vendorid;
    _u16 attr;
} __attribute__((packed));

struct avp
{
    int num;                    /* Number of AVP */
    int m;                      /* Set M? */
    int (*handler) (struct tunnel *, struct call *, void *, int);
    /* This should handle the AVP
       taking a tunnel, call, the data,
       and the length of the AVP as
       parameters.  Should return 0
       upon success */
    char *description;          /* A name, for debugging */
};

extern int handle_avps (struct buffer *buf, struct tunnel *t, struct call *c);

extern char *msgtypes[];

#define VENDOR_ID 0             /* We don't have any extensions
                                   so we shoouldn't have to
                                   worry about this */

/*
 * Macros to extract information from length field of AVP
 */

#define AMBIT(len) (len & 0x8000)       /* Mandatory bit: If this is
                                           set on an unknown AVP, 
                                           we MUST terminate */

#define AHBIT(len) (len & 0x4000)       /* Hidden bit: Specifies
                                           information hiding */

#define AZBITS(len) (len & 0x3C00)      /* Reserved bits:  We must
                                           drop anything with any
                                           of these set.  */

#define ALENGTH(len) (len & 0x03FF)     /* Length:  Lenth of AVP */

#define MAXAVPSIZE 1023

#define MAXTIME 300             /* time to wait before checking
                                   Ns and Nr, in ms */

#define MBIT 0x8000             /* for setting */
#define HBIT 0x4000             /* Set on hidden avp's */

#define ASYNC_FRAMING 2
#define SYNC_FRAMING 1

#define ANALOG_BEARER 2
#define DIGITAL_BEARER 1

#define VENDOR_ERROR 6

#define ERROR_RESERVED 3
#define ERROR_LENGTH 2
#define ERROR_NOTEXIST 1
#define ERROR_NORES 4
#define ERROR_INVALID 6
#define RESULT_CLEAR 1
#define RESULT_ERROR 2
#define RESULT_EXISTS 3
extern void encrypt_avp (struct buffer *, _u16, struct tunnel *);
extern int decrypt_avp (char *, struct tunnel *);
extern int message_type_avp (struct tunnel *, struct call *, void *, int);
extern int protocol_version_avp (struct tunnel *, struct call *, void *, int);
extern int framing_caps_avp (struct tunnel *, struct call *, void *, int);
extern int bearer_caps_avp (struct tunnel *, struct call *, void *, int);
extern int firmware_rev_avp (struct tunnel *, struct call *, void *, int);
extern int hostname_avp (struct tunnel *, struct call *, void *, int);
extern int vendor_avp (struct tunnel *, struct call *, void *, int);
extern int assigned_tunnel_avp (struct tunnel *, struct call *, void *, int);
extern int receive_window_size_avp (struct tunnel *, struct call *, void *,
                                    int);
extern int result_code_avp (struct tunnel *, struct call *, void *, int);
extern int assigned_call_avp (struct tunnel *, struct call *, void *, int);
extern int call_serno_avp (struct tunnel *, struct call *, void *, int);
extern int bearer_type_avp (struct tunnel *, struct call *, void *, int);
extern int call_physchan_avp (struct tunnel *, struct call *, void *, int);
extern int dialed_number_avp (struct tunnel *, struct call *, void *, int);
extern int dialing_number_avp (struct tunnel *, struct call *, void *, int);
extern int sub_address_avp (struct tunnel *, struct call *, void *, int);
extern int frame_type_avp (struct tunnel *, struct call *, void *, int);
extern int rx_speed_avp (struct tunnel *, struct call *, void *, int);
extern int tx_speed_avp (struct tunnel *, struct call *, void *, int);
extern int packet_delay_avp (struct tunnel *, struct call *, void *, int);
extern int ignore_avp (struct tunnel *, struct call *, void *, int);
extern int seq_reqd_avp (struct tunnel *, struct call *, void *, int);
extern int challenge_avp (struct tunnel *, struct call *, void *, int);
extern int chalresp_avp (struct tunnel *, struct call *, void *, int);
extern int rand_vector_avp (struct tunnel *, struct call *, void *, int);

extern int add_challenge_avp (struct buffer *, unsigned char *, int);
extern int add_avp_rws (struct buffer *, _u16);
extern int add_tunnelid_avp (struct buffer *, _u16);
extern int add_vendor_avp (struct buffer *);
extern int add_hostname_avp (struct buffer *, const char *);
extern int add_firmware_avp (struct buffer *);
extern int add_bearer_caps_avp (struct buffer *buf, _u16 caps);
extern int add_frame_caps_avp (struct buffer *buf, _u16 caps);
extern int add_protocol_avp (struct buffer *buf);
extern int add_message_type_avp (struct buffer *buf, _u16 type);
extern int add_result_code_avp (struct buffer *buf, _u16, _u16, char *, int);
extern int add_bearer_avp (struct buffer *, int);
extern int add_frame_avp (struct buffer *, int);
extern int add_rxspeed_avp (struct buffer *, int);
extern int add_txspeed_avp (struct buffer *, int);
extern int add_serno_avp (struct buffer *, unsigned int);
#ifdef TEST_HIDDEN
extern int add_callid_avp (struct buffer *, _u16, struct tunnel *);
#else
extern int add_callid_avp (struct buffer *, _u16);
#endif
extern int add_ppd_avp (struct buffer *, _u16);
extern int add_seqreqd_avp (struct buffer *);
extern int add_chalresp_avp (struct buffer *, unsigned char *, int);
extern int add_randvect_avp (struct buffer *, unsigned char *, int);
extern int add_minbps_avp (struct buffer *buf, int speed);      /* jz: needed for outgoing call */
extern int add_maxbps_avp (struct buffer *buf, int speed);      /* jz: needed for outgoing call */
extern int add_number_avp (struct buffer *buf, char *no);       /* jz: needed for outgoing call */
