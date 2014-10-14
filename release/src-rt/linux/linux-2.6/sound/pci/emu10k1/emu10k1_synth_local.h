#ifndef __EMU10K1_SYNTH_LOCAL_H
#define __EMU10K1_SYNTH_LOCAL_H
/*
 *  Local defininitons for Emu10k1 wavetable
 *
 *  Copyright (C) 2000 Takashi Iwai <tiwai@suse.de>
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

#include <linux/time.h>
#include <sound/core.h>
#include <sound/emu10k1_synth.h>

/* emu10k1_patch.c */
int snd_emu10k1_sample_new(struct snd_emux *private_data,
			   struct snd_sf_sample *sp,
			   struct snd_util_memhdr *hdr,
			   const void __user *_data, long count);
int snd_emu10k1_sample_free(struct snd_emux *private_data,
			    struct snd_sf_sample *sp,
			    struct snd_util_memhdr *hdr);
int snd_emu10k1_memhdr_init(struct snd_emux *emu);

/* emu10k1_callback.c */
void snd_emu10k1_ops_setup(struct snd_emux *emu);
int snd_emu10k1_synth_get_voice(struct snd_emu10k1 *hw);


#endif	/* __EMU10K1_SYNTH_LOCAL_H */
