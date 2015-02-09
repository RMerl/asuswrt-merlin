/* Simple code to turn various tables in an ELF file into alias definitions.
 * This deals with kernel datastructures where they should be
 * dealt with: in the kernel source.
 *
 * Copyright 2002-2003  Rusty Russell, IBM Corporation
 *           2003       Kai Germaschewski
 *
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include "modpost.h"

/* We use the ELF typedefs for kernel_ulong_t but bite the bullet and
 * use either stdint.h or inttypes.h for the rest. */
#if KERNEL_ELFCLASS == ELFCLASS32
typedef Elf32_Addr	kernel_ulong_t;
#define BITS_PER_LONG 32
#else
typedef Elf64_Addr	kernel_ulong_t;
#define BITS_PER_LONG 64
#endif
#ifdef __sun__
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#include <ctype.h>

typedef uint32_t	__u32;
typedef uint16_t	__u16;
typedef unsigned char	__u8;

/* Big exception to the "don't include kernel headers into userspace, which
 * even potentially has different endianness and word sizes, since
 * we handle those differences explicitly below */
#include "../../include/linux/mod_devicetable.h"

#define ADD(str, sep, cond, field)                              \
do {                                                            \
        strcat(str, sep);                                       \
        if (cond)                                               \
                sprintf(str + strlen(str),                      \
                        sizeof(field) == 1 ? "%02X" :           \
                        sizeof(field) == 2 ? "%04X" :           \
                        sizeof(field) == 4 ? "%08X" : "",       \
                        field);                                 \
        else                                                    \
                sprintf(str + strlen(str), "*");                \
} while(0)

/* Always end in a wildcard, for future extension */
static inline void add_wildcard(char *str)
{
	int len = strlen(str);

	if (str[len - 1] != '*')
		strcat(str + len, "*");
}

unsigned int cross_build = 0;
/**
 * Check that sizeof(device_id type) are consistent with size of section
 * in .o file. If in-consistent then userspace and kernel does not agree
 * on actual size which is a bug.
 * Also verify that the final entry in the table is all zeros.
 * Ignore both checks if build host differ from target host and size differs.
 **/
static void device_id_check(const char *modname, const char *device_id,
			    unsigned long size, unsigned long id_size,
			    void *symval)
{
	int i;

	if (size % id_size || size < id_size) {
		if (cross_build != 0)
			return;
		fatal("%s: sizeof(struct %s_device_id)=%lu is not a modulo "
		      "of the size of section __mod_%s_device_table=%lu.\n"
		      "Fix definition of struct %s_device_id "
		      "in mod_devicetable.h\n",
		      modname, device_id, id_size, device_id, size, device_id);
	}
	/* Verify last one is a terminator */
	for (i = 0; i < id_size; i++ ) {
		if (*(uint8_t*)(symval+size-id_size+i)) {
			fprintf(stderr,"%s: struct %s_device_id is %lu bytes.  "
				"The last of %lu is:\n",
				modname, device_id, id_size, size / id_size);
			for (i = 0; i < id_size; i++ )
				fprintf(stderr,"0x%02x ",
					*(uint8_t*)(symval+size-id_size+i) );
			fprintf(stderr,"\n");
			fatal("%s: struct %s_device_id is not terminated "
				"with a NULL entry!\n", modname, device_id);
		}
	}
}

/* USB is special because the bcdDevice can be matched against a numeric range */
/* Looks like "usb:vNpNdNdcNdscNdpNicNiscNipN" */
static void do_usb_entry(struct usb_device_id *id,
			 unsigned int bcdDevice_initial, int bcdDevice_initial_digits,
			 unsigned char range_lo, unsigned char range_hi,
			 unsigned char max, struct module *mod)
{
	char alias[500];
	strcpy(alias, "usb:");
	ADD(alias, "v", id->match_flags&USB_DEVICE_ID_MATCH_VENDOR,
	    id->idVendor);
	ADD(alias, "p", id->match_flags&USB_DEVICE_ID_MATCH_PRODUCT,
	    id->idProduct);

	strcat(alias, "d");
	if (bcdDevice_initial_digits)
		sprintf(alias + strlen(alias), "%0*X",
			bcdDevice_initial_digits, bcdDevice_initial);
	if (range_lo == range_hi)
		sprintf(alias + strlen(alias), "%X", range_lo);
	else if (range_lo > 0 || range_hi < max) {
		if (range_lo > 0x9 || range_hi < 0xA)
			sprintf(alias + strlen(alias),
				"[%X-%X]",
				range_lo,
				range_hi);
		else {
			sprintf(alias + strlen(alias),
				range_lo < 0x9 ? "[%X-9" : "[%X",
				range_lo);
			sprintf(alias + strlen(alias),
				range_hi > 0xA ? "a-%X]" : "%X]",
				range_lo);
		}
	}
	if (bcdDevice_initial_digits < (sizeof(id->bcdDevice_lo) * 2 - 1))
		strcat(alias, "*");

	ADD(alias, "dc", id->match_flags&USB_DEVICE_ID_MATCH_DEV_CLASS,
	    id->bDeviceClass);
	ADD(alias, "dsc",
	    id->match_flags&USB_DEVICE_ID_MATCH_DEV_SUBCLASS,
	    id->bDeviceSubClass);
	ADD(alias, "dp",
	    id->match_flags&USB_DEVICE_ID_MATCH_DEV_PROTOCOL,
	    id->bDeviceProtocol);
	ADD(alias, "ic",
	    id->match_flags&USB_DEVICE_ID_MATCH_INT_CLASS,
	    id->bInterfaceClass);
	ADD(alias, "isc",
	    id->match_flags&USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	    id->bInterfaceSubClass);
	ADD(alias, "ip",
	    id->match_flags&USB_DEVICE_ID_MATCH_INT_PROTOCOL,
	    id->bInterfaceProtocol);

	add_wildcard(alias);
	buf_printf(&mod->dev_table_buf,
		   "MODULE_ALIAS(\"%s\");\n", alias);
}

