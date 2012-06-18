/* exif-entry.c
 *
 * Copyright (c) 2001 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

#include <config.h>

#include <libexif/exif-entry.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-utils.h>
#include <libexif/i18n.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct _ExifEntryPrivate
{
	unsigned int ref_count;

	ExifMem *mem;
};

/* This function is hidden in exif-data.c */
ExifLog *exif_data_get_log (ExifData *);

#ifndef NO_VERBOSE_TAG_STRINGS
static void
exif_entry_log (ExifEntry *e, ExifLogCode code, const char *format, ...)
{
	va_list args;
	ExifLog *l = NULL;

	if (e && e->parent && e->parent->parent)
		l = exif_data_get_log (e->parent->parent);
	va_start (args, format);
	exif_logv (l, code, "ExifEntry", format, args);
	va_end (args);
}
#else
#if defined(__STDC_VERSION__) &&  __STDC_VERSION__ >= 199901L
#define exif_entry_log(...) do { } while (0)
#elif defined(__GNUC__)
#define exif_entry_log(x...) do { } while (0)
#else
#define exif_entry_log (void)
#endif
#endif

static void *
exif_entry_alloc (ExifEntry *e, unsigned int i)
{
	void *d;
	ExifLog *l = NULL;

	if (!e || !e->priv || !i) return NULL;

	d = exif_mem_alloc (e->priv->mem, i);
	if (d) return d;

	if (e->parent && e->parent->parent)
		l = exif_data_get_log (e->parent->parent);
	EXIF_LOG_NO_MEMORY (l, "ExifEntry", i);
	return NULL;
}

static void *
exif_entry_realloc (ExifEntry *e, void *d_orig, unsigned int i)
{
	void *d;
	ExifLog *l = NULL;

	if (!e || !e->priv) return NULL;

	if (!i) { exif_mem_free (e->priv->mem, d_orig); return NULL; }

	d = exif_mem_realloc (e->priv->mem, d_orig, i);
	if (d) return d;

	if (e->parent && e->parent->parent)
		l = exif_data_get_log (e->parent->parent);
	EXIF_LOG_NO_MEMORY (l, "ExifEntry", i);
	return NULL;
}

ExifEntry *
exif_entry_new (void)
{
	ExifMem *mem = exif_mem_new_default ();
	ExifEntry *e = exif_entry_new_mem (mem);

	exif_mem_unref (mem);

	return e;
}

ExifEntry *
exif_entry_new_mem (ExifMem *mem)
{
	ExifEntry *e = NULL;

	e = exif_mem_alloc (mem, sizeof (ExifEntry));
	if (!e) return NULL;
	e->priv = exif_mem_alloc (mem, sizeof (ExifEntryPrivate));
	if (!e->priv) { exif_mem_free (mem, e); return NULL; }
	e->priv->ref_count = 1;

	e->priv->mem = mem;
	exif_mem_ref (mem);

	return e;
}

void
exif_entry_ref (ExifEntry *e)
{
	if (!e) return;

	e->priv->ref_count++;
}

void
exif_entry_unref (ExifEntry *e)
{
	if (!e) return;

	e->priv->ref_count--;
	if (!e->priv->ref_count)
		exif_entry_free (e);
}

void
exif_entry_free (ExifEntry *e)
{
	if (!e) return;

	if (e->priv) {
		ExifMem *mem = e->priv->mem;
		if (e->data)
			exif_mem_free (mem, e->data);
		exif_mem_free (mem, e->priv);
		exif_mem_free (mem, e);
		exif_mem_unref (mem);
	}
}

/*! Get a value and convert it to an ExifShort.
 * \bug Not all types are converted that could be converted and no indication
 *      is made when that occurs
 */
static inline ExifShort
exif_get_short_convert (const unsigned char *buf, ExifFormat format,
			ExifByteOrder order)
{
	switch (format) {
	case EXIF_FORMAT_LONG:
		return (ExifShort) exif_get_long (buf, order);
	case EXIF_FORMAT_SLONG:
		return (ExifShort) exif_get_slong (buf, order);
	case EXIF_FORMAT_SHORT:
		return (ExifShort) exif_get_short (buf, order);
	case EXIF_FORMAT_SSHORT:
		return (ExifShort) exif_get_sshort (buf, order);
	case EXIF_FORMAT_BYTE:
	case EXIF_FORMAT_SBYTE:
		return (ExifShort) buf[0];
	default:
		/* Unsupported type */
		return (ExifShort) 0;
	}
}

