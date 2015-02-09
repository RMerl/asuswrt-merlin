/*
 * Driver for Digigram pcxhr compatible soundcards
 *
 * hwdep device manager
 *
 * Copyright (c) 2004 by Digigram <alsa@digigram.com>
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

#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <sound/core.h>
#include <sound/hwdep.h>
#include "pcxhr.h"
#include "pcxhr_mixer.h"
#include "pcxhr_hwdep.h"
#include "pcxhr_core.h"
#include "pcxhr_mix22.h"


#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
#if !defined(CONFIG_USE_PCXHRLOADER) && !defined(CONFIG_SND_PCXHR) /* built-in kernel \
	*/
#define SND_PCXHR_FW_LOADER	/* use the standard firmware loader */
#endif
#endif


static int pcxhr_sub_init(struct pcxhr_mgr *mgr);
/*
 * get basic information and init pcxhr card
 */
static int pcxhr_init_board(struct pcxhr_mgr *mgr)
{
	int err;
	struct pcxhr_rmh rmh;
	int card_streams;

	/* calc the number of all streams used */
	if (mgr->mono_capture)
		card_streams = mgr->capture_chips * 2;
	else
		card_streams = mgr->capture_chips;
	card_streams += mgr->playback_chips * PCXHR_PLAYBACK_STREAMS;

	/* enable interrupts */
	pcxhr_enable_dsp(mgr);

	pcxhr_init_rmh(&rmh, CMD_SUPPORTED);
	err = pcxhr_send_msg(mgr, &rmh);
	if (err)
		return err;
	/* test 8 or 12 phys out */
	if ((rmh.stat[0] & MASK_FIRST_FIELD) != mgr->playback_chips * 2)
		return -EINVAL;
	/* test 8 or 2 phys in */
	if (((rmh.stat[0] >> (2 * FIELD_SIZE)) & MASK_FIRST_FIELD) <
	    mgr->capture_chips * 2)
		return -EINVAL;
	/* test max nb substream per board */
	if ((rmh.stat[1] & 0x5F) < card_streams)
		return -EINVAL;
	/* test max nb substream per pipe */
	if (((rmh.stat[1] >> 7) & 0x5F) < PCXHR_PLAYBACK_STREAMS)
		return -EINVAL;
	snd_printdd("supported formats : playback=%x capture=%x\n",
		    rmh.stat[2], rmh.stat[3]);

	pcxhr_init_rmh(&rmh, CMD_VERSION);
	/* firmware num for DSP */
	rmh.cmd[0] |= mgr->firmware_num;
	/* transfer granularity in samples (should be multiple of 48) */
	rmh.cmd[1] = (1<<23) + mgr->granularity;
	rmh.cmd_len = 2;
	err = pcxhr_send_msg(mgr, &rmh);
	if (err)
		return err;
	snd_printdd("PCXHR DSP version is %d.%d.%d\n", (rmh.stat[0]>>16)&0xff,
		    (rmh.stat[0]>>8)&0xff, rmh.stat[0]&0xff);
	mgr->dsp_version = rmh.stat[0];

	if (mgr->is_hr_stereo)
		err = hr222_sub_init(mgr);
	else
		err = pcxhr_sub_init(mgr);
	return err;
}

static int pcxhr_sub_init(struct pcxhr_mgr *mgr)
{
	int err;
	struct pcxhr_rmh rmh;

	/* get options */
	pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_READ);
	rmh.cmd[0] |= IO_NUM_REG_STATUS;
	rmh.cmd[1]  = REG_STATUS_OPTIONS;
	rmh.cmd_len = 2;
	err = pcxhr_send_msg(mgr, &rmh);
	if (err)
		return err;

	if ((rmh.stat[1] & REG_STATUS_OPT_DAUGHTER_MASK) ==
	    REG_STATUS_OPT_ANALOG_BOARD)
		mgr->board_has_analog = 1;	/* analog addon board found */

	/* unmute inputs */
	err = pcxhr_write_io_num_reg_cont(mgr, REG_CONT_UNMUTE_INPUTS,
					  REG_CONT_UNMUTE_INPUTS, NULL);
	if (err)
		return err;
	/* unmute outputs (a write to IO_NUM_REG_MUTE_OUT mutes!) */
	pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_READ);
	rmh.cmd[0] |= IO_NUM_REG_MUTE_OUT;
	if (DSP_EXT_CMD_SET(mgr)) {
		rmh.cmd[1]  = 1;	/* unmute digital plugs */
		rmh.cmd_len = 2;
	}
	err = pcxhr_send_msg(mgr, &rmh);
	return err;
}

