/* exif-log.c
 *
 * Copyright (c) 2004 Lutz Mueller <lutz@users.sourceforge.net>
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

#include <libexif/exif-log.h>
#include <libexif/i18n.h>

#include <stdlib.h>
#include <string.h>

struct _ExifLog {
	unsigned int ref_count;

	ExifLogFunc func;
	void *data;

	ExifMem *mem;
};

static const struct {
	ExifLogCode code;
	const char *title;
	const char *message;
} codes[] = {
	{ EXIF_LOG_CODE_DEBUG, N_("Debugging information"),
	  N_("Debugging information is available.") },
	{ EXIF_LOG_CODE_NO_MEMORY, N_("Not enough memory"),
	  N_("The system cannot provide enough memory.") },
	{ EXIF_LOG_CODE_CORRUPT_DATA, N_("Corrupt data"),
	  N_("The data provided does not follow the specification.") },
	{ 0, NULL, NULL }
};

const char *
exif_log_code_get_title (ExifLogCode code)
{
	unsigned int i;

	for (i = 0; codes[i].title; i++) if (codes[i].code == code) break;
	return _(codes[i].title);
}

const char *
exif_log_code_get_message (ExifLogCode code)
{
	unsigned int i;

	for (i = 0; codes[i].message; i++) if (codes[i].code == code) break;
	return _(codes[i].message);
}

ExifLog *
exif_log_new_mem (ExifMem *mem)
{
	ExifLog *log;

	log = exif_mem_alloc (mem, sizeof (ExifLog));
	if (!log) return NULL;
	log->ref_count = 1;

	log->mem = mem;
	exif_mem_ref (mem);

	return log;
}

ExifLog *
exif_log_new (void)
{
	ExifMem *mem = exif_mem_new_default ();
	ExifLog *log = exif_log_new_mem (mem);

	exif_mem_unref (mem);

	return log;
}

void
exif_log_ref (ExifLog *log)
{
	if (!log) return;
	log->ref_count++;
}

void
exif_log_unref (ExifLog *log)
{
	if (!log) return;
	if (log->ref_count > 0) log->ref_count--;
	if (!log->ref_count) exif_log_free (log);
}

void
exif_log_free (ExifLog *log)
{
	ExifMem *mem = log ? log->mem : NULL;

	if (!log) return;

	exif_mem_free (mem, log);
	exif_mem_unref (mem);
}

void
exif_log_set_func (ExifLog *log, ExifLogFunc func, void *data)
{
	if (!log) return;
	log->func = func;
	log->data = data;
}

#ifdef NO_VERBOSE_TAG_STRINGS
/* exif_log forms part of the API and can't be commented away */
#undef exif_log
#endif
void
exif_log (ExifLog *log, ExifLogCode code, const char *domain,
	  const char *format, ...)
{
	va_list args;

	va_start (args, format);
	exif_logv (log, code, domain, format, args);
	va_end (args);
}

void
exif_logv (ExifLog *log, ExifLogCode code, const char *domain,
	   const char *format, va_list args)
{
	if (!log) return;
	if (!log->func) return;
	log->func (log, code, domain, format, args, log->data);
}