/* Handles increment/decrement of BCD formatted integers */
/* Returns the previous value, so it works like i++ or i-- */
static unsigned int incbcd(unsigned int *bcd,
			   int inc,
			   unsigned char max,
			   size_t chars)
{
	unsigned int init = *bcd, i, j;
	unsigned long long c, dec = 0;

	/* If bcd is not in BCD format, just increment */
	if (max > 0x9) {
		*bcd += inc;
		return init;
	}

	/* Convert BCD to Decimal */
	for (i=0 ; i < chars ; i++) {
		c = (*bcd >> (i << 2)) & 0xf;
		c = c > 9 ? 9 : c; /* force to bcd just in case */
		for (j=0 ; j < i ; j++)
			c = c * 10;
		dec += c;
	}

	/* Do our increment/decrement */
	dec += inc;
	*bcd  = 0;

	/* Convert back to BCD */
	for (i=0 ; i < chars ; i++) {
		for (c=1,j=0 ; j < i ; j++)
			c = c * 10;
		c = (dec / c) % 10;
		*bcd += c << (i << 2);
	}
	return init;
}

static void do_usb_entry_multi(struct usb_device_id *id, struct module *mod)
{
	unsigned int devlo, devhi;
	unsigned char chi, clo, max;
	int ndigits;

	id->match_flags = TO_NATIVE(id->match_flags);
	id->idVendor = TO_NATIVE(id->idVendor);
	id->idProduct = TO_NATIVE(id->idProduct);

	devlo = id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO ?
		TO_NATIVE(id->bcdDevice_lo) : 0x0U;
	devhi = id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI ?
		TO_NATIVE(id->bcdDevice_hi) : ~0x0U;

	/* Figure out if this entry is in bcd or hex format */
	max = 0x9; /* Default to decimal format */
	for (ndigits = 0 ; ndigits < sizeof(id->bcdDevice_lo) * 2 ; ndigits++) {
		clo = (devlo >> (ndigits << 2)) & 0xf;
		chi = ((devhi > 0x9999 ? 0x9999 : devhi) >> (ndigits << 2)) & 0xf;
		if (clo > max || chi > max) {
			max = 0xf;
			break;
		}
	}

	/*
	 * Some modules (visor) have empty slots as placeholder for
	 * run-time specification that results in catch-all alias
	 */
	if (!(id->idVendor | id->idProduct | id->bDeviceClass | id->bInterfaceClass))
		return;

	/* Convert numeric bcdDevice range into fnmatch-able pattern(s) */
	for (ndigits = sizeof(id->bcdDevice_lo) * 2 - 1; devlo <= devhi; ndigits--) {
		clo = devlo & 0xf;
		chi = devhi & 0xf;
		if (chi > max)	/* If we are in bcd mode, truncate if necessary */
			chi = max;
		devlo >>= 4;
		devhi >>= 4;

		if (devlo == devhi || !ndigits) {
			do_usb_entry(id, devlo, ndigits, clo, chi, max, mod);
			break;
		}

		if (clo > 0x0)
			do_usb_entry(id,
				     incbcd(&devlo, 1, max,
					    sizeof(id->bcdDevice_lo) * 2),
				     ndigits, clo, max, max, mod);

		if (chi < max)
			do_usb_entry(id,
				     incbcd(&devhi, -1, max,
					    sizeof(id->bcdDevice_lo) * 2),
				     ndigits, 0x0, chi, max, mod);
	}
}

