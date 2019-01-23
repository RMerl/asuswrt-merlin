/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *
 *  Copyright (C) Marcin Krzysztof Porwit    2005,
 *  Copyright (C) Gerald (Jerry) Carter      2005.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _REG_PERFCOUNT_H
#define _REG_PERFCOUNT_H

#include "reg_parse_prs.h"

uint32 reg_perfcount_get_base_index(void);
uint32 reg_perfcount_get_last_counter(uint32 base_index);
uint32 reg_perfcount_get_last_help(uint32 last_counter);
uint32 reg_perfcount_get_counter_help(uint32 base_index, char **retbuf);
uint32 reg_perfcount_get_counter_names(uint32 base_index, char **retbuf);
WERROR reg_perfcount_get_hkpd(prs_struct *ps, uint32 max_buf_size, uint32 *outbuf_len, const char *object_ids);

#endif /* _REG_PERFCOUNT_H */
