/*
 * Global ASSERT Logging Public Interface
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_assert_log.h 241182 2011-02-17 21:50:03Z $
 */
#ifndef _WLC_ASSERT_LOG_H_
#define _WLC_ASSERT_LOG_H_

#include "wlioctl.h"

typedef struct bcm_assert_info bcm_assert_info_t;

extern void bcm_assertlog_init(void);
extern void bcm_assertlog_deinit(void);
extern int bcm_assertlog_get(void *outbuf, int iobuf_len);

extern void bcm_assert_log(char *str);

#endif /* _WLC_ASSERT_LOG_H_ */
