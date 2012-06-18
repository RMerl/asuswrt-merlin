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
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/input.h>

#include "mem_utils.h"
#include "hotplug2.h"
#include "hotplug2_utils.h"
#include "parser_utils.h"

#define MODALIAS_MAX_LEN		1024

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

/**
 * Parses a bitmap; output is a list of offsets of bits of a bitmap
 * of arbitrary size that are set to 1.
 *
 * @1 Name of the bitmap parsed
 * @2 The actual bitmap pointer
 * @3 Lower boundary of the bitmap
 * @4 Upper boundary of the bitmap
 *
 * Returns: Newly allocated string containing the offsets
 */
char *bitmap_to_bitstring(char name, unsigned long *bm, unsigned int min_bit, unsigned int max_bit)
{
	char *rv;
        unsigned int i, len = 0, size = 16, srv;
	
	rv = xmalloc(size);
	
        len += snprintf(rv + len, size - len, "%c", name);
	
        for (i = min_bit; i < max_bit; i++) {
		if (TEST_INPUT_BIT(i, bm)) {
			while ((srv = snprintf(rv + len, size - len, "%X,", i)) >= (size - len)) {
				size = size * 2;
				rv = xrealloc(rv, size);
			}
			len += srv;
		}
	}
	
	return rv;
}

/**
 * Reverses the bitmap_to_bitstring function. 
 *
 * @1 Bitstring to be converted
 * @2 Output bitmap
 * @3 Size of the whole bitmap
 *
 * Returns: void
 */
void string_to_bitmap(char *input, unsigned long *bitmap, int bm_len) {
	char *token, *ptr;
	int i = 0;
	
	ptr = input + strlen(input);
	
	while ((token = dup_token_r(ptr, input, &ptr, isspace)) != NULL) {
		bitmap[i] = strtoul(token, NULL, 16);
		free(token);
		i++;
	}
	
	while (i < bm_len)
		bitmap[i++] = 0;
}

