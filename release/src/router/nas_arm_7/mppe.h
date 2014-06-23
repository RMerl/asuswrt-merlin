/*
 * mppe.h Broadcom support for Microsoft Point-to-Point Encryption Protocol.
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: mppe.h 241182 2011-02-17 21:50:03Z $
 */

#if !defined(_MPPE_H_)
#define _MPPE_H_

void mppe_crypt(unsigned char salt[2], unsigned char *text, int text_len,
                unsigned char *key, int key_len, unsigned char vector[16],
                int encrypt);

#endif /* !defined(_MPPE_H_) */
