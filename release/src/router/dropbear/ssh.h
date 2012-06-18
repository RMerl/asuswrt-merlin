/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

/* This file contains the various numbers in the protocol */


/* message numbers */
#define SSH_MSG_DISCONNECT             1
#define SSH_MSG_IGNORE                 2
#define SSH_MSG_UNIMPLEMENTED          3
#define SSH_MSG_DEBUG                  4
#define SSH_MSG_SERVICE_REQUEST        5
#define SSH_MSG_SERVICE_ACCEPT         6
#define SSH_MSG_KEXINIT                20
#define SSH_MSG_NEWKEYS                21
#define SSH_MSG_KEXDH_INIT             30
#define SSH_MSG_KEXDH_REPLY            31

/* userauth message numbers */
#define SSH_MSG_USERAUTH_REQUEST            50
#define SSH_MSG_USERAUTH_FAILURE            51
#define SSH_MSG_USERAUTH_SUCCESS            52
#define SSH_MSG_USERAUTH_BANNER             53

/* packets 60-79 are method-specific, aren't one-one mapping */
#define SSH_MSG_USERAUTH_SPECIFIC_60   60

#define SSH_MSG_USERAUTH_PASSWD_CHANGEREQ   60

#define SSH_MSG_USERAUTH_PK_OK				60

/* keyboard interactive auth */
#define SSH_MSG_USERAUTH_INFO_REQUEST           60
#define SSH_MSG_USERAUTH_INFO_RESPONSE          61


/* If adding numbers here, check MAX_UNAUTH_PACKET_TYPE in process-packet.c
 * is still valid */

/* connect message numbers */
#define SSH_MSG_GLOBAL_REQUEST                  80
#define SSH_MSG_REQUEST_SUCCESS                 81
#define SSH_MSG_REQUEST_FAILURE                 82
#define SSH_MSG_CHANNEL_OPEN                    90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION       91 
#define SSH_MSG_CHANNEL_OPEN_FAILURE            92
#define SSH_MSG_CHANNEL_WINDOW_ADJUST           93
#define SSH_MSG_CHANNEL_DATA                    94
#define SSH_MSG_CHANNEL_EXTENDED_DATA           95
#define SSH_MSG_CHANNEL_EOF                     96
#define SSH_MSG_CHANNEL_CLOSE                   97
#define SSH_MSG_CHANNEL_REQUEST                 98
#define SSH_MSG_CHANNEL_SUCCESS                 99
#define SSH_MSG_CHANNEL_FAILURE                 100

/* extended data types */
#define SSH_EXTENDED_DATA_STDERR	1

/* disconnect codes */
#define SSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT      1
#define SSH_DISCONNECT_PROTOCOL_ERROR                   2
#define SSH_DISCONNECT_KEY_EXCHANGE_FAILED              3
#define SSH_DISCONNECT_RESERVED                         4
#define SSH_DISCONNECT_MAC_ERROR                        5
#define SSH_DISCONNECT_COMPRESSION_ERROR                6
#define SSH_DISCONNECT_SERVICE_NOT_AVAILABLE            7
#define SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED   8
#define SSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE          9
#define SSH_DISCONNECT_CONNECTION_LOST                 10
#define SSH_DISCONNECT_BY_APPLICATION                  11
#define SSH_DISCONNECT_TOO_MANY_CONNECTIONS            12
#define SSH_DISCONNECT_AUTH_CANCELLED_BY_USER          13
#define SSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE  14
#define SSH_DISCONNECT_ILLEGAL_USER_NAME               15

/* service types */
#define SSH_SERVICE_USERAUTH "ssh-userauth"
#define SSH_SERVICE_USERAUTH_LEN 12
#define SSH_SERVICE_CONNECTION "ssh-connection"
#define SSH_SERVICE_CONNECTION_LEN 14

/* public key types */
#define SSH_SIGNKEY_DSS "ssh-dss"
#define SSH_SIGNKEY_DSS_LEN 7
#define SSH_SIGNKEY_RSA "ssh-rsa"
#define SSH_SIGNKEY_RSA_LEN 7