void
exif_entry_fix (ExifEntry *e)
{
	unsigned int i, newsize;
	unsigned char *newdata;
	ExifByteOrder o;
	ExifRational r;
	ExifSRational sr;

	if (!e || !e->priv) return;

	switch (e->tag) {
	
	/* These tags all need to be of format SHORT. */
	case EXIF_TAG_YCBCR_SUB_SAMPLING:
	case EXIF_TAG_SUBJECT_AREA:
	case EXIF_TAG_COLOR_SPACE:
	case EXIF_TAG_PLANAR_CONFIGURATION:
	case EXIF_TAG_SENSING_METHOD:
	case EXIF_TAG_ORIENTATION:
	case EXIF_TAG_YCBCR_POSITIONING:
	case EXIF_TAG_PHOTOMETRIC_INTERPRETATION:
	case EXIF_TAG_CUSTOM_RENDERED:
	case EXIF_TAG_EXPOSURE_MODE:
	case EXIF_TAG_WHITE_BALANCE:
	case EXIF_TAG_SCENE_CAPTURE_TYPE:
	case EXIF_TAG_GAIN_CONTROL:
	case EXIF_TAG_SATURATION:
	case EXIF_TAG_CONTRAST:
	case EXIF_TAG_SHARPNESS:
	case EXIF_TAG_ISO_SPEED_RATINGS:
		switch (e->format) {
		case EXIF_FORMAT_LONG:
		case EXIF_FORMAT_SLONG:
		case EXIF_FORMAT_BYTE:
		case EXIF_FORMAT_SBYTE:
		case EXIF_FORMAT_SSHORT:
			if (!e->parent || !e->parent->parent) break;
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag '%s' was of format '%s' (which is "
				"against specification) and has been "
				"changed to format '%s'."),
				exif_tag_get_name_in_ifd(e->tag,
							exif_entry_get_ifd(e)),
				exif_format_get_name (e->format),
				exif_format_get_name (EXIF_FORMAT_SHORT));

			o = exif_data_get_byte_order (e->parent->parent);
			newsize = e->components * exif_format_get_size (EXIF_FORMAT_SHORT);
			newdata = exif_entry_alloc (e, newsize);
			if (!newdata) {
				exif_entry_log (e, EXIF_LOG_CODE_NO_MEMORY,
					"Could not allocate %lu byte(s).", (unsigned long)newsize);
				break;
			}

			for (i = 0; i < e->components; i++)
				exif_set_short (
					newdata + i *
					exif_format_get_size (
					 EXIF_FORMAT_SHORT), o,
					 exif_get_short_convert (
					  e->data + i *
					  exif_format_get_size (e->format),
					  e->format, o));

			exif_mem_free (e->priv->mem, e->data);
			e->data = newdata;
			e->size = newsize;
			e->format = EXIF_FORMAT_SHORT;
			break;
		case EXIF_FORMAT_SHORT:
			/* No conversion necessary */
			break;
		default:
			exif_entry_log (e, EXIF_LOG_CODE_CORRUPT_DATA,
				_("Tag '%s' is of format '%s' (which is "
				"against specification) but cannot be changed "
				"to format '%s'."),
				exif_tag_get_name_in_ifd(e->tag,
							exif_entry_get_ifd(e)),
				exif_format_get_name (e->format),
				exif_format_get_name (EXIF_FORMAT_SHORT));
			break;
		}
		break;

	/* All these tags need to be of format 'Rational'. */
	case EXIF_TAG_FNUMBER:
	case EXIF_TAG_APERTURE_VALUE:
	case EXIF_TAG_EXPOSURE_TIME:
	case EXIF_TAG_FOCAL_LENGTH:
		switch (e->format) {
		case EXIF_FORMAT_SRATIONAL:
			if (!e->parent || !e->parent->parent) break;
			o = exif_data_get_byte_order (e->parent->parent);
			for (i = 0; i < e->components; i++) {
				sr = exif_get_srational (e->data + i * 
					exif_format_get_size (
						EXIF_FORMAT_SRATIONAL), o);
				r.numerator = (ExifLong) sr.numerator;
				r.denominator = (ExifLong) sr.denominator;
				exif_set_rational (e->data + i *
					exif_format_get_size (
						EXIF_FORMAT_RATIONAL), o, r);
			}
			e->format = EXIF_FORMAT_RATIONAL;
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag '%s' was of format '%s' (which is "
				"against specification) and has been "
				"changed to format '%s'."),
				exif_tag_get_name_in_ifd(e->tag,
							exif_entry_get_ifd(e)),
				exif_format_get_name (EXIF_FORMAT_SRATIONAL),
				exif_format_get_name (EXIF_FORMAT_RATIONAL));
			break;
		default:
			break;
		}
		break;

	/* All these tags need to be of format 'SRational'. */
	case EXIF_TAG_EXPOSURE_BIAS_VALUE:
	case EXIF_TAG_BRIGHTNESS_VALUE:
	case EXIF_TAG_SHUTTER_SPEED_VALUE:
		switch (e->format) {
		case EXIF_FORMAT_RATIONAL:
			if (!e->parent || !e->parent->parent) break;
			o = exif_data_get_byte_order (e->parent->parent);
			for (i = 0; i < e->components; i++) {
				r = exif_get_rational (e->data + i * 
					exif_format_get_size (
						EXIF_FORMAT_RATIONAL), o);
				sr.numerator = (ExifLong) r.numerator;
				sr.denominator = (ExifLong) r.denominator;
				exif_set_srational (e->data + i *
					exif_format_get_size (
						EXIF_FORMAT_SRATIONAL), o, sr);
			}
			e->format = EXIF_FORMAT_SRATIONAL;
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag '%s' was of format '%s' (which is "
				"against specification) and has been "
				"changed to format '%s'."),
				exif_tag_get_name_in_ifd(e->tag,
							exif_entry_get_ifd(e)),
				exif_format_get_name (EXIF_FORMAT_RATIONAL),
				exif_format_get_name (EXIF_FORMAT_SRATIONAL));
			break;
		default:
			break;
		}
		break;

	case EXIF_TAG_USER_COMMENT:

		/* Format needs to be UNDEFINED. */
		if (e->format != EXIF_FORMAT_UNDEFINED) {
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag 'UserComment' had invalid format '%s'. "
				"Format has been set to 'undefined'."),
				exif_format_get_name (e->format));
			e->format = EXIF_FORMAT_UNDEFINED;
		}

		/* Some packages like Canon ZoomBrowser EX 4.5 store
		   only one zero byte followed by 7 bytes of rubbish */
		if ((e->size >= 8) && (e->data[0] == 0)) {
			memcpy(e->data, "\0\0\0\0\0\0\0\0", 8);
		}

		/* There need to be at least 8 bytes. */
		if (e->size < 8) {
			e->data = exif_entry_realloc (e, e->data, 8 + e->size);
			if (!e->data) {
				e->size = 0;
				e->components = 0;
				return;
			}

			/* Assume ASCII */
			memmove (e->data + 8, e->data, e->size);
			memcpy (e->data, "ASCII\0\0\0", 8);
			e->size += 8;
			e->components += 8;
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag 'UserComment' has been expanded to at "
				"least 8 bytes in order to follow the "
				"specification."));
			break;
		}

		/*
		 * If the first 8 bytes are empty and real data starts
		 * afterwards, let's assume ASCII and claim the 8 first
		 * bytes for the format specifyer.
		 */
		for (i = 0; (i < e->size) && !e->data[i]; i++);
		if (!i) for ( ; (i < e->size) && (e->data[i] == ' '); i++);
		if ((i >= 8) && (i < e->size)) {
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag 'UserComment' is not empty but does not "
				"start with a format identifier. "
				"This has been fixed."));
			memcpy (e->data, "ASCII\0\0\0", 8);
			break;
		}

		/* 
		 * First 8 bytes need to follow the specification. If they don't, 
		 * assume ASCII.
		 */
		if (memcmp (e->data, "ASCII\0\0\0"     , 8) &&
		    memcmp (e->data, "UNICODE\0"       , 8) &&
		    memcmp (e->data, "JIS\0\0\0\0\0"   , 8) &&
		    memcmp (e->data, "\0\0\0\0\0\0\0\0", 8)) {
			e->data = exif_entry_realloc (e, e->data, 8 + e->size);
			if (!e->data) {
				e->size = 0;
				e->components = 0;
				break;
			}

			/* Assume ASCII */
			memmove (e->data + 8, e->data, e->size);
			memcpy (e->data, "ASCII\0\0\0", 8);
			e->size += 8;
			e->components += 8;
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Tag 'UserComment' did not start with a "
				"format identifier. This has been fixed."));
			break;
		}

		break;
	default:
		break;
	}
}

/*! Format the value of an ExifEntry for human display in a generic way.
 * The output is localized. The formatting is independent of the tag number.
 * \pre The buffer at val is entirely cleared to 0. This guarantees that the
 *      resulting string will be NUL terminated.
 * \pre The ExifEntry is already a member of an ExifData.
 * \param[in] e EXIF entry
 * \param[out] val buffer in which to store value
 * \param[in] maxlen one less than the length of the buffer val
 */
