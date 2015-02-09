/*
 * soc-core.c  --  ALSA SoC Audio Layer
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *         with code, comments and ideas from :-
 *         Richard Purdie <richard@openedhand.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  TODO:
 *   o Add hw rules to enforce rates, etc.
 *   o More testing with other codecs/machines.
 *   o Add more codecs and platforms to ensure good API coverage.
 *   o Support TDM on PCM and I2S
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/ac97_codec.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

static DEFINE_MUTEX(pcm_mutex);
static DECLARE_WAIT_QUEUE_HEAD(soc_pm_waitq);

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_root;
#endif

static DEFINE_MUTEX(client_mutex);
static LIST_HEAD(card_list);
static LIST_HEAD(dai_list);
static LIST_HEAD(platform_list);
static LIST_HEAD(codec_list);

static int snd_soc_register_card(struct snd_soc_card *card);
static int snd_soc_unregister_card(struct snd_soc_card *card);

/*
 * This is a timeout to do a DAPM powerdown after a stream is closed().
 * It can be used to eliminate pops between different playback streams, e.g.
 * between two audio tracks.
 */
static int pmdown_time = 5000;
module_param(pmdown_time, int, 0);
MODULE_PARM_DESC(pmdown_time, "DAPM stream powerdown time (msecs)");

/*
 * This function forces any delayed work to be queued and run.
 */
static int run_delayed_work(struct delayed_work *dwork)
{
	int ret;

	/* cancel any work waiting to be queued. */
	ret = cancel_delayed_work(dwork);

	/* if there was any work waiting then we run it now and
	 * wait for it's completion */
	if (ret) {
		schedule_delayed_work(dwork, 0);
		flush_scheduled_work();
	}
	return ret;
}

/* codec register dump */
static ssize_t soc_codec_reg_show(struct snd_soc_codec *codec, char *buf)
{
	int ret, i, step = 1, count = 0;

	if (!codec->reg_cache_size)
		return 0;

	if (codec->reg_cache_step)
		step = codec->reg_cache_step;

	count += sprintf(buf, "%s registers\n", codec->name);
	for (i = 0; i < codec->reg_cache_size; i += step) {
		if (codec->readable_register && !codec->readable_register(i))
			continue;

		count += sprintf(buf + count, "%2x: ", i);
		if (count >= PAGE_SIZE - 1)
			break;

		if (codec->display_register) {
			count += codec->display_register(codec, buf + count,
							 PAGE_SIZE - count, i);
		} else {
			/* If the read fails it's almost certainly due to
			 * the register being volatile and the device being
			 * powered off.
			 */
			ret = codec->read(codec, i);
			if (ret >= 0)
				count += snprintf(buf + count,
						  PAGE_SIZE - count,
						  "%4x", ret);
			else
				count += snprintf(buf + count,
						  PAGE_SIZE - count,
						  "<no data: %d>", ret);
		}

		if (count >= PAGE_SIZE - 1)
			break;

		count += snprintf(buf + count, PAGE_SIZE - count, "\n");
		if (count >= PAGE_SIZE - 1)
			break;
	}

	/* Truncate count; min() would cause a warning */
	if (count >= PAGE_SIZE)
		count = PAGE_SIZE - 1;

	return count;
}
static ssize_t codec_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_device *devdata = dev_get_drvdata(dev);
	return soc_codec_reg_show(devdata->card->codec, buf);
}

static DEVICE_ATTR(codec_reg, 0444, codec_reg_show, NULL);

static ssize_t pmdown_time_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct snd_soc_device *socdev = dev_get_drvdata(dev);
	struct snd_soc_card *card = socdev->card;

	return sprintf(buf, "%ld\n", card->pmdown_time);
}

static ssize_t pmdown_time_set(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct snd_soc_device *socdev = dev_get_drvdata(dev);
	struct snd_soc_card *card = socdev->card;

	strict_strtol(buf, 10, &card->pmdown_time);

	return count;
}

static DEVICE_ATTR(pmdown_time, 0644, pmdown_time_show, pmdown_time_set);

#ifdef CONFIG_DEBUG_FS
static int codec_reg_open_file(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t codec_reg_read_file(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;
	struct snd_soc_codec *codec = file->private_data;
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = soc_codec_reg_show(codec, buf);
	if (ret >= 0)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);
	return ret;
}

static ssize_t codec_reg_write_file(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	char *start = buf;
	unsigned long reg, value;
	int step = 1;
	struct snd_soc_codec *codec = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (codec->reg_cache_step)
		step = codec->reg_cache_step;

	while (*start == ' ')
		start++;
	reg = simple_strtoul(start, &start, 16);
	if ((reg >= codec->reg_cache_size) || (reg % step))
		return -EINVAL;
	while (*start == ' ')
		start++;
	if (strict_strtoul(start, 16, &value))
		return -EINVAL;
	codec->write(codec, reg, value);
	return buf_size;
}

static const struct file_operations codec_reg_fops = {
	.open = codec_reg_open_file,
	.read = codec_reg_read_file,
	.write = codec_reg_write_file,
};

static void soc_init_codec_debugfs(struct snd_soc_codec *codec)
{
	char codec_root[128];

	if (codec->dev)
		snprintf(codec_root, sizeof(codec_root),
			"%s.%s", codec->name, dev_name(codec->dev));
	else
		snprintf(codec_root, sizeof(codec_root),
			"%s", codec->name);

	codec->debugfs_codec_root = debugfs_create_dir(codec_root,
						       debugfs_root);
	if (!codec->debugfs_codec_root) {
		printk(KERN_WARNING
		       "ASoC: Failed to create codec debugfs directory\n");
		return;
	}

	codec->debugfs_reg = debugfs_create_file("codec_reg", 0644,
						 codec->debugfs_codec_root,
						 codec, &codec_reg_fops);
	if (!codec->debugfs_reg)
		printk(KERN_WARNING
		       "ASoC: Failed to create codec register debugfs file\n");

	codec->debugfs_pop_time = debugfs_create_u32("dapm_pop_time", 0644,
						     codec->debugfs_codec_root,
						     &codec->pop_time);
	if (!codec->debugfs_pop_time)
		printk(KERN_WARNING
		       "Failed to create pop time debugfs file\n");

	codec->debugfs_dapm = debugfs_create_dir("dapm",
						 codec->debugfs_codec_root);
	if (!codec->debugfs_dapm)
		printk(KERN_WARNING
		       "Failed to create DAPM debugfs directory\n");

	snd_soc_dapm_debugfs_init(codec);
}

static void soc_cleanup_codec_debugfs(struct snd_soc_codec *codec)
{
	debugfs_remove_recursive(codec->debugfs_codec_root);
}

#else

static inline void soc_init_codec_debugfs(struct snd_soc_codec *codec)
{
}

static inline void soc_cleanup_codec_debugfs(struct snd_soc_codec *codec)
{
}
#endif

#ifdef CONFIG_SND_SOC_AC97_BUS
/* unregister ac97 codec */
static int soc_ac97_dev_unregister(struct snd_soc_codec *codec)
{
	if (codec->ac97->dev.bus)
		device_unregister(&codec->ac97->dev);
	return 0;
}

/* stop no dev release warning */
static void soc_ac97_device_release(struct device *dev){}

/* register ac97 codec to bus */
static int soc_ac97_dev_register(struct snd_soc_codec *codec)
{
	int err;

	codec->ac97->dev.bus = &ac97_bus_type;
	codec->ac97->dev.parent = codec->card->dev;
	codec->ac97->dev.release = soc_ac97_device_release;

	dev_set_name(&codec->ac97->dev, "%d-%d:%s",
		     codec->card->number, 0, codec->name);
	err = device_register(&codec->ac97->dev);
	if (err < 0) {
		snd_printk(KERN_ERR "Can't register ac97 bus\n");
		codec->ac97->dev.bus = NULL;
		return err;
	}
	return 0;
}
#endif

