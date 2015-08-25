/* $Id: stun_config.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_STUN_CONFIG_H__
#define __PJNATH_STUN_CONFIG_H__

/**
 * @file stun_config.h
 * @brief STUN endpoint.
 */

#include <pjnath/stun_msg.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_STUN_CONFIG STUN Config
 * @brief STUN config
 * @ingroup PJNATH_STUN_BASE
 * @{
 */

/**
 * STUN configuration.
 */
typedef struct pj_stun_config
{
	int inst_id;

    /**
     * Pool factory to be used.
     */
    pj_pool_factory	*pf;

    /**
     * Ioqueue.
     */
    pj_ioqueue_t	*ioqueue;

    /**
     * Timer heap instance.
     */
    pj_timer_heap_t	*timer_heap;

    /**
     * Options.
     */
    unsigned		 options;

    /**
     * The default initial STUN round-trip time estimation in msecs.
     * The value normally is PJ_STUN_RTO_VALUE.
     */
    unsigned		 rto_msec;

    /**
     * The interval to cache outgoing  STUN response in the STUN session,
     * in miliseconds. 
     *
     * Default 10000 (10 seconds).
     */
	unsigned		 res_cache_msec;

	int      enable_secure_data;        // secure data. 0 : disable secure data function, 1 enable secure data function

} pj_stun_config;



/**
 * Initialize STUN config.
 */
PJ_INLINE(void) pj_stun_config_init(int inst_id,
					pj_stun_config *cfg,
				    pj_pool_factory *factory,
				    unsigned options,
				    pj_ioqueue_t *ioqueue,
				    pj_timer_heap_t *timer_heap)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->pf = factory;
    cfg->options = options;
    cfg->ioqueue = ioqueue;
    cfg->timer_heap = timer_heap;
    cfg->rto_msec = PJ_STUN_RTO_VALUE;
    cfg->res_cache_msec = PJ_STUN_RES_CACHE_DURATION;
	cfg->inst_id = inst_id;
}


/**
 * Check that STUN config is valid.
 */
PJ_INLINE(pj_status_t) pj_stun_config_check_valid(const pj_stun_config *cfg)
{
    PJ_ASSERT_RETURN(cfg->ioqueue && cfg->pf && cfg->timer_heap &&
		     cfg->rto_msec && cfg->res_cache_msec, PJ_EINVAL);
    return PJ_SUCCESS;
}


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_STUN_CONFIG_H__ */

