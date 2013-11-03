/*
 * generic_cjk
 * Copyright (C) TSUBAKIMOTO Hiroya <zorac@4000do.co.jp> 2004
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <atalk/unicode.h>
#include <iconv.h>
#include "../byteorder.h"

#define CJK_PUSH_BUFFER 4
#define CJK_PULL_BUFFER 8

typedef struct {
  uint16_t range[2];
  const uint16_t (*summary)[2];
} cjk_index_t;

extern size_t cjk_generic_push (size_t (*)(uint8_t*, const ucs2_t*, size_t*),
				   void*, char**, size_t*, char**, size_t*);
extern size_t cjk_generic_pull (size_t (*)(ucs2_t*, const uint8_t*, size_t*),
				   void*, char**, size_t*, char**, size_t*);

extern size_t cjk_char_push (uint16_t, uint8_t*);
extern size_t cjk_char_pull (ucs2_t, ucs2_t*, const uint32_t*);

extern uint16_t cjk_lookup (uint16_t, const cjk_index_t*, const uint16_t*);
extern ucs2_t cjk_compose (ucs2_t, ucs2_t, const uint32_t*, size_t);
extern ucs2_t cjk_compose_seq (const ucs2_t*, size_t*, const uint32_t*, size_t);
