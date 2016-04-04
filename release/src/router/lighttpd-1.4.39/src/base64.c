#include "base64.h"

/* reverse mapping:
 * >= 0: base64 value
 * -1: invalid character
 * -2: skip character (whitespace/control)
 * -3: padding
 */

/* BASE64_STANDARD: "A-Z a-z 0-9 + /" maps to 0-63, pad with "=" */
static const char base64_standard_table[66] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
static const short base64_standard_reverse_table[128] = {
/*	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
	-1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, /* 0x00 - 0x0F */
	-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, /* 0x10 - 0x1F */
	-2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, /* 0x20 - 0x2F */
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -3, -1, -1, /* 0x30 - 0x3F */
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 0x40 - 0x4F */
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, /* 0x50 - 0x5F */
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 0x60 - 0x6F */
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, /* 0x70 - 0x7F */
};

/* BASE64_URL: "A-Z a-z 0-9 - _" maps to 0-63, pad with "." */
static const char base64_url_table[66] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
static const short base64_url_reverse_table[128] = {
/*	 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
	-1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, /* 0x00 - 0x0F */
	-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, /* 0x10 - 0x1F */
	-2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -3, -1, /* 0x20 - 0x2F */
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 0x30 - 0x3F */
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 0x40 - 0x4F */
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63, /* 0x50 - 0x5F */
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 0x60 - 0x6F */
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, /* 0x70 - 0x7F */
};

unsigned char* buffer_append_base64_decode(buffer *out, const char* in, size_t in_length, base64_charset charset) {
	unsigned char *result;
	size_t out_pos = 0; /* current output character (position) that is decoded. can contain partial result */
	unsigned int group = 0; /* how many base64 digits in the current group were decoded already. each group has up to 4 digits */
	size_t i;
	const short* base64_reverse_table;

	switch (charset) {
	case BASE64_STANDARD:
		base64_reverse_table = base64_standard_reverse_table;
		break;
	case BASE64_URL:
		base64_reverse_table = base64_url_reverse_table;
		break;
	default:
		return NULL;
	}

	result = (unsigned char *) buffer_string_prepare_append(out, 3*(in_length / 4) + 3);

	/* run through the whole string, converting as we go */
	for (i = 0; i < in_length; i++) {
		unsigned char c = (unsigned char) in[i];
		short ch;

		if (c == '\0') break;
		if (c >= 128) return NULL; /* only 7-bit characters allowed */

		ch = base64_reverse_table[c];
		if (-3 == ch) {
			/* pad character; can only come after 2 base64 digits in a group */
			if (group < 2) return NULL;
			break;
		} else if (-2 == ch) {
			continue; /* skip character */
		} else if (ch < 0) {
			return NULL; /* invalid character, abort */
		}

		switch(group) {
		case 0:
			result[out_pos] = ch << 2;
			group = 1;
			break;
		case 1:
			result[out_pos++] |= ch >> 4;
			result[out_pos] = (ch & 0x0f) << 4;
			group = 2;
			break;
		case 2:
			result[out_pos++] |= ch >>2;
			result[out_pos] = (ch & 0x03) << 6;
			group = 3;
			break;
		case 3:
			result[out_pos++] |= ch;
			group = 0;
			break;
		}
	}

	switch(group) {
	case 0:
		/* ended on boundary */
		break;
	case 1:
		/* need at least 2 base64 digits per group */
		return NULL;
	case 2:
		/* have 2 base64 digits in last group => one real octect, two zeroes padded */
	case 3:
		/* have 3 base64 digits in last group => two real octects, one zero padded */

		/* for both cases the current index already is on the first zero padded octet
		 * - check it really is zero (overlapping bits) */
		if (0 != result[out_pos]) return NULL;
		break;
	}

	buffer_commit(out, out_pos);

	return result;
}

