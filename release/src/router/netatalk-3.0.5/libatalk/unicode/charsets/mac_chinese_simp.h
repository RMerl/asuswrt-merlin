/*
 * MacChineseSimp
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

static const uint16_t mac_chinese_simp_uni2_page00[][2] = {
  /* 0x00a */ { 0x022d,    0 }, { 0x0080,    5 },
  /* 0x00c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x010 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x014 */ { 0x0110,    6 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x018 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x01c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0200,    8 },
  /* 0x020 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x024 */ { 0x0000,    0 }, { 0x0002,    9 }, { 0x0002,   10 },
};

static const uint16_t mac_chinese_simp_uni2_page1e[][2] = {
  /* 0x1e3 */ { 0x8000,   11 },
};

static const uint16_t mac_chinese_simp_uni2_page20[][2] = {
  /* 0x201 */ { 0x0070,   12 }, { 0x0040,   15 }, { 0x4000,   16 },
};

static const uint16_t mac_chinese_simp_uni2_page21[][2] = {
  /* 0x212 */ { 0x0004,   17 },
};

static const uint16_t mac_chinese_simp_uni2_page22[][2] = {
  /* 0x22e */ { 0x8000,   18 },
};

static const uint16_t mac_chinese_simp_uni2_page30[][2] = {
  /* 0x301 */ { 0x1000,   19 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x304 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x308 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0x30c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0800,   20 },
};

static const uint16_t mac_chinese_simp_uni2_pagee0[][2] = {
  /* 0xe00 */ { 0x07ff,   21 },
};

static const uint16_t mac_chinese_simp_uni2_pagef8[][2] = {
  /* 0xf88 */ { 0x0003,   32 },
};

static const uint16_t mac_chinese_simp_uni2_pagefe[][2] = {
  /* 0xfe3 */ { 0xfffa,   34 }, { 0x001f,   48 },
};

static const uint16_t mac_chinese_simp_uni2_pageff[][2] = {
  /* 0xff5 */ { 0x4000,   53 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0xff8 */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 },
  /* 0xffc */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x002b,   54 },
};

static const cjk_index_t mac_chinese_simp_uni2_index[] = {
  { { 0x00a0, 0x026f }, mac_chinese_simp_uni2_page00 },
  { { 0x1e30, 0x1e3f }, mac_chinese_simp_uni2_page1e },
  { { 0x2010, 0x203f }, mac_chinese_simp_uni2_page20 },
  { { 0x2120, 0x212f }, mac_chinese_simp_uni2_page21 },
  { { 0x22e0, 0x22ef }, mac_chinese_simp_uni2_page22 },
  { { 0x3010, 0x30ff }, mac_chinese_simp_uni2_page30 },
  { { 0xe000, 0xe00f }, mac_chinese_simp_uni2_pagee0 },
  { { 0xf880, 0xf88f }, mac_chinese_simp_uni2_pagef8 },
  { { 0xfe30, 0xfe4f }, mac_chinese_simp_uni2_pagefe },
  { { 0xff50, 0xffef }, mac_chinese_simp_uni2_pageff },
  { { 0, 0 }, NULL }
};

static const uint16_t mac_chinese_simp_uni2_charset[] = {
  0x00a0, 0xa1e9, 0xa1ea, 0xa3a4, 0x00fd, 0xa1a4, 0xa8bd, 0xa8be,
  0xa8bf, 0xa8bb, 0xa8c0, 0xa8bc, 0xa1aa, 0xffff, 0xa1ac, 0x00ff,
  0xa3fe, 0x00fe, 0xa1ad, 0xa1ab, 0xffff, 0x0080, 0xa6f3, 0xa6db,
  0xa6da, 0xa6ec, 0xa6ed, 0xa6de, 0xa6d9, 0xa6dc, 0xa6dd, 0xa6df,
  0x0081, 0x0082, 0xa6f2, 0xa6f4, 0xa6f5, 0xa6e0, 0xa6e1, 0xa6f0,
  0xa6f1, 0xa6e2, 0xa6e3, 0xa6ee, 0xa6ef, 0xa6e6, 0xa6e7, 0xa6e4,
  0xa6e5, 0xa6e8, 0xa6e9, 0xa6ea, 0xa6eb, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff,
};

static const uint16_t mac_chinese_simp_2uni_page00[][2] = {
  /* 0x008 */ { 0x0007,    0 }, { 0x0000,    0 }, { 0x0001,    3 }, { 0x0000,    0 },
  /* 0x00c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0xe000,    4 },
};

static const uint16_t mac_chinese_simp_2uni_pagea1[][2] = {
  /* 0xa1a */ { 0x3c10,    7 }, { 0x0000,    0 },
  /* 0xa1c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0600,   12 },
};

static const uint16_t mac_chinese_simp_2uni_pagea3[][2] = {
  /* 0xa3a */ { 0x0010,   14 }, { 0x0000,    0 },
  /* 0xa3c */ { 0x0000,    0 }, { 0x0000,    0 }, { 0x0000,    0 }, { 0x4000,   15 },
};

static const uint16_t mac_chinese_simp_2uni_pagea6[][2] = {
  /* 0xa6d */ { 0xfe00,   16 }, { 0xffff,   23 }, { 0x003f,   39 },
};

static const uint16_t mac_chinese_simp_2uni_pagea8[][2] = {
  /* 0xa8b */ { 0xf800,   45 }, { 0x0001,   50 },
};

static const cjk_index_t mac_chinese_simp_2uni_index[] = {
  { { 0x0080, 0x00ff }, mac_chinese_simp_2uni_page00 },
  { { 0xa1a0, 0xa1ef }, mac_chinese_simp_2uni_pagea1 },
  { { 0xa3a0, 0xa3ff }, mac_chinese_simp_2uni_pagea3 },
  { { 0xa6d0, 0xa6ff }, mac_chinese_simp_2uni_pagea6 },
  { { 0xa8b0, 0xa8cf }, mac_chinese_simp_2uni_pagea8 },
  { { 0, 0 }, NULL }
};

static const uint16_t mac_chinese_simp_2uni_charset[] = {
  0xe000, 0xf880, 0xf881, 0x00a0, 0x00a9, 0x2122, 0x2026, 0x00b7,
  0x2014, 0x301c, 0x2016, 0x22ef, 0x00a2, 0x00a3, 0x00a5, 0x203e,
  0xe007, 0xe003, 0xe002, 0xe008, 0xe009, 0xe006, 0xe00a, 0xfe35,
  0xfe36, 0xfe39, 0xfe3a, 0xfe3f, 0xfe40, 0xfe3d, 0xfe3e, 0xfe41,
  0xfe42, 0xfe43, 0xfe44, 0xe004, 0xe005, 0xfe3b, 0xfe3c, 0xfe37,
  0xfe38, 0xfe31, 0xe001, 0xfe33, 0xfe34, 0x0251, 0x1e3f, 0x0144,
  0x0148, 0x01f9, 0x0261,
};

static const uint32_t mac_chinese_simp_compose[] = {
  0x00fcf87f, 0x22eff87e, 0x3001f87e, 0x3002f87e,
  0x3016f87e, 0x3017f87e, 0xff01f87e, 0xff0cf87e,
  0xff1af87e, 0xff1bf87e, 0xff1ff87e,
};