void pcxhr_reset_board(struct pcxhr_mgr *mgr)
{
	struct pcxhr_rmh rmh;

	if (mgr->dsp_loaded & (1 << PCXHR_FIRMWARE_DSP_MAIN_INDEX)) {
		/* mute outputs */
	    if (!mgr->is_hr_stereo) {
		/* a read to IO_NUM_REG_MUTE_OUT register unmutes! */
		pcxhr_init_rmh(&rmh, CMD_ACCESS_IO_WRITE);
		rmh.cmd[0] |= IO_NUM_REG_MUTE_OUT;
		pcxhr_send_msg(mgr, &rmh);
		/* mute inputs */
		pcxhr_write_io_num_reg_cont(mgr, REG_CONT_UNMUTE_INPUTS,
					    0, NULL);
	    }
		/* stereo cards mute with reset of dsp */
	}
	/* reset pcxhr dsp */
	if (mgr->dsp_loaded & (1 << PCXHR_FIRMWARE_DSP_EPRM_INDEX))
		pcxhr_reset_dsp(mgr);
	/* reset second xilinx */
	if (mgr->dsp_loaded & (1 << PCXHR_FIRMWARE_XLX_COM_INDEX)) {
		pcxhr_reset_xilinx_com(mgr);
		mgr->dsp_loaded = 1;
	}
	return;
}


/*
 *  allocate a playback/capture pipe (pcmp0/pcmc0)
 */
static int pcxhr_dsp_allocate_pipe(struct pcxhr_mgr *mgr,
				   struct pcxhr_pipe *pipe,
				   int is_capture, int pin)
{
	int stream_count, audio_count;
	int err;
	struct pcxhr_rmh rmh;

	if (is_capture) {
		stream_count = 1;
		if (mgr->mono_capture)
			audio_count = 1;
		else
			audio_count = 2;
	} else {
		stream_count = PCXHR_PLAYBACK_STREAMS;
		audio_count = 2;	/* always stereo */
	}
	snd_printdd("snd_add_ref_pipe pin(%d) pcm%c0\n",
		    pin, is_capture ? 'c' : 'p');
	pipe->is_capture = is_capture;
	pipe->first_audio = pin;
	/* define pipe (P_PCM_ONLY_MASK (0x020000) is not necessary) */
	pcxhr_init_rmh(&rmh, CMD_RES_PIPE);
	pcxhr_set_pipe_cmd_params(&rmh, is_capture, pin,
				  audio_count, stream_count);
	rmh.cmd[1] |= 0x020000; /* add P_PCM_ONLY_MASK */
	if (DSP_EXT_CMD_SET(mgr)) {
		/* add channel mask to command */
	  rmh.cmd[rmh.cmd_len++] = (audio_count == 1) ? 0x01 : 0x03;
	}
	err = pcxhr_send_msg(mgr, &rmh);
	if (err < 0) {
		snd_printk(KERN_ERR "error pipe allocation "
			   "(CMD_RES_PIPE) err=%x!\n", err);
		return err;
	}
	pipe->status = PCXHR_PIPE_DEFINED;

	return 0;
}

/*
 *  free playback/capture pipe (pcmp0/pcmc0)
 */


static int pcxhr_config_pipes(struct pcxhr_mgr *mgr)
{
	int err, i, j;
	struct snd_pcxhr *chip;
	struct pcxhr_pipe *pipe;

	/* allocate the pipes on the dsp */
	for (i = 0; i < mgr->num_cards; i++) {
		chip = mgr->chip[i];
		if (chip->nb_streams_play) {
			pipe = &chip->playback_pipe;
			err = pcxhr_dsp_allocate_pipe( mgr, pipe, 0, i*2);
			if (err)
				return err;
			for(j = 0; j < chip->nb_streams_play; j++)
				chip->playback_stream[j].pipe = pipe;
		}
		for (j = 0; j < chip->nb_streams_capt; j++) {
			pipe = &chip->capture_pipe[j];
			err = pcxhr_dsp_allocate_pipe(mgr, pipe, 1, i*2 + j);
			if (err)
				return err;
			chip->capture_stream[j].pipe = pipe;
		}
	}
	return 0;
}

