/* $Id: types.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_TYPES_H__
#define __PJNATH_TYPES_H__

/**
 * @file types.h
 * @brief PJNATH types.
 */

#include <pj/types.h>
#include <pjnath/config.h>

/**
 * @defgroup PJNATH NAT Traversal Helper Library
 * @{
 */

PJ_BEGIN_DECL

/**
 * This constant describes a number to be used to identify an invalid TURN
 * channel number.
 */
#define PJ_TURN_INVALID_CHANNEL	    0xFFFF


/**
 * Initialize pjnath library.
 *
 * @return	    Initialization status.
 */
PJ_DECL(pj_status_t) pjnath_init(void);


/**
 * Display error to the log. 
 *
 * @param sender    The sender name.
 * @param title	    Title message.
 * @param status    The error status.
 */
#if PJNATH_ERROR_LEVEL <= PJ_LOG_MAX_LEVEL
PJ_DECL(void) pjnath_perror(const char *sender, const char *title,
			    pj_status_t status);
#else
# define pjnath_perror(sender, title, status)
#endif



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJNATH_TYPES_H__ */