#define GET_BITMAP(mapkey, bitmap, name, min) \
	if (TEST_INPUT_BIT(EV_ ## mapkey, ev_bits)) { \
		token = getenv(#mapkey); \
		if (token == NULL) \
			return -1; \
		\
		string_to_bitmap(token, bitmap ## _bits, NBITS(mapkey ## _MAX)); \
	} \
	bitmap = bitmap_to_bitstring(name, bitmap ## _bits, min, mapkey ## _MAX);

/**
 * Creates an input modalias out of preset environmental variables.
 *
 * @1 Pointer to where modalias will be created
 * @2 Maximum size of the modalias
 *
 * Returns: 0 if success, -1 otherwise
 */
int get_input_modalias(char *modalias, int modalias_len) {
	char *product_env;
	char *ptr;
	char *token;
	unsigned int bustype, vendor, product, version;
	
	char *ev, *key, *rel, *abs, *sw, *msc, *led, *snd, *ff;
	
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
	
	product_env = getenv("PRODUCT");
	
	if (product_env == NULL)
		return -1;
	
	/* PRODUCT */
	ptr = strchr(product_env, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;

	bustype = strtoul(product_env, NULL, 16);
	vendor = strtoul(ptr+1, NULL, 16);
	ptr = strchr(ptr+1, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	
	product = strtoul(ptr+1, NULL, 16);
	ptr = strchr(ptr+1, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;

	version = strtoul(ptr+1, NULL, 16);
	
	/* EV */
	token = getenv("EV");
	if (token == NULL)
		return -1;
	
	string_to_bitmap(token, ev_bits, NBITS(EV_MAX));
	
	ev = bitmap_to_bitstring('e', ev_bits, 0, EV_MAX);
	GET_BITMAP(KEY, key, 'k', KEY_MIN_INTERESTING);
	GET_BITMAP(REL, rel, 'r', 0);
	GET_BITMAP(ABS, abs, 'a', 0);
	GET_BITMAP(MSC, msc, 'm', 0);
	GET_BITMAP(LED, led, 'l', 0);
	GET_BITMAP(SND, snd, 's', 0);
	GET_BITMAP(FF, ff, 'f', 0);
	#if defined(SW_MAX) && defined(EV_SW)
	GET_BITMAP(SW, sw, 'w', 0);
	#else
	sw=strdup("");
	#endif
	
	snprintf(modalias, modalias_len, 
		"MODALIAS=input:b%04Xv%04Xp%04Xe%04X-"
		"%s%s%s%s%s%s%s%s%s",
		bustype, vendor, product, version,
		ev, key, rel, abs, msc, led, snd, ff, sw);
	
	/* Ugly but straightforward*/
	free(ev);
	free(key);
	free(rel);
	free(abs);
	free(msc);
	free(led);
	free(snd);
	free(ff);
	free(sw);
	
	return 0;
}
#undef NBITS
#undef TEST_INPUT_BIT

/**
 * Creates a PCI modalias out of preset environmental variables.
 *
 * @1 Pointer to where modalias will be created
 * @2 Maximum size of the modalias
 *
 * Returns: 0 if success, -1 otherwise
 */
int get_pci_modalias(char *modalias, int modalias_len) {
	char *class_env, *id_env, *subsys_env;
	char *ptr;
	unsigned long vendor, device, sub_vendor, sub_device, class_type;
	unsigned char baseclass, subclass, interface;
	
	id_env = getenv("PCI_ID");
	subsys_env = getenv("PCI_SUBSYS_ID");
	class_env = getenv("PCI_CLASS");
	if (id_env == NULL || subsys_env == NULL || class_env == NULL)
		return -1;
	
	if (strlen(id_env) < 9 || strlen(subsys_env) < 9)
		return -1;
	
	/* PCI_ID */
	ptr = strchr(id_env, ':');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	
	vendor = strtoul(id_env, NULL, 16);
	device = strtoul(ptr+1, NULL, 16);
	
	/* PCI_SUBSYS_ID */
	ptr = strchr(subsys_env, ':');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	
	sub_vendor = strtoul(id_env, NULL, 16);
	sub_device = strtoul(ptr+1, NULL, 16);
	
	/* PCI_CLASS */
	class_type = strtoul(class_env, NULL, 16);
	baseclass = (unsigned char)(class_type >> 16);
	subclass = (unsigned char)(class_type >> 8);
	interface = (unsigned char)class_type;
	
	snprintf(modalias, modalias_len, 
		"MODALIAS=pci:v%08lXd%08lXsv%08lXsd%08lXbc%02Xsc%02Xi%02X",
		vendor, device, sub_vendor, sub_device,
		baseclass, subclass, interface);
	
	return 0;
}

/**
 * Creates an IEEE1394 (FireWire) modalias out of preset environmental
 * variables.
 *
 * @1 Pointer to where modalias will be created
 * @2 Maximum size of the modalias
 *
 * Returns: 0 if success, -1 otherwise
 */
int get_ieee1394_modalias(char *modalias, int modalias_len) {
	char *vendor_env, *model_env;
	char *specifier_env, *version_env;
	unsigned long vendor, model;
	unsigned long specifier, version;
	
	vendor_env = getenv("VENDOR_ID");
	model_env = getenv("MODEL_ID");
	specifier_env = getenv("SPECIFIER_ID");
	version_env = getenv("VERSION");
	
	if (vendor_env == NULL || model_env == NULL  || 
		specifier_env == NULL || version_env == NULL)
		return -1;
	
	vendor = strtoul(vendor_env, NULL, 16);
	model = strtoul(model_env, NULL, 16);
	specifier = strtoul(specifier_env, NULL, 16);
	version = strtoul(version_env, NULL, 16);
	
	snprintf(modalias, modalias_len, 
		"MODALIAS=ieee1394:ven%08lXmo%08lXsp%08lXver%08lX",
		vendor, model, specifier, version);
	
	return 0;
}

/**
 * Creates a serio modalias out of preset environmental variables.
 *
 * @1 Pointer to where modalias will be created
 * @2 Maximum size of the modalias
 *
 * Returns: 0 if success, -1 otherwise
 */
int get_serio_modalias(char *modalias, int modalias_len) {
	char *serio_type_env, *serio_proto_env;
	char *serio_id_env, *serio_extra_env;
	unsigned int serio_type, serio_proto;
	unsigned int serio_specifier, serio_version;
	
	serio_type_env = getenv("SERIO_TYPE");
	serio_proto_env = getenv("SERIO_PROTO");
	serio_id_env = getenv("SERIO_ID");
	serio_extra_env = getenv("SERIO_EXTRA");
	
	if (serio_type_env == NULL || serio_proto_env == NULL  || 
		serio_id_env == NULL || serio_extra_env == NULL)
		return -1;
	
	serio_type = strtoul(serio_type_env, NULL, 16);
	serio_proto = strtoul(serio_proto_env, NULL, 16);
	serio_specifier = strtoul(serio_id_env, NULL, 16);
	serio_version = strtoul(serio_extra_env, NULL, 16);
	
	snprintf(modalias, modalias_len, 
		"MODALIAS=serio:ty%02Xpr%02Xid%02Xex%02X",
		serio_type, serio_proto, serio_specifier, serio_version);
	
	return 0;
}

/**
 * Creates an USB modalias out of preset environmental variables.
 *
 * @1 Pointer to where modalias will be created
 * @2 Maximum size of the modalias
 *
 * Returns: 0 if success, -1 otherwise
 */
int get_usb_modalias(char *modalias, int modalias_len) {
	char *product_env, *type_env, *interface_env;
	char *ptr;
	unsigned int idVendor, idProduct, bcdDevice;
	unsigned int device_class, device_subclass, device_protocol;
	unsigned int interface_class, interface_subclass, interface_protocol;
	
	product_env = getenv("PRODUCT");
	type_env = getenv("TYPE");
	interface_env = getenv("INTERFACE");
	
	if (product_env == NULL || type_env == NULL)
		return -1;
	
	/* PRODUCT */
	ptr = strchr(product_env, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	idVendor = strtoul(product_env, NULL, 16);
	idProduct = strtoul(ptr+1, NULL, 16);
	ptr = strchr(ptr+1, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	bcdDevice = strtoul(ptr+1, NULL, 16);
	
	/* TYPE */
	ptr = strchr(type_env, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	device_class = strtoul(type_env, NULL, 10);
	device_subclass = strtoul(ptr+1, NULL, 10);
	ptr = strchr(ptr+1, '/');
	if (ptr == NULL || ptr[1] == '\0')
		return -1;
	device_protocol = strtoul(ptr+1, NULL, 10);
	
	/* INTERFACE */
	if (interface_env != NULL) {
		ptr = strchr(interface_env, '/');
		if (ptr == NULL || ptr[1] == '\0')
			return -1;
		interface_class = strtoul(interface_env, NULL, 10);
		interface_subclass = strtoul(ptr+1, NULL, 10);
		ptr = strchr(ptr+1, '/');
		if (ptr == NULL || ptr[1] == '\0')
			return -1;
		interface_protocol = strtoul(ptr+1, NULL, 10);
		
		snprintf(modalias, modalias_len, 
			"MODALIAS=usb:v%04Xp%04Xd%04X"
			"dc%02Xdsc%02Xdp%02Xic%02Xisc%02Xip%02X",
			idVendor, idProduct, bcdDevice, 
			device_class, device_subclass, device_protocol,
			interface_class, interface_subclass, interface_protocol);
	} else {
		snprintf(modalias, modalias_len, 
			"MODALIAS=usb:v%04Xp%04Xd%04X"
			"dc%02Xdsc%02Xdp%02Xic*isc*ip*",
			idVendor, idProduct, bcdDevice, 
			device_class, device_subclass, device_protocol);
	}
	
	return 0;
}

/**
 * Distributes modalias generating according to the bus name.
 *
 * @1 Bus name
 * @2 Pointer to where modalias will be created
 * @3 Maximum size of the modalias
 *
 * Returns: The return value of the subsystem modalias function, or -1 if
 * no match.
 */
int get_modalias(char *bus, char *modalias, int modalias_len) {
	memset(modalias, 0, modalias_len);
	
	if (!strcmp(bus, "pci"))
		return get_pci_modalias(modalias, modalias_len);
	
	if (!strcmp(bus, "usb"))
		return get_usb_modalias(modalias, modalias_len);
	
	if (!strcmp(bus, "ieee1394"))
		return get_ieee1394_modalias(modalias, modalias_len);
	
	if (!strcmp(bus, "serio"))
		return get_serio_modalias(modalias, modalias_len);
	
	if (!strcmp(bus, "input"))
		return get_input_modalias(modalias, modalias_len);

	/* 'ccw' devices do not generate events, we do not need to handle them */
	/* 'of' devices do not generate events either */
	/* 'pnp' devices do generate events, but they lack any device */
	/* description whatsoever. */
	
	return -1;
}

/**
 * Turns all environmental variables as set when invoked by /proc/sys/hotplug
 * into an uevent formatted (thus not null-terminated) string.
 *
 * @1 All environmental variables
 * @2 Bus of the event (as read from argv)
 * @3 Pointer to size of the returned uevent string
 *
 * Returns: Not null terminated uevent string.
 */
inline char *get_uevent_string(char **environ, char *bus, unsigned long *uevent_string_len) {
	char *uevent_string;
	char *tmp;
	char modalias[MODALIAS_MAX_LEN];
	unsigned long offset;
	
	tmp = getenv("ACTION");
	if (tmp == NULL)
		return NULL;
	
	*uevent_string_len = strlen(tmp) + 1;
	offset = *uevent_string_len - 1;
	uevent_string = xmalloc(*uevent_string_len);
	strcpy(uevent_string, tmp);
	uevent_string[offset] = '@';
	offset++;
	
	for (; *environ != NULL; environ++) {
		*uevent_string_len += strlen(*environ) + 1;
		uevent_string = xrealloc(uevent_string, *uevent_string_len);
		strcpy(&uevent_string[offset], *environ);
		offset = *uevent_string_len;
	}
	
	if (getenv("SEQNUM") == NULL) {
		/* 64 + 7 ('SEQNUM=') + 1 ('\0') */
		tmp = xmalloc(72);
		memset(tmp, 0, 72);
		snprintf(tmp, 72, "SEQNUM=%llu", get_kernel_seqnum());
		
		*uevent_string_len += strlen(tmp) + 1;
		uevent_string = xrealloc(uevent_string, *uevent_string_len);
		strcpy(&uevent_string[offset], tmp);
		offset = *uevent_string_len;
		
		free(tmp);
	}
	
	if (getenv("SUBSYSTEM") == NULL) {
		/* 10 ('SUBSYSTEM=') + 1 ('\0') */
		tmp = xmalloc(11 + strlen(bus));
		strcpy(tmp, "SUBSYSTEM=");
		strcat(tmp+10, bus);
		
		*uevent_string_len += strlen(tmp) + 1;
		uevent_string = xrealloc(uevent_string, *uevent_string_len);
		strcpy(&uevent_string[offset], tmp);
		offset = *uevent_string_len;
		
		free(tmp);
	}
	
	/* Only create our own MODALIAS if we do not have one set... */
	if (getenv("MODALIAS") == NULL) {
		if (!get_modalias(bus, modalias, MODALIAS_MAX_LEN)) {
			*uevent_string_len += strlen(modalias) + 1;
			uevent_string = xrealloc(uevent_string, *uevent_string_len);
			strcpy(&uevent_string[offset], modalias);
			offset = *uevent_string_len;
		}
	}
	
	return uevent_string;
}

int main(int argc, char *argv[], char **environ) {
	char *uevent_string;
	unsigned long uevent_string_len;
	int netlink_socket;
	
	if (argv[1] == NULL) {
		ERROR("parsing arguments", "Malformed event arguments (bus missing).");
		return 1;
	}
		
	uevent_string = get_uevent_string(environ, argv[1], &uevent_string_len);
	if (uevent_string == NULL) {
		ERROR("parsing env vars", "Malformed event environmental variables.");
		return 1;
	}
	
	netlink_socket = init_netlink_socket(NETLINK_CONNECT);
	if (netlink_socket == -1) {
		ERROR("netlink init","Unable to open netlink socket.");
		goto exit;
	}
	
	if (send(netlink_socket, uevent_string, uevent_string_len, 0) == -1) {
		ERROR("sending data","send failed: %s.", strerror(errno));
	}
	close(netlink_socket);

exit:
	free(uevent_string);
	
	return 0;
}
