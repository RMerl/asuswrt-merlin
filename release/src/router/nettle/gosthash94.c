/* gost.c - an implementation of GOST Hash Function
 * based on the Russian Standard GOST R 34.11-94.
 * See also RFC 4357.
 *
 * Copyright: 2009-2012 Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Ported to nettle by Nikos Mavrogiannopoulos.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "macros.h"
#include "nettle-write.h"
#include "gosthash94.h"

/* pre-initialized GOST lookup tables based on rotated S-Box */
static const uint32_t gosthash94_sbox[4][256] = {
  {
    0x72000, 0x75000, 0x74800, 0x71000, 0x76800,
    0x74000, 0x70000, 0x77000, 0x73000, 0x75800,
    0x70800, 0x76000, 0x73800, 0x77800, 0x72800,
    0x71800, 0x5A000, 0x5D000, 0x5C800, 0x59000,
    0x5E800, 0x5C000, 0x58000, 0x5F000, 0x5B000,
    0x5D800, 0x58800, 0x5E000, 0x5B800, 0x5F800,
    0x5A800, 0x59800, 0x22000, 0x25000, 0x24800,
    0x21000, 0x26800, 0x24000, 0x20000, 0x27000,
    0x23000, 0x25800, 0x20800, 0x26000, 0x23800,
    0x27800, 0x22800, 0x21800, 0x62000, 0x65000,
    0x64800, 0x61000, 0x66800, 0x64000, 0x60000,
    0x67000, 0x63000, 0x65800, 0x60800, 0x66000,
    0x63800, 0x67800, 0x62800, 0x61800, 0x32000,
    0x35000, 0x34800, 0x31000, 0x36800, 0x34000,
    0x30000, 0x37000, 0x33000, 0x35800, 0x30800,
    0x36000, 0x33800, 0x37800, 0x32800, 0x31800,
    0x6A000, 0x6D000, 0x6C800, 0x69000, 0x6E800,
    0x6C000, 0x68000, 0x6F000, 0x6B000, 0x6D800,
    0x68800, 0x6E000, 0x6B800, 0x6F800, 0x6A800,
    0x69800, 0x7A000, 0x7D000, 0x7C800, 0x79000,
    0x7E800, 0x7C000, 0x78000, 0x7F000, 0x7B000,
    0x7D800, 0x78800, 0x7E000, 0x7B800, 0x7F800,
    0x7A800, 0x79800, 0x52000, 0x55000, 0x54800,
    0x51000, 0x56800, 0x54000, 0x50000, 0x57000,
    0x53000, 0x55800, 0x50800, 0x56000, 0x53800,
    0x57800, 0x52800, 0x51800, 0x12000, 0x15000,
    0x14800, 0x11000, 0x16800, 0x14000, 0x10000,
    0x17000, 0x13000, 0x15800, 0x10800, 0x16000,
    0x13800, 0x17800, 0x12800, 0x11800, 0x1A000,
    0x1D000, 0x1C800, 0x19000, 0x1E800, 0x1C000,
    0x18000, 0x1F000, 0x1B000, 0x1D800, 0x18800,
    0x1E000, 0x1B800, 0x1F800, 0x1A800, 0x19800,
    0x42000, 0x45000, 0x44800, 0x41000, 0x46800,
    0x44000, 0x40000, 0x47000, 0x43000, 0x45800,
    0x40800, 0x46000, 0x43800, 0x47800, 0x42800,
    0x41800, 0xA000,  0xD000,  0xC800,  0x9000,
    0xE800,  0xC000,  0x8000,  0xF000,  0xB000,
    0xD800,  0x8800,  0xE000,  0xB800,  0xF800,
    0xA800,  0x9800,  0x2000,  0x5000,  0x4800,
    0x1000,  0x6800,  0x4000,  0x0,     0x7000,
    0x3000,  0x5800,  0x800,   0x6000,  0x3800,
    0x7800,  0x2800,  0x1800,  0x3A000, 0x3D000,
    0x3C800, 0x39000, 0x3E800, 0x3C000, 0x38000,
    0x3F000, 0x3B000, 0x3D800, 0x38800, 0x3E000,
    0x3B800, 0x3F800, 0x3A800, 0x39800, 0x2A000,
    0x2D000, 0x2C800, 0x29000, 0x2E800, 0x2C000,
    0x28000, 0x2F000, 0x2B000, 0x2D800, 0x28800,
    0x2E000, 0x2B800, 0x2F800, 0x2A800, 0x29800,
    0x4A000, 0x4D000, 0x4C800, 0x49000, 0x4E800,
    0x4C000, 0x48000, 0x4F000, 0x4B000, 0x4D800,
    0x48800, 0x4E000, 0x4B800, 0x4F800, 0x4A800,
    0x49800
  }, {
    0x3A80000, 0x3C00000, 0x3880000, 0x3E80000, 0x3D00000,
    0x3980000, 0x3A00000, 0x3900000, 0x3F00000, 0x3F80000,
    0x3E00000, 0x3B80000, 0x3B00000, 0x3800000, 0x3C80000,
    0x3D80000, 0x6A80000, 0x6C00000, 0x6880000, 0x6E80000,
    0x6D00000, 0x6980000, 0x6A00000, 0x6900000, 0x6F00000,
    0x6F80000, 0x6E00000, 0x6B80000, 0x6B00000, 0x6800000,
    0x6C80000, 0x6D80000, 0x5280000, 0x5400000, 0x5080000,
    0x5680000, 0x5500000, 0x5180000, 0x5200000, 0x5100000,
    0x5700000, 0x5780000, 0x5600000, 0x5380000, 0x5300000,
    0x5000000, 0x5480000, 0x5580000, 0xA80000,  0xC00000,
    0x880000,  0xE80000,  0xD00000,  0x980000,  0xA00000,
    0x900000,  0xF00000,  0xF80000,  0xE00000,  0xB80000,
    0xB00000,  0x800000,  0xC80000,  0xD80000,  0x280000,
    0x400000,  0x80000,   0x680000,  0x500000,  0x180000,
    0x200000,  0x100000,  0x700000,  0x780000,  0x600000,
    0x380000,  0x300000,  0x0,       0x480000,  0x580000,
    0x4280000, 0x4400000, 0x4080000, 0x4680000, 0x4500000,
    0x4180000, 0x4200000, 0x4100000, 0x4700000, 0x4780000,
    0x4600000, 0x4380000, 0x4300000, 0x4000000, 0x4480000,
    0x4580000, 0x4A80000, 0x4C00000, 0x4880000, 0x4E80000,
    0x4D00000, 0x4980000, 0x4A00000, 0x4900000, 0x4F00000,
    0x4F80000, 0x4E00000, 0x4B80000, 0x4B00000, 0x4800000,
    0x4C80000, 0x4D80000, 0x7A80000, 0x7C00000, 0x7880000,
    0x7E80000, 0x7D00000, 0x7980000, 0x7A00000, 0x7900000,
    0x7F00000, 0x7F80000, 0x7E00000, 0x7B80000, 0x7B00000,
    0x7800000, 0x7C80000, 0x7D80000, 0x7280000, 0x7400000,
    0x7080000, 0x7680000, 0x7500000, 0x7180000, 0x7200000,
    0x7100000, 0x7700000, 0x7780000, 0x7600000, 0x7380000,
    0x7300000, 0x7000000, 0x7480000, 0x7580000, 0x2280000,
    0x2400000, 0x2080000, 0x2680000, 0x2500000, 0x2180000,
    0x2200000, 0x2100000, 0x2700000, 0x2780000, 0x2600000,
    0x2380000, 0x2300000, 0x2000000, 0x2480000, 0x2580000,
    0x3280000, 0x3400000, 0x3080000, 0x3680000, 0x3500000,
    0x3180000, 0x3200000, 0x3100000, 0x3700000, 0x3780000,
    0x3600000, 0x3380000, 0x3300000, 0x3000000, 0x3480000,
    0x3580000, 0x6280000, 0x6400000, 0x6080000, 0x6680000,
    0x6500000, 0x6180000, 0x6200000, 0x6100000, 0x6700000,
    0x6780000, 0x6600000, 0x6380000, 0x6300000, 0x6000000,
    0x6480000, 0x6580000, 0x5A80000, 0x5C00000, 0x5880000,
    0x5E80000, 0x5D00000, 0x5980000, 0x5A00000, 0x5900000,
    0x5F00000, 0x5F80000, 0x5E00000, 0x5B80000, 0x5B00000,
    0x5800000, 0x5C80000, 0x5D80000, 0x1280000, 0x1400000,
    0x1080000, 0x1680000, 0x1500000, 0x1180000, 0x1200000,
    0x1100000, 0x1700000, 0x1780000, 0x1600000, 0x1380000,
    0x1300000, 0x1000000, 0x1480000, 0x1580000, 0x2A80000,
    0x2C00000, 0x2880000, 0x2E80000, 0x2D00000, 0x2980000,
    0x2A00000, 0x2900000, 0x2F00000, 0x2F80000, 0x2E00000,
    0x2B80000, 0x2B00000, 0x2800000, 0x2C80000, 0x2D80000,
    0x1A80000, 0x1C00000, 0x1880000, 0x1E80000, 0x1D00000,
    0x1980000, 0x1A00000, 0x1900000, 0x1F00000, 0x1F80000,
    0x1E00000, 0x1B80000, 0x1B00000, 0x1800000, 0x1C80000,
    0x1D80000
  }, {
    0x30000002, 0x60000002, 0x38000002, 0x8000002,
    0x28000002, 0x78000002, 0x68000002, 0x40000002, 
    0x20000002, 0x50000002, 0x48000002, 0x70000002, 
    0x2,        0x18000002, 0x58000002, 0x10000002, 
    0xB0000005, 0xE0000005, 0xB8000005, 0x88000005,
    0xA8000005, 0xF8000005, 0xE8000005, 0xC0000005,
    0xA0000005, 0xD0000005, 0xC8000005, 0xF0000005, 
    0x80000005, 0x98000005, 0xD8000005, 0x90000005, 
    0x30000005, 0x60000005, 0x38000005, 0x8000005, 
    0x28000005, 0x78000005, 0x68000005, 0x40000005,
    0x20000005, 0x50000005, 0x48000005, 0x70000005, 
    0x5,        0x18000005, 0x58000005, 0x10000005, 
    0x30000000, 0x60000000, 0x38000000, 0x8000000, 
    0x28000000, 0x78000000, 0x68000000, 0x40000000, 
    0x20000000, 0x50000000, 0x48000000, 0x70000000,
    0x0,        0x18000000, 0x58000000, 0x10000000, 
    0xB0000003, 0xE0000003, 0xB8000003, 0x88000003, 
    0xA8000003, 0xF8000003, 0xE8000003, 0xC0000003, 
    0xA0000003, 0xD0000003, 0xC8000003, 0xF0000003, 
    0x80000003, 0x98000003, 0xD8000003, 0x90000003,
    0x30000001, 0x60000001, 0x38000001, 0x8000001,
    0x28000001, 0x78000001, 0x68000001, 0x40000001, 
    0x20000001, 0x50000001, 0x48000001, 0x70000001, 
    0x1,        0x18000001, 0x58000001, 0x10000001, 
    0xB0000000, 0xE0000000, 0xB8000000, 0x88000000,
    0xA8000000, 0xF8000000, 0xE8000000, 0xC0000000,
    0xA0000000, 0xD0000000, 0xC8000000, 0xF0000000, 
    0x80000000, 0x98000000, 0xD8000000, 0x90000000, 
    0xB0000006, 0xE0000006, 0xB8000006, 0x88000006, 
    0xA8000006, 0xF8000006, 0xE8000006, 0xC0000006,
    0xA0000006, 0xD0000006, 0xC8000006, 0xF0000006,
    0x80000006, 0x98000006, 0xD8000006, 0x90000006, 
    0xB0000001, 0xE0000001, 0xB8000001, 0x88000001, 
    0xA8000001, 0xF8000001, 0xE8000001, 0xC0000001, 
    0xA0000001, 0xD0000001, 0xC8000001, 0xF0000001,
    0x80000001, 0x98000001, 0xD8000001, 0x90000001,
    0x30000003, 0x60000003, 0x38000003, 0x8000003, 
    0x28000003, 0x78000003, 0x68000003, 0x40000003, 
    0x20000003, 0x50000003, 0x48000003, 0x70000003, 
    0x3,        0x18000003, 0x58000003, 0x10000003,
    0x30000004, 0x60000004, 0x38000004, 0x8000004,
    0x28000004, 0x78000004, 0x68000004, 0x40000004, 
    0x20000004, 0x50000004, 0x48000004, 0x70000004, 
    0x4,        0x18000004, 0x58000004, 0x10000004, 
    0xB0000002, 0xE0000002, 0xB8000002, 0x88000002,
    0xA8000002, 0xF8000002, 0xE8000002, 0xC0000002,
    0xA0000002, 0xD0000002, 0xC8000002, 0xF0000002, 
    0x80000002, 0x98000002, 0xD8000002, 0x90000002, 
    0xB0000004, 0xE0000004, 0xB8000004, 0x88000004, 
    0xA8000004, 0xF8000004, 0xE8000004, 0xC0000004,
    0xA0000004, 0xD0000004, 0xC8000004, 0xF0000004,
    0x80000004, 0x98000004, 0xD8000004, 0x90000004, 
    0x30000006, 0x60000006, 0x38000006, 0x8000006, 
    0x28000006, 0x78000006, 0x68000006, 0x40000006, 
    0x20000006, 0x50000006, 0x48000006, 0x70000006,
    0x6,        0x18000006, 0x58000006, 0x10000006, 
    0xB0000007, 0xE0000007, 0xB8000007, 0x88000007, 
    0xA8000007, 0xF8000007, 0xE8000007, 0xC0000007, 
    0xA0000007, 0xD0000007, 0xC8000007, 0xF0000007, 
    0x80000007, 0x98000007, 0xD8000007, 0x90000007,
    0x30000007, 0x60000007, 0x38000007, 0x8000007,
    0x28000007, 0x78000007, 0x68000007, 0x40000007, 
    0x20000007, 0x50000007, 0x48000007, 0x70000007, 
    0x7,        0x18000007, 0x58000007, 0x10000007
  }, {
    0xE8,  0xD8,  0xA0,  0x88,  0x98,  0xF8,  0xA8,  0xC8,  0x80,  0xD0,
    0xF0,  0xB8,  0xB0,  0xC0,  0x90,  0xE0,  0x7E8, 0x7D8, 0x7A0, 0x788,
    0x798, 0x7F8, 0x7A8, 0x7C8, 0x780, 0x7D0, 0x7F0, 0x7B8, 0x7B0, 0x7C0,
    0x790, 0x7E0, 0x6E8, 0x6D8, 0x6A0, 0x688, 0x698, 0x6F8, 0x6A8, 0x6C8,
    0x680, 0x6D0, 0x6F0, 0x6B8, 0x6B0, 0x6C0, 0x690, 0x6E0, 0x68,  0x58,
    0x20,  0x8,   0x18,  0x78,  0x28,   0x48,  0x0,   0x50,  0x70,  0x38,
    0x30,  0x40,  0x10,  0x60,  0x2E8, 0x2D8, 0x2A0, 0x288, 0x298, 0x2F8,
    0x2A8, 0x2C8, 0x280, 0x2D0, 0x2F0, 0x2B8, 0x2B0, 0x2C0, 0x290, 0x2E0,
    0x3E8, 0x3D8, 0x3A0, 0x388, 0x398, 0x3F8, 0x3A8, 0x3C8, 0x380, 0x3D0,
    0x3F0, 0x3B8, 0x3B0, 0x3C0, 0x390, 0x3E0, 0x568, 0x558, 0x520, 0x508,
    0x518, 0x578, 0x528, 0x548, 0x500, 0x550, 0x570, 0x538, 0x530, 0x540,
    0x510, 0x560, 0x268, 0x258, 0x220, 0x208, 0x218, 0x278, 0x228, 0x248,
    0x200, 0x250, 0x270, 0x238, 0x230, 0x240, 0x210, 0x260, 0x4E8, 0x4D8,
    0x4A0, 0x488, 0x498, 0x4F8, 0x4A8, 0x4C8, 0x480, 0x4D0, 0x4F0, 0x4B8,
    0x4B0, 0x4C0, 0x490, 0x4E0, 0x168, 0x158, 0x120, 0x108, 0x118, 0x178,
    0x128, 0x148, 0x100, 0x150, 0x170, 0x138, 0x130, 0x140, 0x110, 0x160,
    0x1E8, 0x1D8, 0x1A0, 0x188, 0x198, 0x1F8, 0x1A8, 0x1C8, 0x180, 0x1D0,
    0x1F0, 0x1B8, 0x1B0, 0x1C0, 0x190, 0x1E0, 0x768, 0x758, 0x720, 0x708,
    0x718, 0x778, 0x728, 0x748, 0x700, 0x750, 0x770, 0x738, 0x730, 0x740,
    0x710, 0x760, 0x368, 0x358, 0x320, 0x308, 0x318, 0x378, 0x328, 0x348,
    0x300, 0x350, 0x370, 0x338, 0x330, 0x340, 0x310, 0x360, 0x5E8, 0x5D8,
    0x5A0, 0x588, 0x598, 0x5F8, 0x5A8, 0x5C8, 0x580, 0x5D0, 0x5F0, 0x5B8,
    0x5B0, 0x5C0, 0x590, 0x5E0, 0x468, 0x458, 0x420, 0x408, 0x418, 0x478,
    0x428, 0x448, 0x400, 0x450, 0x470, 0x438, 0x430, 0x440, 0x410, 0x460,
    0x668, 0x658, 0x620, 0x608, 0x618, 0x678, 0x628, 0x648, 0x600, 0x650,
    0x670, 0x638, 0x630, 0x640, 0x610, 0x660
  }
};