static int pcxhr_start_pipes(struct pcxhr_mgr *mgr)
{
	int i, j;
	struct snd_pcxhr *chip;
	int playback_mask = 0;
	int capture_mask = 0;

	/* start all the pipes on the dsp */
	for (i = 0; i < mgr->num_cards; i++) {
		chip = mgr->chip[i];
		if (chip->nb_streams_play)
			playback_mask |= 1 << chip->playback_pipe.first_audio;
		for (j = 0; j < chip->nb_streams_capt; j++)
			capture_mask |= 1 << chip->capture_pipe[j].first_audio;
	}
	return pcxhr_set_pipe_state(mgr, playback_mask, capture_mask, 1);
}


static int pcxhr_dsp_load(struct pcxhr_mgr *mgr, int index,
			  const struct firmware *dsp)
{
	int err, card_index;

	snd_printdd("loading dsp [%d] size = %Zd\n", index, dsp->size);

	switch (index) {
	case PCXHR_FIRMWARE_XLX_INT_INDEX:
		pcxhr_reset_xilinx_com(mgr);
		return pcxhr_load_xilinx_binary(mgr, dsp, 0);

	case PCXHR_FIRMWARE_XLX_COM_INDEX:
		pcxhr_reset_xilinx_com(mgr);
		return pcxhr_load_xilinx_binary(mgr, dsp, 1);

	case PCXHR_FIRMWARE_DSP_EPRM_INDEX:
		pcxhr_reset_dsp(mgr);
		return pcxhr_load_eeprom_binary(mgr, dsp);

	case PCXHR_FIRMWARE_DSP_BOOT_INDEX:
		return pcxhr_load_boot_binary(mgr, dsp);

	case PCXHR_FIRMWARE_DSP_MAIN_INDEX:
		err = pcxhr_load_dsp_binary(mgr, dsp);
		if (err)
			return err;
		break;	/* continue with first init */
	default:
		snd_printk(KERN_ERR "wrong file index\n");
		return -EFAULT;
	} /* end of switch file index*/

	/* first communication with embedded */
	err = pcxhr_init_board(mgr);
        if (err < 0) {
		snd_printk(KERN_ERR "pcxhr could not be set up\n");
		return err;
	}
	err = pcxhr_config_pipes(mgr);
        if (err < 0) {
		snd_printk(KERN_ERR "pcxhr pipes could not be set up\n");
		return err;
	}
       	/* create devices and mixer in accordance with HW options*/
        for (card_index = 0; card_index < mgr->num_cards; card_index++) {
		struct snd_pcxhr *chip = mgr->chip[card_index];

		if ((err = pcxhr_create_pcm(chip)) < 0)
			return err;

		if (card_index == 0) {
			if ((err = pcxhr_create_mixer(chip->mgr)) < 0)
				return err;
		}
		if ((err = snd_card_register(chip->card)) < 0)
			return err;
	}
	err = pcxhr_start_pipes(mgr);
        if (err < 0) {
		snd_printk(KERN_ERR "pcxhr pipes could not be started\n");
		return err;
	}
	snd_printdd("pcxhr firmware downloaded and successfully set up\n");

	return 0;
}

/*
 * fw loader entry
 */
#ifdef SND_PCXHR_FW_LOADER

int pcxhr_setup_firmware(struct pcxhr_mgr *mgr)
{
	static char *fw_files[][5] = {
	[0] = { "xlxint.dat", "xlxc882hr.dat",
		"dspe882.e56", "dspb882hr.b56", "dspd882.d56" },
	[1] = { "xlxint.dat", "xlxc882e.dat",
		"dspe882.e56", "dspb882e.b56", "dspd882.d56" },
	[2] = { "xlxint.dat", "xlxc1222hr.dat",
		"dspe882.e56", "dspb1222hr.b56", "dspd1222.d56" },
	[3] = { "xlxint.dat", "xlxc1222e.dat",
		"dspe882.e56", "dspb1222e.b56", "dspd1222.d56" },
	[4] = { NULL, "xlxc222.dat",
		"dspe924.e56", "dspb924.b56", "dspd222.d56" },
	[5] = { NULL, "xlxc924.dat",
		"dspe924.e56", "dspb924.b56", "dspd222.d56" },
	};
	char path[32];

	const struct firmware *fw_entry;
	int i, err;
	int fw_set = mgr->fw_file_set;

	for (i = 0; i < 5; i++) {
		if (!fw_files[fw_set][i])
			continue;
		sprintf(path, "pcxhr/%s", fw_files[fw_set][i]);
		if (request_firmware(&fw_entry, path, &mgr->pci->dev)) {
			snd_printk(KERN_ERR "pcxhr: can't load firmware %s\n",
				   path);
			return -ENOENT;
		}
		/* fake hwdep dsp record */
		err = pcxhr_dsp_load(mgr, i, fw_entry);
		release_firmware(fw_entry);
		if (err < 0)
			return err;
		mgr->dsp_loaded |= 1 << i;
	}
	return 0;
}

