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
 * $Id: wps_dh.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _WPS_DH_H_
#define _WPS_DH_H_

#include <dh.h>

#ifdef EXTERNAL_OPENSSL

#define WPS_DH_GENERATE_KEY(a, b)	DH_generate_key(b)
#define DH_compute_key_bn(a, b, c)	DH_compute_key(a, b, c)

#else /* bcmcrypto */

#define WPS_DH_GENERATE_KEY(a, b)	DH_generate_key(a, b)

#endif /* EXTERNAL_OPENSSL */
#endif /* _WPS_DH_H_ */