/**
 * Initialize algorithm context before calculating hash
 * with test parameters set.
 *
 * @param ctx context to initalize
 */
void
gosthash94_init (struct gosthash94_ctx *ctx)
{
    memset (ctx, 0, sizeof (struct gosthash94_ctx));
}

/*
 *  A macro that performs a full encryption round of GOST 28147-89.
 *  Temporary variables tmp assumed and variables r and l for left and right
 *  blocks.
 */
#define GOST_ENCRYPT_ROUND(key1, key2, sbox) \
  tmp = (key1) + r; \
  l ^= (sbox)[0][tmp & 0xff] ^ (sbox)[1][(tmp >> 8) & 0xff] ^ \
    (sbox)[2][(tmp >> 16) & 0xff] ^ (sbox)[3][tmp >> 24]; \
  tmp = (key2) + l; \
  r ^= (sbox)[0][tmp & 0xff] ^ (sbox)[1][(tmp >> 8) & 0xff] ^ \
    (sbox)[2][(tmp >> 16) & 0xff] ^ (sbox)[3][tmp >> 24];

/* encrypt a block with the given key */
#define GOST_ENCRYPT(result, i, key, hash, sbox) \
  r = hash[i], l = hash[i + 1]; \
  GOST_ENCRYPT_ROUND(key[0], key[1], sbox) \
  GOST_ENCRYPT_ROUND(key[2], key[3], sbox) \
  GOST_ENCRYPT_ROUND(key[4], key[5], sbox) \
  GOST_ENCRYPT_ROUND(key[6], key[7], sbox) \
  GOST_ENCRYPT_ROUND(key[0], key[1], sbox) \
  GOST_ENCRYPT_ROUND(key[2], key[3], sbox) \
  GOST_ENCRYPT_ROUND(key[4], key[5], sbox) \
  GOST_ENCRYPT_ROUND(key[6], key[7], sbox) \
  GOST_ENCRYPT_ROUND(key[0], key[1], sbox) \
  GOST_ENCRYPT_ROUND(key[2], key[3], sbox) \
  GOST_ENCRYPT_ROUND(key[4], key[5], sbox) \
  GOST_ENCRYPT_ROUND(key[6], key[7], sbox) \
  GOST_ENCRYPT_ROUND(key[7], key[6], sbox) \
  GOST_ENCRYPT_ROUND(key[5], key[4], sbox) \
  GOST_ENCRYPT_ROUND(key[3], key[2], sbox) \
  GOST_ENCRYPT_ROUND(key[1], key[0], sbox) \
  result[i] = l, result[i + 1] = r;

