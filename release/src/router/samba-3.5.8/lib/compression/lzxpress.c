/*
 * Copyright (C) Matthieu Suiche 2008
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "replace.h"
#include "lzxpress.h"


#define __BUF_POS_CONST(buf,ofs)(((const uint8_t *)buf)+(ofs))
#define __PULL_BYTE(buf,ofs) \
	((uint8_t)((*__BUF_POS_CONST(buf,ofs)) & 0xFF))

#ifndef PULL_LE_UINT16
#define PULL_LE_UINT16(buf,ofs) ((uint16_t)( \
	((uint16_t)(((uint16_t)(__PULL_BYTE(buf,(ofs)+0))) << 0)) | \
	((uint16_t)(((uint16_t)(__PULL_BYTE(buf,(ofs)+1))) << 8)) \
))
#endif

#ifndef PULL_LE_UINT32
#define PULL_LE_UINT32(buf,ofs) ((uint32_t)( \
	((uint32_t)(((uint32_t)(__PULL_BYTE(buf,(ofs)+0))) <<  0)) | \
	((uint32_t)(((uint32_t)(__PULL_BYTE(buf,(ofs)+1))) <<  8)) | \
	((uint32_t)(((uint32_t)(__PULL_BYTE(buf,(ofs)+2))) << 16)) | \
	((uint32_t)(((uint32_t)(__PULL_BYTE(buf,(ofs)+3))) << 24)) \
))
#endif

ssize_t lzxpress_compress(const uint8_t *uncompressed,
			  uint32_t uncompressed_size,
			  uint8_t *compressed,
			  uint32_t max_compressed_size)
{
	uint32_t uncompressed_pos, compressed_pos, byte_left;
	uint32_t max_offset, best_offset;
	int32_t offset;
	uint32_t max_len, len, best_len;
	const uint8_t *str1, *str2;
	uint32_t indic;
	uint8_t *indic_pos;
	uint32_t indic_bit, nibble_index;

	uint32_t metadata_size;
	uint16_t metadata;
	uint16_t *dest;

	if (!uncompressed_size) {
		return 0;
	}

	uncompressed_pos = 0;
	indic = 0;
	compressed_pos = sizeof(uint32_t);
	indic_pos = &compressed[0];

	byte_left = uncompressed_size;
	indic_bit = 0;
	nibble_index = 0;

	if (uncompressed_pos > XPRESS_BLOCK_SIZE)
		return 0;

	do {
		bool found = false;

		max_offset = uncompressed_pos;

		str1 = &uncompressed[uncompressed_pos];

		best_len = 2;
		best_offset = 0;

		max_offset = MIN(0x1FFF, max_offset);

		/* search for the longest match in the window for the lookahead buffer */
		for (offset = 1; (uint32_t)offset <= max_offset; offset++) {
			str2 = &str1[-offset];

			/* maximum len we can encode into metadata */
			max_len = MIN((255 + 15 + 7 + 3), byte_left);

			for (len = 0; (len < max_len) && (str1[len] == str2[len]); len++);

			/*
			 * We check if len is better than the value found before, including the
			 * sequence of identical bytes
			 */
			if (len > best_len) {
				found = true;
				best_len = len;
				best_offset = offset;
			}
		}

		if (found) {
			metadata_size = 0;
			dest = (uint16_t *)&compressed[compressed_pos];

			if (best_len < 10) {
				/* Classical meta-data */
				metadata = (uint16_t)(((best_offset - 1) << 3) | (best_len - 3));
				dest[metadata_size / sizeof(uint16_t)] = metadata;
				metadata_size += sizeof(uint16_t);
			} else {
				metadata = (uint16_t)(((best_offset - 1) << 3) | 7);
				dest[metadata_size / sizeof(uint16_t)] = metadata;
				metadata_size = sizeof(uint16_t);

				if (best_len < (15 + 7 + 3)) {
					/* Shared byte */
					if (!nibble_index) {
						compressed[compressed_pos + metadata_size] = (best_len - (3 + 7)) & 0xF;
						metadata_size += sizeof(uint8_t);
					} else {
						compressed[nibble_index] &= 0xF;
						compressed[nibble_index] |= (best_len - (3 + 7)) * 16;
					}
				} else if (best_len < (3 + 7 + 15 + 255)) {
					/* Shared byte */
					if (!nibble_index) {
						compressed[compressed_pos + metadata_size] = 15;
						metadata_size += sizeof(uint8_t);
					} else {
						compressed[nibble_index] &= 0xF;
						compressed[nibble_index] |= (15 * 16);
					}

					/* Additionnal best_len */
					compressed[compressed_pos + metadata_size] = (best_len - (3 + 7 + 15)) & 0xFF;
					metadata_size += sizeof(uint8_t);
				} else {
					/* Shared byte */
					if (!nibble_index) {
						compressed[compressed_pos + metadata_size] |= 15;
						metadata_size += sizeof(uint8_t);
					} else {
						compressed[nibble_index] |= 15 << 4;
					}

					/* Additionnal best_len */
					compressed[compressed_pos + metadata_size] = 255;

					metadata_size += sizeof(uint8_t);

					compressed[compressed_pos + metadata_size] = (best_len - 3) & 0xFF;
					compressed[compressed_pos + metadata_size + 1] = ((best_len - 3) >> 8) & 0xFF;
					metadata_size += sizeof(uint16_t);
				}
			}

			indic |= 1 << (32 - ((indic_bit % 32) + 1));

			if (best_len > 9) {
				if (nibble_index == 0) {
					nibble_index = compressed_pos + sizeof(uint16_t);
				} else {
					nibble_index = 0;
				}
			}

			compressed_pos += metadata_size;
			uncompressed_pos += best_len;
			byte_left -= best_len;
		} else {
			compressed[compressed_pos++] = uncompressed[uncompressed_pos++];
			byte_left--;
		}
		indic_bit++;

		if ((indic_bit - 1) % 32 > (indic_bit % 32)) {
			*(uint32_t *)indic_pos = indic;
			indic = 0;
			indic_pos = &compressed[compressed_pos];
			compressed_pos += sizeof(uint32_t);
		}
	} while (byte_left > 3);

	do {
		compressed[compressed_pos] = uncompressed[uncompressed_pos];
		indic_bit++;

		uncompressed_pos++;
		compressed_pos++;
                if (((indic_bit - 1) % 32) > (indic_bit % 32)){
			*(uint32_t *)indic_pos = indic;
			indic = 0;
			indic_pos = &compressed[compressed_pos];
			compressed_pos += sizeof(uint32_t);
		}
	} while (uncompressed_pos < uncompressed_size);

	if ((indic_bit % 32) > 0) {
		for (; (indic_bit % 32) != 0; indic_bit++)
			indic |= 0 << (32 - ((indic_bit % 32) + 1));

		*(uint32_t *)indic_pos = indic;
		compressed_pos += sizeof(uint32_t);
	}

	return compressed_pos;
}