static void
exif_entry_format_value(ExifEntry *e, char *val, size_t maxlen)
{
	ExifByte v_byte;
	ExifShort v_short;
	ExifSShort v_sshort;
	ExifLong v_long;
	ExifRational v_rat;
	ExifSRational v_srat;
	ExifSLong v_slong;
	char b[64];
	unsigned int i;
	const ExifByteOrder o = exif_data_get_byte_order (e->parent->parent);

	if (!e->size)
		return;
	switch (e->format) {
	case EXIF_FORMAT_UNDEFINED:
		snprintf (val, maxlen, _("%i bytes undefined data"), e->size);
		break;
	case EXIF_FORMAT_BYTE:
	case EXIF_FORMAT_SBYTE:
		v_byte = e->data[0];
		snprintf (val, maxlen, "0x%02x", v_byte);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_byte = e->data[i];
			snprintf (b, sizeof (b), ", 0x%02x", v_byte);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed)maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_SHORT:
		v_short = exif_get_short (e->data, o);
		snprintf (val, maxlen, "%u", v_short);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_short = exif_get_short (e->data +
				exif_format_get_size (e->format) * i, o);
			snprintf (b, sizeof (b), ", %u", v_short);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed)maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_SSHORT:
		v_sshort = exif_get_sshort (e->data, o);
		snprintf (val, maxlen, "%i", v_sshort);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_sshort = exif_get_short (e->data +
				exif_format_get_size (e->format) *
				i, o);
			snprintf (b, sizeof (b), ", %i", v_sshort);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed)maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_LONG:
		v_long = exif_get_long (e->data, o);
		snprintf (val, maxlen, "%lu", (long int) v_long);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_long = exif_get_long (e->data +
				exif_format_get_size (e->format) *
				i, o);
			snprintf (b, sizeof (b), ", %lu", (long int) v_long);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed)maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_SLONG:
		v_slong = exif_get_slong (e->data, o);
		snprintf (val, maxlen, "%li", (long) v_slong);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_slong = exif_get_slong (e->data +
				exif_format_get_size (e->format) * i, o);
			snprintf (b, sizeof (b), ", %li", (long) v_slong);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed)maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_ASCII:
		strncpy (val, (char *) e->data, MIN (maxlen, e->size));
		break;
	case EXIF_FORMAT_RATIONAL:
		v_rat = exif_get_rational (e->data, o);
		if (v_rat.denominator)
			snprintf (val, maxlen, "%2.2lf",
				  (double) v_rat.numerator /
				  (double) v_rat.denominator);
		else
			snprintf (val, maxlen, "%lu/%lu",
				  (unsigned long) v_rat.numerator,
				  (unsigned long) v_rat.denominator);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_rat = exif_get_rational (
				e->data + 8 * i, o);
			if (v_rat.denominator)
				snprintf (b, sizeof (b), ", %2.2lf",
					  (double) v_rat.numerator /
					  (double) v_rat.denominator);
			else
				snprintf (b, sizeof (b), ", %lu/%lu",
				  (unsigned long) v_rat.numerator,
				  (unsigned long) v_rat.denominator);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed) maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_SRATIONAL:
		v_srat = exif_get_srational (e->data, o);
		if (v_srat.denominator) {
			snprintf (val, maxlen, "%2.2f",
				  (double)v_srat.numerator / v_srat.denominator);
		} else {
			snprintf (val, maxlen, "%li/%li",
				  (long) v_srat.numerator,
				  (long) v_srat.denominator);
		}
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_srat = exif_get_srational (
				e->data + 8 * i, o);
			if (v_srat.denominator)
				snprintf (b, sizeof (b), ", %2.2f",
					  (double)v_srat.numerator / v_srat.denominator);
			else
				snprintf (b, sizeof (b), ", %li/%li",
					  (long) v_srat.numerator,
					  (long) v_srat.denominator);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed) maxlen <= 0) break;
		}
		break;
	case EXIF_FORMAT_DOUBLE:
	case EXIF_FORMAT_FLOAT:
	default:
		snprintf (val, maxlen, _("%i bytes unsupported data type"),
			  e->size);
		break;
	}
}

void
exif_entry_dump (ExifEntry *e, unsigned int indent)
{
	char buf[1024];
	char value[1024];
	unsigned int i;

	for (i = 0; i < 2 * indent; i++)
		buf[i] = ' ';
	buf[i] = '\0';

	if (!e)
		return;

	printf ("%sTag: 0x%x ('%s')\n", buf, e->tag,
		exif_tag_get_name (e->tag));
	printf ("%s  Format: %i ('%s')\n", buf, e->format,
		exif_format_get_name (e->format));
	printf ("%s  Components: %i\n", buf, (int) e->components);
	printf ("%s  Size: %i\n", buf, e->size);
	printf ("%s  Value: %s\n", buf, exif_entry_get_value (e, value, sizeof(value)));
}

#define CF(entry,target,v,maxlen)					\
{									\
	if (entry->format != target) {					\
		exif_entry_log (entry, EXIF_LOG_CODE_CORRUPT_DATA,	\
			_("The tag '%s' contains data of an invalid "	\
			"format ('%s', expected '%s')."),		\
			exif_tag_get_name (entry->tag),			\
			exif_format_get_name (entry->format),		\
			exif_format_get_name (target));			\
		break;							\
	}								\
}

#define CC(entry,target,v,maxlen)					\
{									\
	if (entry->components != target) {				\
		exif_entry_log (entry, EXIF_LOG_CODE_CORRUPT_DATA,	\
			_("The tag '%s' contains an invalid number of "	\
			  "components (%i, expected %i)."),		\
			exif_tag_get_name (entry->tag),		\
			(int) entry->components, (int) target);		\
		break;							\
	}								\
}

static const struct {
	ExifTag tag;
	const char *strings[10];
} list[] = {
#ifndef NO_VERBOSE_TAG_DATA
  { EXIF_TAG_PLANAR_CONFIGURATION,
    { N_("chunky format"), N_("planar format"), NULL}},
  { EXIF_TAG_SENSING_METHOD,
    { "", N_("Not defined"), N_("One-chip color area sensor"),
      N_("Two-chip color area sensor"), N_("Three-chip color area sensor"),
      N_("Color sequential area sensor"), "", N_("Trilinear sensor"),
      N_("Color sequential linear sensor"), NULL}},
  { EXIF_TAG_ORIENTATION,
    { "", N_("top - left"), N_("top - right"), N_("bottom - right"),
      N_("bottom - left"), N_("left - top"), N_("right - top"),
      N_("right - bottom"), N_("left - bottom"), NULL}},
  { EXIF_TAG_YCBCR_POSITIONING,
    { "", N_("centered"), N_("co-sited"), NULL}},
  { EXIF_TAG_PHOTOMETRIC_INTERPRETATION,
    { N_("Reversed mono"), N_("Normal mono"), N_("RGB"), N_("Palette"), "",
      N_("CMYK"), N_("YCbCr"), "", N_("CieLAB"), NULL}},
  { EXIF_TAG_CUSTOM_RENDERED,
    { N_("Normal process"), N_("Custom process"), NULL}},
  { EXIF_TAG_EXPOSURE_MODE,
    { N_("Auto exposure"), N_("Manual exposure"), N_("Auto bracket"), NULL}},
  { EXIF_TAG_WHITE_BALANCE,
    { N_("Auto white balance"), N_("Manual white balance"), NULL}},
  { EXIF_TAG_SCENE_CAPTURE_TYPE,
    { N_("Standard"), N_("Landscape"), N_("Portrait"),
      N_("Night scene"), NULL}},
  { EXIF_TAG_GAIN_CONTROL,
    { N_("Normal"), N_("Low gain up"), N_("High gain up"),
      N_("Low gain down"), N_("High gain down"), NULL}},
  { EXIF_TAG_SATURATION,
    { N_("Normal"), N_("Low saturation"), N_("High saturation"), NULL}},
  { EXIF_TAG_CONTRAST , {N_("Normal"), N_("Soft"), N_("Hard"), NULL}},
  { EXIF_TAG_SHARPNESS, {N_("Normal"), N_("Soft"), N_("Hard"), NULL}},
#endif
  { 0, {NULL}}
};

