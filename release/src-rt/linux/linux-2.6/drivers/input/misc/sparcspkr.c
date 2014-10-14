/*
 *  Driver for PC-speaker like devices found on various Sparc systems.
 *
 *  Copyright (c) 2002 Vojtech Pavlik
 *  Copyright (c) 2002, 2006, 2008 David S. Miller (davem@davemloft.net)
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include <asm/io.h>

MODULE_AUTHOR("David S. Miller <davem@davemloft.net>");
MODULE_DESCRIPTION("Sparc Speaker beeper driver");
MODULE_LICENSE("GPL");

struct grover_beep_info {
	void __iomem	*freq_regs;
	void __iomem	*enable_reg;
};

struct bbc_beep_info {
	u32		clock_freq;
	void __iomem	*regs;
};

struct sparcspkr_state {
	const char		*name;
	int (*event)(struct input_dev *dev, unsigned int type, unsigned int code, int value);
	spinlock_t		lock;
	struct input_dev	*input_dev;
	union {
		struct grover_beep_info grover;
		struct bbc_beep_info bbc;
	} u;
};

static u32 bbc_count_to_reg(struct bbc_beep_info *info, unsigned int count)
{
	u32 val, clock_freq = info->clock_freq;
	int i;

	if (!count)
		return 0;

	if (count <= clock_freq >> 20)
		return 1 << 18;

	if (count >= clock_freq >> 12)
		return 1 << 10;

	val = 1 << 18;
	for (i = 19; i >= 11; i--) {
		val >>= 1;
		if (count <= clock_freq >> i)
			break;
	}

	return val;
}

static int bbc_spkr_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	struct sparcspkr_state *state = dev_get_drvdata(dev->dev.parent);
	struct bbc_beep_info *info = &state->u.bbc;
	unsigned int count = 0;
	unsigned long flags;

	if (type != EV_SND)
		return -1;

	switch (code) {
		case SND_BELL: if (value) value = 1000;
		case SND_TONE: break;
		default: return -1;
	}

	if (value > 20 && value < 32767)
		count = 1193182 / value;

	count = bbc_count_to_reg(info, count);

	spin_lock_irqsave(&state->lock, flags);

	if (count) {
		outb(0x01,                 info->regs + 0);
		outb(0x00,                 info->regs + 2);
		outb((count >> 16) & 0xff, info->regs + 3);
		outb((count >>  8) & 0xff, info->regs + 4);
		outb(0x00,                 info->regs + 5);
	} else {
		outb(0x00,                 info->regs + 0);
	}

	spin_unlock_irqrestore(&state->lock, flags);

	return 0;
}

static int grover_spkr_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	struct sparcspkr_state *state = dev_get_drvdata(dev->dev.parent);
	struct grover_beep_info *info = &state->u.grover;
	unsigned int count = 0;
	unsigned long flags;

	if (type != EV_SND)
		return -1;

	switch (code) {
		case SND_BELL: if (value) value = 1000;
		case SND_TONE: break;
		default: return -1;
	}

	if (value > 20 && value < 32767)
		count = 1193182 / value;

	spin_lock_irqsave(&state->lock, flags);

	if (count) {
		/* enable counter 2 */
		outb(inb(info->enable_reg) | 3, info->enable_reg);
		/* set command for counter 2, 2 byte write */
		outb(0xB6, info->freq_regs + 1);
		/* select desired HZ */
		outb(count & 0xff, info->freq_regs + 0);
		outb((count >> 8) & 0xff, info->freq_regs + 0);
	} else {
		/* disable counter 2 */
		outb(inb_p(info->enable_reg) & 0xFC, info->enable_reg);
	}

	spin_unlock_irqrestore(&state->lock, flags);

	return 0;
}

static int __devinit sparcspkr_probe(struct device *dev)
{
	struct sparcspkr_state *state = dev_get_drvdata(dev);
	struct input_dev *input_dev;
	int error;

	input_dev = input_allocate_device();
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = state->name;
	input_dev->phys = "sparc/input0";
	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x001f;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->dev.parent = dev;

	input_dev->evbit[0] = BIT_MASK(EV_SND);
	input_dev->sndbit[0] = BIT_MASK(SND_BELL) | BIT_MASK(SND_TONE);

	input_dev->event = state->event;

	error = input_register_device(input_dev);
	if (error) {
		input_free_device(input_dev);
		return error;
	}

	state->input_dev = input_dev;

	return 0;
}

static void sparcspkr_shutdown(struct platform_device *dev)
{
	struct sparcspkr_state *state = dev_get_drvdata(&dev->dev);
	struct input_dev *input_dev = state->input_dev;

	/* turn off the speaker */
	state->event(input_dev, EV_SND, SND_BELL, 0);
}

