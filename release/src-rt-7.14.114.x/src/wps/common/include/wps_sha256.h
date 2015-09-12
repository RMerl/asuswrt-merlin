/*
 * For WPS to adapt to OpenSSL crypto library
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_sha256.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _WPS_SHA256_H_
#define _WPS_SHA256_H_

#ifdef EXTERNAL_OPENSSL

#include <sha.h>

#else

#include <sha256.h>
#include <hmac_sha256.h>

#endif /* EXTERNAL_OPENSSL */
#endif /* _WPS_SHA256_H_ */