/**
 * The core transformation. Process a 512-bit block.
 *
 * @param hash intermediate message hash
 * @param block the message block to process
 */
static void
gost_block_compress (struct gosthash94_ctx *ctx, const uint32_t *block)
{
    unsigned i;
    uint32_t key[8], u[8], v[8], w[8], s[8];
    uint32_t l, r, tmp;

    /* u := hash, v := <256-bit message block> */
    memcpy (u, ctx->hash, sizeof (u));
    memcpy (v, block, sizeof (v));

    /* w := u xor v */
    w[0] = u[0] ^ v[0], w[1] = u[1] ^ v[1];
    w[2] = u[2] ^ v[2], w[3] = u[3] ^ v[3];
    w[4] = u[4] ^ v[4], w[5] = u[5] ^ v[5];
    w[6] = u[6] ^ v[6], w[7] = u[7] ^ v[7];

    /* calculate keys, encrypt hash and store result to the s[] array */
    for (i = 0;; i += 2)
      {
          /* key generation: key_i := P(w) */
          key[0] =
              (w[0] & 0x000000ff) | ((w[2] & 0x000000ff) << 8) |
              ((w[4] & 0x000000ff) << 16) | ((w[6] & 0x000000ff) << 24);
          key[1] =
              ((w[0] & 0x0000ff00) >> 8) | (w[2] & 0x0000ff00) |
              ((w[4] & 0x0000ff00) << 8) | ((w[6] & 0x0000ff00) << 16);
          key[2] =
              ((w[0] & 0x00ff0000) >> 16) | ((w[2] & 0x00ff0000) >> 8) |
              (w[4] & 0x00ff0000) | ((w[6] & 0x00ff0000) << 8);
          key[3] =
              ((w[0] & 0xff000000) >> 24) | ((w[2] & 0xff000000) >> 16) |
              ((w[4] & 0xff000000) >> 8) | (w[6] & 0xff000000);
          key[4] =
              (w[1] & 0x000000ff) | ((w[3] & 0x000000ff) << 8) |
              ((w[5] & 0x000000ff) << 16) | ((w[7] & 0x000000ff) << 24);
          key[5] =
              ((w[1] & 0x0000ff00) >> 8) | (w[3] & 0x0000ff00) |
              ((w[5] & 0x0000ff00) << 8) | ((w[7] & 0x0000ff00) << 16);
          key[6] =
              ((w[1] & 0x00ff0000) >> 16) | ((w[3] & 0x00ff0000) >> 8) |
              (w[5] & 0x00ff0000) | ((w[7] & 0x00ff0000) << 8);
          key[7] =
              ((w[1] & 0xff000000) >> 24) | ((w[3] & 0xff000000) >> 16) |
              ((w[5] & 0xff000000) >> 8) | (w[7] & 0xff000000);

          /* encryption: s_i := E_{key_i} (h_i) */
          GOST_ENCRYPT (s, i, key, ctx->hash, gosthash94_sbox);

          if (i == 0)
            {
                /* w:= A(u) ^ A^2(v) */
                w[0] = u[2] ^ v[4], w[1] = u[3] ^ v[5];
                w[2] = u[4] ^ v[6], w[3] = u[5] ^ v[7];
                w[4] = u[6] ^ (v[0] ^= v[2]);
                w[5] = u[7] ^ (v[1] ^= v[3]);
                w[6] = (u[0] ^= u[2]) ^ (v[2] ^= v[4]);
                w[7] = (u[1] ^= u[3]) ^ (v[3] ^= v[5]);
            }
          else if ((i & 2) != 0)
            {
                if (i == 6)
                    break;

                /* w := A^2(u) xor A^4(v) xor C_3; u := A(u) xor C_3 */
                /* C_3=0xff00ffff000000ffff0000ff00ffff0000ff00ff00ff00ffff00ff00ff00ff00 */
                u[2] ^= u[4] ^ 0x000000ff;
                u[3] ^= u[5] ^ 0xff00ffff;
                u[4] ^= 0xff00ff00;
                u[5] ^= 0xff00ff00;
                u[6] ^= 0x00ff00ff;
                u[7] ^= 0x00ff00ff;
                u[0] ^= 0x00ffff00;
                u[1] ^= 0xff0000ff;

                w[0] = u[4] ^ v[0];
                w[2] = u[6] ^ v[2];
                w[4] = u[0] ^ (v[4] ^= v[6]);
                w[6] = u[2] ^ (v[6] ^= v[0]);
                w[1] = u[5] ^ v[1];
                w[3] = u[7] ^ v[3];
                w[5] = u[1] ^ (v[5] ^= v[7]);
                w[7] = u[3] ^ (v[7] ^= v[1]);
            }
          else
            {
                /* i==4 here */
                /* w:= A( A^2(u) xor C_3 ) xor A^6(v) */
                w[0] = u[6] ^ v[4], w[1] = u[7] ^ v[5];
                w[2] = u[0] ^ v[6], w[3] = u[1] ^ v[7];
                w[4] = u[2] ^ (v[0] ^= v[2]);
                w[5] = u[3] ^ (v[1] ^= v[3]);
                w[6] = (u[4] ^= u[6]) ^ (v[2] ^= v[4]);
                w[7] = (u[5] ^= u[7]) ^ (v[3] ^= v[5]);
            }
      }

    /* step hash function: x(block, hash) := psi^61(hash xor psi(block xor psi^12(S))) */

    /* 12 rounds of the LFSR and xor in <message block> */
    u[0] = block[0] ^ s[6];
    u[1] = block[1] ^ s[7];
    u[2] =
        block[2] ^ (s[0] << 16) ^ (s[0] >> 16) ^ (s[0] & 0xffff) ^ (s[1] &
                                                                    0xffff)
        ^ (s[1] >> 16) ^ (s[2] << 16) ^ s[6] ^ (s[6] << 16) ^ (s[7] &
                                                               0xffff0000)
        ^ (s[7] >> 16);
    u[3] =
        block[3] ^ (s[0] & 0xffff) ^ (s[0] << 16) ^ (s[1] & 0xffff) ^ (s[1]
                                                                       <<
                                                                       16)
        ^ (s[1] >> 16) ^ (s[2] << 16) ^ (s[2] >> 16) ^ (s[3] << 16) ^ s[6]
        ^ (s[6] << 16) ^ (s[6] >> 16) ^ (s[7] & 0xffff) ^ (s[7] << 16) ^
        (s[7] >> 16);
    u[4] =
        block[4] ^ (s[0] & 0xffff0000) ^ (s[0] << 16) ^ (s[0] >> 16) ^
        (s[1] & 0xffff0000) ^ (s[1] >> 16) ^ (s[2] << 16) ^ (s[2] >> 16) ^
        (s[3] << 16) ^ (s[3] >> 16) ^ (s[4] << 16) ^ (s[6] << 16) ^ (s[6]
                                                                     >> 16)
        ^ (s[7] & 0xffff) ^ (s[7] << 16) ^ (s[7] >> 16);
    u[5] =
        block[5] ^ (s[0] << 16) ^ (s[0] >> 16) ^ (s[0] & 0xffff0000) ^
        (s[1] & 0xffff) ^ s[2] ^ (s[2] >> 16) ^ (s[3] << 16) ^ (s[3] >> 16)
        ^ (s[4] << 16) ^ (s[4] >> 16) ^ (s[5] << 16) ^ (s[6] << 16) ^ (s[6]
                                                                       >>
                                                                       16)
        ^ (s[7] & 0xffff0000) ^ (s[7] << 16) ^ (s[7] >> 16);
    u[6] =
        block[6] ^ s[0] ^ (s[1] >> 16) ^ (s[2] << 16) ^ s[3] ^ (s[3] >> 16)
        ^ (s[4] << 16) ^ (s[4] >> 16) ^ (s[5] << 16) ^ (s[5] >> 16) ^ s[6]
        ^ (s[6] << 16) ^ (s[6] >> 16) ^ (s[7] << 16);
    u[7] =
        block[7] ^ (s[0] & 0xffff0000) ^ (s[0] << 16) ^ (s[1] & 0xffff) ^
        (s[1] << 16) ^ (s[2] >> 16) ^ (s[3] << 16) ^ s[4] ^ (s[4] >> 16) ^
        (s[5] << 16) ^ (s[5] >> 16) ^ (s[6] >> 16) ^ (s[7] & 0xffff) ^
        (s[7] << 16) ^ (s[7] >> 16);

    /* 1 round of the LFSR (a mixing transformation) and xor with <hash> */
    v[0] = ctx->hash[0] ^ (u[1] << 16) ^ (u[0] >> 16);
    v[1] = ctx->hash[1] ^ (u[2] << 16) ^ (u[1] >> 16);
    v[2] = ctx->hash[2] ^ (u[3] << 16) ^ (u[2] >> 16);
    v[3] = ctx->hash[3] ^ (u[4] << 16) ^ (u[3] >> 16);
    v[4] = ctx->hash[4] ^ (u[5] << 16) ^ (u[4] >> 16);
    v[5] = ctx->hash[5] ^ (u[6] << 16) ^ (u[5] >> 16);
    v[6] = ctx->hash[6] ^ (u[7] << 16) ^ (u[6] >> 16);
    v[7] =
        ctx->
        hash[7] ^ (u[0] & 0xffff0000) ^ (u[0] << 16) ^ (u[1] & 0xffff0000)
        ^ (u[1] << 16) ^ (u[6] << 16) ^ (u[7] & 0xffff0000) ^ (u[7] >> 16);

    /* 61 rounds of LFSR, mixing up hash */
    ctx->hash[0] = (v[0] & 0xffff0000) ^ (v[0] << 16) ^ (v[0] >> 16) ^
        (v[1] >> 16) ^ (v[1] & 0xffff0000) ^ (v[2] << 16) ^
        (v[3] >> 16) ^ (v[4] << 16) ^ (v[5] >> 16) ^ v[5] ^
        (v[6] >> 16) ^ (v[7] << 16) ^ (v[7] >> 16) ^ (v[7] & 0xffff);
    ctx->hash[1] = (v[0] << 16) ^ (v[0] >> 16) ^ (v[0] & 0xffff0000) ^
        (v[1] & 0xffff) ^ v[2] ^ (v[2] >> 16) ^ (v[3] << 16) ^
        (v[4] >> 16) ^ (v[5] << 16) ^ (v[6] << 16) ^ v[6] ^
        (v[7] & 0xffff0000) ^ (v[7] >> 16);
    ctx->hash[2] = (v[0] & 0xffff) ^ (v[0] << 16) ^ (v[1] << 16) ^
        (v[1] >> 16) ^ (v[1] & 0xffff0000) ^ (v[2] << 16) ^ (v[3] >> 16) ^
        v[3] ^ (v[4] << 16) ^ (v[5] >> 16) ^ v[6] ^ (v[6] >> 16) ^
        (v[7] & 0xffff) ^ (v[7] << 16) ^ (v[7] >> 16);
    ctx->hash[3] = (v[0] << 16) ^ (v[0] >> 16) ^ (v[0] & 0xffff0000) ^
        (v[1] & 0xffff0000) ^ (v[1] >> 16) ^ (v[2] << 16) ^
        (v[2] >> 16) ^ v[2] ^ (v[3] << 16) ^ (v[4] >> 16) ^ v[4] ^
        (v[5] << 16) ^ (v[6] << 16) ^ (v[7] & 0xffff) ^ (v[7] >> 16);
    ctx->hash[4] =
        (v[0] >> 16) ^ (v[1] << 16) ^ v[1] ^ (v[2] >> 16) ^ v[2] ^ (v[3] <<
                                                                    16) ^
        (v[3] >> 16) ^ v[3] ^ (v[4] << 16) ^ (v[5] >> 16) ^ v[5] ^ (v[6] <<
                                                                    16) ^
        (v[6] >> 16) ^ (v[7] << 16);
    ctx->hash[5] =
        (v[0] << 16) ^ (v[0] & 0xffff0000) ^ (v[1] << 16) ^ (v[1] >> 16) ^
        (v[1] & 0xffff0000) ^ (v[2] << 16) ^ v[2] ^ (v[3] >> 16) ^ v[3] ^
        (v[4] << 16) ^ (v[4] >> 16) ^ v[4] ^ (v[5] << 16) ^ (v[6] << 16) ^
        (v[6] >> 16) ^ v[6] ^ (v[7] << 16) ^ (v[7] >> 16) ^ (v[7] &
                                                             0xffff0000);
    ctx->hash[6] =
        v[0] ^ v[2] ^ (v[2] >> 16) ^ v[3] ^ (v[3] << 16) ^ v[4] ^ (v[4] >>
                                                                   16) ^
        (v[5] << 16) ^ (v[5] >> 16) ^ v[5] ^ (v[6] << 16) ^ (v[6] >> 16) ^
        v[6] ^ (v[7] << 16) ^ v[7];
    ctx->hash[7] =
        v[0] ^ (v[0] >> 16) ^ (v[1] << 16) ^ (v[1] >> 16) ^ (v[2] << 16) ^
        (v[3] >> 16) ^ v[3] ^ (v[4] << 16) ^ v[4] ^ (v[5] >> 16) ^ v[5] ^
        (v[6] << 16) ^ (v[6] >> 16) ^ (v[7] << 16) ^ v[7];
}