static int soc_pcm_apply_symmetry(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	int ret;

	if (codec_dai->symmetric_rates || cpu_dai->symmetric_rates ||
	    machine->symmetric_rates) {
		dev_dbg(card->dev, "Symmetry forces %dHz rate\n",
			machine->rate);

		ret = snd_pcm_hw_constraint_minmax(substream->runtime,
						   SNDRV_PCM_HW_PARAM_RATE,
						   machine->rate,
						   machine->rate);
		if (ret < 0) {
			dev_err(card->dev,
				"Unable to apply rate symmetry constraint: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

/*
 * Called by ALSA when a PCM substream is opened, the runtime->hw record is
 * then initialized and any private data can be allocated. This also calls
 * startup for the cpu DAI, platform, machine and codec DAI.
 */
static int soc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_card *card = socdev->card;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	int ret = 0;

	mutex_lock(&pcm_mutex);

	/* startup the audio subsystem */
	if (cpu_dai->ops->startup) {
		ret = cpu_dai->ops->startup(substream, cpu_dai);
		if (ret < 0) {
			printk(KERN_ERR "asoc: can't open interface %s\n",
				cpu_dai->name);
			goto out;
		}
	}

	if (platform->pcm_ops->open) {
		ret = platform->pcm_ops->open(substream);
		if (ret < 0) {
			printk(KERN_ERR "asoc: can't open platform %s\n", platform->name);
			goto platform_err;
		}
	}

	if (codec_dai->ops->startup) {
		ret = codec_dai->ops->startup(substream, codec_dai);
		if (ret < 0) {
			printk(KERN_ERR "asoc: can't open codec %s\n",
				codec_dai->name);
			goto codec_dai_err;
		}
	}

	if (machine->ops && machine->ops->startup) {
		ret = machine->ops->startup(substream);
		if (ret < 0) {
			printk(KERN_ERR "asoc: %s startup failed\n", machine->name);
			goto machine_err;
		}
	}

	/* Check that the codec and cpu DAI's are compatible */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		runtime->hw.rate_min =
			max(codec_dai->playback.rate_min,
			    cpu_dai->playback.rate_min);
		runtime->hw.rate_max =
			min(codec_dai->playback.rate_max,
			    cpu_dai->playback.rate_max);
		runtime->hw.channels_min =
			max(codec_dai->playback.channels_min,
				cpu_dai->playback.channels_min);
		runtime->hw.channels_max =
			min(codec_dai->playback.channels_max,
				cpu_dai->playback.channels_max);
		runtime->hw.formats =
			codec_dai->playback.formats & cpu_dai->playback.formats;
		runtime->hw.rates =
			codec_dai->playback.rates & cpu_dai->playback.rates;
		if (codec_dai->playback.rates
			   & (SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS))
			runtime->hw.rates |= cpu_dai->playback.rates;
		if (cpu_dai->playback.rates
			   & (SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS))
			runtime->hw.rates |= codec_dai->playback.rates;
	} else {
		runtime->hw.rate_min =
			max(codec_dai->capture.rate_min,
			    cpu_dai->capture.rate_min);
		runtime->hw.rate_max =
			min(codec_dai->capture.rate_max,
			    cpu_dai->capture.rate_max);
		runtime->hw.channels_min =
			max(codec_dai->capture.channels_min,
				cpu_dai->capture.channels_min);
		runtime->hw.channels_max =
			min(codec_dai->capture.channels_max,
				cpu_dai->capture.channels_max);
		runtime->hw.formats =
			codec_dai->capture.formats & cpu_dai->capture.formats;
		runtime->hw.rates =
			codec_dai->capture.rates & cpu_dai->capture.rates;
		if (codec_dai->capture.rates
			   & (SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS))
			runtime->hw.rates |= cpu_dai->capture.rates;
		if (cpu_dai->capture.rates
			   & (SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS))
			runtime->hw.rates |= codec_dai->capture.rates;
	}

	snd_pcm_limit_hw_rates(runtime);
	if (!runtime->hw.rates) {
		printk(KERN_ERR "asoc: %s <-> %s No matching rates\n",
			codec_dai->name, cpu_dai->name);
		goto config_err;
	}
	if (!runtime->hw.formats) {
		printk(KERN_ERR "asoc: %s <-> %s No matching formats\n",
			codec_dai->name, cpu_dai->name);
		goto config_err;
	}
	if (!runtime->hw.channels_min || !runtime->hw.channels_max) {
		printk(KERN_ERR "asoc: %s <-> %s No matching channels\n",
			codec_dai->name, cpu_dai->name);
		goto config_err;
	}

	/* Symmetry only applies if we've already got an active stream. */
	if (cpu_dai->active || codec_dai->active) {
		ret = soc_pcm_apply_symmetry(substream);
		if (ret != 0)
			goto config_err;
	}

	pr_debug("asoc: %s <-> %s info:\n", codec_dai->name, cpu_dai->name);
	pr_debug("asoc: rate mask 0x%x\n", runtime->hw.rates);
	pr_debug("asoc: min ch %d max ch %d\n", runtime->hw.channels_min,
		 runtime->hw.channels_max);
	pr_debug("asoc: min rate %d max rate %d\n", runtime->hw.rate_min,
		 runtime->hw.rate_max);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		cpu_dai->playback.active++;
		codec_dai->playback.active++;
	} else {
		cpu_dai->capture.active++;
		codec_dai->capture.active++;
	}
	cpu_dai->active++;
	codec_dai->active++;
	card->codec->active++;
	mutex_unlock(&pcm_mutex);
	return 0;

config_err:
	if (machine->ops && machine->ops->shutdown)
		machine->ops->shutdown(substream);

machine_err:
	if (codec_dai->ops->shutdown)
		codec_dai->ops->shutdown(substream, codec_dai);

codec_dai_err:
	if (platform->pcm_ops->close)
		platform->pcm_ops->close(substream);

platform_err:
	if (cpu_dai->ops->shutdown)
		cpu_dai->ops->shutdown(substream, cpu_dai);
out:
	mutex_unlock(&pcm_mutex);
	return ret;
}

/*
 * Power down the audio subsystem pmdown_time msecs after close is called.
 * This is to ensure there are no pops or clicks in between any music tracks
 * due to DAPM power cycling.
 */
static void close_delayed_work(struct work_struct *work)
{
	struct snd_soc_card *card = container_of(work, struct snd_soc_card,
						 delayed_work.work);
	struct snd_soc_codec *codec = card->codec;
	struct snd_soc_dai *codec_dai;
	int i;

	mutex_lock(&pcm_mutex);
	for (i = 0; i < codec->num_dai; i++) {
		codec_dai = &codec->dai[i];

		pr_debug("pop wq checking: %s status: %s waiting: %s\n",
			 codec_dai->playback.stream_name,
			 codec_dai->playback.active ? "active" : "inactive",
			 codec_dai->pop_wait ? "yes" : "no");

		/* are we waiting on this codec DAI stream */
		if (codec_dai->pop_wait == 1) {
			codec_dai->pop_wait = 0;
			snd_soc_dapm_stream_event(codec,
				codec_dai->playback.stream_name,
				SND_SOC_DAPM_STREAM_STOP);
		}
	}
	mutex_unlock(&pcm_mutex);
}

/*
 * Called by ALSA when a PCM substream is closed. Private data can be
 * freed here. The cpu DAI, codec DAI, machine and platform are also
 * shutdown.
 */
static int soc_codec_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	struct snd_soc_codec *codec = card->codec;

	mutex_lock(&pcm_mutex);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		cpu_dai->playback.active--;
		codec_dai->playback.active--;
	} else {
		cpu_dai->capture.active--;
		codec_dai->capture.active--;
	}

	cpu_dai->active--;
	codec_dai->active--;
	codec->active--;

	/* Muting the DAC suppresses artifacts caused during digital
	 * shutdown, for example from stopping clocks.
	 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_dai_digital_mute(codec_dai, 1);

	if (cpu_dai->ops->shutdown)
		cpu_dai->ops->shutdown(substream, cpu_dai);

	if (codec_dai->ops->shutdown)
		codec_dai->ops->shutdown(substream, codec_dai);

	if (machine->ops && machine->ops->shutdown)
		machine->ops->shutdown(substream);

	if (platform->pcm_ops->close)
		platform->pcm_ops->close(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* start delayed pop wq here for playback streams */
		codec_dai->pop_wait = 1;
		schedule_delayed_work(&card->delayed_work,
			msecs_to_jiffies(card->pmdown_time));
	} else {
		/* capture streams can be powered down now */
		snd_soc_dapm_stream_event(codec,
			codec_dai->capture.stream_name,
			SND_SOC_DAPM_STREAM_STOP);
	}

	mutex_unlock(&pcm_mutex);
	return 0;
}

/*
 * Called by ALSA when the PCM substream is prepared, can set format, sample
 * rate, etc.  This function is non atomic and can be called multiple times,
 * it can refer to the runtime info.
 */
static int soc_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	struct snd_soc_codec *codec = card->codec;
	int ret = 0;

	mutex_lock(&pcm_mutex);

	if (machine->ops && machine->ops->prepare) {
		ret = machine->ops->prepare(substream);
		if (ret < 0) {
			printk(KERN_ERR "asoc: machine prepare error\n");
			goto out;
		}
	}

	if (platform->pcm_ops->prepare) {
		ret = platform->pcm_ops->prepare(substream);
		if (ret < 0) {
			printk(KERN_ERR "asoc: platform prepare error\n");
			goto out;
		}
	}

	if (codec_dai->ops->prepare) {
		ret = codec_dai->ops->prepare(substream, codec_dai);
		if (ret < 0) {
			printk(KERN_ERR "asoc: codec DAI prepare error\n");
			goto out;
		}
	}

	if (cpu_dai->ops->prepare) {
		ret = cpu_dai->ops->prepare(substream, cpu_dai);
		if (ret < 0) {
			printk(KERN_ERR "asoc: cpu DAI prepare error\n");
			goto out;
		}
	}

	/* cancel any delayed stream shutdown that is pending */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
	    codec_dai->pop_wait) {
		codec_dai->pop_wait = 0;
		cancel_delayed_work(&card->delayed_work);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_dapm_stream_event(codec,
					  codec_dai->playback.stream_name,
					  SND_SOC_DAPM_STREAM_START);
	else
		snd_soc_dapm_stream_event(codec,
					  codec_dai->capture.stream_name,
					  SND_SOC_DAPM_STREAM_START);

	snd_soc_dai_digital_mute(codec_dai, 0);

out:
	mutex_unlock(&pcm_mutex);
	return ret;
}

