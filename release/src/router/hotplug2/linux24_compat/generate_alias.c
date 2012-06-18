/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/input.h>

#include "../mem_utils.h"
#include "../parser_utils.h"
#include "../filemap_utils.h"

#define JUMP_TO_NEXT	{ free(line); free(module); continue; }

#define PRINT_WILDCARD(prefix, format, variable, any) \
	if (variable == any)\
		fprintf(fp, prefix "*"); \
	else \
		fprintf(fp, prefix format, variable);
		
#define PRINT_WILDCARD_COND(prefix, format, variable, condition) \
	if (condition)\
		fprintf(fp, prefix format, variable); \
	else \
		fprintf(fp, prefix "*");

#define SET_OFFSET(offset, token, prefix, prefix_len) \
	offset = strncmp(token, prefix, prefix_len) ? 0 : prefix_len;

#ifndef KEY_MIN_INTERESTING
#define KEY_MIN_INTERESTING		KEY_MUTE
#endif

/* Some kernel headers appear to define it even without __KERNEL__ */
#ifndef BITS_PER_LONG
#define BITS_PER_LONG			(sizeof(long) * 8)
#endif
#ifndef NBITS
#define NBITS(x) 			((x / BITS_PER_LONG) + 1)
#endif

#define TEST_INPUT_BIT(i,bm)	(bm[i / BITS_PER_LONG] & (((unsigned long)1) << (i%BITS_PER_LONG)))

void module_tr(char *modname) {
	char *ptr;
	
	for (ptr = modname; *ptr != '\0'; ptr++) {
		if (*ptr == '-')
			*ptr = '_';
	}
}

int hexchar_to_int(char c) {
	c = tolower(c);
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= '0' && c <= '9') {
		return c - '0';
	}
	
	return 0;
}

void alias_from_pcimap(FILE *fp, char *prefix) {
	struct filemap_t pcimap;
	char *line, *nline, *nptr;
	char *token;
	char *module;
	char *filename;
	
	unsigned long vendor, device, sub_vendor, sub_device, class_type, class_mask;
	
	filename = xmalloc(strlen(prefix) + strlen("modules.pcimap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.pcimap");
	
	if (map_file(filename, &pcimap)) {
		free(filename);
		return;
	}
	
	nptr = pcimap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		vendor = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		device = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		sub_vendor = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		sub_device = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		class_type = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		class_mask = strtoul(token, NULL, 0);
		free(token);

		fprintf(fp, "alias pci:");
		
		PRINT_WILDCARD("v", "%08lX", vendor, 0xffffffff);
		PRINT_WILDCARD("d", "%08lX", device, 0xffffffff);
		PRINT_WILDCARD("sv", "%08lX", sub_vendor, 0xffffffff);
		PRINT_WILDCARD("sd", "%08lX", sub_device, 0xffffffff);
		
		/* FIXME ! USE PRINT_WIL*/
		PRINT_WILDCARD_COND("bc", "%02X", 
		(unsigned int)((class_type & 0x00ff0000 & class_mask) >> 16),
		class_mask & 0x00ff0000);
		
		PRINT_WILDCARD_COND("sc", "%02X", 
		(unsigned int)((class_type & 0x0000ff00 & class_mask) >> 8),
		class_mask & 0x00ff0000);
		
		PRINT_WILDCARD_COND("i", "%02X", 
		(unsigned int)((class_type & 0x000000ff & class_mask)),
		class_mask & 0x00ff0000);
		
		module_tr(module);
		fprintf(fp, " %s\n", module);
		
		free(module);
		free(line);
	}
	
	unmap_file(&pcimap);
	free(filename);
}

