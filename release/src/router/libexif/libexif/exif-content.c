/* exif-content.c
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

#include <libexif/exif-content.h>
#include <libexif/exif-system.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* unused constant
 * static const unsigned char ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
 */

struct _ExifContentPrivate
{
	unsigned int ref_count;

	ExifMem *mem;
	ExifLog *log;
};

ExifContent *
exif_content_new (void)
{
	ExifMem *mem = exif_mem_new_default ();
	ExifContent *content = exif_content_new_mem (mem);

	exif_mem_unref (mem);

	return content;
}

ExifContent *
exif_content_new_mem (ExifMem *mem)
{
	ExifContent *content;

	if (!mem) return NULL;

	content = exif_mem_alloc (mem, (ExifLong) sizeof (ExifContent));
	if (!content)
		return NULL;
	content->priv = exif_mem_alloc (mem,
				(ExifLong) sizeof (ExifContentPrivate));
	if (!content->priv) {
		exif_mem_free (mem, content);
		return NULL;
	}

	content->priv->ref_count = 1;

	content->priv->mem = mem;
	exif_mem_ref (mem);

	return content;
}

void
exif_content_ref (ExifContent *content)
{
	content->priv->ref_count++;
}

void
exif_content_unref (ExifContent *content)
{
	content->priv->ref_count--;
	if (!content->priv->ref_count)
		exif_content_free (content);
}

void
exif_content_free (ExifContent *content)
{
	ExifMem *mem = (content && content->priv) ? content->priv->mem : NULL;
	unsigned int i;

	if (!content) return;

	for (i = 0; i < content->count; i++)
		exif_entry_unref (content->entries[i]);
	exif_mem_free (mem, content->entries);

	if (content->priv) {
		exif_log_unref (content->priv->log);
	}

	exif_mem_free (mem, content->priv);
	exif_mem_free (mem, content);
	exif_mem_unref (mem);
}

void
exif_content_dump (ExifContent *content, unsigned int indent)
{
	char buf[1024];
	unsigned int i;

	for (i = 0; i < 2 * indent; i++)
		buf[i] = ' ';
	buf[i] = '\0';

	if (!content)
		return;

	printf ("%sDumping exif content (%u entries)...\n", buf,
		content->count);
	for (i = 0; i < content->count; i++)
		exif_entry_dump (content->entries[i], indent + 1);
}

void
exif_content_add_entry (ExifContent *c, ExifEntry *entry)
{
	ExifEntry **entries;
	if (!c || !c->priv || !entry || entry->parent) return;

	/* One tag can only be added once to an IFD. */
	if (exif_content_get_entry (c, entry->tag)) {
		exif_log (c->priv->log, EXIF_LOG_CODE_DEBUG, "ExifContent",
			"An attempt has been made to add "
			"the tag '%s' twice to an IFD. This is against "
			"specification.", exif_tag_get_name (entry->tag));
		return;
	}

	entries = exif_mem_realloc (c->priv->mem,
		c->entries, sizeof (ExifEntry*) * (c->count + 1));
	if (!entries) return;
	entry->parent = c;
	entries[c->count++] = entry;
	c->entries = entries;
	exif_entry_ref (entry);
}

void
exif_content_remove_entry (ExifContent *c, ExifEntry *e)
{
	unsigned int i;
	ExifEntry **t, *temp;

	if (!c || !c->priv || !e || (e->parent != c)) return;

	/* Search the entry */
	for (i = 0; i < c->count; i++)
			if (c->entries[i] == e)
					break;

	if (i == c->count)
			return;

	/* Remove the entry */
	temp = c->entries[c->count-1];
	if (c->count > 1) {
		t = exif_mem_realloc (c->priv->mem, c->entries,
					sizeof(ExifEntry*) * (c->count - 1));
		if (!t) {
			return;
		}
		c->entries = t;
		c->count--;
		if (i != c->count) { /* we deallocated the last slot already */ 
			memmove (&t[i], &t[i + 1], sizeof (ExifEntry*) * (c->count - i - 1));
			t[c->count-1] = temp;
		} 
	} else {
		exif_mem_free (c->priv->mem, c->entries);
		c->entries = NULL;
		c->count = 0;
	}
	e->parent = NULL;
	exif_entry_unref (e);
}