/*
 * Called by ALSA when the hardware params are set by application. This
 * function can also be called multiple times and can allocate buffers
 * (using snd_pcm_lib_* ). It's non-atomic.
 */
static int soc_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	int ret = 0;

	mutex_lock(&pcm_mutex);

	if (machine->ops && machine->ops->hw_params) {
		ret = machine->ops->hw_params(substream, params);
		if (ret < 0) {
			printk(KERN_ERR "asoc: machine hw_params failed\n");
			goto out;
		}
	}

	if (codec_dai->ops->hw_params) {
		ret = codec_dai->ops->hw_params(substream, params, codec_dai);
		if (ret < 0) {
			printk(KERN_ERR "asoc: can't set codec %s hw params\n",
				codec_dai->name);
			goto codec_err;
		}
	}

	if (cpu_dai->ops->hw_params) {
		ret = cpu_dai->ops->hw_params(substream, params, cpu_dai);
		if (ret < 0) {
			printk(KERN_ERR "asoc: interface %s hw params failed\n",
				cpu_dai->name);
			goto interface_err;
		}
	}

	if (platform->pcm_ops->hw_params) {
		ret = platform->pcm_ops->hw_params(substream, params);
		if (ret < 0) {
			printk(KERN_ERR "asoc: platform %s hw params failed\n",
				platform->name);
			goto platform_err;
		}
	}

	machine->rate = params_rate(params);

out:
	mutex_unlock(&pcm_mutex);
	return ret;

platform_err:
	if (cpu_dai->ops->hw_free)
		cpu_dai->ops->hw_free(substream, cpu_dai);

interface_err:
	if (codec_dai->ops->hw_free)
		codec_dai->ops->hw_free(substream, codec_dai);

codec_err:
	if (machine->ops && machine->ops->hw_free)
		machine->ops->hw_free(substream);

	mutex_unlock(&pcm_mutex);
	return ret;
}

/*
 * Free's resources allocated by hw_params, can be called multiple times
 */
static int soc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	struct snd_soc_codec *codec = card->codec;

	mutex_lock(&pcm_mutex);

	/* apply codec digital mute */
	if (!codec->active)
		snd_soc_dai_digital_mute(codec_dai, 1);

	/* free any machine hw params */
	if (machine->ops && machine->ops->hw_free)
		machine->ops->hw_free(substream);

	/* free any DMA resources */
	if (platform->pcm_ops->hw_free)
		platform->pcm_ops->hw_free(substream);

	/* now free hw params for the DAI's  */
	if (codec_dai->ops->hw_free)
		codec_dai->ops->hw_free(substream, codec_dai);

	if (cpu_dai->ops->hw_free)
		cpu_dai->ops->hw_free(substream, cpu_dai);

	mutex_unlock(&pcm_mutex);
	return 0;
}

static int soc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_card *card= socdev->card;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	int ret;

	if (codec_dai->ops->trigger) {
		ret = codec_dai->ops->trigger(substream, cmd, codec_dai);
		if (ret < 0)
			return ret;
	}

	if (platform->pcm_ops->trigger) {
		ret = platform->pcm_ops->trigger(substream, cmd);
		if (ret < 0)
			return ret;
	}

	if (cpu_dai->ops->trigger) {
		ret = cpu_dai->ops->trigger(substream, cmd, cpu_dai);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/*
 * soc level wrapper for pointer callback
 * If cpu_dai, codec_dai, platform driver has the delay callback, than
 * the runtime->delay will be updated accordingly.
 */
static snd_pcm_uframes_t soc_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai_link *machine = rtd->dai;
	struct snd_soc_dai *cpu_dai = machine->cpu_dai;
	struct snd_soc_dai *codec_dai = machine->codec_dai;
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t offset = 0;
	snd_pcm_sframes_t delay = 0;

	if (platform->pcm_ops->pointer)
		offset = platform->pcm_ops->pointer(substream);

	if (cpu_dai->ops->delay)
		delay += cpu_dai->ops->delay(substream, cpu_dai);

	if (codec_dai->ops->delay)
		delay += codec_dai->ops->delay(substream, codec_dai);

	if (platform->delay)
		delay += platform->delay(substream, codec_dai);

	runtime->delay = delay;

	return offset;
}

/* ASoC PCM operations */
static struct snd_pcm_ops soc_pcm_ops = {
	.open		= soc_pcm_open,
	.close		= soc_codec_close,
	.hw_params	= soc_pcm_hw_params,
	.hw_free	= soc_pcm_hw_free,
	.prepare	= soc_pcm_prepare,
	.trigger	= soc_pcm_trigger,
	.pointer	= soc_pcm_pointer,
};

#ifdef CONFIG_PM
/* powers down audio subsystem for suspend */
static int soc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_codec_device *codec_dev = socdev->codec_dev;
	struct snd_soc_codec *codec = card->codec;
	int i;

	/* If the initialization of this soc device failed, there is no codec
	 * associated with it. Just bail out in this case.
	 */
	if (!codec)
		return 0;

	/* Due to the resume being scheduled into a workqueue we could
	* suspend before that's finished - wait for it to complete.
	 */
	snd_power_lock(codec->card);
	snd_power_wait(codec->card, SNDRV_CTL_POWER_D0);
	snd_power_unlock(codec->card);

	/* we're going to block userspace touching us until resume completes */
	snd_power_change_state(codec->card, SNDRV_CTL_POWER_D3hot);

	/* mute any active DAC's */
	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai *dai = card->dai_link[i].codec_dai;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (dai->ops->digital_mute && dai->playback.active)
			dai->ops->digital_mute(dai, 1);
	}

	/* suspend all pcms */
	for (i = 0; i < card->num_links; i++) {
		if (card->dai_link[i].ignore_suspend)
			continue;

		snd_pcm_suspend_all(card->dai_link[i].pcm);
	}

	if (card->suspend_pre)
		card->suspend_pre(pdev, PMSG_SUSPEND);

	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai  *cpu_dai = card->dai_link[i].cpu_dai;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (cpu_dai->suspend && !cpu_dai->ac97_control)
			cpu_dai->suspend(cpu_dai);
		if (platform->suspend)
			platform->suspend(&card->dai_link[i]);
	}

	/* close any waiting streams and save state */
	run_delayed_work(&card->delayed_work);
	codec->suspend_bias_level = codec->bias_level;

	for (i = 0; i < codec->num_dai; i++) {
		char *stream = codec->dai[i].playback.stream_name;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (stream != NULL)
			snd_soc_dapm_stream_event(codec, stream,
				SND_SOC_DAPM_STREAM_SUSPEND);
		stream = codec->dai[i].capture.stream_name;
		if (stream != NULL)
			snd_soc_dapm_stream_event(codec, stream,
				SND_SOC_DAPM_STREAM_SUSPEND);
	}

	/* If there are paths active then the CODEC will be held with
	 * bias _ON and should not be suspended. */
	if (codec_dev->suspend) {
		switch (codec->bias_level) {
		case SND_SOC_BIAS_STANDBY:
		case SND_SOC_BIAS_OFF:
			codec_dev->suspend(pdev, PMSG_SUSPEND);
			break;
		default:
			dev_dbg(socdev->dev, "CODEC is on over suspend\n");
			break;
		}
	}

	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai *cpu_dai = card->dai_link[i].cpu_dai;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (cpu_dai->suspend && cpu_dai->ac97_control)
			cpu_dai->suspend(cpu_dai);
	}

	if (card->suspend_post)
		card->suspend_post(pdev, PMSG_SUSPEND);

	return 0;
}

/* deferred resume work, so resume can complete before we finished
 * setting our codec back up, which can be very slow on I2C
 */