/* Yes, this -is- ugly. */
void alias_from_usbmap_print_pattern(FILE *fp, 
	unsigned long mflags, unsigned long ven,
	unsigned long pro, unsigned long bcdl, unsigned long bcdh,
	unsigned long bdc, unsigned long bdsc, unsigned long bdp,
	unsigned long bic, unsigned long bisc, unsigned long bip, 
	int len, unsigned long left, int ndigits, 
	char *module) {
	
	fprintf(fp, "alias usb:");
	
	PRINT_WILDCARD_COND("v", "%04lX", ven, mflags & 0x0001);
	PRINT_WILDCARD_COND("p", "%04lX", pro, mflags & 0x0002);
	
	fprintf(fp, "d");
	
	if (mflags & 0x000C) {
		if (ndigits > 0)
			fprintf(fp, "%.*lX", ndigits - 1, left);

		if (bcdl == 0 && bcdh == 0xf)
			fprintf(fp, "*"), ndigits = len;
		else if (bcdl == bcdh)
			fprintf(fp, "%lX", bcdl);
		else if (bcdl / 10 != bcdh / 10)
			fprintf(fp, "[%lX-%X%X-%lX]", bcdl, 9, 10, bcdh);
		else
			fprintf(fp, "[%lX-%lX]", bcdl, bcdh);

		if (ndigits < len)
			fprintf(fp, "*");
	} else {
		fprintf(fp, "*");
	}
	
	PRINT_WILDCARD_COND("dc", "%02lX", bdc, mflags & 0x0010);
	PRINT_WILDCARD_COND("dsc", "%02lX", bdsc, mflags & 0x0020);
	PRINT_WILDCARD_COND("dp", "%02lX", bdp, mflags & 0x0040);
	
	PRINT_WILDCARD_COND("ic", "%02lX", bic, mflags & 0x0080);
	PRINT_WILDCARD_COND("isc", "%02lX", bisc, mflags & 0x0100);
	PRINT_WILDCARD_COND("ip", "%02lX", bip, mflags & 0x0200);

	fprintf(fp, " %s\n", module);
}

void alias_from_usbmap(FILE *fp, char *prefix) {
	struct filemap_t usbmap;
	char *line, *nline, *nptr;
	char *token;
	char *module;
	char *filename;
	
	unsigned long match_flags;
	unsigned long vendor, product;
	unsigned long bcddev_lo, bcddev_hi;	/* some devices violate bcd */
	unsigned long bdevclass, bdevsubclass, bdevproto;
	unsigned long bifclass, bifsubclass, bifproto;
	
	unsigned long cmin, cmax;
	int len, ndigits;
	
	filename = xmalloc(strlen(prefix) + strlen("modules.usbmap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.usbmap");
	
	if (map_file(filename, &usbmap)) {
		free(filename);
		return;
	}
	
	nptr = usbmap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		match_flags = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		vendor = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		product = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bcddev_lo = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bcddev_hi = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bdevclass = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bdevsubclass = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bdevproto = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bifclass = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bifsubclass = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bifproto = strtoul(token, NULL, 0);
		free(token);
		
		module_tr(module);
		
		for (len = ndigits = 4; bcddev_lo <= bcddev_hi; ndigits--) {
			cmin = bcddev_lo & 0xf;
			cmax = bcddev_hi & 0xf;
			
			bcddev_lo >>= 4;
			bcddev_hi >>= 4;
			
			if (bcddev_lo == bcddev_hi || ndigits == 0) {
				alias_from_usbmap_print_pattern(fp, 
				match_flags, vendor, product, 
				cmin, cmax, 
				bdevclass, bdevsubclass, bdevproto, 
				bifclass, bifsubclass, bifproto, 
				len, bcddev_lo, ndigits,
				module);
				break;
			}
			
			if (cmin > 0) {
				alias_from_usbmap_print_pattern(fp, 
				match_flags, vendor, product, 
				cmin, 0xf, 
				bdevclass, bdevsubclass, bdevproto, 
				bifclass, bifsubclass, bifproto, 
				len, bcddev_lo++, ndigits, 
				module);
			} 
			
			if (cmin < 0xf) {
				alias_from_usbmap_print_pattern(fp, 
				match_flags, vendor, product, 
				0x0, cmax, 
				bdevclass, bdevsubclass, bdevproto, 
				bifclass, bifsubclass, bifproto, 
				len, bcddev_hi--, ndigits, 
				module);
			}
		}
		
		free(module);
		free(line);
	}
	
	unmap_file(&usbmap);
	free(filename);
}