static void do_usb_table(void *symval, unsigned long size,
			 struct module *mod)
{
	unsigned int i;
	const unsigned long id_size = sizeof(struct usb_device_id);

	device_id_check(mod->name, "usb", size, id_size, symval);

	/* Leave last one: it's the terminator. */
	size -= id_size;

	for (i = 0; i < size; i += id_size)
		do_usb_entry_multi(symval + i, mod);
}

/* Looks like: hid:bNvNpN */
static int do_hid_entry(const char *filename,
			     struct hid_device_id *id, char *alias)
{
	id->bus = TO_NATIVE(id->bus);
	id->vendor = TO_NATIVE(id->vendor);
	id->product = TO_NATIVE(id->product);

	sprintf(alias, "hid:b%04X", id->bus);
	ADD(alias, "v", id->vendor != HID_ANY_ID, id->vendor);
	ADD(alias, "p", id->product != HID_ANY_ID, id->product);

	return 1;
}

/* Looks like: ieee1394:venNmoNspNverN */
static int do_ieee1394_entry(const char *filename,
			     struct ieee1394_device_id *id, char *alias)
{
	id->match_flags = TO_NATIVE(id->match_flags);
	id->vendor_id = TO_NATIVE(id->vendor_id);
	id->model_id = TO_NATIVE(id->model_id);
	id->specifier_id = TO_NATIVE(id->specifier_id);
	id->version = TO_NATIVE(id->version);

	strcpy(alias, "ieee1394:");
	ADD(alias, "ven", id->match_flags & IEEE1394_MATCH_VENDOR_ID,
	    id->vendor_id);
	ADD(alias, "mo", id->match_flags & IEEE1394_MATCH_MODEL_ID,
	    id->model_id);
	ADD(alias, "sp", id->match_flags & IEEE1394_MATCH_SPECIFIER_ID,
	    id->specifier_id);
	ADD(alias, "ver", id->match_flags & IEEE1394_MATCH_VERSION,
	    id->version);

	add_wildcard(alias);
	return 1;
}

/* Looks like: pci:vNdNsvNsdNbcNscNiN. */
static int do_pci_entry(const char *filename,
			struct pci_device_id *id, char *alias)
{
	/* Class field can be divided into these three. */
	unsigned char baseclass, subclass, interface,
		baseclass_mask, subclass_mask, interface_mask;

	id->vendor = TO_NATIVE(id->vendor);
	id->device = TO_NATIVE(id->device);
	id->subvendor = TO_NATIVE(id->subvendor);
	id->subdevice = TO_NATIVE(id->subdevice);
	id->class = TO_NATIVE(id->class);
	id->class_mask = TO_NATIVE(id->class_mask);

	strcpy(alias, "pci:");
	ADD(alias, "v", id->vendor != PCI_ANY_ID, id->vendor);
	ADD(alias, "d", id->device != PCI_ANY_ID, id->device);
	ADD(alias, "sv", id->subvendor != PCI_ANY_ID, id->subvendor);
	ADD(alias, "sd", id->subdevice != PCI_ANY_ID, id->subdevice);

	baseclass = (id->class) >> 16;
	baseclass_mask = (id->class_mask) >> 16;
	subclass = (id->class) >> 8;
	subclass_mask = (id->class_mask) >> 8;
	interface = id->class;
	interface_mask = id->class_mask;

	if ((baseclass_mask != 0 && baseclass_mask != 0xFF)
	    || (subclass_mask != 0 && subclass_mask != 0xFF)
	    || (interface_mask != 0 && interface_mask != 0xFF)) {
		warn("Can't handle masks in %s:%04X\n",
		     filename, id->class_mask);
		return 0;
	}

	ADD(alias, "bc", baseclass_mask == 0xFF, baseclass);
	ADD(alias, "sc", subclass_mask == 0xFF, subclass);
	ADD(alias, "i", interface_mask == 0xFF, interface);
	add_wildcard(alias);
	return 1;
}

/* looks like: "ccw:tNmNdtNdmN" */
static int do_ccw_entry(const char *filename,
			struct ccw_device_id *id, char *alias)
{
	id->match_flags = TO_NATIVE(id->match_flags);
	id->cu_type = TO_NATIVE(id->cu_type);
	id->cu_model = TO_NATIVE(id->cu_model);
	id->dev_type = TO_NATIVE(id->dev_type);
	id->dev_model = TO_NATIVE(id->dev_model);