static void soc_resume_deferred(struct work_struct *work)
{
	struct snd_soc_card *card = container_of(work,
						 struct snd_soc_card,
						 deferred_resume_work);
	struct snd_soc_device *socdev = card->socdev;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_codec_device *codec_dev = socdev->codec_dev;
	struct snd_soc_codec *codec = card->codec;
	struct platform_device *pdev = to_platform_device(socdev->dev);
	int i;

	/* our power state is still SNDRV_CTL_POWER_D3hot from suspend time,
	 * so userspace apps are blocked from touching us
	 */

	dev_dbg(socdev->dev, "starting resume work\n");

	/* Bring us up into D2 so that DAPM starts enabling things */
	snd_power_change_state(codec->card, SNDRV_CTL_POWER_D2);

	if (card->resume_pre)
		card->resume_pre(pdev);

	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai *cpu_dai = card->dai_link[i].cpu_dai;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (cpu_dai->resume && cpu_dai->ac97_control)
			cpu_dai->resume(cpu_dai);
	}

	/* If the CODEC was idle over suspend then it will have been
	 * left with bias OFF or STANDBY and suspended so we must now
	 * resume.  Otherwise the suspend was suppressed.
	 */
	if (codec_dev->resume) {
		switch (codec->bias_level) {
		case SND_SOC_BIAS_STANDBY:
		case SND_SOC_BIAS_OFF:
			codec_dev->resume(pdev);
			break;
		default:
			dev_dbg(socdev->dev, "CODEC was on over suspend\n");
			break;
		}
	}

	for (i = 0; i < codec->num_dai; i++) {
		char *stream = codec->dai[i].playback.stream_name;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (stream != NULL)
			snd_soc_dapm_stream_event(codec, stream,
				SND_SOC_DAPM_STREAM_RESUME);
		stream = codec->dai[i].capture.stream_name;
		if (stream != NULL)
			snd_soc_dapm_stream_event(codec, stream,
				SND_SOC_DAPM_STREAM_RESUME);
	}

	/* unmute any active DACs */
	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai *dai = card->dai_link[i].codec_dai;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (dai->ops->digital_mute && dai->playback.active)
			dai->ops->digital_mute(dai, 0);
	}

	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai *cpu_dai = card->dai_link[i].cpu_dai;

		if (card->dai_link[i].ignore_suspend)
			continue;

		if (cpu_dai->resume && !cpu_dai->ac97_control)
			cpu_dai->resume(cpu_dai);
		if (platform->resume)
			platform->resume(&card->dai_link[i]);
	}

	if (card->resume_post)
		card->resume_post(pdev);

	dev_dbg(socdev->dev, "resume work completed\n");

	/* userspace can access us now we are back as we were before */
	snd_power_change_state(codec->card, SNDRV_CTL_POWER_D0);
}

/* powers up audio subsystem after a suspend */
static int soc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_dai *cpu_dai = card->dai_link[0].cpu_dai;

	/* If the initialization of this soc device failed, there is no codec
	 * associated with it. Just bail out in this case.
	 */
	if (!card->codec)
		return 0;

	/* AC97 devices might have other drivers hanging off them so
	 * need to resume immediately.  Other drivers don't have that
	 * problem and may take a substantial amount of time to resume
	 * due to I/O costs and anti-pop so handle them out of line.
	 */
	if (cpu_dai->ac97_control) {
		dev_dbg(socdev->dev, "Resuming AC97 immediately\n");
		soc_resume_deferred(&card->deferred_resume_work);
	} else {
		dev_dbg(socdev->dev, "Scheduling resume work\n");
		if (!schedule_work(&card->deferred_resume_work))
			dev_err(socdev->dev, "resume work item may be lost\n");
	}

	return 0;
}
#else
#define soc_suspend	NULL
#define soc_resume	NULL
#endif

static struct snd_soc_dai_ops null_dai_ops = {
};

static void snd_soc_instantiate_card(struct snd_soc_card *card)
{
	struct platform_device *pdev = container_of(card->dev,
						    struct platform_device,
						    dev);
	struct snd_soc_codec_device *codec_dev = card->socdev->codec_dev;
	struct snd_soc_codec *codec;
	struct snd_soc_platform *platform;
	struct snd_soc_dai *dai;
	int i, found, ret, ac97;

	if (card->instantiated)
		return;

	found = 0;
	list_for_each_entry(platform, &platform_list, list)
		if (card->platform == platform) {
			found = 1;
			break;
		}
	if (!found) {
		dev_dbg(card->dev, "Platform %s not registered\n",
			card->platform->name);
		return;
	}

	ac97 = 0;
	for (i = 0; i < card->num_links; i++) {
		found = 0;
		list_for_each_entry(dai, &dai_list, list)
			if (card->dai_link[i].cpu_dai == dai) {
				found = 1;
				break;
			}
		if (!found) {
			dev_dbg(card->dev, "DAI %s not registered\n",
				card->dai_link[i].cpu_dai->name);
			return;
		}

		if (card->dai_link[i].cpu_dai->ac97_control)
			ac97 = 1;
	}

	for (i = 0; i < card->num_links; i++) {
		if (!card->dai_link[i].codec_dai->ops)
			card->dai_link[i].codec_dai->ops = &null_dai_ops;
	}

	/* If we have AC97 in the system then don't wait for the
	 * codec.  This will need revisiting if we have to handle
	 * systems with mixed AC97 and non-AC97 parts.  Only check for
	 * DAIs currently; we can't do this per link since some AC97
	 * codecs have non-AC97 DAIs.
	 */
	if (!ac97)
		for (i = 0; i < card->num_links; i++) {
			found = 0;
			list_for_each_entry(dai, &dai_list, list)
				if (card->dai_link[i].codec_dai == dai) {
					found = 1;
					break;
				}
			if (!found) {
				dev_dbg(card->dev, "DAI %s not registered\n",
					card->dai_link[i].codec_dai->name);
				return;
			}
		}

	/* Note that we do not current check for codec components */

	dev_dbg(card->dev, "All components present, instantiating\n");

	/* Found everything, bring it up */
	card->pmdown_time = pmdown_time;

	if (card->probe) {
		ret = card->probe(pdev);
		if (ret < 0)
			return;
	}

	for (i = 0; i < card->num_links; i++) {
		struct snd_soc_dai *cpu_dai = card->dai_link[i].cpu_dai;
		if (cpu_dai->probe) {
			ret = cpu_dai->probe(pdev, cpu_dai);
			if (ret < 0)
				goto cpu_dai_err;
		}
	}

	if (codec_dev->probe) {
		ret = codec_dev->probe(pdev);
		if (ret < 0)
			goto cpu_dai_err;
	}
	codec = card->codec;

	if (platform->probe) {
		ret = platform->probe(pdev);
		if (ret < 0)
			goto platform_err;
	}

	/* DAPM stream work */
	INIT_DELAYED_WORK(&card->delayed_work, close_delayed_work);
#ifdef CONFIG_PM
	/* deferred resume work */
	INIT_WORK(&card->deferred_resume_work, soc_resume_deferred);
#endif

	for (i = 0; i < card->num_links; i++) {
		if (card->dai_link[i].init) {
			ret = card->dai_link[i].init(codec);
			if (ret < 0) {
				printk(KERN_ERR "asoc: failed to init %s\n",
					card->dai_link[i].stream_name);
				continue;
			}
		}
		if (card->dai_link[i].codec_dai->ac97_control)
			ac97 = 1;
	}

	snprintf(codec->card->shortname, sizeof(codec->card->shortname),
		 "%s",  card->name);
	snprintf(codec->card->longname, sizeof(codec->card->longname),
		 "%s (%s)", card->name, codec->name);

	/* Make sure all DAPM widgets are instantiated */
	snd_soc_dapm_new_widgets(codec);

	ret = snd_card_register(codec->card);
	if (ret < 0) {
		printk(KERN_ERR "asoc: failed to register soundcard for %s\n",
				codec->name);
		goto card_err;
	}

	mutex_lock(&codec->mutex);
#ifdef CONFIG_SND_SOC_AC97_BUS
	/* Only instantiate AC97 if not already done by the adaptor
	 * for the generic AC97 subsystem.
	 */
	if (ac97 && strcmp(codec->name, "AC97") != 0) {
		ret = soc_ac97_dev_register(codec);
		if (ret < 0) {
			printk(KERN_ERR "asoc: AC97 device register failed\n");
			snd_card_free(codec->card);
			mutex_unlock(&codec->mutex);
			goto card_err;
		}
	}
#endif

	ret = snd_soc_dapm_sys_add(card->socdev->dev);
	if (ret < 0)
		printk(KERN_WARNING "asoc: failed to add dapm sysfs entries\n");

	ret = device_create_file(card->socdev->dev, &dev_attr_pmdown_time);
	if (ret < 0)
		printk(KERN_WARNING "asoc: failed to add pmdown_time sysfs\n");

	ret = device_create_file(card->socdev->dev, &dev_attr_codec_reg);
	if (ret < 0)
		printk(KERN_WARNING "asoc: failed to add codec sysfs files\n");

	soc_init_codec_debugfs(codec);
	mutex_unlock(&codec->mutex);

	card->instantiated = 1;

	return;

card_err:
	if (platform->remove)
		platform->remove(pdev);

platform_err:
	if (codec_dev->remove)
		codec_dev->remove(pdev);

cpu_dai_err:
	for (i--; i >= 0; i--) {
		struct snd_soc_dai *cpu_dai = card->dai_link[i].cpu_dai;
		if (cpu_dai->remove)
			cpu_dai->remove(pdev, cpu_dai);
	}

	if (card->remove)
		card->remove(pdev);
}

/*
 * Attempt to initialise any uninitialised cards.  Must be called with
 * client_mutex.
 */
static void snd_soc_instantiate_cards(void)
{
	struct snd_soc_card *card;
	list_for_each_entry(card, &card_list, list)
		snd_soc_instantiate_card(card);
}

/* probes a new socdev */
static int soc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_card *card = socdev->card;

	/* Bodge while we push things out of socdev */
	card->socdev = socdev;

	/* Bodge while we unpick instantiation */
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register card\n");
		return ret;
	}

	return 0;
}

