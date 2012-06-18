/* cyassl_error.h
 *
 * Copyright (C) 2006-2009 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */



#ifndef CYASSL_ERROR_H
#define CYASSL_ERROR_H

#include "error.h"   /* CTaoCrypt errors */

#ifdef __cplusplus
    extern "C" {
#endif

enum CyaSSL_ErrorCodes {
    PREFIX_ERROR           = -202,            /* bad index to key rounds  */
    MEMORY_ERROR           = -203,            /* out of memory            */
    VERIFY_FINISHED_ERROR  = -204,            /* verify problem on finished */
    VERIFY_MAC_ERROR       = -205,            /* verify mac problem       */
    PARSE_ERROR            = -206,            /* parse error on header    */
    UNKNOWN_HANDSHAKE_TYPE = -207,            /* weird handshake type     */
    SOCKET_ERROR_E         = -208,            /* error state on socket    */
    SOCKET_NODATA          = -209,            /* expected data, not there */
    INCOMPLETE_DATA        = -210,            /* don't have enough data to 
                                                 complete task            */
    UNKNOWN_RECORD_TYPE    = -211,            /* unknown type in record hdr */
    DECRYPT_ERROR          = -212,            /* error during decryption  */
    FATAL_ERROR            = -213,            /* revcd alert fatal error  */
    ENCRYPT_ERROR          = -214,            /* error during encryption  */
    FREAD_ERROR            = -215,            /* fread problem            */
    NO_PEER_KEY            = -216,            /* need peer's key          */
    NO_PRIVATE_KEY         = -217,            /* need the private key     */
    RSA_PRIVATE_ERROR      = -218,            /* error during rsa priv op */
    BUILD_MSG_ERROR        = -220,            /* build message failure    */

    BAD_HELLO              = -221,            /* client hello malformed   */
    DOMAIN_NAME_MISMATCH   = -222,            /* peer subject name mismatch */
    WANT_READ              = -223,            /* want read, call again    */
    NOT_READY_ERROR        = -224,            /* handshake layer not ready */
    PMS_VERSION_ERROR      = -225,            /* pre m secret version error */
    VERSION_ERROR          = -226,            /* record layer version error */
    WANT_WRITE             = -227,            /* want write, call again   */
    BUFFER_ERROR           = -228,            /* malformed buffer input   */
    VERIFY_CERT_ERROR      = -229,            /* verify cert error        */
    VERIFY_SIGN_ERROR      = -230,            /* verify sign error        */
    CLIENT_ID_ERROR        = -231,            /* psk client identity error  */
    SERVER_HINT_ERROR      = -232,            /* psk server hint error  */
    PSK_KEY_ERROR          = -233,            /* psk key error  */
    ZLIB_INIT_ERROR        = -234,            /* zlib init error  */
    ZLIB_COMPRESS_ERROR    = -235,            /* zlib compression error  */
    ZLIB_DECOMPRESS_ERROR  = -236,            /* zlib decompression error  */

    GETTIME_ERROR          = -237,            /* gettimeofday failed ??? */
    GETITIMER_ERROR        = -238,            /* getitimer failed ??? */
    SIGACT_ERROR           = -239,            /* sigaction failed ??? */
    SETITIMER_ERROR        = -240,            /* setitimer failed ??? */
    LENGTH_ERROR           = -241,            /* record layer length error */
    PEER_KEY_ERROR         = -242,            /* cant decode peer key */
    ZERO_RETURN            = -243,            /* peer sent close notify */
    SIDE_ERROR             = -244,            /* wrong client/server type */
    NO_PEER_CERT           = -245,            /* peer didn't send key */
    /* add strings to SetErrorString !!!!! */

    /* begin negotiation parameter errors */
    UNSUPPORTED_SUITE      = -260,            /* unsupported cipher suite */
    MATCH_SUITE_ERROR      = -261             /* can't match cipher suite */
    /* end   negotiation parameter errors only 10 for now */
    /* add strings to SetErrorString !!!!! */
};


#ifdef CYASSL_CALLBACKS
    enum {
        MIN_PARAM_ERR = UNSUPPORTED_SUITE,
        MAX_PARAM_ERR = MIN_PARAM_ERR - 10
    };
#endif


void SetErrorString(int error, char* buffer);


#ifdef __cplusplus
    }  /* extern "C" */
#endif


#endif /* CyaSSL_ERROR_H */