/**
 * This function calculates hash value by 256-bit blocks.
 * It updates 256-bit check sum as follows:
 *    *(uint256_t)(ctx->sum) += *(uint256_t*)block;
 * and then updates intermediate hash value ctx->hash 
 * by calling gost_block_compress().
 *
 * @param ctx algorithm context
 * @param block the 256-bit message block to process
 */
static void
gost_compute_sum_and_hash (struct gosthash94_ctx *ctx, const uint8_t *block)
{
    uint32_t block_le[8];
    unsigned i, carry;

    /* compute the 256-bit sum */
    for (i = carry = 0; i < 8; i++, block += 4)
      {
	  block_le[i] = LE_READ_UINT32(block);
          ctx->sum[i] += carry;
	  carry = (ctx->sum[i] < carry);
          ctx->sum[i] += block_le[i];
          carry += (ctx->sum[i] < block_le[i]);
      }

    /* update message hash */
    gost_block_compress (ctx, block_le);
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
void
gosthash94_update (struct gosthash94_ctx *ctx,
		   size_t length, const uint8_t *msg)
{
    unsigned index = (unsigned) ctx->length & 31;
    ctx->length += length;

    /* fill partial block */
    if (index)
      {
          unsigned left = GOSTHASH94_BLOCK_SIZE - index;
          memcpy (ctx->message + index, msg, (length < left ? length : left));
          if (length < left)
              return;

          /* process partial block */
          gost_compute_sum_and_hash (ctx, ctx->message);
          msg += left;
          length -= left;
      }
    while (length >= GOSTHASH94_BLOCK_SIZE)
      {
          gost_compute_sum_and_hash (ctx, msg);
          msg += GOSTHASH94_BLOCK_SIZE;
          length -= GOSTHASH94_BLOCK_SIZE;
      }
    if (length)
      {
          /* save leftovers */
          memcpy (ctx->message, msg, length);
      }
}

/**
 * Finish hashing and store message digest into given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
void
gosthash94_digest (struct gosthash94_ctx *ctx,
		   size_t length, uint8_t *result)
{
    unsigned index = ctx->length & 31;
    uint32_t msg32[8];

    assert(length <= GOSTHASH94_DIGEST_SIZE);

    /* pad the last block with zeroes and hash it */
    if (index > 0)
      {
          memset (ctx->message + index, 0, 32 - index);
          gost_compute_sum_and_hash (ctx, ctx->message);
      }

    /* hash the message length and the sum */
    msg32[0] = ctx->length << 3;
    msg32[1] = ctx->length >> 29;
    memset (msg32 + 2, 0, sizeof (uint32_t) * 6);

    gost_block_compress (ctx, msg32);
    gost_block_compress (ctx, ctx->sum);

    /* convert hash state to result bytes */
    _nettle_write_le32(length, result, ctx->hash);
    gosthash94_init (ctx);
}