/* removes a socdev */
static int soc_remove(struct platform_device *pdev)
{
	int i;
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_codec_device *codec_dev = socdev->codec_dev;

	if (card->instantiated) {
		run_delayed_work(&card->delayed_work);

		if (platform->remove)
			platform->remove(pdev);

		if (codec_dev->remove)
			codec_dev->remove(pdev);

		for (i = 0; i < card->num_links; i++) {
			struct snd_soc_dai *cpu_dai = card->dai_link[i].cpu_dai;
			if (cpu_dai->remove)
				cpu_dai->remove(pdev, cpu_dai);
		}

		if (card->remove)
			card->remove(pdev);
	}

	snd_soc_unregister_card(card);

	return 0;
}

static int soc_poweroff(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_card *card = socdev->card;

	if (!card->instantiated)
		return 0;

	/* Flush out pmdown_time work - we actually do want to run it
	 * now, we're shutting down so no imminent restart. */
	run_delayed_work(&card->delayed_work);

	snd_soc_dapm_shutdown(socdev);

	return 0;
}

static const struct dev_pm_ops soc_pm_ops = {
	.suspend = soc_suspend,
	.resume = soc_resume,
	.poweroff = soc_poweroff,
};

/* ASoC platform driver */
static struct platform_driver soc_driver = {
	.driver		= {
		.name		= "soc-audio",
		.owner		= THIS_MODULE,
		.pm		= &soc_pm_ops,
	},
	.probe		= soc_probe,
	.remove		= soc_remove,
};

/* create a new pcm */
static int soc_new_pcm(struct snd_soc_device *socdev,
	struct snd_soc_dai_link *dai_link, int num)
{
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_codec *codec = card->codec;
	struct snd_soc_platform *platform = card->platform;
	struct snd_soc_dai *codec_dai = dai_link->codec_dai;
	struct snd_soc_dai *cpu_dai = dai_link->cpu_dai;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_pcm *pcm;
	char new_name[64];
	int ret = 0, playback = 0, capture = 0;

	rtd = kzalloc(sizeof(struct snd_soc_pcm_runtime), GFP_KERNEL);
	if (rtd == NULL)
		return -ENOMEM;

	rtd->dai = dai_link;
	rtd->socdev = socdev;
	codec_dai->codec = card->codec;

	/* check client and interface hw capabilities */
	snprintf(new_name, sizeof(new_name), "%s %s-%d",
		 dai_link->stream_name, codec_dai->name, num);

	if (codec_dai->playback.channels_min)
		playback = 1;
	if (codec_dai->capture.channels_min)
		capture = 1;

	ret = snd_pcm_new(codec->card, new_name, codec->pcm_devs++, playback,
		capture, &pcm);
	if (ret < 0) {
		printk(KERN_ERR "asoc: can't create pcm for codec %s\n",
			codec->name);
		kfree(rtd);
		return ret;
	}

	dai_link->pcm = pcm;
	pcm->private_data = rtd;
	soc_pcm_ops.mmap = platform->pcm_ops->mmap;
	soc_pcm_ops.ioctl = platform->pcm_ops->ioctl;
	soc_pcm_ops.copy = platform->pcm_ops->copy;
	soc_pcm_ops.silence = platform->pcm_ops->silence;
	soc_pcm_ops.ack = platform->pcm_ops->ack;
	soc_pcm_ops.page = platform->pcm_ops->page;

	if (playback)
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &soc_pcm_ops);

	if (capture)
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &soc_pcm_ops);

	ret = platform->pcm_new(codec->card, codec_dai, pcm);
	if (ret < 0) {
		printk(KERN_ERR "asoc: platform pcm constructor failed\n");
		kfree(rtd);
		return ret;
	}

	pcm->private_free = platform->pcm_free;
	printk(KERN_INFO "asoc: %s <-> %s mapping ok\n", codec_dai->name,
		cpu_dai->name);
	return ret;
}

/**
 * snd_soc_codec_volatile_register: Report if a register is volatile.
 *
 * @codec: CODEC to query.
 * @reg: Register to query.
 *
 * Boolean function indiciating if a CODEC register is volatile.
 */
int snd_soc_codec_volatile_register(struct snd_soc_codec *codec, int reg)
{
	if (codec->volatile_register)
		return codec->volatile_register(reg);
	else
		return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_codec_volatile_register);

/**
 * snd_soc_new_ac97_codec - initailise AC97 device
 * @codec: audio codec
 * @ops: AC97 bus operations
 * @num: AC97 codec number
 *
 * Initialises AC97 codec resources for use by ad-hoc devices only.
 */
int snd_soc_new_ac97_codec(struct snd_soc_codec *codec,
	struct snd_ac97_bus_ops *ops, int num)
{
	mutex_lock(&codec->mutex);

	codec->ac97 = kzalloc(sizeof(struct snd_ac97), GFP_KERNEL);
	if (codec->ac97 == NULL) {
		mutex_unlock(&codec->mutex);
		return -ENOMEM;
	}

	codec->ac97->bus = kzalloc(sizeof(struct snd_ac97_bus), GFP_KERNEL);
	if (codec->ac97->bus == NULL) {
		kfree(codec->ac97);
		codec->ac97 = NULL;
		mutex_unlock(&codec->mutex);
		return -ENOMEM;
	}