static const struct {
  ExifTag tag;
  struct {
    int index;
    const char *values[4];
  } elem[25];
} list2[] = {
#ifndef NO_VERBOSE_TAG_DATA
  { EXIF_TAG_METERING_MODE,
    { {  0, {N_("Unknown"), NULL}},
      {  1, {N_("Average"), N_("avg"), NULL}},
      {  2, {N_("Center-Weighted Average"), N_("Center-Weight"), NULL}},
      {  3, {N_("Spot"), NULL}},
      {  4, {N_("Multi Spot"), NULL}},
      {  5, {N_("Pattern"), NULL}},
      {  6, {N_("Partial"), NULL}},
      {255, {N_("Other"), NULL}},
      {  0, {NULL}}}},
  { EXIF_TAG_COMPRESSION,
    { {1, {N_("Uncompressed"), NULL}},
      {5, {N_("LZW compression"), NULL}},
      {6, {N_("JPEG compression"), NULL}},
      {7, {N_("JPEG compression"), NULL}},
      {8, {N_("Deflate/ZIP compression"), NULL}},
      {32773, {N_("PackBits compression"), NULL}},
      {0, {NULL}}}},
  { EXIF_TAG_LIGHT_SOURCE,
    { {  0, {N_("Unknown"), NULL}},
      {  1, {N_("Daylight"), NULL}},
      {  2, {N_("Fluorescent"), NULL}},
      {  3, {N_("Tungsten incandescent light"), N_("Tungsten"), NULL}},
      {  4, {N_("Flash"), NULL}},
      {  9, {N_("Fine weather"), NULL}},
      { 10, {N_("Cloudy weather"), N_("Cloudy"), NULL}},
      { 11, {N_("Shade"), NULL}},
      { 12, {N_("Daylight fluorescent"), NULL}},
      { 13, {N_("Day white fluorescent"), NULL}},
      { 14, {N_("Cool white fluorescent"), NULL}},
      { 15, {N_("White fluorescent"), NULL}},
      { 17, {N_("Standard light A"), NULL}},
      { 18, {N_("Standard light B"), NULL}},
      { 19, {N_("Standard light C"), NULL}},
      { 20, {N_("D55"), NULL}},
      { 21, {N_("D65"), NULL}},
      { 22, {N_("D75"), NULL}},
      { 24, {N_("ISO studio tungsten"),NULL}},
      {255, {N_("Other"), NULL}},
      {  0, {NULL}}}},
  { EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT,
    { {2, {N_("Inch"), N_("in"), NULL}},
      {3, {N_("Centimeter"), N_("cm"), NULL}},
      {0, {NULL}}}},
  { EXIF_TAG_RESOLUTION_UNIT,
    { {2, {N_("Inch"), N_("in"), NULL}},
      {3, {N_("Centimeter"), N_("cm"), NULL}}, 
      {0, {NULL}}}},
  { EXIF_TAG_EXPOSURE_PROGRAM,
    { {0, {N_("Not defined"), NULL}},
      {1, {N_("Manual"), NULL}},
      {2, {N_("Normal program"), N_("Normal"), NULL}},
      {3, {N_("Aperture priority"), N_("Aperture"), NULL}},
      {4, {N_("Shutter priority"),N_("Shutter"), NULL}},
      {5, {N_("Creative program (biased toward depth of field)"),
	   N_("Creative"), NULL}},
      {6, {N_("Creative program (biased toward fast shutter speed)"),
	   N_("Action"), NULL}},
      {7, {N_("Portrait mode (for closeup photos with the background out "
	      "of focus)"), N_("Portrait"), NULL}},
      {8, {N_("Landscape mode (for landscape photos with the background "
	      "in focus)"), N_("Landscape"), NULL}},
      {0, {NULL}}}},
  { EXIF_TAG_FLASH,
    { {0x0000, {N_("Flash did not fire"), N_("no flash"), NULL}},
      {0x0001, {N_("Flash fired"), N_("flash"), N_("Yes"), NULL}},
      {0x0005, {N_("Strobe return light not detected"), N_("W/o strobe"),
		NULL}},
      {0x0007, {N_("Strobe return light detected"), N_("W. strobe"), NULL}},
      {0x0008, {N_("Flash did not fire"), NULL}}, /* Olympus E-330 */
      {0x0009, {N_("Flash fired, compulsory flash mode"), NULL}},
      {0x000d, {N_("Flash fired, compulsory flash mode, return light "
		   "not detected"), NULL}},
      {0x000f, {N_("Flash fired, compulsory flash mode, return light "
		   "detected"), NULL}},
      {0x0010, {N_("Flash did not fire, compulsory flash mode"), NULL}},
      {0x0018, {N_("Flash did not fire, auto mode"), NULL}},
      {0x0019, {N_("Flash fired, auto mode"), NULL}},
      {0x001d, {N_("Flash fired, auto mode, return light not detected"),
		NULL}},
      {0x001f, {N_("Flash fired, auto mode, return light detected"), NULL}},
      {0x0020, {N_("No flash function"),NULL}},
      {0x0041, {N_("Flash fired, red-eye reduction mode"), NULL}},
      {0x0045, {N_("Flash fired, red-eye reduction mode, return light "
		   "not detected"), NULL}},
      {0x0047, {N_("Flash fired, red-eye reduction mode, return light "
		   "detected"), NULL}},
      {0x0049, {N_("Flash fired, compulsory flash mode, red-eye reduction "
		   "mode"), NULL}},
      {0x004d, {N_("Flash fired, compulsory flash mode, red-eye reduction "
		  "mode, return light not detected"), NULL}},
      {0x004f, {N_("Flash fired, compulsory flash mode, red-eye reduction mode, "
		   "return light detected"), NULL}},
      {0x0058, {N_("Flash did not fire, auto mode, red-eye reduction mode"), NULL}},
      {0x0059, {N_("Flash fired, auto mode, red-eye reduction mode"), NULL}},
      {0x005d, {N_("Flash fired, auto mode, return light not detected, "
		   "red-eye reduction mode"), NULL}},
      {0x005f, {N_("Flash fired, auto mode, return light detected, "
		   "red-eye reduction mode"), NULL}},
      {0x0000, {NULL}}}},
  { EXIF_TAG_SUBJECT_DISTANCE_RANGE, 
    { {0, {N_("Unknown"), N_("?"), NULL}},
      {1, {N_("Macro"), NULL}},
      {2, {N_("Close view"), N_("Close"), NULL}},
      {3, {N_("Distant view"), N_("Distant"), NULL}},
      {0, {NULL}}}},
  { EXIF_TAG_COLOR_SPACE,
    { {1, {N_("sRGB"), NULL}},
      {2, {N_("Adobe RGB"), NULL}},
      {0xffff, {N_("Uncalibrated"), NULL}},
      {0x0000, {NULL}}}},
#endif
  {0, { { 0, {NULL}}} }
};