ssize_t lzxpress_decompress(const uint8_t *input,
			    uint32_t input_size,
			    uint8_t *output,
			    uint32_t max_output_size)
{
	uint32_t output_index, input_index;
	uint32_t indicator, indicator_bit;
	uint32_t length;
	uint32_t offset;
	uint32_t nibble_index;

	output_index = 0;
	input_index = 0;
	indicator = 0;
	indicator_bit = 0;
	length = 0;
	offset = 0;
	nibble_index = 0;

	do {
		if (indicator_bit == 0) {
			indicator = PULL_LE_UINT32(input, input_index);
			input_index += sizeof(uint32_t);
			indicator_bit = 32;
		}
		indicator_bit--;

		/*
		 * check whether the bit specified by indicator_bit is set or not
		 * set in indicator. For example, if indicator_bit has value 4
		 * check whether the 4th bit of the value in indicator is set
		 */
		if (((indicator >> indicator_bit) & 1) == 0) {
			output[output_index] = input[input_index];
			input_index += sizeof(uint8_t);
			output_index += sizeof(uint8_t);
		} else {
			length = PULL_LE_UINT16(input, input_index);
			input_index += sizeof(uint16_t);
			offset = length / 8;
			length = length % 8;

			if (length == 7) {
				if (nibble_index == 0) {
					nibble_index = input_index;
					length = input[input_index] % 16;
					input_index += sizeof(uint8_t);
				} else {
					length = input[nibble_index] / 16;
					nibble_index = 0;
				}

				if (length == 15) {
					length = input[input_index];
					input_index += sizeof(uint8_t);
					if (length == 255) {
						length = PULL_LE_UINT16(input, input_index);
						input_index += sizeof(uint16_t);
						length -= (15 + 7);
					}
					length += 15;
				}
				length += 7;
			}

			length += 3;

			do {
				if ((output_index >= max_output_size) || ((offset + 1) > output_index)) break;

				output[output_index] = output[output_index - offset - 1];

				output_index += sizeof(uint8_t);
				length -= sizeof(uint8_t);
			} while (length != 0);
		}
	} while ((output_index < max_output_size) && (input_index < (input_size)));

	return output_index;
}