	codec->ac97->bus->ops = ops;
	codec->ac97->num = num;
	codec->dev = &codec->ac97->dev;
	mutex_unlock(&codec->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_new_ac97_codec);

/**
 * snd_soc_free_ac97_codec - free AC97 codec device
 * @codec: audio codec
 *
 * Frees AC97 codec device resources.
 */
void snd_soc_free_ac97_codec(struct snd_soc_codec *codec)
{
	mutex_lock(&codec->mutex);
	kfree(codec->ac97->bus);
	kfree(codec->ac97);
	codec->ac97 = NULL;
	mutex_unlock(&codec->mutex);
}
EXPORT_SYMBOL_GPL(snd_soc_free_ac97_codec);

/**
 * snd_soc_update_bits - update codec register bits
 * @codec: audio codec
 * @reg: codec register
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change else 0.
 */
int snd_soc_update_bits(struct snd_soc_codec *codec, unsigned short reg,
				unsigned int mask, unsigned int value)
{
	int change;
	unsigned int old, new;

	old = snd_soc_read(codec, reg);
	new = (old & ~mask) | value;
	change = old != new;
	if (change)
		snd_soc_write(codec, reg, new);

	return change;
}
EXPORT_SYMBOL_GPL(snd_soc_update_bits);

/**
 * snd_soc_update_bits_locked - update codec register bits
 * @codec: audio codec
 * @reg: codec register
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value, and takes the codec mutex.
 *
 * Returns 1 for change else 0.
 */
int snd_soc_update_bits_locked(struct snd_soc_codec *codec,
			       unsigned short reg, unsigned int mask,
			       unsigned int value)
{
	int change;

	mutex_lock(&codec->mutex);
	change = snd_soc_update_bits(codec, reg, mask, value);
	mutex_unlock(&codec->mutex);

	return change;
}
EXPORT_SYMBOL_GPL(snd_soc_update_bits_locked);

/**
 * snd_soc_test_bits - test register for change
 * @codec: audio codec
 * @reg: codec register
 * @mask: register mask
 * @value: new value
 *
 * Tests a register with a new value and checks if the new value is
 * different from the old value.
 *
 * Returns 1 for change else 0.
 */
int snd_soc_test_bits(struct snd_soc_codec *codec, unsigned short reg,
				unsigned int mask, unsigned int value)
{
	int change;
	unsigned int old, new;

	old = snd_soc_read(codec, reg);
	new = (old & ~mask) | value;
	change = old != new;

	return change;
}
EXPORT_SYMBOL_GPL(snd_soc_test_bits);

/**
 * snd_soc_new_pcms - create new sound card and pcms
 * @socdev: the SoC audio device
 * @idx: ALSA card index
 * @xid: card identification
 *
 * Create a new sound card based upon the codec and interface pcms.
 *
 * Returns 0 for success, else error.
 */
int snd_soc_new_pcms(struct snd_soc_device *socdev, int idx, const char *xid)
{
	struct snd_soc_card *card = socdev->card;
	struct snd_soc_codec *codec = card->codec;
	int ret, i;

	mutex_lock(&codec->mutex);

	/* register a sound card */
	ret = snd_card_create(idx, xid, codec->owner, 0, &codec->card);
	if (ret < 0) {
		printk(KERN_ERR "asoc: can't create sound card for codec %s\n",
			codec->name);
		mutex_unlock(&codec->mutex);
		return ret;
	}

	codec->socdev = socdev;
	codec->card->dev = socdev->dev;
	codec->card->private_data = codec;
	strncpy(codec->card->driver, codec->name, sizeof(codec->card->driver));

	/* create the pcms */
	for (i = 0; i < card->num_links; i++) {
		ret = soc_new_pcm(socdev, &card->dai_link[i], i);
		if (ret < 0) {
			printk(KERN_ERR "asoc: can't create pcm %s\n",
				card->dai_link[i].stream_name);
			mutex_unlock(&codec->mutex);
			return ret;
		}
		/* Check for codec->ac97 to handle the ac97.c fun */
		if (card->dai_link[i].codec_dai->ac97_control && codec->ac97) {
			snd_ac97_dev_add_pdata(codec->ac97,
				card->dai_link[i].cpu_dai->ac97_pdata);
		}
	}

	mutex_unlock(&codec->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_new_pcms);

/**
 * snd_soc_free_pcms - free sound card and pcms
 * @socdev: the SoC audio device
 *
 * Frees sound card and pcms associated with the socdev.
 * Also unregister the codec if it is an AC97 device.
 */
void snd_soc_free_pcms(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->card->codec;
#ifdef CONFIG_SND_SOC_AC97_BUS
	struct snd_soc_dai *codec_dai;
	int i;
#endif

	mutex_lock(&codec->mutex);
	soc_cleanup_codec_debugfs(codec);
#ifdef CONFIG_SND_SOC_AC97_BUS
	for (i = 0; i < codec->num_dai; i++) {
		codec_dai = &codec->dai[i];
		if (codec_dai->ac97_control && codec->ac97 &&
		    strcmp(codec->name, "AC97") != 0) {
			soc_ac97_dev_unregister(codec);
			goto free_card;
		}
	}
free_card:
#endif

	if (codec->card)
		snd_card_free(codec->card);
	device_remove_file(socdev->dev, &dev_attr_codec_reg);
	mutex_unlock(&codec->mutex);
}
EXPORT_SYMBOL_GPL(snd_soc_free_pcms);

/**
 * snd_soc_set_runtime_hwparams - set the runtime hardware parameters
 * @substream: the pcm substream
 * @hw: the hardware parameters
 *
 * Sets the substream runtime hardware parameters.
 */
int snd_soc_set_runtime_hwparams(struct snd_pcm_substream *substream,
	const struct snd_pcm_hardware *hw)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	runtime->hw.info = hw->info;
	runtime->hw.formats = hw->formats;
	runtime->hw.period_bytes_min = hw->period_bytes_min;
	runtime->hw.period_bytes_max = hw->period_bytes_max;
	runtime->hw.periods_min = hw->periods_min;
	runtime->hw.periods_max = hw->periods_max;
	runtime->hw.buffer_bytes_max = hw->buffer_bytes_max;
	runtime->hw.fifo_size = hw->fifo_size;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_set_runtime_hwparams);

/**
 * snd_soc_cnew - create new control
 * @_template: control template
 * @data: control private data
 * @long_name: control long name
 *
 * Create a new mixer control from a template control.
 *
 * Returns 0 for success, else error.
 */
struct snd_kcontrol *snd_soc_cnew(const struct snd_kcontrol_new *_template,
	void *data, char *long_name)
{
	struct snd_kcontrol_new template;

	memcpy(&template, _template, sizeof(template));
	if (long_name)
		template.name = long_name;
	template.index = 0;

	return snd_ctl_new1(&template, data);
}
EXPORT_SYMBOL_GPL(snd_soc_cnew);

/**
 * snd_soc_add_controls - add an array of controls to a codec.
 * Convienience function to add a list of controls. Many codecs were
 * duplicating this code.
 *
 * @codec: codec to add controls to
 * @controls: array of controls to add
 * @num_controls: number of elements in the array
 *
 * Return 0 for success, else error.
 */
int snd_soc_add_controls(struct snd_soc_codec *codec,
	const struct snd_kcontrol_new *controls, int num_controls)
{
	struct snd_card *card = codec->card;
	int err, i;

	for (i = 0; i < num_controls; i++) {
		const struct snd_kcontrol_new *control = &controls[i];
		err = snd_ctl_add(card, snd_soc_cnew(control, codec, NULL));
		if (err < 0) {
			dev_err(codec->dev, "%s: Failed to add %s\n",
				codec->name, control->name);
			return err;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_add_controls);

/**
 * snd_soc_info_enum_double - enumerated double mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a double enumerated
 * mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_info_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = e->shift_l == e->shift_r ? 1 : 2;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;
	strcpy(uinfo->value.enumerated.name,
		e->texts[uinfo->value.enumerated.item]);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_enum_double);

/**
 * snd_soc_get_enum_double - enumerated double mixer get callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to get the value of a double enumerated mixer.
 *
 * Returns 0 for success.
 */
int snd_soc_get_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val, bitmask;

	for (bitmask = 1; bitmask < e->max; bitmask <<= 1)
		;
	val = snd_soc_read(codec, e->reg);
	ucontrol->value.enumerated.item[0]
		= (val >> e->shift_l) & (bitmask - 1);
	if (e->shift_l != e->shift_r)
		ucontrol->value.enumerated.item[1] =
			(val >> e->shift_r) & (bitmask - 1);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_get_enum_double);

/**
 * snd_soc_put_enum_double - enumerated double mixer put callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to set the value of a double enumerated mixer.
 *
 * Returns 0 for success.
 */
int snd_soc_put_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned int mask, bitmask;

	for (bitmask = 1; bitmask < e->max; bitmask <<= 1)
		;
	if (ucontrol->value.enumerated.item[0] > e->max - 1)
		return -EINVAL;
	val = ucontrol->value.enumerated.item[0] << e->shift_l;
	mask = (bitmask - 1) << e->shift_l;
	if (e->shift_l != e->shift_r) {
		if (ucontrol->value.enumerated.item[1] > e->max - 1)
			return -EINVAL;
		val |= ucontrol->value.enumerated.item[1] << e->shift_r;
		mask |= (bitmask - 1) << e->shift_r;
	}

	return snd_soc_update_bits_locked(codec, e->reg, mask, val);
}
EXPORT_SYMBOL_GPL(snd_soc_put_enum_double);

/**
 * snd_soc_get_value_enum_double - semi enumerated double mixer get callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to get the value of a double semi enumerated mixer.
 *
 * Semi enumerated mixer: the enumerated items are referred as values. Can be
 * used for handling bitfield coded enumeration for example.
 *
 * Returns 0 for success.
 */
int snd_soc_get_value_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg_val, val, mux;

	reg_val = snd_soc_read(codec, e->reg);
	val = (reg_val >> e->shift_l) & e->mask;
	for (mux = 0; mux < e->max; mux++) {
		if (val == e->values[mux])
			break;
	}
	ucontrol->value.enumerated.item[0] = mux;
	if (e->shift_l != e->shift_r) {
		val = (reg_val >> e->shift_r) & e->mask;
		for (mux = 0; mux < e->max; mux++) {
			if (val == e->values[mux])
				break;
		}
		ucontrol->value.enumerated.item[1] = mux;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_get_value_enum_double);

/**
 * snd_soc_put_value_enum_double - semi enumerated double mixer put callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to set the value of a double semi enumerated mixer.
 *
 * Semi enumerated mixer: the enumerated items are referred as values. Can be
 * used for handling bitfield coded enumeration for example.
 *
 * Returns 0 for success.
 */
int snd_soc_put_value_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned int mask;

	if (ucontrol->value.enumerated.item[0] > e->max - 1)
		return -EINVAL;
	val = e->values[ucontrol->value.enumerated.item[0]] << e->shift_l;
	mask = e->mask << e->shift_l;
	if (e->shift_l != e->shift_r) {
		if (ucontrol->value.enumerated.item[1] > e->max - 1)
			return -EINVAL;
		val |= e->values[ucontrol->value.enumerated.item[1]] << e->shift_r;
		mask |= e->mask << e->shift_r;
	}

	return snd_soc_update_bits_locked(codec, e->reg, mask, val);
}
EXPORT_SYMBOL_GPL(snd_soc_put_value_enum_double);

/**
 * snd_soc_info_enum_ext - external enumerated single mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about an external enumerated
 * single mixer.
 *
 * Returns 0 for success.
 */
int snd_soc_info_enum_ext(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;
	strcpy(uinfo->value.enumerated.name,
		e->texts[uinfo->value.enumerated.item]);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_enum_ext);

/**
 * snd_soc_info_volsw_ext - external single mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a single external mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_info_volsw_ext(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	int max = kcontrol->private_value;

	if (max == 1 && !strstr(kcontrol->id.name, " Volume"))
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_volsw_ext);

/**
 * snd_soc_info_volsw - single mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_info_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int platform_max;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;

	if (!mc->platform_max)
		mc->platform_max = mc->max;
	platform_max = mc->platform_max;

	if (platform_max == 1 && !strstr(kcontrol->id.name, " Volume"))
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = shift == rshift ? 1 : 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = platform_max;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_volsw);

/**
 * snd_soc_get_volsw - single mixer get callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to get the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_get_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec, reg) >> shift) & mask;
	if (shift != rshift)
		ucontrol->value.integer.value[1] =
			(snd_soc_read(codec, reg) >> rshift) & mask;
	if (invert) {
		ucontrol->value.integer.value[0] =
			max - ucontrol->value.integer.value[0];
		if (shift != rshift)
			ucontrol->value.integer.value[1] =
				max - ucontrol->value.integer.value[1];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_get_volsw);

/**
 * snd_soc_put_volsw - single mixer put callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to set the value of a single mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_put_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val, val2, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;
	if (shift != rshift) {
		val2 = (ucontrol->value.integer.value[1] & mask);
		if (invert)
			val2 = max - val2;
		val_mask |= mask << rshift;
		val |= val2 << rshift;
	}
	return snd_soc_update_bits_locked(codec, reg, val_mask, val);
}
EXPORT_SYMBOL_GPL(snd_soc_put_volsw);

/**
 * snd_soc_info_volsw_2r - double mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a double mixer control that
 * spans 2 codec registers.
 *
 * Returns 0 for success.
 */