const char *
exif_entry_get_value (ExifEntry *e, char *val, unsigned int maxlen)
{
	unsigned int i, j, k;
	const unsigned char *t;
	ExifShort v_short, v_short2, v_short3, v_short4;
	ExifByte v_byte;
	ExifRational v_rat;
	ExifSRational v_srat;
	char b[64];
	const char *c;
	ExifByteOrder o;
	double d;
	ExifEntry *entry;
	static const struct {
		char label[5];
		char major, minor;
	} versions[] = {
		{"0110", 1,  1},
		{"0120", 1,  2},
		{"0200", 2,  0},
		{"0210", 2,  1},
		{"0220", 2,  2},
		{"0221", 2, 21},
		{""    , 0,  0}
	};

	/* FIXME: This belongs to somewhere else. */
	/* libexif should use the default system locale.
	 * If an application specifically requires UTF-8, then we
	 * must give the application a way to tell libexif that.
	 * 
	 * bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	 */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

	/* make sure the returned string is zero terminated */
	memset (val, 0, maxlen);
	maxlen--;
	memset (b, 0, sizeof (b));

	/* We need the byte order */
	if (!e || !e->parent || !e->parent->parent)
		return val;
	o = exif_data_get_byte_order (e->parent->parent);

	/* Sanity check */
	if (e->size != e->components * exif_format_get_size (e->format)) {
		snprintf (val, maxlen, _("Invalid size of entry (%i, "
			"expected %li x %i)."), e->size, e->components,
				exif_format_get_size (e->format));
		return val;
	}

	switch (e->tag) {
	case EXIF_TAG_USER_COMMENT:

		/*
		 * The specification says UNDEFINED, but some
		 * manufacturers don't care and use ASCII. If this is the
		 * case here, only refuse to read it if there is no chance
		 * of finding readable data.
		 */
		if ((e->format != EXIF_FORMAT_ASCII) || 
		    (e->size <= 8) ||
		    ( memcmp (e->data, "ASCII\0\0\0"  , 8) &&
		      memcmp (e->data, "UNICODE\0"    , 8) &&
		      memcmp (e->data, "JIS\0\0\0\0\0", 8) &&
		      memcmp (e->data, "\0\0\0\0\0\0\0\0", 8)))
			CF (e, EXIF_FORMAT_UNDEFINED, val, maxlen);

		/*
		 * Note that, according to the specification (V2.1, p 40),
		 * the user comment field does not have to be 
		 * NULL terminated.
		 */
		if ((e->size >= 8) && !memcmp (e->data, "ASCII\0\0\0", 8)) {
			strncpy (val, (char *) e->data + 8, MIN (e->size - 8, maxlen));
			break;
		}
		if ((e->size >= 8) && !memcmp (e->data, "UNICODE\0", 8)) {
			strncpy (val, _("Unsupported UNICODE string"), maxlen);
		/* FIXME: use iconv to convert into the locale encoding.
		 * EXIF 2.2 implies (but does not say) that this encoding is
		 * UCS-2.
		 */
			break;
		}
		if ((e->size >= 8) && !memcmp (e->data, "JIS\0\0\0\0\0", 8)) {
			strncpy (val, _("Unsupported JIS string"), maxlen);
		/* FIXME: use iconv to convert into the locale encoding */
			break;
		}

		/* Check if there is really some information in the tag. */
		for (i = 0; (i < e->size) &&
			    (!e->data[i] || (e->data[i] == ' ')); i++);
		if (i == e->size) break;

		/*
		 * If we reach this point, the tag does not
		 * comply with the standard and seems to contain data.
		 * Print as much as possible.
		 */
		exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
			_("Tag UserComment does not comply "
			"with standard but contains data."));
		for (; (i < e->size)  && (strlen (val) < maxlen - 1); i++) {
			exif_entry_log (e, EXIF_LOG_CODE_DEBUG,
				_("Byte at position %i: 0x%02x"), i, e->data[i]);
			val[strlen (val)] =
				isprint (e->data[i]) ? e->data[i] : '.';
		}
		break;

	case EXIF_TAG_EXIF_VERSION:
		CF (e, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (e, 4, val, maxlen);
		strncpy (val, _("Unknown Exif Version"), maxlen);
		for (i = 0; *versions[i].label; i++) {
			if (!memcmp (e->data, versions[i].label, 4)) {
    				snprintf (val, maxlen,
					_("Exif Version %d.%d"),
					versions[i].major,
					versions[i].minor);
    				break;
			}
		}
		break;
	case EXIF_TAG_FLASH_PIX_VERSION:
		CF (e, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (e, 4, val, maxlen);
		if (!memcmp (e->data, "0100", 4))
			strncpy (val, _("FlashPix Version 1.0"), maxlen);
		else if (!memcmp (e->data, "0101", 4))
			strncpy (val, _("FlashPix Version 1.01"), maxlen);
		else
			strncpy (val, _("Unknown FlashPix Version"), maxlen);
		break;
	case EXIF_TAG_COPYRIGHT:
		CF (e, EXIF_FORMAT_ASCII, val, maxlen);

		/*
		 * First part: Photographer.
		 * Some cameras store a string like "   " here. Ignore it.
		 */
		if (e->size && e->data &&
		    (strspn ((char *)e->data, " ") != strlen ((char *) e->data)))
			strncpy (val, (char *) e->data, MIN (maxlen, e->size));
		else
			strncpy (val, _("[None]"), maxlen);
		strncat (val, " ", maxlen - strlen (val));
		strncat (val, _("(Photographer)"), maxlen - strlen (val));

		/* Second part: Editor. */
		strncat (val, " - ", maxlen - strlen (val));
		if (e->size && e->data) {
			size_t ts;
			t = e->data + strlen ((char *) e->data) + 1;
			ts = e->data + e->size - t;
			if ((ts > 0) && (strspn ((char *)t, " ") != ts))
				strncat (val, (char *)t, MIN (maxlen - strlen (val), ts));
		} else {
			strncat (val, _("[None]"), maxlen - strlen (val));
		}
		strncat (val, " ", maxlen - strlen (val));
		strncat (val, _("(Editor)"), maxlen - strlen (val));

		break;
	case EXIF_TAG_FNUMBER:
		CF (e, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_rat = exif_get_rational (e->data, o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_rat.numerator / (double) v_rat.denominator;
		snprintf (val, maxlen, "f/%.01lf", d);
		break;
	case EXIF_TAG_APERTURE_VALUE:
	case EXIF_TAG_MAX_APERTURE_VALUE:
		CF (e, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_rat = exif_get_rational (e->data, o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_rat.numerator / (double) v_rat.denominator;
		snprintf (val, maxlen, _("%.02lf EV"), d);
		snprintf (b, sizeof (b), _(" (f/%.01f)"), pow (2, d / 2.));
		if (maxlen > strlen (val) + strlen (b))
			strncat (val, b, maxlen - strlen (val));
		break;
	case EXIF_TAG_FOCAL_LENGTH:
		CF (e, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_rat = exif_get_rational (e->data, o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}

		/*
		 * For calculation of the 35mm equivalent,
		 * Minolta cameras need a multiplier that depends on the
		 * camera model.
		 */
		d = 0.;
		entry = exif_content_get_entry (
			e->parent->parent->ifd[EXIF_IFD_0], EXIF_TAG_MAKE);
		if (entry && entry->data &&
		    !strncmp ((char *)entry->data, "Minolta", 7)) {
			entry = exif_content_get_entry (
					e->parent->parent->ifd[EXIF_IFD_0],
					EXIF_TAG_MODEL);
			if (entry && entry->data) {
				if (!strncmp ((char *)entry->data, "DiMAGE 7", 8))
					d = 3.9;
				else if (!strncmp ((char *)entry->data, "DiMAGE 5", 8))
					d = 4.9;
			}
		}
		if (d)
			snprintf (b, sizeof (b), _(" (35 equivalent: %d mm)"),
				  (int) (d * (double) v_rat.numerator /
				  	     (double) v_rat.denominator));

		d = (double) v_rat.numerator / (double) v_rat.denominator;
		snprintf (val, maxlen, "%.1lf mm", d);
		if (maxlen > strlen (val) + strlen (b))
			strncat (val, b, maxlen - strlen (val));
		break;
	case EXIF_TAG_SUBJECT_DISTANCE:
		CF (e, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_rat = exif_get_rational (e->data, o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_rat.numerator / (double) v_rat.denominator;
		snprintf (val, maxlen, "%.1lf m", d);
		break;
	case EXIF_TAG_EXPOSURE_TIME:
		CF (e, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_rat = exif_get_rational (e->data, o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_rat.numerator / (double) v_rat.denominator;
		if (d < 1)
			snprintf (val, maxlen, _("1/%i"), (int) (0.5 + 1. / d));
		else
			snprintf (val, maxlen, "%i", (int) d);
		if (maxlen > strlen (val) + strlen (_(" sec.")))
			strncat (val, _(" sec."), maxlen - strlen (val));
		break;
	case EXIF_TAG_SHUTTER_SPEED_VALUE:
		CF (e, EXIF_FORMAT_SRATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_srat = exif_get_srational (e->data, o);
		if (!v_srat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_srat.numerator / (double) v_srat.denominator;
		snprintf (val, maxlen, _("%.02f EV"), d);
		d = 1. / pow (2, d);
		if (d < 1)
		  snprintf (b, sizeof (b), _(" (1/%d sec.)"), (int) (1. / d));
		else
		  snprintf (b, sizeof (b), _(" (%d sec.)"), (int) d);
		strncat (val, b, maxlen - strlen (val));
		break;
	case EXIF_TAG_BRIGHTNESS_VALUE:
		CF (e, EXIF_FORMAT_SRATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_srat = exif_get_srational (e->data, o);
		if (!v_srat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_srat.numerator / (double) v_srat.denominator;
		snprintf (val, maxlen, _("%.02f EV"), d);
		snprintf (b, sizeof (b), _(" (%.02f cd/m^2)"),
			1. / (M_PI * 0.3048 * 0.3048) * pow (2, d));
		if (maxlen > strlen (val) + strlen (b))
			strncat (val, b, maxlen - strlen (val));
		break;
	case EXIF_TAG_FILE_SOURCE:
		CF (e, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (e, 1, val, maxlen);
		v_byte = e->data[0];
		if (v_byte == 3)
			strncpy (val, _("DSC"), maxlen);
		else
			snprintf (val, maxlen, _("Internal error (unknown "
				  "value %i)"), v_byte);
		break;
	case EXIF_TAG_COMPONENTS_CONFIGURATION:
		CF (e, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (e, 4, val, maxlen);
		for (i = 0; i < 4; i++) {
			switch (e->data[i]) {
			case 0: c = _("-"); break;
			case 1: c = _("Y"); break;
			case 2: c = _("Cb"); break;
			case 3: c = _("Cr"); break;
			case 4: c = _("R"); break;
			case 5: c = _("G"); break;
			case 6: c = _("B"); break;
			default: c = _("reserved"); break;
			}
			strncat (val, c, maxlen - strlen (val));
			if (i < 3)
				strncat (val, " ", maxlen - strlen (val));
		}
		break;
	case EXIF_TAG_EXPOSURE_BIAS_VALUE:
		CF (e, EXIF_FORMAT_SRATIONAL, val, maxlen);
		CC (e, 1, val, maxlen);
		v_srat = exif_get_srational (e->data, o);
		if (!v_srat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_srat.numerator / (double) v_srat.denominator;
		snprintf (val, maxlen, _("%.02f EV"), d);
		break;
	case EXIF_TAG_SCENE_TYPE:
		CF (e, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (e, 1, val, maxlen);
		v_byte = e->data[0];
		if (v_byte == 1)
			strncpy (val, _("Directly photographed"), maxlen);
		else
			snprintf (val, maxlen, _("Internal error (unknown "
				  "value %i)"), v_byte);
		break;
	case EXIF_TAG_YCBCR_SUB_SAMPLING:
		CF (e, EXIF_FORMAT_SHORT, val, maxlen);
		CC (e, 2, val, maxlen);
		v_short  = exif_get_short (e->data, o);
		v_short2 = exif_get_short (
			e->data + exif_format_get_size (e->format),
			o);
		if ((v_short == 2) && (v_short2 == 1))
			strncpy (val, _("YCbCr4:2:2"), maxlen);
		else if ((v_short == 2) && (v_short2 == 2))
			strncpy (val, _("YCbCr4:2:0"), maxlen);
		else
			snprintf (val, maxlen, "%u, %u", v_short, v_short2);
		break;
	case EXIF_TAG_SUBJECT_AREA:
		CF (e, EXIF_FORMAT_SHORT, val, maxlen);
		switch (e->components) {
		case 2:
			v_short  = exif_get_short (e->data, o);
			v_short2 = exif_get_short (e->data + 2, o);
			snprintf (val, maxlen, "(x,y) = (%i,%i)",
				  v_short, v_short2);
			break;
		case 3:
			v_short  = exif_get_short (e->data, o);
			v_short2 = exif_get_short (e->data + 2, o);
			v_short3 = exif_get_short (e->data + 4, o);
			snprintf (val, maxlen, _("Within distance %i of "
				"(x,y) = (%i,%i)"), v_short3, v_short,
				v_short2);
			break;
		case 4:
			v_short  = exif_get_short (e->data, o);
			v_short2 = exif_get_short (e->data + 2, o);
			v_short3 = exif_get_short (e->data + 4, o);
			v_short4 = exif_get_short (e->data + 6, o);
			snprintf (val, maxlen, _("Within rectangle "
				"(width %i, height %i) around "
				"(x,y) = (%i,%i)"), v_short3, v_short4,
				v_short, v_short2);
			break;
		default:
			snprintf (val, maxlen, _("Unexpected number "
				"of components (%li, expected 2, 3, or 4)."),
				e->components);	
		}
		break;
	case EXIF_TAG_GPS_VERSION_ID:
		/* This is only valid in the GPS IFD */
		CF (e, EXIF_FORMAT_BYTE, val, maxlen);
		CC (e, 4, val, maxlen);
		v_byte = e->data[0];
		snprintf (val, maxlen, "%u", v_byte);
		maxlen -= strlen (val);
		for (i = 1; i < e->components; i++) {
			v_byte = e->data[i];
			snprintf (b, sizeof (b), ".%u", v_byte);
			strncat (val, b, maxlen);
			maxlen -= strlen (b);
			if ((signed)maxlen <= 0) break;
		}
		break;
	case EXIF_TAG_INTEROPERABILITY_VERSION:
	/* a.k.a. case EXIF_TAG_GPS_LATITUDE: */
		/* This tag occurs in EXIF_IFD_INTEROPERABILITY */
		if (e->format == EXIF_FORMAT_UNDEFINED) {
			strncpy (val, (char *) e->data, MIN (maxlen, e->size));
			break;
		}
		/* EXIF_TAG_GPS_LATITUDE is the same numerically as
		 * EXIF_TAG_INTEROPERABILITY_VERSION but in EXIF_IFD_GPS
		 */
		exif_entry_format_value(e, val, maxlen);
		break;
	case EXIF_TAG_GPS_ALTITUDE_REF:
		/* This is only valid in the GPS IFD */
		CF (e, EXIF_FORMAT_BYTE, val, maxlen);
		CC (e, 1, val, maxlen);
		v_byte = e->data[0];
		if (v_byte == 0)
			strncpy (val, _("Sea level"), maxlen);
		else if (v_byte == 1)
			strncpy (val, _("Sea level reference"), maxlen);
		else
			snprintf (val, maxlen, _("Internal error (unknown "
				  "value %i)"), v_byte);
		break;
	case EXIF_TAG_GPS_TIME_STAMP:
		/* This is only valid in the GPS IFD */
		CF (e, EXIF_FORMAT_RATIONAL, val, maxlen);
		CC (e, 3, val, maxlen);

		v_rat  = exif_get_rational (e->data, o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		i = v_rat.numerator / v_rat.denominator;

		v_rat = exif_get_rational (e->data +
					     exif_format_get_size (e->format),
					   o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		j = v_rat.numerator / v_rat.denominator;

		v_rat = exif_get_rational (e->data +
					     2*exif_format_get_size (e->format),
					     o);
		if (!v_rat.denominator) {
			exif_entry_format_value(e, val, maxlen);
			break;
		}
		d = (double) v_rat.numerator / (double) v_rat.denominator;
		snprintf (val, maxlen, "%02u:%02u:%05.2f", i, j, d);
		break;

	case EXIF_TAG_METERING_MODE:
	case EXIF_TAG_COMPRESSION:
	case EXIF_TAG_LIGHT_SOURCE:
	case EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT:
	case EXIF_TAG_RESOLUTION_UNIT:
	case EXIF_TAG_EXPOSURE_PROGRAM:
	case EXIF_TAG_FLASH:
	case EXIF_TAG_SUBJECT_DISTANCE_RANGE:
	case EXIF_TAG_COLOR_SPACE:
		CF (e,EXIF_FORMAT_SHORT, val, maxlen);
		CC (e, 1, val, maxlen);
		v_short = exif_get_short (e->data, o);

		/* Search the tag */
		for (i = 0; list2[i].tag && (list2[i].tag != e->tag); i++);
		if (!list2[i].tag) {
			snprintf (val, maxlen, _("Internal error (unknown "
				  "value %i)"), v_short);
			break;
		}

		/* Find the value */
		for (j = 0; list2[i].elem[j].values[0] &&
			    (list2[i].elem[j].index < v_short); j++);
		if (list2[i].elem[j].index != v_short) {
			snprintf (val, maxlen, _("Internal error (unknown "
				  "value %i)"), v_short);
			break;
		}

		/* Find a short enough value */
		memset (val, 0, maxlen);
		for (k = 0; list2[i].elem[j].values[k]; k++) {
			size_t l = strlen (_(list2[i].elem[j].values[k]));
			if ((maxlen > l) && (strlen (val) < l))
				strncpy (val, _(list2[i].elem[j].values[k]), maxlen);
		}
		if (!val[0]) snprintf (val, maxlen, "%i", v_short);

		break;

	case EXIF_TAG_PLANAR_CONFIGURATION:
	case EXIF_TAG_SENSING_METHOD:
	case EXIF_TAG_ORIENTATION:
	case EXIF_TAG_YCBCR_POSITIONING:
	case EXIF_TAG_PHOTOMETRIC_INTERPRETATION:
	case EXIF_TAG_CUSTOM_RENDERED:
	case EXIF_TAG_EXPOSURE_MODE:
	case EXIF_TAG_WHITE_BALANCE:
	case EXIF_TAG_SCENE_CAPTURE_TYPE:
	case EXIF_TAG_GAIN_CONTROL:
	case EXIF_TAG_SATURATION:
	case EXIF_TAG_CONTRAST:
	case EXIF_TAG_SHARPNESS:
		CF (e, EXIF_FORMAT_SHORT, val, maxlen);
		CC (e, 1, val, maxlen);
		v_short = exif_get_short (e->data, o);

		/* Search the tag */
		for (i = 0; list[i].tag && (list[i].tag != e->tag); i++);
		if (!list[i].tag) {
			snprintf (val, maxlen, _("Internal error (unknown "
				  "value %i)"), v_short);
			break;
		}

		/* Find the value */
		for (j = 0; list[i].strings[j] && (j < v_short); j++);
		if (!list[i].strings[j])
			snprintf (val, maxlen, "%i", v_short);
		else if (!*list[i].strings[j])
			snprintf (val, maxlen, _("Unknown value %i"), v_short);
		else
			strncpy (val, _(list[i].strings[j]), maxlen);
		break;

	case EXIF_TAG_XP_TITLE:
	case EXIF_TAG_XP_COMMENT:
	case EXIF_TAG_XP_AUTHOR:
	case EXIF_TAG_XP_KEYWORDS:
	case EXIF_TAG_XP_SUBJECT:
		/* Warning! The texts are converted from UTF16 to UTF8 */
		/* FIXME: use iconv to convert into the locale encoding */
		exif_convert_utf16_to_utf8(val, (unsigned short*)e->data, MIN(maxlen, e->size));
		break;

	default:
		/* Use a generic value formatting */
		exif_entry_format_value(e, val, maxlen);
	}

	return val;
}


/*!
 * \bug Log and report failed exif_mem_malloc() calls.
 */
void
exif_entry_initialize (ExifEntry *e, ExifTag tag)
{
	ExifRational r;
	ExifByteOrder o;

	/* We need the byte order */
	if (!e || !e->parent || e->data || !e->parent->parent)
		return;
	o = exif_data_get_byte_order (e->parent->parent);

	e->tag = tag;
	switch (tag) {

	/* LONG, 1 component, no default */
	case EXIF_TAG_PIXEL_X_DIMENSION:
	case EXIF_TAG_PIXEL_Y_DIMENSION:
	case EXIF_TAG_EXIF_IFD_POINTER:
	case EXIF_TAG_GPS_INFO_IFD_POINTER:
	case EXIF_TAG_INTEROPERABILITY_IFD_POINTER:
	case EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH:
	case EXIF_TAG_JPEG_INTERCHANGE_FORMAT:
		e->components = 1;
		e->format = EXIF_FORMAT_LONG;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		break;

	/* SHORT, 1 component, no default */
	case EXIF_TAG_SUBJECT_LOCATION:
	case EXIF_TAG_SENSING_METHOD:
	case EXIF_TAG_PHOTOMETRIC_INTERPRETATION:
	case EXIF_TAG_COMPRESSION:
	case EXIF_TAG_EXPOSURE_MODE:
	case EXIF_TAG_WHITE_BALANCE:
	case EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM:
	case EXIF_TAG_GAIN_CONTROL:
	case EXIF_TAG_SUBJECT_DISTANCE_RANGE:
	case EXIF_TAG_FLASH:
	case EXIF_TAG_ISO_SPEED_RATINGS:

	/* SHORT, 1 component, default 0 */
	case EXIF_TAG_IMAGE_WIDTH:
	case EXIF_TAG_IMAGE_LENGTH:
	case EXIF_TAG_EXPOSURE_PROGRAM:
	case EXIF_TAG_LIGHT_SOURCE:
	case EXIF_TAG_METERING_MODE:
	case EXIF_TAG_CUSTOM_RENDERED:
	case EXIF_TAG_SCENE_CAPTURE_TYPE:
	case EXIF_TAG_CONTRAST:
	case EXIF_TAG_SATURATION:
	case EXIF_TAG_SHARPNESS:
		e->components = 1;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 0);
		break;

	/* SHORT, 1 component, default 1 */
	case EXIF_TAG_ORIENTATION:
	case EXIF_TAG_PLANAR_CONFIGURATION:
	case EXIF_TAG_YCBCR_POSITIONING:
		e->components = 1;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 1);
		break;

	/* SHORT, 1 component, default 2 */
	case EXIF_TAG_RESOLUTION_UNIT:
	case EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT:
		e->components = 1;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 2);
		break;

	/* SHORT, 1 component, default 3 */
	case EXIF_TAG_SAMPLES_PER_PIXEL:
		e->components = 1;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 3);
		break;

	case EXIF_TAG_COLOR_SPACE:
	/* SHORT, 1 component, default 0xffff */
		e->components = 1;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 0xffff);
		break;

	case EXIF_TAG_BITS_PER_SAMPLE:
		e->components = 3;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 8);
		exif_set_short (
			e->data + exif_format_get_size (e->format),
			o, 8);
		exif_set_short (
			e->data + 2 * exif_format_get_size (e->format),
			o, 8);
		break;

	case EXIF_TAG_YCBCR_SUB_SAMPLING:
		e->components = 2;
		e->format = EXIF_FORMAT_SHORT;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		exif_set_short (e->data, o, 2);
		exif_set_short (
			e->data + exif_format_get_size (e->format),
			o, 1);
		break;

	/* SRATIONAL, 1 component, no default */
	case EXIF_TAG_EXPOSURE_BIAS_VALUE:
	case EXIF_TAG_BRIGHTNESS_VALUE:
	case EXIF_TAG_SHUTTER_SPEED_VALUE:
		e->components = 1;
		e->format = EXIF_FORMAT_SRATIONAL;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		break;

	/* RATIONAL, 1 component, no default */
	case EXIF_TAG_EXPOSURE_TIME:
	case EXIF_TAG_FOCAL_PLANE_X_RESOLUTION:
	case EXIF_TAG_FOCAL_PLANE_Y_RESOLUTION:
	case EXIF_TAG_EXPOSURE_INDEX:
	case EXIF_TAG_FLASH_ENERGY:
	case EXIF_TAG_FNUMBER:
	case EXIF_TAG_FOCAL_LENGTH:
	case EXIF_TAG_SUBJECT_DISTANCE:
	case EXIF_TAG_MAX_APERTURE_VALUE:
	case EXIF_TAG_APERTURE_VALUE:
	case EXIF_TAG_COMPRESSED_BITS_PER_PIXEL:
	case EXIF_TAG_PRIMARY_CHROMATICITIES:
	case EXIF_TAG_DIGITAL_ZOOM_RATIO:
		e->components = 1;
		e->format = EXIF_FORMAT_RATIONAL;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		break;

	/* RATIONAL, 1 component, default 72/1 */
	case EXIF_TAG_X_RESOLUTION:
	case EXIF_TAG_Y_RESOLUTION:
		e->components = 1;
		e->format = EXIF_FORMAT_RATIONAL;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		r.numerator = 72;
		r.denominator = 1;
		exif_set_rational (e->data, o, r);
		break;

	/* RATIONAL, 2 components, no default */
	case EXIF_TAG_WHITE_POINT:
		e->components = 2;
		e->format = EXIF_FORMAT_RATIONAL;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		break;

	/* RATIONAL, 6 components */
	case EXIF_TAG_REFERENCE_BLACK_WHITE:
		e->components = 6;
		e->format = EXIF_FORMAT_RATIONAL;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		r.denominator = 1;
		r.numerator = 0;
		exif_set_rational (e->data, o, r);
		r.numerator = 255;
		exif_set_rational (
			e->data + exif_format_get_size (e->format), o, r);
		r.numerator = 0;
		exif_set_rational (
			e->data + 2 * exif_format_get_size (e->format), o, r);
		r.numerator = 255;
		exif_set_rational (
			e->data + 3 * exif_format_get_size (e->format), o, r);
		r.numerator = 0;
		exif_set_rational (
			e->data + 4 * exif_format_get_size (e->format), o, r);
		r.numerator = 255;
		exif_set_rational (
			e->data + 5 * exif_format_get_size (e->format), o, r);
		break;

	/* ASCII, 20 components */
	case EXIF_TAG_DATE_TIME:
	case EXIF_TAG_DATE_TIME_ORIGINAL:
	case EXIF_TAG_DATE_TIME_DIGITIZED:
	{
		time_t t;
#ifdef HAVE_LOCALTIME_R
		struct tm tms;
#endif
		struct tm *tm;

		t = time (NULL);
#ifdef HAVE_LOCALTIME_R
		tm = localtime_r (&t, &tms);
#else
		tm = localtime (&t);
#endif
		e->components = 20;
		e->format = EXIF_FORMAT_ASCII;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		snprintf ((char *) e->data, e->size,
			  "%04i:%02i:%02i %02i:%02i:%02i",
			  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			  tm->tm_hour, tm->tm_min, tm->tm_sec);
		break;
	}

	/* ASCII, no default */
	case EXIF_TAG_SUB_SEC_TIME:
	case EXIF_TAG_SUB_SEC_TIME_ORIGINAL:
	case EXIF_TAG_SUB_SEC_TIME_DIGITIZED:
		e->components = 0;
		e->format = EXIF_FORMAT_ASCII;
		e->size = 0;
		e->data = NULL;
		break;
	case EXIF_TAG_IMAGE_DESCRIPTION:
	case EXIF_TAG_MAKE:
	case EXIF_TAG_MODEL:
	case EXIF_TAG_SOFTWARE:
	case EXIF_TAG_ARTIST:
		e->components = strlen (_("[None]")) + 1;
		e->format = EXIF_FORMAT_ASCII;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		strncpy ((char *)e->data, _("[None]"), e->size);
		break;
	case EXIF_TAG_COPYRIGHT:
		e->components = (strlen (_("[None]")) + 1) * 2;
		e->format = EXIF_FORMAT_ASCII;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		strcpy (((char *)e->data) + 0, _("[None]"));
		strcpy (((char *)e->data) + strlen (_("[None]")) + 1, _("[None]"));
		break;

	/* UNDEFINED, no components, no default */
	/* Use this if the tag is otherwise unsupported */
	case EXIF_TAG_MAKER_NOTE:
	case EXIF_TAG_USER_COMMENT:
	default:
		e->components = 0;
		e->format = EXIF_FORMAT_UNDEFINED;
		e->size = 0;
		e->data = NULL;
		break;

	/* UNDEFINED, 1 component, default 1 */
	case EXIF_TAG_SCENE_TYPE:
		e->components = 1;
		e->format = EXIF_FORMAT_UNDEFINED;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		e->data[0] = 0x01;
		break;

	/* UNDEFINED, 1 component, default 3 */
	case EXIF_TAG_FILE_SOURCE:
		e->components = 1;
		e->format = EXIF_FORMAT_UNDEFINED;
		e->size = exif_format_get_size (e->format) * e->components;
		e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		e->data[0] = 0x03;
		break;

	/* UNDEFINED, 4 components, default 0 1 0 0 */
        case EXIF_TAG_FLASH_PIX_VERSION:
                e->components = 4;
                e->format = EXIF_FORMAT_UNDEFINED;
                e->size = exif_format_get_size (e->format) * e->components;
                e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
                memcpy (e->data, "0100", 4);
                break;

        /* UNDEFINED, 4 components, default 0 2 1 0 */
        case EXIF_TAG_EXIF_VERSION:
                e->components = 4;
                e->format = EXIF_FORMAT_UNDEFINED;
                e->size = exif_format_get_size (e->format) * e->components;
                e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
                memcpy (e->data, "0210", 4);
                break;

        /* UNDEFINED, 4 components, default 1,2,3,0 */
        case EXIF_TAG_COMPONENTS_CONFIGURATION:
                e->components = 4;
                e->format = EXIF_FORMAT_UNDEFINED;
                e->size = exif_format_get_size (e->format) * e->components;
                e->data = exif_entry_alloc (e, e->size);
		if (!e->data) break;
		e->data[0] = 1;
		e->data[1] = 2;
		e->data[2] = 3;
		e->data[3] = 0;
                break;
	}
}
