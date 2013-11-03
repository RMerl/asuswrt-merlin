/*
 * MacChineseTrad
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
 *
 * Reference
 * http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/
 */

static const uint16_t mac_chinese_trad_uni2_page00[][2] = {
  /* 0x00a */ { 0x0201,    0 }, { 0x0080,    2 },
};

static const uint16_t mac_chinese_trad_uni2_page20[][2] = {
  /* 0x202 */ { 0x0044,    3 },
};

static const uint16_t mac_chinese_trad_uni2_page21[][2] = {
  /* 0x212 */ { 0x0004,    5 },
};

static const uint16_t mac_chinese_trad_uni2_page22[][2] = {
  /* 0x229 */ { 0x0020,    6 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x22c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x8000,    7 },
};

static const uint16_t mac_chinese_trad_uni2_page25[][2] = {
  /* 0x259 */ { 0x0020,    8 },
};

static const uint16_t mac_chinese_trad_uni2_page26[][2] = {
  /* 0x264 */ { 0x0002,    9 },
};

static const uint16_t mac_chinese_trad_uni2_pagee0[][2] = {
  /* 0xe00 */ { 0xffff,   10 }, { 0x00ff,   26 },
};

static const uint16_t mac_chinese_trad_uni2_pagef8[][2] = {
  /* 0xf88 */ { 0x0003,   34 },
};

static const uint16_t mac_chinese_trad_uni2_pagefe[][2] = {
  /* 0xfe4 */ { 0x1000,   36 }, { 0x7ef5,   37 },
};

static const uint16_t mac_chinese_trad_uni2_pageff[][2] = {
  /* 0xff6 */ { 0x0010,   49 },
};

static const cjk_index_t mac_chinese_trad_uni2_index[] = {
  { { 0x00a0, 0x00bf }, mac_chinese_trad_uni2_page00 },
  { { 0x2020, 0x202f }, mac_chinese_trad_uni2_page20 },
  { { 0x2120, 0x212f }, mac_chinese_trad_uni2_page21 },
  { { 0x2290, 0x22ef }, mac_chinese_trad_uni2_page22 },
  { { 0x2590, 0x259f }, mac_chinese_trad_uni2_page25 },
  { { 0x2640, 0x264f }, mac_chinese_trad_uni2_page26 },
  { { 0xe000, 0xe01f }, mac_chinese_trad_uni2_pagee0 },
  { { 0xf880, 0xf88f }, mac_chinese_trad_uni2_pagef8 },
  { { 0xfe40, 0xfe5f }, mac_chinese_trad_uni2_pagefe },
  { { 0xff60, 0xff6f }, mac_chinese_trad_uni2_pageff },
  { { 0, 0 }, NULL }
};

static const uint16_t mac_chinese_trad_uni2_charset[] = {
  0x00a0, 0x00fd, 0xa145, 0xffff, 0x00ff, 0x00fe, 0xa1f2, 0xa14b,
  0xffff, 0xffff, 0x0080, 0xa1c3, 0xa279, 0xa14e, 0xa1a3, 0xa1a4,
  0xa2cc, 0xa2ce, 0xa1cb, 0xa154, 0xa17d, 0xa17e, 0xa14d, 0xa14f,
  0xa150, 0xa1fe, 0xa152, 0xa151, 0xa153, 0xa240, 0xa1c5, 0xa15a,
  0xa1a1, 0xa1a2, 0x0081, 0x0082, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff,
};

static const uint16_t mac_chinese_trad_2uni_page00[][2] = {
  /* 0x008 */ { 0x0007,    0 }, { 0x0000,    0 }, { 0x0001,    3 }, { 0x0000,    0 },
  /* 0x00c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0xe000,    4 },
};

static const uint16_t mac_chinese_trad_2uni_pagea1[][2] = {
  /* 0xa14 */ { 0xe820,    7 }, { 0x041f,   12 }, { 0x0000,    0 }, { 0x6000,   18 },
  /* 0xa18 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x001e,   20 }, { 0x0000,    0 },
  /* 0xa1c */ { 0x0828,   24 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x4004,   27 },
  /* 0xa20 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0xa24 */ { 0x0001,   29 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0200,   30 },
  /* 0xa28 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0xa2c */ { 0x5000,   31 },
};

static const cjk_index_t mac_chinese_trad_2uni_index[] = {
  { { 0x0080, 0x00ff }, mac_chinese_trad_2uni_page00 },
  { { 0xa140, 0xa2cf }, mac_chinese_trad_2uni_pagea1 },
  { { 0, 0 }, NULL }
};

static const uint16_t mac_chinese_trad_2uni_charset[] = {
  0xe000, 0xf880, 0xf881, 0x00a0, 0x00a9, 0x2122, 0x2026, 0x00b7,
  0x22ef, 0xe00c, 0xe003, 0xe00d, 0xe00e, 0xe011, 0xe010, 0xe012,
  0xe009, 0xe015, 0xe00a, 0xe00b, 0xe016, 0xe017, 0xe004, 0xe005,
  0xe001, 0xe014, 0xe008, 0x2295, 0xe00f, 0xe013, 0xe002, 0xe006,
  0xe007,
};

static const uint32_t mac_chinese_trad_compose[] = {
  0x005cf87f, 0x203ef87c, 0x2502f87f, 0x3001f87d,
  0x3014f87f, 0x3015f87f, 0x5341f87f, 0x5345f87f,
  0xfe4bf87c, 0xff01f87d, 0xff08f87f, 0xff09f87f,
  0xff0cf87d, 0xff0ef87d, 0xff0ef87e, 0xff0ff87f,
  0xff1af87d, 0xff1bf87d, 0xff1ff87d, 0xff3cf87f,
  0xff3ff87c, 0xff3ff87f, 0xff5bf87f, 0xff5df87f,
};