int snd_soc_info_volsw_2r(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int platform_max;

	if (!mc->platform_max)
		mc->platform_max = mc->max;
	platform_max = mc->platform_max;

	if (platform_max == 1 && !strstr(kcontrol->id.name, " Volume"))
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = platform_max;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_volsw_2r);

/**
 * snd_soc_get_volsw_2r - double mixer get callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to get the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int snd_soc_get_volsw_2r(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec, reg) >> shift) & mask;
	ucontrol->value.integer.value[1] =
		(snd_soc_read(codec, reg2) >> shift) & mask;
	if (invert) {
		ucontrol->value.integer.value[0] =
			max - ucontrol->value.integer.value[0];
		ucontrol->value.integer.value[1] =
			max - ucontrol->value.integer.value[1];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_get_volsw_2r);

/**
 * snd_soc_put_volsw_2r - double mixer set callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to set the value of a double mixer control that spans 2 registers.
 *
 * Returns 0 for success.
 */
int snd_soc_put_volsw_2r(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val2, val_mask;

	val_mask = mask << shift;
	val = (ucontrol->value.integer.value[0] & mask);
	val2 = (ucontrol->value.integer.value[1] & mask);

	if (invert) {
		val = max - val;
		val2 = max - val2;
	}

	val = val << shift;
	val2 = val2 << shift;

	err = snd_soc_update_bits_locked(codec, reg, val_mask, val);
	if (err < 0)
		return err;

	err = snd_soc_update_bits_locked(codec, reg2, val_mask, val2);
	return err;
}
EXPORT_SYMBOL_GPL(snd_soc_put_volsw_2r);

/**
 * snd_soc_info_volsw_s8 - signed mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Callback to provide information about a signed mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_info_volsw_s8(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int platform_max;
	int min = mc->min;

	if (!mc->platform_max)
		mc->platform_max = mc->max;
	platform_max = mc->platform_max;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = platform_max - min;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_volsw_s8);

/**
 * snd_soc_get_volsw_s8 - signed mixer get callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to get the value of a signed mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_get_volsw_s8(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	int min = mc->min;
	int val = snd_soc_read(codec, reg);

	ucontrol->value.integer.value[0] =
		((signed char)(val & 0xff))-min;
	ucontrol->value.integer.value[1] =
		((signed char)((val >> 8) & 0xff))-min;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_get_volsw_s8);

/**
 * snd_soc_put_volsw_sgn - signed mixer put callback
 * @kcontrol: mixer control
 * @ucontrol: control element information
 *
 * Callback to set the value of a signed mixer control.
 *
 * Returns 0 for success.
 */
int snd_soc_put_volsw_s8(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	int min = mc->min;
	unsigned int val;

	val = (ucontrol->value.integer.value[0]+min) & 0xff;
	val |= ((ucontrol->value.integer.value[1]+min) & 0xff) << 8;

	return snd_soc_update_bits_locked(codec, reg, 0xffff, val);
}
EXPORT_SYMBOL_GPL(snd_soc_put_volsw_s8);

/**
 * snd_soc_limit_volume - Set new limit to an existing volume control.
 *
 * @codec: where to look for the control
 * @name: Name of the control
 * @max: new maximum limit
 *
 * Return 0 for success, else error.
 */
int snd_soc_limit_volume(struct snd_soc_codec *codec,
	const char *name, int max)
{
	struct snd_card *card = codec->card;
	struct snd_kcontrol *kctl;
	struct soc_mixer_control *mc;
	int found = 0;
	int ret = -EINVAL;

	/* Sanity check for name and max */
	if (unlikely(!name || max <= 0))
		return -EINVAL;

	list_for_each_entry(kctl, &card->controls, list) {
		if (!strncmp(kctl->id.name, name, sizeof(kctl->id.name))) {
			found = 1;
			break;
		}
	}
	if (found) {
		mc = (struct soc_mixer_control *)kctl->private_value;
		if (max <= mc->max) {
			mc->platform_max = max;
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_limit_volume);

/**
 * snd_soc_info_volsw_2r_sx - double with tlv and variable data size
 *  mixer info callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Returns 0 for success.
 */
int snd_soc_info_volsw_2r_sx(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int max = mc->max;
	int min = mc->min;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max-min;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_info_volsw_2r_sx);

/**
 * snd_soc_get_volsw_2r_sx - double with tlv and variable data size
 *  mixer get callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Returns 0 for success.
 */
int snd_soc_get_volsw_2r_sx(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int mask = (1<<mc->shift)-1;
	int min = mc->min;
	int val = snd_soc_read(codec, mc->reg) & mask;
	int valr = snd_soc_read(codec, mc->rreg) & mask;

	ucontrol->value.integer.value[0] = ((val & 0xff)-min) & mask;
	ucontrol->value.integer.value[1] = ((valr & 0xff)-min) & mask;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_get_volsw_2r_sx);

/**
 * snd_soc_put_volsw_2r_sx - double with tlv and variable data size
 *  mixer put callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Returns 0 for success.
 */
int snd_soc_put_volsw_2r_sx(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int mask = (1<<mc->shift)-1;
	int min = mc->min;
	int ret;
	unsigned int val, valr, oval, ovalr;

	val = ((ucontrol->value.integer.value[0]+min) & 0xff);
	val &= mask;
	valr = ((ucontrol->value.integer.value[1]+min) & 0xff);
	valr &= mask;

	oval = snd_soc_read(codec, mc->reg) & mask;
	ovalr = snd_soc_read(codec, mc->rreg) & mask;

	ret = 0;
	if (oval != val) {
		ret = snd_soc_write(codec, mc->reg, val);
		if (ret < 0)
			return ret;
	}
	if (ovalr != valr) {
		ret = snd_soc_write(codec, mc->rreg, valr);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_put_volsw_2r_sx);

/**
 * snd_soc_dai_set_sysclk - configure DAI system or master clock.
 * @dai: DAI
 * @clk_id: DAI specific clock ID
 * @freq: new clock frequency in Hz
 * @dir: new clock direction - input/output.
 *
 * Configures the DAI master (MCLK) or system (SYSCLK) clocking.
 */
int snd_soc_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
	unsigned int freq, int dir)
{
	if (dai->ops && dai->ops->set_sysclk)
		return dai->ops->set_sysclk(dai, clk_id, freq, dir);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_sysclk);

/**
 * snd_soc_dai_set_clkdiv - configure DAI clock dividers.
 * @dai: DAI
 * @div_id: DAI specific clock divider ID
 * @div: new clock divisor.
 *
 * Configures the clock dividers. This is used to derive the best DAI bit and
 * frame clocks from the system or master clock. It's best to set the DAI bit
 * and frame clocks as low as possible to save system power.
 */
int snd_soc_dai_set_clkdiv(struct snd_soc_dai *dai,
	int div_id, int div)
{
	if (dai->ops && dai->ops->set_clkdiv)
		return dai->ops->set_clkdiv(dai, div_id, div);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_clkdiv);

/**
 * snd_soc_dai_set_pll - configure DAI PLL.
 * @dai: DAI
 * @pll_id: DAI specific PLL ID
 * @source: DAI specific source for the PLL
 * @freq_in: PLL input clock frequency in Hz
 * @freq_out: requested PLL output clock frequency in Hz
 *
 * Configures and enables PLL to generate output clock based on input clock.
 */
int snd_soc_dai_set_pll(struct snd_soc_dai *dai, int pll_id, int source,
	unsigned int freq_in, unsigned int freq_out)
{
	if (dai->ops && dai->ops->set_pll)
		return dai->ops->set_pll(dai, pll_id, source,
					 freq_in, freq_out);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_pll);

/**
 * snd_soc_dai_set_fmt - configure DAI hardware audio format.
 * @dai: DAI
 * @fmt: SND_SOC_DAIFMT_ format value.
 *
 * Configures the DAI hardware format and clocking.
 */
int snd_soc_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	if (dai->ops && dai->ops->set_fmt)
		return dai->ops->set_fmt(dai, fmt);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_fmt);

/**
 * snd_soc_dai_set_tdm_slot - configure DAI TDM.
 * @dai: DAI
 * @tx_mask: bitmask representing active TX slots.
 * @rx_mask: bitmask representing active RX slots.
 * @slots: Number of slots in use.
 * @slot_width: Width in bits for each slot.
 *
 * Configures a DAI for TDM operation. Both mask and slots are codec and DAI
 * specific.
 */
int snd_soc_dai_set_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	if (dai->ops && dai->ops->set_tdm_slot)
		return dai->ops->set_tdm_slot(dai, tx_mask, rx_mask,
				slots, slot_width);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_tdm_slot);