size_t li_to_base64_no_padding(char* out, size_t out_length, const unsigned char* in, size_t in_length, base64_charset charset) {
	const size_t full_tuples = in_length / 3;
	const size_t in_tuple_remainder = in_length % 3;
	const size_t out_tuple_remainder = in_tuple_remainder ? 1 + in_tuple_remainder : 0;
	const size_t require_space = 4 * full_tuples + out_tuple_remainder;
	const char* base64_table;
	size_t i;
	size_t out_pos = 0;

	switch (charset) {
	case BASE64_STANDARD:
		base64_table = base64_standard_table;
		break;
	case BASE64_URL:
		base64_table = base64_url_table;
		break;
	default:
		force_assert(0 && "invalid charset");
	}

	/* check overflows */
	force_assert(full_tuples < 2*full_tuples);
	force_assert(full_tuples < 4*full_tuples);
	force_assert(4*full_tuples < 4*full_tuples + out_tuple_remainder);
	force_assert(require_space <= out_length);

	for (i = 2; i < in_length; i += 3) {
		unsigned int v = (in[i-2] << 16) | (in[i-1] << 8) | in[i];
		out[out_pos+3] = base64_table[v & 0x3f]; v >>= 6;
		out[out_pos+2] = base64_table[v & 0x3f]; v >>= 6;
		out[out_pos+1] = base64_table[v & 0x3f]; v >>= 6;
		out[out_pos+0] = base64_table[v & 0x3f];
		out_pos += 4;
	}
	switch (in_tuple_remainder) {
	case 0:
		break;
	case 1:
		{
			/* pretend in[i-1] = in[i] = 0, don't write last two (out_pos+3, out_pos+2) characters */
			unsigned int v = (in[i-2] << 4);
			out[out_pos+1] = base64_table[v & 0x3f]; v >>= 6;
			out[out_pos+0] = base64_table[v & 0x3f];
			out_pos += 2;
		}
		break;
	case 2:
		{
			/* pretend in[i] = 0, don't write last (out_pos+3) character */
			unsigned int v = (in[i-2] << 10) | (in[i-1] << 2);
			out[out_pos+2] = base64_table[v & 0x3f]; v >>= 6;
			out[out_pos+1] = base64_table[v & 0x3f]; v >>= 6;
			out[out_pos+0] = base64_table[v & 0x3f];
			out_pos += 3;
		}
		break;
	}
	force_assert(out_pos <= out_length);
	return out_pos;
}

size_t li_to_base64(char* out, size_t out_length, const unsigned char* in, size_t in_length, base64_charset charset) {
	const size_t in_tuple_remainder = in_length % 3;
	size_t padding_length = in_tuple_remainder ? 3 - in_tuple_remainder : 0;

	char padding;
	size_t out_pos;

	switch (charset) {
	case BASE64_STANDARD:
		padding = base64_standard_table[64];
		break;
	case BASE64_URL:
		padding = base64_url_table[64];
		break;
	default:
		force_assert(0 && "invalid charset");
	}

	force_assert(out_length >= padding_length);
	out_pos = li_to_base64_no_padding(out, out_length - padding_length, in, in_length, charset);

	while (padding_length > 0) {
		out[out_pos++] = padding;
		padding_length--;
	}

	force_assert(out_pos <= out_length);

	return out_pos;
}


char* buffer_append_base64_encode_no_padding(buffer *out, const unsigned char* in, size_t in_length, base64_charset charset) {
	size_t reserve = 4*(in_length/3) + 4;
	char* result = buffer_string_prepare_append(out, reserve);
	size_t out_pos = li_to_base64_no_padding(result, reserve, in, in_length, charset);

	buffer_commit(out, out_pos);

	return result;
}

char* buffer_append_base64_encode(buffer *out, const unsigned char* in, size_t in_length, base64_charset charset) {
	size_t reserve = 4*(in_length/3) + 4;
	char* result = buffer_string_prepare_append(out, reserve);
	size_t out_pos = li_to_base64(result, reserve, in, in_length, charset);

	buffer_commit(out, out_pos);

	return result;
}