	strcpy(alias, "ccw:");
	ADD(alias, "t", id->match_flags&CCW_DEVICE_ID_MATCH_CU_TYPE,
	    id->cu_type);
	ADD(alias, "m", id->match_flags&CCW_DEVICE_ID_MATCH_CU_MODEL,
	    id->cu_model);
	ADD(alias, "dt", id->match_flags&CCW_DEVICE_ID_MATCH_DEVICE_TYPE,
	    id->dev_type);
	ADD(alias, "dm", id->match_flags&CCW_DEVICE_ID_MATCH_DEVICE_MODEL,
	    id->dev_model);
	add_wildcard(alias);
	return 1;
}

/* looks like: "ap:tN" */
static int do_ap_entry(const char *filename,
		       struct ap_device_id *id, char *alias)
{
	sprintf(alias, "ap:t%02X*", id->dev_type);
	return 1;
}

/* looks like: "css:tN" */
static int do_css_entry(const char *filename,
			struct css_device_id *id, char *alias)
{
	sprintf(alias, "css:t%01X", id->type);
	return 1;
}

/* Looks like: "serio:tyNprNidNexN" */
static int do_serio_entry(const char *filename,
			  struct serio_device_id *id, char *alias)
{
	id->type = TO_NATIVE(id->type);
	id->proto = TO_NATIVE(id->proto);
	id->id = TO_NATIVE(id->id);
	id->extra = TO_NATIVE(id->extra);

	strcpy(alias, "serio:");
	ADD(alias, "ty", id->type != SERIO_ANY, id->type);
	ADD(alias, "pr", id->proto != SERIO_ANY, id->proto);
	ADD(alias, "id", id->id != SERIO_ANY, id->id);
	ADD(alias, "ex", id->extra != SERIO_ANY, id->extra);

	add_wildcard(alias);
	return 1;
}

/* looks like: "acpi:ACPI0003 or acpi:PNP0C0B" or "acpi:LNXVIDEO" */
static int do_acpi_entry(const char *filename,
			struct acpi_device_id *id, char *alias)
{
	sprintf(alias, "acpi*:%s:*", id->id);
	return 1;
}

/* looks like: "pnp:dD" */
static void do_pnp_device_entry(void *symval, unsigned long size,
				struct module *mod)
{
	const unsigned long id_size = sizeof(struct pnp_device_id);
	const unsigned int count = (size / id_size)-1;
	const struct pnp_device_id *devs = symval;
	unsigned int i;

	device_id_check(mod->name, "pnp", size, id_size, symval);

	for (i = 0; i < count; i++) {
		const char *id = (char *)devs[i].id;
		char acpi_id[sizeof(devs[0].id)];
		int j;

		buf_printf(&mod->dev_table_buf,
			   "MODULE_ALIAS(\"pnp:d%s*\");\n", id);

		/* fix broken pnp bus lowercasing */
		for (j = 0; j < sizeof(acpi_id); j++)
			acpi_id[j] = toupper(id[j]);
		buf_printf(&mod->dev_table_buf,
			   "MODULE_ALIAS(\"acpi*:%s:*\");\n", acpi_id);
	}
}

/* looks like: "pnp:dD" for every device of the card */
static void do_pnp_card_entries(void *symval, unsigned long size,
				struct module *mod)
{
	const unsigned long id_size = sizeof(struct pnp_card_device_id);
	const unsigned int count = (size / id_size)-1;
	const struct pnp_card_device_id *cards = symval;
	unsigned int i;

	device_id_check(mod->name, "pnp", size, id_size, symval);

	for (i = 0; i < count; i++) {
		unsigned int j;
		const struct pnp_card_device_id *card = &cards[i];

		for (j = 0; j < PNP_MAX_DEVICES; j++) {
			const char *id = (char *)card->devs[j].id;
			int i2, j2;
			int dup = 0;

			if (!id[0])
				break;

			/* find duplicate, already added value */
			for (i2 = 0; i2 < i && !dup; i2++) {
				const struct pnp_card_device_id *card2 = &cards[i2];

				for (j2 = 0; j2 < PNP_MAX_DEVICES; j2++) {
					const char *id2 = (char *)card2->devs[j2].id;

					if (!id2[0])
						break;

					if (!strcmp(id, id2)) {
						dup = 1;
						break;
					}
				}
			}

			/* add an individual alias for every device entry */
			if (!dup) {
				char acpi_id[sizeof(card->devs[0].id)];
				int k;

				buf_printf(&mod->dev_table_buf,
					   "MODULE_ALIAS(\"pnp:d%s*\");\n", id);

				/* fix broken pnp bus lowercasing */
				for (k = 0; k < sizeof(acpi_id); k++)
					acpi_id[k] = toupper(id[k]);
				buf_printf(&mod->dev_table_buf,
					   "MODULE_ALIAS(\"acpi*:%s:*\");\n", acpi_id);
			}
		}
	}
}