/**
 * snd_soc_dai_set_channel_map - configure DAI audio channel map
 * @dai: DAI
 * @tx_num: how many TX channels
 * @tx_slot: pointer to an array which imply the TX slot number channel
 *           0~num-1 uses
 * @rx_num: how many RX channels
 * @rx_slot: pointer to an array which imply the RX slot number channel
 *           0~num-1 uses
 *
 * configure the relationship between channel number and TDM slot number.
 */
int snd_soc_dai_set_channel_map(struct snd_soc_dai *dai,
	unsigned int tx_num, unsigned int *tx_slot,
	unsigned int rx_num, unsigned int *rx_slot)
{
	if (dai->ops && dai->ops->set_channel_map)
		return dai->ops->set_channel_map(dai, tx_num, tx_slot,
			rx_num, rx_slot);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_channel_map);

/**
 * snd_soc_dai_set_tristate - configure DAI system or master clock.
 * @dai: DAI
 * @tristate: tristate enable
 *
 * Tristates the DAI so that others can use it.
 */
int snd_soc_dai_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	if (dai->ops && dai->ops->set_tristate)
		return dai->ops->set_tristate(dai, tristate);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_tristate);

/**
 * snd_soc_dai_digital_mute - configure DAI system or master clock.
 * @dai: DAI
 * @mute: mute enable
 *
 * Mutes the DAI DAC.
 */
int snd_soc_dai_digital_mute(struct snd_soc_dai *dai, int mute)
{
	if (dai->ops && dai->ops->digital_mute)
		return dai->ops->digital_mute(dai, mute);
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_digital_mute);

/**
 * snd_soc_register_card - Register a card with the ASoC core
 *
 * @card: Card to register
 *
 * Note that currently this is an internal only function: it will be
 * exposed to machine drivers after further backporting of ASoC v2
 * registration APIs.
 */
static int snd_soc_register_card(struct snd_soc_card *card)
{
	if (!card->name || !card->dev)
		return -EINVAL;

	INIT_LIST_HEAD(&card->list);
	card->instantiated = 0;

	mutex_lock(&client_mutex);
	list_add(&card->list, &card_list);
	snd_soc_instantiate_cards();
	mutex_unlock(&client_mutex);

	dev_dbg(card->dev, "Registered card '%s'\n", card->name);

	return 0;
}

/**
 * snd_soc_unregister_card - Unregister a card with the ASoC core
 *
 * @card: Card to unregister
 *
 * Note that currently this is an internal only function: it will be
 * exposed to machine drivers after further backporting of ASoC v2
 * registration APIs.
 */
static int snd_soc_unregister_card(struct snd_soc_card *card)
{
	mutex_lock(&client_mutex);
	list_del(&card->list);
	mutex_unlock(&client_mutex);

	dev_dbg(card->dev, "Unregistered card '%s'\n", card->name);

	return 0;
}

/**
 * snd_soc_register_dai - Register a DAI with the ASoC core
 *
 * @dai: DAI to register
 */
int snd_soc_register_dai(struct snd_soc_dai *dai)
{
	if (!dai->name)
		return -EINVAL;

	/* The device should become mandatory over time */
	if (!dai->dev)
		printk(KERN_WARNING "No device for DAI %s\n", dai->name);

	if (!dai->ops)
		dai->ops = &null_dai_ops;

	INIT_LIST_HEAD(&dai->list);

	mutex_lock(&client_mutex);
	list_add(&dai->list, &dai_list);
	snd_soc_instantiate_cards();
	mutex_unlock(&client_mutex);

	pr_debug("Registered DAI '%s'\n", dai->name);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_register_dai);

/**
 * snd_soc_unregister_dai - Unregister a DAI from the ASoC core
 *
 * @dai: DAI to unregister
 */
void snd_soc_unregister_dai(struct snd_soc_dai *dai)
{
	mutex_lock(&client_mutex);
	list_del(&dai->list);
	mutex_unlock(&client_mutex);

	pr_debug("Unregistered DAI '%s'\n", dai->name);
}
EXPORT_SYMBOL_GPL(snd_soc_unregister_dai);

/**
 * snd_soc_register_dais - Register multiple DAIs with the ASoC core
 *
 * @dai: Array of DAIs to register
 * @count: Number of DAIs
 */
int snd_soc_register_dais(struct snd_soc_dai *dai, size_t count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = snd_soc_register_dai(&dai[i]);
		if (ret != 0)
			goto err;
	}

	return 0;

err:
	for (i--; i >= 0; i--)
		snd_soc_unregister_dai(&dai[i]);

	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_register_dais);

/**
 * snd_soc_unregister_dais - Unregister multiple DAIs from the ASoC core
 *
 * @dai: Array of DAIs to unregister
 * @count: Number of DAIs
 */
void snd_soc_unregister_dais(struct snd_soc_dai *dai, size_t count)
{
	int i;

	for (i = 0; i < count; i++)
		snd_soc_unregister_dai(&dai[i]);
}
EXPORT_SYMBOL_GPL(snd_soc_unregister_dais);

/**
 * snd_soc_register_platform - Register a platform with the ASoC core
 *
 * @platform: platform to register
 */
int snd_soc_register_platform(struct snd_soc_platform *platform)
{
	if (!platform->name)
		return -EINVAL;

	INIT_LIST_HEAD(&platform->list);

	mutex_lock(&client_mutex);
	list_add(&platform->list, &platform_list);
	snd_soc_instantiate_cards();
	mutex_unlock(&client_mutex);

	pr_debug("Registered platform '%s'\n", platform->name);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_register_platform);

/**
 * snd_soc_unregister_platform - Unregister a platform from the ASoC core
 *
 * @platform: platform to unregister
 */
void snd_soc_unregister_platform(struct snd_soc_platform *platform)
{
	mutex_lock(&client_mutex);
	list_del(&platform->list);
	mutex_unlock(&client_mutex);

	pr_debug("Unregistered platform '%s'\n", platform->name);
}
EXPORT_SYMBOL_GPL(snd_soc_unregister_platform);

static u64 codec_format_map[] = {
	SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE,
	SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE,
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE,
	SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_U24_BE,
	SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE,
	SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_U32_BE,
	SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_U24_3BE,
	SNDRV_PCM_FMTBIT_U24_3LE | SNDRV_PCM_FMTBIT_U24_3BE,
	SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE,
	SNDRV_PCM_FMTBIT_U20_3LE | SNDRV_PCM_FMTBIT_U20_3BE,
	SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S18_3BE,
	SNDRV_PCM_FMTBIT_U18_3LE | SNDRV_PCM_FMTBIT_U18_3BE,
	SNDRV_PCM_FMTBIT_FLOAT_LE | SNDRV_PCM_FMTBIT_FLOAT_BE,
	SNDRV_PCM_FMTBIT_FLOAT64_LE | SNDRV_PCM_FMTBIT_FLOAT64_BE,
	SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE
	| SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_BE,
};

/* Fix up the DAI formats for endianness: codecs don't actually see
 * the endianness of the data but we're using the CPU format
 * definitions which do need to include endianness so we ensure that
 * codec DAIs always have both big and little endian variants set.
 */
static void fixup_codec_formats(struct snd_soc_pcm_stream *stream)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(codec_format_map); i++)
		if (stream->formats & codec_format_map[i])
			stream->formats |= codec_format_map[i];
}

/**
 * snd_soc_register_codec - Register a codec with the ASoC core
 *
 * @codec: codec to register
 */
int snd_soc_register_codec(struct snd_soc_codec *codec)
{
	int i;

	if (!codec->name)
		return -EINVAL;

	/* The device should become mandatory over time */
	if (!codec->dev)
		printk(KERN_WARNING "No device for codec %s\n", codec->name);

	INIT_LIST_HEAD(&codec->list);

	for (i = 0; i < codec->num_dai; i++) {
		fixup_codec_formats(&codec->dai[i].playback);
		fixup_codec_formats(&codec->dai[i].capture);
	}

	mutex_lock(&client_mutex);
	list_add(&codec->list, &codec_list);
	snd_soc_instantiate_cards();
	mutex_unlock(&client_mutex);

	pr_debug("Registered codec '%s'\n", codec->name);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_register_codec);

/**
 * snd_soc_unregister_codec - Unregister a codec from the ASoC core
 *
 * @codec: codec to unregister
 */
void snd_soc_unregister_codec(struct snd_soc_codec *codec)
{
	mutex_lock(&client_mutex);
	list_del(&codec->list);
	mutex_unlock(&client_mutex);

	pr_debug("Unregistered codec '%s'\n", codec->name);
}
EXPORT_SYMBOL_GPL(snd_soc_unregister_codec);

static int __init snd_soc_init(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_root = debugfs_create_dir("asoc", NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		printk(KERN_WARNING
		       "ASoC: Failed to create debugfs directory\n");
		debugfs_root = NULL;
	}
#endif

	return platform_driver_register(&soc_driver);
}

static void __exit snd_soc_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(debugfs_root);
#endif
	platform_driver_unregister(&soc_driver);
}

module_init(snd_soc_init);
module_exit(snd_soc_exit);

/* Module information */
MODULE_AUTHOR("Liam Girdwood, lrg@slimlogic.co.uk");
MODULE_DESCRIPTION("ALSA SoC Core");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:soc-audio");