ExifEntry *
exif_content_get_entry (ExifContent *content, ExifTag tag)
{
	unsigned int i;

	if (!content)
		return (NULL);

	for (i = 0; i < content->count; i++)
		if (content->entries[i]->tag == tag)
			return (content->entries[i]);
	return (NULL);
}

void
exif_content_foreach_entry (ExifContent *content,
			    ExifContentForeachEntryFunc func, void *data)
{
	unsigned int i;

	if (!content || !func)
		return;

	for (i = 0; i < content->count; i++)
		func (content->entries[i], data);
}

void
exif_content_log (ExifContent *content, ExifLog *log)
{
	if (!content || !content->priv || !log || content->priv->log == log)
		return;

	if (content->priv->log) exif_log_unref (content->priv->log);
	content->priv->log = log;
	exif_log_ref (log);
}

ExifIfd
exif_content_get_ifd (ExifContent *c)
{
	if (!c || !c->parent) return EXIF_IFD_COUNT;

	return 
		((c)->parent->ifd[EXIF_IFD_0] == (c)) ? EXIF_IFD_0 :
		((c)->parent->ifd[EXIF_IFD_1] == (c)) ? EXIF_IFD_1 :
		((c)->parent->ifd[EXIF_IFD_EXIF] == (c)) ? EXIF_IFD_EXIF :
		((c)->parent->ifd[EXIF_IFD_GPS] == (c)) ? EXIF_IFD_GPS :
		((c)->parent->ifd[EXIF_IFD_INTEROPERABILITY] == (c)) ? EXIF_IFD_INTEROPERABILITY :
		EXIF_IFD_COUNT;
}

static void
fix_func (ExifEntry *e, void *UNUSED(data))
{
	exif_entry_fix (e);
}

/*!
 * Check if this entry is unknown and if so, delete it.
 * \note Be careful calling this function in a loop. Deleting an entry from
 * an ExifContent changes the index of subsequent entries, as well as the
 * total size of the entries array.
 */
static void
remove_not_recorded (ExifEntry *e, void *UNUSED(data))
{
	ExifIfd ifd = exif_entry_get_ifd(e) ;
	ExifContent *c = e->parent;
	ExifDataType dt = exif_data_get_data_type (c->parent);
	ExifTag t = e->tag;

	if (exif_tag_get_support_level_in_ifd (t, ifd, dt) ==
			 EXIF_SUPPORT_LEVEL_NOT_RECORDED) {
		exif_log (c->priv->log, EXIF_LOG_CODE_DEBUG, "exif-content",
				"Tag 0x%04x is not recorded in IFD '%s' and has therefore been "
				"removed.", t, exif_ifd_get_name (ifd));
		exif_content_remove_entry (c, e);
	}

}

void
exif_content_fix (ExifContent *c)
{
	ExifIfd ifd = exif_content_get_ifd (c);
	ExifDataType dt;
	ExifEntry *e;
	unsigned int i, num;

	if (!c)
		return;

	dt = exif_data_get_data_type (c->parent);

	/*
	 * First of all, fix all existing entries.
	 */
	exif_content_foreach_entry (c, fix_func, NULL);

	/*
	 * Go through each tag and if it's not recorded, remove it. If one
	 * is removed, exif_content_foreach_entry() will skip the next entry,
	 * so if this happens do the loop again from the beginning to ensure
	 * they're all checked. This could be avoided if we stop relying on
	 * exif_content_foreach_entry but loop intelligently here.
	 */
	do {
		num = c->count;
		exif_content_foreach_entry (c, remove_not_recorded, NULL);
	} while (num != c->count);

	/*
	 * Then check for non-existing mandatory tags and create them if needed
	 */
	num = exif_tag_table_count();
	for (i = 0; i < num; ++i) {
		const ExifTag t = exif_tag_table_get_tag (i);
		if (exif_tag_get_support_level_in_ifd (t, ifd, dt) ==
			EXIF_SUPPORT_LEVEL_MANDATORY) {
			if (exif_content_get_entry (c, t))
				/* This tag already exists */
				continue;
			exif_log (c->priv->log, EXIF_LOG_CODE_DEBUG, "exif-content",
					"Tag '%s' is mandatory in IFD '%s' and has therefore been added.",
					exif_tag_get_name_in_ifd (t, ifd), exif_ifd_get_name (ifd));
			e = exif_entry_new ();
			exif_content_add_entry (c, e);
			exif_entry_initialize (e, t);
			exif_entry_unref (e);
		}
	}
}