/* Looks like: pcmcia:mNcNfNfnNpfnNvaNvbNvcNvdN. */
static int do_pcmcia_entry(const char *filename,
			   struct pcmcia_device_id *id, char *alias)
{
	unsigned int i;

	id->match_flags = TO_NATIVE(id->match_flags);
	id->manf_id = TO_NATIVE(id->manf_id);
	id->card_id = TO_NATIVE(id->card_id);
	id->func_id = TO_NATIVE(id->func_id);
	id->function = TO_NATIVE(id->function);
	id->device_no = TO_NATIVE(id->device_no);

	for (i=0; i<4; i++) {
		id->prod_id_hash[i] = TO_NATIVE(id->prod_id_hash[i]);
       }

       strcpy(alias, "pcmcia:");
       ADD(alias, "m", id->match_flags & PCMCIA_DEV_ID_MATCH_MANF_ID,
	   id->manf_id);
       ADD(alias, "c", id->match_flags & PCMCIA_DEV_ID_MATCH_CARD_ID,
	   id->card_id);
       ADD(alias, "f", id->match_flags & PCMCIA_DEV_ID_MATCH_FUNC_ID,
	   id->func_id);
       ADD(alias, "fn", id->match_flags & PCMCIA_DEV_ID_MATCH_FUNCTION,
	   id->function);
       ADD(alias, "pfn", id->match_flags & PCMCIA_DEV_ID_MATCH_DEVICE_NO,
	   id->device_no);
       ADD(alias, "pa", id->match_flags & PCMCIA_DEV_ID_MATCH_PROD_ID1, id->prod_id_hash[0]);
       ADD(alias, "pb", id->match_flags & PCMCIA_DEV_ID_MATCH_PROD_ID2, id->prod_id_hash[1]);
       ADD(alias, "pc", id->match_flags & PCMCIA_DEV_ID_MATCH_PROD_ID3, id->prod_id_hash[2]);
       ADD(alias, "pd", id->match_flags & PCMCIA_DEV_ID_MATCH_PROD_ID4, id->prod_id_hash[3]);

	add_wildcard(alias);
       return 1;
}



static int do_of_entry (const char *filename, struct of_device_id *of, char *alias)
{
    int len;
    char *tmp;
    len = sprintf (alias, "of:N%sT%s",
                    of->name[0] ? of->name : "*",
                    of->type[0] ? of->type : "*");

    if (of->compatible[0])
        sprintf (&alias[len], "%sC%s",
                     of->type[0] ? "*" : "",
                     of->compatible);

    /* Replace all whitespace with underscores */
    for (tmp = alias; tmp && *tmp; tmp++)
        if (isspace (*tmp))
            *tmp = '_';

    add_wildcard(alias);
    return 1;
}