void alias_from_ieee1394map(FILE *fp, char *prefix) {
	struct filemap_t ieee1394map;
	char *line, *nline, *nptr;
	char *token;
	char *module;
	char *filename;
	
	unsigned long match_flags, vendor, product, specifier, version;
	
	filename = xmalloc(strlen(prefix) + strlen("modules.ieee1394map") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.ieee1394map");
	
	if (map_file(filename, &ieee1394map)) {
		free(filename);
		return;
	}
	
	nptr = ieee1394map.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;

		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		match_flags = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		vendor = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		product = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		specifier = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		version = strtoul(token, NULL, 0);
		free(token);
		
		fprintf(fp, "alias ieee1394:");
		
		PRINT_WILDCARD_COND("ven", "%08lX", vendor, match_flags & 0x01);
		PRINT_WILDCARD_COND("mo", "%08lX", product, match_flags & 0x02);
		PRINT_WILDCARD_COND("sp", "%08lX", specifier, match_flags & 0x04);
		PRINT_WILDCARD_COND("ver", "%08lX", version, match_flags & 0x08);
		if (match_flags & 0x08)
			fprintf(fp, "*");
		
		module_tr(module);
		fprintf(fp, " %s\n", module);
		
		free(module);
		free(line);
	}
	
	unmap_file(&ieee1394map);
	free(filename);
}

void alias_from_isapnpmap(FILE *fp, char *prefix) {
	struct filemap_t isapnpmap;
	char *line, *nline, *nptr;
	char *module;
	char *filename;
	char *token;
	
	int offset;
	
	unsigned char vendor_sig[3], vendor[2], function[2];
	unsigned char card_vendor_sig[3], card_vendor[2], card_device[2];
	
	filename = xmalloc(strlen(prefix) + strlen("modules.isapnpmap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.isapnpmap");
	
	if (map_file(filename, &isapnpmap)) {
		free(filename);
		return;
	}
	
	nptr = isapnpmap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		
		SET_OFFSET(offset, token, "0x", 2);
		card_vendor[1] =  hexchar_to_int(token[offset]) * 16 + hexchar_to_int(token[offset + 1]);
		card_vendor[0] =  hexchar_to_int(token[offset + 2]) * 16 + hexchar_to_int(token[offset + 3]);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		
		SET_OFFSET(offset, token, "0x", 2);
		card_device[1] =  hexchar_to_int(token[offset]) * 16 + hexchar_to_int(token[offset + 1]);
		card_device[0] =  hexchar_to_int(token[offset + 2]) * 16 + hexchar_to_int(token[offset + 3]);
		free(token);
		
		/* jump driverdata */
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		
		SET_OFFSET(offset, token, "0x", 2);
		vendor[1] = hexchar_to_int(token[offset]) * 16 + hexchar_to_int(token[offset + 1]);
		vendor[0] = hexchar_to_int(token[offset + 2]) * 16 + hexchar_to_int(token[offset+ 3]);
		
		vendor_sig[0] = ((vendor[0] >> 2) & 0x1f) + ('A' - 1);
		vendor_sig[1] = (((vendor[0] & 3) << 3) | (vendor[1] >> 5)) + ('A' - 1);
		vendor_sig[2] = (vendor[1] & 0x1f) + ('A' - 1);
		
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		
		SET_OFFSET(offset, token, "0x", 2);
		function[1] = hexchar_to_int(token[offset]) * 16 + hexchar_to_int(token[offset + 1]);
		function[0] = hexchar_to_int(token[offset + 2]) * 16 + hexchar_to_int(token[offset + 3]);
		
		free(token);
		
		fprintf(fp, "alias pnp:");
		if (memcmp("\xff\xff", card_vendor, 2) && memcmp("\xff\xff", card_device, 2)) {
			card_vendor_sig[0] = ((card_vendor[0] >> 2) & 0x1f) + ('A' - 1);
			card_vendor_sig[1] = (((card_vendor[0] & 3) << 3) | (card_vendor[1] >> 5)) + ('A' - 1);
			card_vendor_sig[2] = (card_vendor[1] & 0x1f) + ('A' - 1);
			
			fprintf(fp, "c%c%c%c%02x%02x", card_vendor_sig[0], card_vendor_sig[1], card_vendor_sig[2], card_device[0], card_device[1]);
		}
		
		fprintf(fp, "d%c%c%c%02x%02x", vendor_sig[0], vendor_sig[1], vendor_sig[2], function[0], function[1]);
		while ((token = dup_token(nline, &nline, isspace)) != NULL) {
			if (!token)
				break;
			
			SET_OFFSET(offset, token, "0x", 2);
			vendor[1] = hexchar_to_int(token[offset]) * 16 + hexchar_to_int(token[offset + 1]);
			vendor[0] = hexchar_to_int(token[offset + 2]) * 16 + hexchar_to_int(token[offset+ 3]);
			
			vendor_sig[0] = ((vendor[0] >> 2) & 0x1f) + ('A' - 1);
			vendor_sig[1] = (((vendor[0] & 3) << 3) | (vendor[1] >> 5)) + ('A' - 1);
			vendor_sig[2] = (vendor[1] & 0x1f) + ('A' - 1);
			
			free(token);
			
			token = dup_token(nline, &nline, isspace);
			if (!token)
				break;
			
			SET_OFFSET(offset, token, "0x", 2);
			function[1] = hexchar_to_int(token[offset]) * 16 + hexchar_to_int(token[offset + 1]);
			function[0] = hexchar_to_int(token[offset + 2]) * 16 + hexchar_to_int(token[offset + 3]);
			
			free(token);
			
			fprintf(fp, "d%c%c%c%02x%02x", vendor_sig[0], vendor_sig[1], vendor_sig[2], function[0], function[1]);
		}
		
		module_tr(module);
		fprintf(fp, "* %s\n", module);
		
		free(module);
		free(line);
	}
	
	unmap_file(&isapnpmap);
	free(filename);
}

