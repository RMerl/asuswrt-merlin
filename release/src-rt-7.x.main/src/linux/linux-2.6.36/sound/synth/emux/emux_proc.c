/*
 *  Copyright (C) 2000 Takashi Iwai <tiwai@suse.de>
 *
 *  Proc interface for Emu8k/Emu10k1 WaveTable synth
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/wait.h>
#include <sound/core.h>
#include <sound/emux_synth.h>
#include <sound/info.h>
#include "emux_voice.h"

#ifdef CONFIG_PROC_FS

static void
snd_emux_proc_info_read(struct snd_info_entry *entry, 
			struct snd_info_buffer *buf)
{
	struct snd_emux *emu;
	int i;

	emu = entry->private_data;
	mutex_lock(&emu->register_mutex);
	if (emu->name)
		snd_iprintf(buf, "Device: %s\n", emu->name);
	snd_iprintf(buf, "Ports: %d\n", emu->num_ports);
	snd_iprintf(buf, "Addresses:");
	for (i = 0; i < emu->num_ports; i++)
		snd_iprintf(buf, " %d:%d", emu->client, emu->ports[i]);
	snd_iprintf(buf, "\n");
	snd_iprintf(buf, "Use Counter: %d\n", emu->used);
	snd_iprintf(buf, "Max Voices: %d\n", emu->max_voices);
	snd_iprintf(buf, "Allocated Voices: %d\n", emu->num_voices);
	if (emu->memhdr) {
		snd_iprintf(buf, "Memory Size: %d\n", emu->memhdr->size);
		snd_iprintf(buf, "Memory Available: %d\n", snd_util_mem_avail(emu->memhdr));
		snd_iprintf(buf, "Allocated Blocks: %d\n", emu->memhdr->nblocks);
	} else {
		snd_iprintf(buf, "Memory Size: 0\n");
	}
	if (emu->sflist) {
		mutex_lock(&emu->sflist->presets_mutex);
		snd_iprintf(buf, "SoundFonts: %d\n", emu->sflist->fonts_size);
		snd_iprintf(buf, "Instruments: %d\n", emu->sflist->zone_counter);
		snd_iprintf(buf, "Samples: %d\n", emu->sflist->sample_counter);
		snd_iprintf(buf, "Locked Instruments: %d\n", emu->sflist->zone_locked);
		snd_iprintf(buf, "Locked Samples: %d\n", emu->sflist->sample_locked);
		mutex_unlock(&emu->sflist->presets_mutex);
	}
	mutex_unlock(&emu->register_mutex);
}


void snd_emux_proc_init(struct snd_emux *emu, struct snd_card *card, int device)
{
	struct snd_info_entry *entry;
	char name[64];

	sprintf(name, "wavetableD%d", device);
	entry = snd_info_create_card_entry(card, name, card->proc_root);
	if (entry == NULL)
		return;

	entry->content = SNDRV_INFO_CONTENT_TEXT;
	entry->private_data = emu;
	entry->c.text.read = snd_emux_proc_info_read;
	if (snd_info_register(entry) < 0)
		snd_info_free_entry(entry);
	else
		emu->proc = entry;
}

void snd_emux_proc_free(struct snd_emux *emu)
{
	snd_info_free_entry(emu->proc);
	emu->proc = NULL;
}

#endif /* CONFIG_PROC_FS */