static int do_vio_entry(const char *filename, struct vio_device_id *vio,
		char *alias)
{
	char *tmp;

	sprintf(alias, "vio:T%sS%s", vio->type[0] ? vio->type : "*",
			vio->compat[0] ? vio->compat : "*");

	/* Replace all whitespace with underscores */
	for (tmp = alias; tmp && *tmp; tmp++)
		if (isspace (*tmp))
			*tmp = '_';

	add_wildcard(alias);
	return 1;
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static void do_input(char *alias,
		     kernel_ulong_t *arr, unsigned int min, unsigned int max)
{
	unsigned int i;

	for (i = min; i < max; i++)
		if (arr[i / BITS_PER_LONG] & (1L << (i%BITS_PER_LONG)))
			sprintf(alias + strlen(alias), "%X,*", i);
}

/* input:b0v0p0e0-eXkXrXaXmXlXsXfXwX where X is comma-separated %02X. */
static int do_input_entry(const char *filename, struct input_device_id *id,
			  char *alias)
{
	sprintf(alias, "input:");

	ADD(alias, "b", id->flags & INPUT_DEVICE_ID_MATCH_BUS, id->bustype);
	ADD(alias, "v", id->flags & INPUT_DEVICE_ID_MATCH_VENDOR, id->vendor);
	ADD(alias, "p", id->flags & INPUT_DEVICE_ID_MATCH_PRODUCT, id->product);
	ADD(alias, "e", id->flags & INPUT_DEVICE_ID_MATCH_VERSION, id->version);

	sprintf(alias + strlen(alias), "-e*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_EVBIT)
		do_input(alias, id->evbit, 0, INPUT_DEVICE_ID_EV_MAX);
	sprintf(alias + strlen(alias), "k*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_KEYBIT)
		do_input(alias, id->keybit,
			 INPUT_DEVICE_ID_KEY_MIN_INTERESTING,
			 INPUT_DEVICE_ID_KEY_MAX);
	sprintf(alias + strlen(alias), "r*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_RELBIT)
		do_input(alias, id->relbit, 0, INPUT_DEVICE_ID_REL_MAX);
	sprintf(alias + strlen(alias), "a*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_ABSBIT)
		do_input(alias, id->absbit, 0, INPUT_DEVICE_ID_ABS_MAX);
	sprintf(alias + strlen(alias), "m*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_MSCIT)
		do_input(alias, id->mscbit, 0, INPUT_DEVICE_ID_MSC_MAX);
	sprintf(alias + strlen(alias), "l*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_LEDBIT)
		do_input(alias, id->ledbit, 0, INPUT_DEVICE_ID_LED_MAX);
	sprintf(alias + strlen(alias), "s*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_SNDBIT)
		do_input(alias, id->sndbit, 0, INPUT_DEVICE_ID_SND_MAX);
	sprintf(alias + strlen(alias), "f*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_FFBIT)
		do_input(alias, id->ffbit, 0, INPUT_DEVICE_ID_FF_MAX);
	sprintf(alias + strlen(alias), "w*");
	if (id->flags & INPUT_DEVICE_ID_MATCH_SWBIT)
		do_input(alias, id->swbit, 0, INPUT_DEVICE_ID_SW_MAX);
	return 1;
}

static int do_eisa_entry(const char *filename, struct eisa_device_id *eisa,
		char *alias)
{
	if (eisa->sig[0])
		sprintf(alias, EISA_DEVICE_MODALIAS_FMT "*", eisa->sig);
	else
		strcat(alias, "*");
	return 1;
}

/* Looks like: parisc:tNhvNrevNsvN */
static int do_parisc_entry(const char *filename, struct parisc_device_id *id,
		char *alias)
{
	id->hw_type = TO_NATIVE(id->hw_type);
	id->hversion = TO_NATIVE(id->hversion);
	id->hversion_rev = TO_NATIVE(id->hversion_rev);
	id->sversion = TO_NATIVE(id->sversion);

	strcpy(alias, "parisc:");
	ADD(alias, "t", id->hw_type != PA_HWTYPE_ANY_ID, id->hw_type);
	ADD(alias, "hv", id->hversion != PA_HVERSION_ANY_ID, id->hversion);
	ADD(alias, "rev", id->hversion_rev != PA_HVERSION_REV_ANY_ID, id->hversion_rev);
	ADD(alias, "sv", id->sversion != PA_SVERSION_ANY_ID, id->sversion);

	add_wildcard(alias);
	return 1;
}

/* Looks like: sdio:cNvNdN. */
static int do_sdio_entry(const char *filename,
			struct sdio_device_id *id, char *alias)
{
	id->class = TO_NATIVE(id->class);
	id->vendor = TO_NATIVE(id->vendor);
	id->device = TO_NATIVE(id->device);

	strcpy(alias, "sdio:");
	ADD(alias, "c", id->class != (__u8)SDIO_ANY_ID, id->class);
	ADD(alias, "v", id->vendor != (__u16)SDIO_ANY_ID, id->vendor);
	ADD(alias, "d", id->device != (__u16)SDIO_ANY_ID, id->device);
	add_wildcard(alias);
	return 1;
}

/* Looks like: ssb:vNidNrevN. */
static int do_ssb_entry(const char *filename,
			struct ssb_device_id *id, char *alias)
{
	id->vendor = TO_NATIVE(id->vendor);
	id->coreid = TO_NATIVE(id->coreid);
	id->revision = TO_NATIVE(id->revision);

	strcpy(alias, "ssb:");
	ADD(alias, "v", id->vendor != SSB_ANY_VENDOR, id->vendor);
	ADD(alias, "id", id->coreid != SSB_ANY_ID, id->coreid);
	ADD(alias, "rev", id->revision != SSB_ANY_REV, id->revision);
	add_wildcard(alias);
	return 1;
}

/* Looks like: virtio:dNvN */
static int do_virtio_entry(const char *filename, struct virtio_device_id *id,
			   char *alias)
{
	id->device = TO_NATIVE(id->device);
	id->vendor = TO_NATIVE(id->vendor);

	strcpy(alias, "virtio:");
	ADD(alias, "d", id->device != VIRTIO_DEV_ANY_ID, id->device);
	ADD(alias, "v", id->vendor != VIRTIO_DEV_ANY_ID, id->vendor);

	add_wildcard(alias);
	return 1;
}

/* Looks like: i2c:S */
static int do_i2c_entry(const char *filename, struct i2c_device_id *id,
			char *alias)
{
	sprintf(alias, I2C_MODULE_PREFIX "%s", id->name);

	return 1;
}

/* Looks like: spi:S */
static int do_spi_entry(const char *filename, struct spi_device_id *id,
			char *alias)
{
	sprintf(alias, SPI_MODULE_PREFIX "%s", id->name);

	return 1;
}

static const struct dmifield {
	const char *prefix;
	int field;
} dmi_fields[] = {
	{ "bvn", DMI_BIOS_VENDOR },
	{ "bvr", DMI_BIOS_VERSION },
	{ "bd",  DMI_BIOS_DATE },
	{ "svn", DMI_SYS_VENDOR },
	{ "pn",  DMI_PRODUCT_NAME },
	{ "pvr", DMI_PRODUCT_VERSION },
	{ "rvn", DMI_BOARD_VENDOR },
	{ "rn",  DMI_BOARD_NAME },
	{ "rvr", DMI_BOARD_VERSION },
	{ "cvn", DMI_CHASSIS_VENDOR },
	{ "ct",  DMI_CHASSIS_TYPE },
	{ "cvr", DMI_CHASSIS_VERSION },
	{ NULL,  DMI_NONE }
};

static void dmi_ascii_filter(char *d, const char *s)
{
	/* Filter out characters we don't want to see in the modalias string */
	for (; *s; s++)
		if (*s > ' ' && *s < 127 && *s != ':')
			*(d++) = *s;

	*d = 0;
}


static int do_dmi_entry(const char *filename, struct dmi_system_id *id,
			char *alias)
{
	int i, j;

	sprintf(alias, "dmi*");

	for (i = 0; i < ARRAY_SIZE(dmi_fields); i++) {
		for (j = 0; j < 4; j++) {
			if (id->matches[j].slot &&
			    id->matches[j].slot == dmi_fields[i].field) {
				sprintf(alias + strlen(alias), ":%s*",
					dmi_fields[i].prefix);
				dmi_ascii_filter(alias + strlen(alias),
						 id->matches[j].substr);
				strcat(alias, "*");
			}
		}
	}

	strcat(alias, ":");
	return 1;
}

static int do_platform_entry(const char *filename,
			     struct platform_device_id *id, char *alias)
{
	sprintf(alias, PLATFORM_MODULE_PREFIX "%s", id->name);
	return 1;
}

static int do_mdio_entry(const char *filename,
			 struct mdio_device_id *id, char *alias)
{
	int i;

	alias += sprintf(alias, MDIO_MODULE_PREFIX);

	for (i = 0; i < 32; i++) {
		if (!((id->phy_id_mask >> (31-i)) & 1))
			*(alias++) = '?';
		else if ((id->phy_id >> (31-i)) & 1)
			*(alias++) = '1';
		else
			*(alias++) = '0';
	}

	/* Terminate the string */
	*alias = 0;

	return 1;
}

/* Looks like: zorro:iN. */
static int do_zorro_entry(const char *filename, struct zorro_device_id *id,
			  char *alias)
{
	id->id = TO_NATIVE(id->id);
	strcpy(alias, "zorro:");
	ADD(alias, "i", id->id != ZORRO_WILDCARD, id->id);
	return 1;
}

/* looks like: "pnp:dD" */
static int do_isapnp_entry(const char *filename,
			   struct isapnp_device_id *id, char *alias)
{
	sprintf(alias, "pnp:d%c%c%c%x%x%x%x*",
		'A' + ((id->vendor >> 2) & 0x3f) - 1,
		'A' + (((id->vendor & 3) << 3) | ((id->vendor >> 13) & 7)) - 1,
		'A' + ((id->vendor >> 8) & 0x1f) - 1,
		(id->function >> 4) & 0x0f, id->function & 0x0f,
		(id->function >> 12) & 0x0f, (id->function >> 8) & 0x0f);
	return 1;
}

/* Ignore any prefix, eg. some architectures prepend _ */
static inline int sym_is(const char *symbol, const char *name)
{
	const char *match;

	match = strstr(symbol, name);
	if (!match)
		return 0;
	return match[strlen(name)] == '\0';
}

static void do_table(void *symval, unsigned long size,
		     unsigned long id_size,
		     const char *device_id,
		     void *function,
		     struct module *mod)
{
	unsigned int i;
	char alias[500];
	int (*do_entry)(const char *, void *entry, char *alias) = function;

	device_id_check(mod->name, device_id, size, id_size, symval);
	/* Leave last one: it's the terminator. */
	size -= id_size;

	for (i = 0; i < size; i += id_size) {
		if (do_entry(mod->name, symval+i, alias)) {
			buf_printf(&mod->dev_table_buf,
				   "MODULE_ALIAS(\"%s\");\n", alias);
		}
	}
}

/* Create MODULE_ALIAS() statements.
 * At this time, we cannot write the actual output C source yet,
 * so we write into the mod->dev_table_buf buffer. */
void handle_moddevtable(struct module *mod, struct elf_info *info,
			Elf_Sym *sym, const char *symname)
{
	void *symval;
	char *zeros = NULL;

	/* We're looking for a section relative symbol */
	if (!sym->st_shndx || get_secindex(info, sym) >= info->num_sections)
		return;

	/* Handle all-NULL symbols allocated into .bss */
	if (info->sechdrs[get_secindex(info, sym)].sh_type & SHT_NOBITS) {
		zeros = calloc(1, sym->st_size);
		symval = zeros;
	} else {
		symval = (void *)info->hdr
			+ info->sechdrs[get_secindex(info, sym)].sh_offset
			+ sym->st_value;
	}

	if (sym_is(symname, "__mod_pci_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct pci_device_id), "pci",
			 do_pci_entry, mod);
	else if (sym_is(symname, "__mod_usb_device_table"))
		/* special case to handle bcdDevice ranges */
		do_usb_table(symval, sym->st_size, mod);
	else if (sym_is(symname, "__mod_hid_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct hid_device_id), "hid",
			 do_hid_entry, mod);
	else if (sym_is(symname, "__mod_ieee1394_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct ieee1394_device_id), "ieee1394",
			 do_ieee1394_entry, mod);
	else if (sym_is(symname, "__mod_ccw_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct ccw_device_id), "ccw",
			 do_ccw_entry, mod);
	else if (sym_is(symname, "__mod_ap_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct ap_device_id), "ap",
			 do_ap_entry, mod);
	else if (sym_is(symname, "__mod_css_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct css_device_id), "css",
			 do_css_entry, mod);
	else if (sym_is(symname, "__mod_serio_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct serio_device_id), "serio",
			 do_serio_entry, mod);
	else if (sym_is(symname, "__mod_acpi_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct acpi_device_id), "acpi",
			 do_acpi_entry, mod);
	else if (sym_is(symname, "__mod_pnp_device_table"))
		do_pnp_device_entry(symval, sym->st_size, mod);
	else if (sym_is(symname, "__mod_pnp_card_device_table"))
		do_pnp_card_entries(symval, sym->st_size, mod);
	else if (sym_is(symname, "__mod_pcmcia_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct pcmcia_device_id), "pcmcia",
			 do_pcmcia_entry, mod);
        else if (sym_is(symname, "__mod_of_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct of_device_id), "of",
			 do_of_entry, mod);
        else if (sym_is(symname, "__mod_vio_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct vio_device_id), "vio",
			 do_vio_entry, mod);
	else if (sym_is(symname, "__mod_input_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct input_device_id), "input",
			 do_input_entry, mod);
	else if (sym_is(symname, "__mod_eisa_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct eisa_device_id), "eisa",
			 do_eisa_entry, mod);
	else if (sym_is(symname, "__mod_parisc_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct parisc_device_id), "parisc",
			 do_parisc_entry, mod);
	else if (sym_is(symname, "__mod_sdio_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct sdio_device_id), "sdio",
			 do_sdio_entry, mod);
	else if (sym_is(symname, "__mod_ssb_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct ssb_device_id), "ssb",
			 do_ssb_entry, mod);
	else if (sym_is(symname, "__mod_virtio_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct virtio_device_id), "virtio",
			 do_virtio_entry, mod);
	else if (sym_is(symname, "__mod_i2c_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct i2c_device_id), "i2c",
			 do_i2c_entry, mod);
	else if (sym_is(symname, "__mod_spi_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct spi_device_id), "spi",
			 do_spi_entry, mod);
	else if (sym_is(symname, "__mod_dmi_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct dmi_system_id), "dmi",
			 do_dmi_entry, mod);
	else if (sym_is(symname, "__mod_platform_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct platform_device_id), "platform",
			 do_platform_entry, mod);
	else if (sym_is(symname, "__mod_mdio_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct mdio_device_id), "mdio",
			 do_mdio_entry, mod);
	else if (sym_is(symname, "__mod_zorro_device_table"))
		do_table(symval, sym->st_size,
			 sizeof(struct zorro_device_id), "zorro",
			 do_zorro_entry, mod);
	else if (sym_is(symname, "__mod_isapnp_device_table"))
		do_table(symval, sym->st_size,
			sizeof(struct isapnp_device_id), "isa",
			do_isapnp_entry, mod);
	free(zeros);
}

/* Now add out buffered information to the generated C source */
void add_moddevtable(struct buffer *buf, struct module *mod)
{
	buf_printf(buf, "\n");
	buf_write(buf, mod->dev_table_buf.p, mod->dev_table_buf.pos);
	free(mod->dev_table_buf.p);
}