void alias_from_seriomap(FILE *fp, char *prefix) {
	struct filemap_t seriomap;
	char *line, *nline, *nptr;
	char *token;
	char *module;
	char *filename;
	
	unsigned char type, extra, id, proto;
	
	filename = xmalloc(strlen(prefix) + strlen("modules.seriomap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.seriomap");
	
	if (map_file(filename, &seriomap)) {
		free(filename);
		return;
	}
	
	nptr = seriomap.map;

	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		type = strtoul(token, NULL, 16);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		extra = strtoul(token, NULL, 16);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		id = strtoul(token, NULL, 16);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		proto = strtoul(token, NULL, 16);
		free(token);
		
		fprintf(fp, "alias serio:");
		PRINT_WILDCARD("ty", "%02X", type, 0xff);
		PRINT_WILDCARD("pr", "%02X", proto, 0xff);
		PRINT_WILDCARD("id", "%02X", id, 0xff);
		PRINT_WILDCARD("ex", "%02X", extra, 0xff);
		fprintf(fp, " %s\n", module);
		
		free(module);
		free(line);
	}
	
	unmap_file(&seriomap);
	free(filename);
}

inline int iscolon(int c) {
	return (c == ':');
}

char *bitmap_to_bitstring(char name, unsigned long *bm, unsigned int min_bit, unsigned int max_bit)
{
	char *rv;
        unsigned int i, len = 0, size = 16, srv;
	
	rv = xmalloc(size);
	
        len += snprintf(rv + len, size - len, "%c*", name);
	
        for (i = min_bit; i < max_bit; i++) {
		if (TEST_INPUT_BIT(i, bm)) {
			while ((srv = snprintf(rv + len, size - len, "%X,", i)) >= (size - len)) {
				size = size * 2;
				rv = xrealloc(rv, size);
			}
			len += srv;
		}
	}
	
	/* read: if (we had any match) { */
	if (len > 2) {
		while ((srv = snprintf(rv + len, size - len, "*")) >= (size - len)) {
			size = size * 2;
			rv = xrealloc(rv, size);
		}
	}
	
	return rv;
}

void string_to_bitmap(char *input, unsigned long *bitmap, int bm_len) {
	char *token, *ptr;
	int i = 0;
	
	ptr = input + strlen(input);
	
	while ((token = dup_token_r(ptr, input, &ptr, iscolon)) != NULL) {
		bitmap[i] = strtoul(token, NULL, 16);
		free(token);
		i++;
	}
	
	while (i < bm_len)
		bitmap[i++] = 0;
}