static int __devinit bbc_beep_probe(struct platform_device *op)
{
	struct sparcspkr_state *state;
	struct bbc_beep_info *info;
	struct device_node *dp;
	int err = -ENOMEM;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		goto out_err;

	state->name = "Sparc BBC Speaker";
	state->event = bbc_spkr_event;
	spin_lock_init(&state->lock);

	dp = of_find_node_by_path("/");
	err = -ENODEV;
	if (!dp)
		goto out_free;

	info = &state->u.bbc;
	info->clock_freq = of_getintprop_default(dp, "clock-frequency", 0);
	if (!info->clock_freq)
		goto out_free;

	info->regs = of_ioremap(&op->resource[0], 0, 6, "bbc beep");
	if (!info->regs)
		goto out_free;

	dev_set_drvdata(&op->dev, state);

	err = sparcspkr_probe(&op->dev);
	if (err)
		goto out_clear_drvdata;

	return 0;

out_clear_drvdata:
	dev_set_drvdata(&op->dev, NULL);
	of_iounmap(&op->resource[0], info->regs, 6);

out_free:
	kfree(state);
out_err:
	return err;
}

static int __devexit bbc_remove(struct platform_device *op)
{
	struct sparcspkr_state *state = dev_get_drvdata(&op->dev);
	struct input_dev *input_dev = state->input_dev;
	struct bbc_beep_info *info = &state->u.bbc;

	/* turn off the speaker */
	state->event(input_dev, EV_SND, SND_BELL, 0);

	input_unregister_device(input_dev);

	of_iounmap(&op->resource[0], info->regs, 6);

	dev_set_drvdata(&op->dev, NULL);
	kfree(state);

	return 0;
}

static const struct of_device_id bbc_beep_match[] = {
	{
		.name = "beep",
		.compatible = "SUNW,bbc-beep",
	},
	{},
};

static struct platform_driver bbc_beep_driver = {
	.driver = {
		.name = "bbcbeep",
		.owner = THIS_MODULE,
		.of_match_table = bbc_beep_match,
	},
	.probe		= bbc_beep_probe,
	.remove		= __devexit_p(bbc_remove),
	.shutdown	= sparcspkr_shutdown,
};

static int __devinit grover_beep_probe(struct platform_device *op)
{
	struct sparcspkr_state *state;
	struct grover_beep_info *info;
	int err = -ENOMEM;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		goto out_err;

	state->name = "Sparc Grover Speaker";
	state->event = grover_spkr_event;
	spin_lock_init(&state->lock);

	info = &state->u.grover;
	info->freq_regs = of_ioremap(&op->resource[2], 0, 2, "grover beep freq");
	if (!info->freq_regs)
		goto out_free;

	info->enable_reg = of_ioremap(&op->resource[3], 0, 1, "grover beep enable");
	if (!info->enable_reg)
		goto out_unmap_freq_regs;

	dev_set_drvdata(&op->dev, state);

	err = sparcspkr_probe(&op->dev);
	if (err)
		goto out_clear_drvdata;

	return 0;

out_clear_drvdata:
	dev_set_drvdata(&op->dev, NULL);
	of_iounmap(&op->resource[3], info->enable_reg, 1);

out_unmap_freq_regs:
	of_iounmap(&op->resource[2], info->freq_regs, 2);
out_free:
	kfree(state);
out_err:
	return err;
}

static int __devexit grover_remove(struct platform_device *op)
{
	struct sparcspkr_state *state = dev_get_drvdata(&op->dev);
	struct grover_beep_info *info = &state->u.grover;
	struct input_dev *input_dev = state->input_dev;

	/* turn off the speaker */
	state->event(input_dev, EV_SND, SND_BELL, 0);

	input_unregister_device(input_dev);

	of_iounmap(&op->resource[3], info->enable_reg, 1);
	of_iounmap(&op->resource[2], info->freq_regs, 2);

	dev_set_drvdata(&op->dev, NULL);
	kfree(state);

	return 0;
}

static const struct of_device_id grover_beep_match[] = {
	{
		.name = "beep",
		.compatible = "SUNW,smbus-beep",
	},
	{},
};

static struct platform_driver grover_beep_driver = {
	.driver = {
		.name = "groverbeep",
		.owner = THIS_MODULE,
		.of_match_table = grover_beep_match,
	},
	.probe		= grover_beep_probe,
	.remove		= __devexit_p(grover_remove),
	.shutdown	= sparcspkr_shutdown,
};

static int __init sparcspkr_init(void)
{
	int err = platform_driver_register(&bbc_beep_driver);

	if (!err) {
		err = platform_driver_register(&grover_beep_driver);
		if (err)
			platform_driver_unregister(&bbc_beep_driver);
	}

	return err;
}

static void __exit sparcspkr_exit(void)
{
	platform_driver_unregister(&bbc_beep_driver);
	platform_driver_unregister(&grover_beep_driver);
}

module_init(sparcspkr_init);
module_exit(sparcspkr_exit);
