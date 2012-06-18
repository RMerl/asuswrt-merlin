/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * rc4.h
 * RC4 stream cipher
 *
 * Copyright 2003, ASUSTeK Inc.
 * All Rights Reserved.
 *     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.; the
 * contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 */

#ifndef _RC4_H_
#define _RC4_H_

#include <typedefs.h>

#define RC4_STATE_NBYTES 256

typedef struct rc4_ks
{ 
   uchar state[RC4_STATE_NBYTES];
   uchar x;
   uchar y;
} rc4_ks_t;

void prepare_key(uchar *key_data_ptr, int key_data_len, rc4_ks_t *key);

void rc4(uchar *buffer_ptr, int buffer_len, rc4_ks_t *key);

#endif /* _RC4_H_ */

