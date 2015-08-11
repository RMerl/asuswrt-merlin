/* $Id: stun.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

static int decode_test(void)
{
    /* Invalid message type */

    /* Short message */

    /* Long, random message */

    /* Message length in header is shorter */

    /* Message length in header is longer */

    /* Invalid magic */

    /* Attribute length is not valid */

    /* Unknown mandatory attribute type should generate error */

    /* Unknown but non-mandatory should be okay */

    /* String/binary attribute length is larger than the message */

    /* Valid message with MESSAGE-INTEGRITY */

    /* Valid message with FINGERPRINT */

    /* Valid message with MESSAGE-INTEGRITY and FINGERPRINT */

    /* Another attribute not FINGERPRINT exists after MESSAGE-INTEGRITY */

    /* Another attribute exists after FINGERPRINT */

    return 0;
}

static int decode_verify(void)
{
    /* Decode all attribute types */
    return 0;
}

static int auth_test(void)
{
    /* REALM and USERNAME is present, but MESSAGE-INTEGRITY is not present.
     * For short term, must with reply 401 without REALM.
     * For long term, must reply with 401 with REALM.
     */

    /* USERNAME is not present, server must respond with 432 (Missing
     * Username).
     */

    /* If long term credential is wanted and REALM is not present, server 
     * must respond with 434 (Missing Realm) 
     */

    /* If REALM doesn't match, server must respond with 434 (Missing Realm)
     * too, containing REALM and NONCE attribute.
     */

    /* When long term authentication is wanted and NONCE is NOT present,
     * server must respond with 435 (Missing Nonce), containing REALM and
     * NONCE attribute.
     */

    /* Simulate 438 (Stale Nonce) */
    
    /* Simulate 436 (Unknown Username) */

    /* When server wants to use short term credential, but request has
     * REALM, reject with .... ???
     */

    /* Invalid HMAC */

    /* Valid static short term, without NONCE */

    /* Valid static short term, WITH NONCE */

    /* Valid static long term (with NONCE */

    /* Valid dynamic short term (without NONCE) */

    /* Valid dynamic short term (with NONCE) */

    /* Valid dynamic long term (with NONCE) */

    return 0;
}


int stun_test(void)
{
    decode_verify();
    decode_test();
    auth_test();
    return 0;
}