#define GET_BITMAP(mapkey, bitmap, name, min) \
	token = dup_token(nline, &nline, isspace); \
	if (!token) \
		JUMP_TO_NEXT; \
	if (TEST_INPUT_BIT(EV_ ## mapkey, ev_bits)) { \
		if (token == NULL) \
			JUMP_TO_NEXT; \
		\
		string_to_bitmap(token, bitmap ## _bits, NBITS(mapkey ## _MAX)); \
	} \
	bitmap = bitmap_to_bitstring(name, bitmap ## _bits, min, mapkey ## _MAX); \
	free(token);

void alias_from_inputmap(FILE *fp, char *prefix) {
	struct filemap_t inputmap;
	char *line, *nline, *nptr;
	char *token, *sw_token;
	char *filename;
	char *module;
	
	int new_format;
	
	char *ev, *key, *rel, *abs, *sw, *msc, *led, *snd, *ff;
	
	unsigned long bustype, vendor, product, version;
	unsigned long ev_bits[NBITS(EV_MAX)];
	unsigned long key_bits[NBITS(KEY_MAX)];
	unsigned long rel_bits[NBITS(REL_MAX)];
	unsigned long abs_bits[NBITS(ABS_MAX)];
	unsigned long msc_bits[NBITS(MSC_MAX)];
	unsigned long led_bits[NBITS(LED_MAX)];
	unsigned long snd_bits[NBITS(SND_MAX)];
	unsigned long ff_bits[NBITS(FF_MAX)];
	#if defined(SW_MAX) && defined(EV_SW)
	unsigned long sw_bits[NBITS(SW_MAX)];
	#endif
	
	memset(ev_bits, 0, NBITS(EV_MAX) * sizeof(long));
	memset(key_bits, 0, NBITS(KEY_MAX) * sizeof(long));
	memset(rel_bits, 0, NBITS(REL_MAX) * sizeof(long));
	memset(abs_bits, 0, NBITS(ABS_MAX) * sizeof(long));
	memset(msc_bits, 0, NBITS(MSC_MAX) * sizeof(long));
	memset(led_bits, 0, NBITS(LED_MAX) * sizeof(long));
	memset(snd_bits, 0, NBITS(SND_MAX) * sizeof(long));
	memset(ff_bits, 0, NBITS(FF_MAX) * sizeof(long));
	#if defined(SW_MAX) && defined(EV_SW)
	memset(sw_bits, 0, NBITS(SW_MAX) * sizeof(long));
	#endif
	
	filename = xmalloc(strlen(prefix) + strlen("modules.inputmap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.inputmap");
	
	if (map_file(filename, &inputmap)) {
		free(filename);
		return;
	}
	
	nptr = inputmap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		/* jump matchbits */
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		bustype = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		vendor = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		product = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		version = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		
		string_to_bitmap(token, ev_bits, NBITS(EV_MAX));
		ev = bitmap_to_bitstring('e', ev_bits, 0, EV_MAX);
		free(token);
		
		GET_BITMAP(KEY, key, 'k', KEY_MIN_INTERESTING);
		GET_BITMAP(REL, rel, 'r', 0);
		GET_BITMAP(ABS, abs, 'a', 0);
		GET_BITMAP(MSC, msc, 'm', 0);
		GET_BITMAP(LED, led, 'l', 0);
		GET_BITMAP(SND, snd, 's', 0);
		GET_BITMAP(FF, ff, 'f', 0);
		
		sw_token = nline;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		new_format = token ? 1 : 0;
		free(token);
		
		#if defined(SW_MAX) && defined(EV_SW)
		if (new_format) {
			nline = sw_token;
			GET_BITMAP(SW, sw, 'w', 0);
		} else {
			sw = strdup("");
		}
		#else
		sw = strdup("");
		#endif
		
		fprintf(fp, "alias input:");
		
		PRINT_WILDCARD("b", "%04lX", bustype, 0x0);
		PRINT_WILDCARD("v", "%04lX", vendor, 0x0);
		PRINT_WILDCARD("p", "%04lX", product, 0x0);
		PRINT_WILDCARD("e", "%04lX", version, 0x0);
		fprintf(fp, "-");
		
		fprintf(fp, "%s%s%s%s%s%s%s%s%s", 
			ev, key, rel, abs, msc, led, snd, ff, sw);
		
		fprintf(fp, " %s\n", module);
		
		free(ev);
		free(key);
		free(rel);
		free(abs);
		free(msc);
		free(led);
		free(snd);
		free(ff);
		free(sw);
		
		free(module);
		free(line);
	}
	
	unmap_file(&inputmap);
	free(filename);
}

void alias_from_ccwmap(FILE *fp, char *prefix) {
	struct filemap_t ccwmap;
	char *line, *nline, *nptr;
	char *token;
	char *module;
	char *filename;
	
	unsigned long match_flags, cu_type, cu_model, dev_type, dev_model;
	
	filename = xmalloc(strlen(prefix) + strlen("modules.ccwmap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.ccwmap");
	
	if (map_file(filename, &ccwmap)) {
		free(filename);
		return;
	}
	
	nptr = ccwmap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		match_flags = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		cu_type = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		cu_model = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		dev_type = strtoul(token, NULL, 0);
		free(token);
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		dev_model = strtoul(token, NULL, 0);
		free(token);
		
		fprintf(fp, "alias ccw:");
		PRINT_WILDCARD_COND("t", "%04lX", cu_type, match_flags & 0x01);
		PRINT_WILDCARD_COND("m", "%02lX", cu_model, match_flags & 0x02);
		PRINT_WILDCARD_COND("dt", "%04lX", dev_type, match_flags & 0x04);
		PRINT_WILDCARD_COND("dm", "%02lX", dev_model, match_flags & 0x08);
		if (match_flags & 0x08)
			fprintf(fp, "*");
		fprintf(fp, " %s\n", module);
		
		free(module);
		free(line);
	}
	
	unmap_file(&ccwmap);
	free(filename);
}

void alias_from_ofmap(FILE *fp, char *prefix) {
	struct filemap_t ofmap;
	char *line, *nline, *nptr;
	char *token;
	char *module;
	char *filename;
	
	char *name, *type, *compatible;
	
	filename = xmalloc(strlen(prefix) + strlen("modules.ofmap") + 2);
	strcpy(filename, prefix);
	strcat(filename, "/modules.ofmap");
	
	if (map_file(filename, &ofmap)) {
		free(filename);
		return;
	}
	
	nptr = ofmap.map;
	
	while ((line = dup_line(nptr, &nptr)) != NULL) {
		nline = line;
		
		module = dup_token(nline, &nline, isspace);
		if (!module || module[0] == '#') 
			JUMP_TO_NEXT;
		
		token = dup_token(nline, &nline, isspace);
		if (!token)
			JUMP_TO_NEXT;
		name = token;
		
		token = dup_token(nline, &nline, isspace);
		if (!token) {
			free(name);
			JUMP_TO_NEXT;
		}
		type = token;
		
		token = dup_token(nline, &nline, isspace);
		if (!token) {
			free(name);
			free(type);
			JUMP_TO_NEXT;
		}
		compatible = token;
		
		fprintf(fp, "alias of:");
		fprintf(fp, "N%sT%sC%s", name, type, compatible);
		fprintf(fp, " %s\n", module);
		
		free(name);
		free(type);
		free(compatible);
		
		free(module);
		free(line);
	}
	
	unmap_file(&ofmap);
	free(filename);
}

int main(int argc, char *argv[]) {
	FILE *fp = NULL;
	char *prefix = NULL;
	int i;
	
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--output") && i < argc - 1 && !fp) {
			i++;
			if (!strcmp(argv[i], "-")) {
				fp = stdout;
			} else {
				fp = fopen(argv[i], "wb");
				if (!fp) {
					fprintf(stderr, "Unable to open output file.\n");
					return 1;
				}
			}
		} else if (!strcmp(argv[i], "--prefix") && i < argc - 1 && !prefix) {
			i++;
			prefix = argv[i];
		}
	}
	
	if (!fp)
		fp = stdout;
	if (!prefix)
		prefix = ".";
	
	alias_from_ofmap(fp, prefix);
	alias_from_ieee1394map(fp, prefix);
	alias_from_ccwmap(fp, prefix);
	alias_from_seriomap(fp, prefix);
	alias_from_ieee1394map(fp, prefix);
	alias_from_pcimap(fp, prefix);
	alias_from_isapnpmap(fp, prefix);
	alias_from_usbmap(fp, prefix);
	alias_from_inputmap(fp, prefix);
	
	if (fp != stdout)
		fclose(fp);
	
	return 0;
}