MODULE_FIRMWARE("pcxhr/xlxint.dat");
MODULE_FIRMWARE("pcxhr/xlxc882hr.dat");
MODULE_FIRMWARE("pcxhr/xlxc882e.dat");
MODULE_FIRMWARE("pcxhr/dspe882.e56");
MODULE_FIRMWARE("pcxhr/dspb882hr.b56");
MODULE_FIRMWARE("pcxhr/dspb882e.b56");
MODULE_FIRMWARE("pcxhr/dspd882.d56");

MODULE_FIRMWARE("pcxhr/xlxc1222hr.dat");
MODULE_FIRMWARE("pcxhr/xlxc1222e.dat");
MODULE_FIRMWARE("pcxhr/dspb1222hr.b56");
MODULE_FIRMWARE("pcxhr/dspb1222e.b56");
MODULE_FIRMWARE("pcxhr/dspd1222.d56");

MODULE_FIRMWARE("pcxhr/xlxc222.dat");
MODULE_FIRMWARE("pcxhr/xlxc924.dat");
MODULE_FIRMWARE("pcxhr/dspe924.e56");
MODULE_FIRMWARE("pcxhr/dspb924.b56");
MODULE_FIRMWARE("pcxhr/dspd222.d56");


#else /* old style firmware loading */

/* pcxhr hwdep interface id string */
#define PCXHR_HWDEP_ID       "pcxhr loader"


static int pcxhr_hwdep_dsp_status(struct snd_hwdep *hw,
				  struct snd_hwdep_dsp_status *info)
{
	struct pcxhr_mgr *mgr = hw->private_data;
	sprintf(info->id, "pcxhr%d", mgr->fw_file_set);
        info->num_dsps = PCXHR_FIRMWARE_FILES_MAX_INDEX;

	if (hw->dsp_loaded & (1 << PCXHR_FIRMWARE_DSP_MAIN_INDEX))
		info->chip_ready = 1;

	info->version = PCXHR_DRIVER_VERSION;
	return 0;
}

static int pcxhr_hwdep_dsp_load(struct snd_hwdep *hw,
				struct snd_hwdep_dsp_image *dsp)
{
	struct pcxhr_mgr *mgr = hw->private_data;
	int err;
	struct firmware fw;

	fw.size = dsp->length;
	fw.data = vmalloc(fw.size);
	if (! fw.data) {
		snd_printk(KERN_ERR "pcxhr: cannot allocate dsp image "
			   "(%lu bytes)\n", (unsigned long)fw.size);
		return -ENOMEM;
	}
	if (copy_from_user((void *)fw.data, dsp->image, dsp->length)) {
		vfree(fw.data);
		return -EFAULT;
	}
	err = pcxhr_dsp_load(mgr, dsp->index, &fw);
	vfree(fw.data);
	if (err < 0)
		return err;
	mgr->dsp_loaded |= 1 << dsp->index;
	return 0;
}

int pcxhr_setup_firmware(struct pcxhr_mgr *mgr)
{
	int err;
	struct snd_hwdep *hw;

	/* only create hwdep interface for first cardX
	 * (see "index" module parameter)
	 */
	err = snd_hwdep_new(mgr->chip[0]->card, PCXHR_HWDEP_ID, 0, &hw);
	if (err < 0)
		return err;

	hw->iface = SNDRV_HWDEP_IFACE_PCXHR;
	hw->private_data = mgr;
	hw->ops.dsp_status = pcxhr_hwdep_dsp_status;
	hw->ops.dsp_load = pcxhr_hwdep_dsp_load;
	hw->exclusive = 1;
	/* stereo cards don't need fw_file_0 -> dsp_loaded = 1 */
	hw->dsp_loaded = mgr->is_hr_stereo ? 1 : 0;
	mgr->dsp_loaded = 0;
	sprintf(hw->name, PCXHR_HWDEP_ID);

	err = snd_card_register(mgr->chip[0]->card);
	if (err < 0)
		return err;
	return 0;
}

#endif /* SND_PCXHR_FW_LOADER */
