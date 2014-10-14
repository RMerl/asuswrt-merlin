/*
 * i2c tv tuner chip device driver
 * core core, i.e. kernel interfaces, registering and so on
 *
 * Copyright(c) by Ralph Metzler, Gerd Knorr, Gunther Mayer
 *
 * Copyright(c) 2005-2011 by Mauro Carvalho Chehab
 *	- Added support for a separate Radio tuner
 *	- Major rework and cleanups at the code
 *
 * This driver supports many devices and the idea is to let the driver
 * detect which device is present. So rather than listing all supported
 * devices here, we pretend to support a single, fake device type that will
 * handle both radio and analog TV tuning.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/videodev2.h>
#include <media/tuner.h>
#include <media/tuner-types.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include "mt20xx.h"
#include "tda8290.h"
#include "tea5761.h"
#include "tea5767.h"
#include "tuner-xc2028.h"
#include "tuner-simple.h"
#include "tda9887.h"
#include "xc5000.h"
#include "tda18271.h"

#define UNSET (-1U)

#define PREFIX (t->i2c->driver->driver.name)

/*
 * Driver modprobe parameters
 */

/* insmod options used at init time => read/only */
static unsigned int addr;
static unsigned int no_autodetect;
static unsigned int show_i2c;

module_param(addr, int, 0444);
module_param(no_autodetect, int, 0444);
module_param(show_i2c, int, 0444);

/* insmod options used at runtime => read/write */
static int tuner_debug;
static unsigned int tv_range[2] = { 44, 958 };
static unsigned int radio_range[2] = { 65, 108 };
static char pal[] = "--";
static char secam[] = "--";
static char ntsc[] = "-";

module_param_named(debug, tuner_debug, int, 0644);
module_param_array(tv_range, int, NULL, 0644);
module_param_array(radio_range, int, NULL, 0644);
module_param_string(pal, pal, sizeof(pal), 0644);
module_param_string(secam, secam, sizeof(secam), 0644);
module_param_string(ntsc, ntsc, sizeof(ntsc), 0644);

/*
 * Static vars
 */

static LIST_HEAD(tuner_list);
static const struct v4l2_subdev_ops tuner_ops;

/*
 * Debug macros
 */

#define tuner_warn(fmt, arg...) do {			\
	printk(KERN_WARNING "%s %d-%04x: " fmt, PREFIX, \
	       i2c_adapter_id(t->i2c->adapter),		\
	       t->i2c->addr, ##arg);			\
	 } while (0)

#define tuner_info(fmt, arg...) do {			\
	printk(KERN_INFO "%s %d-%04x: " fmt, PREFIX,	\
	       i2c_adapter_id(t->i2c->adapter),		\
	       t->i2c->addr, ##arg);			\
	 } while (0)

#define tuner_err(fmt, arg...) do {			\
	printk(KERN_ERR "%s %d-%04x: " fmt, PREFIX,	\
	       i2c_adapter_id(t->i2c->adapter),		\
	       t->i2c->addr, ##arg);			\
	 } while (0)

#define tuner_dbg(fmt, arg...) do {				\
	if (tuner_debug)					\
		printk(KERN_DEBUG "%s %d-%04x: " fmt, PREFIX,	\
		       i2c_adapter_id(t->i2c->adapter),		\
		       t->i2c->addr, ##arg);			\
	 } while (0)

/*
 * Internal struct used inside the driver
 */

struct tuner {
	/* device */
	struct dvb_frontend fe;
	struct i2c_client   *i2c;
	struct v4l2_subdev  sd;
	struct list_head    list;

	/* keep track of the current settings */
	v4l2_std_id         std;
	unsigned int        tv_freq;
	unsigned int        radio_freq;
	unsigned int        audmode;

	enum v4l2_tuner_type mode;
	unsigned int        mode_mask; /* Combination of allowable modes */

	bool                standby;	/* Standby mode */

	unsigned int        type; /* chip type id */
	unsigned int        config;
	const char          *name;
};

/*
 * Function prototypes
 */

static void set_tv_freq(struct i2c_client *c, unsigned int freq);
static void set_radio_freq(struct i2c_client *c, unsigned int freq);

/*
 * tuner attach/detach logic
 */

/* This macro allows us to probe dynamically, avoiding static links */
#ifdef CONFIG_MEDIA_ATTACH
#define tuner_symbol_probe(FUNCTION, ARGS...) ({ \
	int __r = -EINVAL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (int) __a(ARGS); \
		symbol_put(FUNCTION); \
	} else { \
		printk(KERN_ERR "TUNER: Unable to find " \
				"symbol "#FUNCTION"()\n"); \
	} \
	__r; \
})

static void tuner_detach(struct dvb_frontend *fe)
{
	if (fe->ops.tuner_ops.release) {
		fe->ops.tuner_ops.release(fe);
		symbol_put_addr(fe->ops.tuner_ops.release);
	}
	if (fe->ops.analog_ops.release) {
		fe->ops.analog_ops.release(fe);
		symbol_put_addr(fe->ops.analog_ops.release);
	}
}
#else
#define tuner_symbol_probe(FUNCTION, ARGS...) ({ \
	FUNCTION(ARGS); \
})

static void tuner_detach(struct dvb_frontend *fe)
{
	if (fe->ops.tuner_ops.release)
		fe->ops.tuner_ops.release(fe);
	if (fe->ops.analog_ops.release)
		fe->ops.analog_ops.release(fe);
}
#endif


static inline struct tuner *to_tuner(struct v4l2_subdev *sd)
{
	return container_of(sd, struct tuner, sd);
}

/*
 * struct analog_demod_ops callbacks
 */

static void fe_set_params(struct dvb_frontend *fe,
			  struct analog_parameters *params)
{
	struct dvb_tuner_ops *fe_tuner_ops = &fe->ops.tuner_ops;
	struct tuner *t = fe->analog_demod_priv;

	if (NULL == fe_tuner_ops->set_analog_params) {
		tuner_warn("Tuner frontend module has no way to set freq\n");
		return;
	}
	fe_tuner_ops->set_analog_params(fe, params);
}

static void fe_standby(struct dvb_frontend *fe)
{
	struct dvb_tuner_ops *fe_tuner_ops = &fe->ops.tuner_ops;

	if (fe_tuner_ops->sleep)
		fe_tuner_ops->sleep(fe);
}

static int fe_has_signal(struct dvb_frontend *fe)
{
	u16 strength = 0;

	if (fe->ops.tuner_ops.get_rf_strength)
		fe->ops.tuner_ops.get_rf_strength(fe, &strength);

	return strength;
}

static int fe_set_config(struct dvb_frontend *fe, void *priv_cfg)
{
	struct dvb_tuner_ops *fe_tuner_ops = &fe->ops.tuner_ops;
	struct tuner *t = fe->analog_demod_priv;

	if (fe_tuner_ops->set_config)
		return fe_tuner_ops->set_config(fe, priv_cfg);

	tuner_warn("Tuner frontend module has no way to set config\n");

	return 0;
}

static void tuner_status(struct dvb_frontend *fe);

static struct analog_demod_ops tuner_analog_ops = {
	.set_params     = fe_set_params,
	.standby        = fe_standby,
	.has_signal     = fe_has_signal,
	.set_config     = fe_set_config,
	.tuner_status   = tuner_status
};

/*
 * Functions to select between radio and TV and tuner probe/remove functions
 */

/**
 * set_type - Sets the tuner type for a given device
 *
 * @c:			i2c_client descriptoy
 * @type:		type of the tuner (e. g. tuner number)
 * @new_mode_mask:	Indicates if tuner supports TV and/or Radio
 * @new_config:		an optional parameter ranging from 0-255 used by
			a few tuners to adjust an internal parameter,
			like LNA mode
 * @tuner_callback:	an optional function to be called when switching
 *			to analog mode
 *
 * This function applys the tuner config to tuner specified
 * by tun_setup structure. It contains several per-tuner initialization "magic"
 */
static void set_type(struct i2c_client *c, unsigned int type,
		     unsigned int new_mode_mask, unsigned int new_config,
		     int (*tuner_callback) (void *dev, int component, int cmd, int arg))
{
	struct tuner *t = to_tuner(i2c_get_clientdata(c));
	struct dvb_tuner_ops *fe_tuner_ops = &t->fe.ops.tuner_ops;
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;
	unsigned char buffer[4];
	int tune_now = 1;

	if (type == UNSET || type == TUNER_ABSENT) {
		tuner_dbg("tuner 0x%02x: Tuner type absent\n", c->addr);
		return;
	}

	t->type = type;
	/* prevent invalid config values */
	t->config = new_config < 256 ? new_config : 0;
	if (tuner_callback != NULL) {
		tuner_dbg("defining GPIO callback\n");
		t->fe.callback = tuner_callback;
	}

	/* discard private data, in case set_type() was previously called */
	tuner_detach(&t->fe);
	t->fe.analog_demod_priv = NULL;

	switch (t->type) {
	case TUNER_MT2032:
		if (!dvb_attach(microtune_attach,
			   &t->fe, t->i2c->adapter, t->i2c->addr))
			goto attach_failed;
		break;
	case TUNER_PHILIPS_TDA8290:
	{
		struct tda829x_config cfg = {
			.lna_cfg        = t->config,
		};
		if (!dvb_attach(tda829x_attach, &t->fe, t->i2c->adapter,
				t->i2c->addr, &cfg))
			goto attach_failed;
		break;
	}
	case TUNER_TEA5767:
		if (!dvb_attach(tea5767_attach, &t->fe,
				t->i2c->adapter, t->i2c->addr))
			goto attach_failed;
		t->mode_mask = T_RADIO;
		break;
	case TUNER_TEA5761:
		if (!dvb_attach(tea5761_attach, &t->fe,
				t->i2c->adapter, t->i2c->addr))
			goto attach_failed;
		t->mode_mask = T_RADIO;
		break;
	case TUNER_PHILIPS_FMD1216ME_MK3:
		buffer[0] = 0x0b;
		buffer[1] = 0xdc;
		buffer[2] = 0x9c;
		buffer[3] = 0x60;
		i2c_master_send(c, buffer, 4);
		mdelay(1);
		buffer[2] = 0x86;
		buffer[3] = 0x54;
		i2c_master_send(c, buffer, 4);
		if (!dvb_attach(simple_tuner_attach, &t->fe,
				t->i2c->adapter, t->i2c->addr, t->type))
			goto attach_failed;
		break;
	case TUNER_PHILIPS_TD1316:
		buffer[0] = 0x0b;
		buffer[1] = 0xdc;
		buffer[2] = 0x86;
		buffer[3] = 0xa4;
		i2c_master_send(c, buffer, 4);
		if (!dvb_attach(simple_tuner_attach, &t->fe,
				t->i2c->adapter, t->i2c->addr, t->type))
			goto attach_failed;
		break;
	case TUNER_XC2028:
	{
		struct xc2028_config cfg = {
			.i2c_adap  = t->i2c->adapter,
			.i2c_addr  = t->i2c->addr,
		};
		if (!dvb_attach(xc2028_attach, &t->fe, &cfg))
			goto attach_failed;
		tune_now = 0;
		break;
	}
	case TUNER_TDA9887:
		if (!dvb_attach(tda9887_attach,
			   &t->fe, t->i2c->adapter, t->i2c->addr))
			goto attach_failed;
		break;
	case TUNER_XC5000:
	{
		struct xc5000_config xc5000_cfg = {
			.i2c_address = t->i2c->addr,
			/* if_khz will be set at dvb_attach() */
			.if_khz	  = 0,
		};

		if (!dvb_attach(xc5000_attach,
				&t->fe, t->i2c->adapter, &xc5000_cfg))
			goto attach_failed;
		tune_now = 0;
		break;
	}
	case TUNER_NXP_TDA18271:
	{
		struct tda18271_config cfg = {
			.config = t->config,
			.small_i2c = TDA18271_03_BYTE_CHUNK_INIT,
		};

		if (!dvb_attach(tda18271_attach, &t->fe, t->i2c->addr,
				t->i2c->adapter, &cfg))
			goto attach_failed;
		tune_now = 0;
		break;
	}
	default:
		if (!dvb_attach(simple_tuner_attach, &t->fe,
				t->i2c->adapter, t->i2c->addr, t->type))
			goto attach_failed;

		break;
	}

	if ((NULL == analog_ops->set_params) &&
	    (fe_tuner_ops->set_analog_params)) {

		t->name = fe_tuner_ops->info.name;

		t->fe.analog_demod_priv = t;
		memcpy(analog_ops, &tuner_analog_ops,
		       sizeof(struct analog_demod_ops));

	} else {
		t->name = analog_ops->info.name;
	}

	tuner_dbg("type set to %s\n", t->name);

	t->mode_mask = new_mode_mask;

	/* Some tuners require more initialization setup before use,
	   such as firmware download or device calibration.
	   trying to set a frequency here will just fail
	   FIXME: better to move set_freq to the tuner code. This is needed
	   on analog tuners for PLL to properly work
	 */
	if (tune_now) {
		if (V4L2_TUNER_RADIO == t->mode)
			set_radio_freq(c, t->radio_freq);
		else
			set_tv_freq(c, t->tv_freq);
	}

	tuner_dbg("%s %s I2C addr 0x%02x with type %d used for 0x%02x\n",
		  c->adapter->name, c->driver->driver.name, c->addr << 1, type,
		  t->mode_mask);
	return;

attach_failed:
	tuner_dbg("Tuner attach for type = %d failed.\n", t->type);
	t->type = TUNER_ABSENT;

	return;
}

/**
 * tuner_s_type_addr - Sets the tuner type for a device
 *
 * @sd:		subdev descriptor
 * @tun_setup:	type to be associated to a given tuner i2c address
 *
 * This function applys the tuner config to tuner specified
 * by tun_setup structure.
 * If tuner I2C address is UNSET, then it will only set the device
 * if the tuner supports the mode specified in the call.
 * If the address is specified, the change will be applied only if
 * tuner I2C address matches.
 * The call can change the tuner number and the tuner mode.
 */
static int tuner_s_type_addr(struct v4l2_subdev *sd,
			     struct tuner_setup *tun_setup)
{
	struct tuner *t = to_tuner(sd);
	struct i2c_client *c = v4l2_get_subdevdata(sd);

	tuner_dbg("Calling set_type_addr for type=%d, addr=0x%02x, mode=0x%02x, config=0x%02x\n",
			tun_setup->type,
			tun_setup->addr,
			tun_setup->mode_mask,
			tun_setup->config);

	if ((t->type == UNSET && ((tun_setup->addr == ADDR_UNSET) &&
	    (t->mode_mask & tun_setup->mode_mask))) ||
	    (tun_setup->addr == c->addr)) {
		set_type(c, tun_setup->type, tun_setup->mode_mask,
			 tun_setup->config, tun_setup->tuner_callback);
	} else
		tuner_dbg("set addr discarded for type %i, mask %x. "
			  "Asked to change tuner at addr 0x%02x, with mask %x\n",
			  t->type, t->mode_mask,
			  tun_setup->addr, tun_setup->mode_mask);

	return 0;
}

/**
 * tuner_s_config - Sets tuner configuration
 *
 * @sd:		subdev descriptor
 * @cfg:	tuner configuration
 *
 * Calls tuner set_config() private function to set some tuner-internal
 * parameters
 */
static int tuner_s_config(struct v4l2_subdev *sd,
			  const struct v4l2_priv_tun_config *cfg)
{
	struct tuner *t = to_tuner(sd);
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	if (t->type != cfg->tuner)
		return 0;

	if (analog_ops->set_config) {
		analog_ops->set_config(&t->fe, cfg->priv);
		return 0;
	}

	tuner_dbg("Tuner frontend module has no way to set config\n");
	return 0;
}

/**
 * tuner_lookup - Seek for tuner adapters
 *
 * @adap:	i2c_adapter struct
 * @radio:	pointer to be filled if the adapter is radio
 * @tv:		pointer to be filled if the adapter is TV
 *
 * Search for existing radio and/or TV tuners on the given I2C adapter,
 * discarding demod-only adapters (tda9887).
 *
 * Note that when this function is called from tuner_probe you can be
 * certain no other devices will be added/deleted at the same time, I2C
 * core protects against that.
 */
static void tuner_lookup(struct i2c_adapter *adap,
		struct tuner **radio, struct tuner **tv)
{
	struct tuner *pos;

	*radio = NULL;
	*tv = NULL;

	list_for_each_entry(pos, &tuner_list, list) {
		int mode_mask;

		if (pos->i2c->adapter != adap ||
		    strcmp(pos->i2c->driver->driver.name, "tuner"))
			continue;

		mode_mask = pos->mode_mask;
		if (*radio == NULL && mode_mask == T_RADIO)
			*radio = pos;
		/* Note: currently TDA9887 is the only demod-only
		   device. If other devices appear then we need to
		   make this test more general. */
		else if (*tv == NULL && pos->type != TUNER_TDA9887 &&
			 (pos->mode_mask & T_ANALOG_TV))
			*tv = pos;
	}
}

/**
 *tuner_probe - Probes the existing tuners on an I2C bus
 *
 * @client:	i2c_client descriptor
 * @id:		not used
 *
 * This routine probes for tuners at the expected I2C addresses. On most
 * cases, if a device answers to a given I2C address, it assumes that the
 * device is a tuner. On a few cases, however, an additional logic is needed
 * to double check if the device is really a tuner, or to identify the tuner
 * type, like on tea5767/5761 devices.
 *
 * During client attach, set_type is called by adapter's attach_inform callback.
 * set_type must then be completed by tuner_probe.
 */
static int tuner_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct tuner *t;
	struct tuner *radio;
	struct tuner *tv;

	t = kzalloc(sizeof(struct tuner), GFP_KERNEL);
	if (NULL == t)
		return -ENOMEM;
	v4l2_i2c_subdev_init(&t->sd, client, &tuner_ops);
	t->i2c = client;
	t->name = "(tuner unset)";
	t->type = UNSET;
	t->audmode = V4L2_TUNER_MODE_STEREO;
	t->standby = 1;
	t->radio_freq = 87.5 * 16000;	/* Initial freq range */
	t->tv_freq = 400 * 16; /* Sets freq to VHF High - needed for some PLL's to properly start */

	if (show_i2c) {
		unsigned char buffer[16];
		int i, rc;

		memset(buffer, 0, sizeof(buffer));
		rc = i2c_master_recv(client, buffer, sizeof(buffer));
		tuner_info("I2C RECV = ");
		for (i = 0; i < rc; i++)
			printk(KERN_CONT "%02x ", buffer[i]);
		printk("\n");
	}

	/* autodetection code based on the i2c addr */
	if (!no_autodetect) {
		switch (client->addr) {
		case 0x10:
			if (tuner_symbol_probe(tea5761_autodetection,
					       t->i2c->adapter,
					       t->i2c->addr) >= 0) {
				t->type = TUNER_TEA5761;
				t->mode_mask = T_RADIO;
				tuner_lookup(t->i2c->adapter, &radio, &tv);
				if (tv)
					tv->mode_mask &= ~T_RADIO;

				goto register_client;
			}
			kfree(t);
			return -ENODEV;
		case 0x42:
		case 0x43:
		case 0x4a:
		case 0x4b:
			/* If chip is not tda8290, don't register.
			   since it can be tda9887*/
			if (tuner_symbol_probe(tda829x_probe, t->i2c->adapter,
					       t->i2c->addr) >= 0) {
				tuner_dbg("tda829x detected\n");
			} else {
				/* Default is being tda9887 */
				t->type = TUNER_TDA9887;
				t->mode_mask = T_RADIO | T_ANALOG_TV;
				goto register_client;
			}
			break;
		case 0x60:
			if (tuner_symbol_probe(tea5767_autodetection,
					       t->i2c->adapter, t->i2c->addr)
					>= 0) {
				t->type = TUNER_TEA5767;
				t->mode_mask = T_RADIO;
				/* Sets freq to FM range */
				tuner_lookup(t->i2c->adapter, &radio, &tv);
				if (tv)
					tv->mode_mask &= ~T_RADIO;

				goto register_client;
			}
			break;
		}
	}

	/* Initializes only the first TV tuner on this adapter. Why only the
	   first? Because there are some devices (notably the ones with TI
	   tuners) that have more than one i2c address for the *same* device.
	   Experience shows that, except for just one case, the first
	   address is the right one. The exception is a Russian tuner
	   (ACORP_Y878F). So, the desired behavior is just to enable the
	   first found TV tuner. */
	tuner_lookup(t->i2c->adapter, &radio, &tv);
	if (tv == NULL) {
		t->mode_mask = T_ANALOG_TV;
		if (radio == NULL)
			t->mode_mask |= T_RADIO;
		tuner_dbg("Setting mode_mask to 0x%02x\n", t->mode_mask);
	}

	/* Should be just before return */
register_client:
	/* Sets a default mode */
	if (t->mode_mask & T_ANALOG_TV)
		t->mode = V4L2_TUNER_ANALOG_TV;
	else
		t->mode = V4L2_TUNER_RADIO;
	set_type(client, t->type, t->mode_mask, t->config, t->fe.callback);
	list_add_tail(&t->list, &tuner_list);

	tuner_info("Tuner %d found with type(s)%s%s.\n",
		   t->type,
		   t->mode_mask & T_RADIO ? " Radio" : "",
		   t->mode_mask & T_ANALOG_TV ? " TV" : "");
	return 0;
}

/**
 * tuner_remove - detaches a tuner
 *
 * @client:	i2c_client descriptor
 */

static int tuner_remove(struct i2c_client *client)
{
	struct tuner *t = to_tuner(i2c_get_clientdata(client));

	v4l2_device_unregister_subdev(&t->sd);
	tuner_detach(&t->fe);
	t->fe.analog_demod_priv = NULL;

	list_del(&t->list);
	kfree(t);
	return 0;
}

/*
 * Functions to switch between Radio and TV
 *
 * A few cards have a separate I2C tuner for radio. Those routines
 * take care of switching between TV/Radio mode, filtering only the
 * commands that apply to the Radio or TV tuner.
 */

/**
 * check_mode - Verify if tuner supports the requested mode
 * @t: a pointer to the module's internal struct_tuner
 *
 * This function checks if the tuner is capable of tuning analog TV,
 * digital TV or radio, depending on what the caller wants. If the
 * tuner can't support that mode, it returns -EINVAL. Otherwise, it
 * returns 0.
 * This function is needed for boards that have a separate tuner for
 * radio (like devices with tea5767).
 */
static inline int check_mode(struct tuner *t, enum v4l2_tuner_type mode)
{
	if ((1 << mode & t->mode_mask) == 0)
		return -EINVAL;

	return 0;
}

/**
 * set_mode - Switch tuner to other mode.
 * @t:		a pointer to the module's internal struct_tuner
 * @mode:	enum v4l2_type (radio or TV)
 *
 * If tuner doesn't support the needed mode (radio or TV), prints a
 * debug message and returns -EINVAL, changing its state to standby.
 * Otherwise, changes the mode and returns 0.
 */
static int set_mode(struct tuner *t, enum v4l2_tuner_type mode)
{
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	if (mode != t->mode) {
		if (check_mode(t, mode) == -EINVAL) {
			tuner_dbg("Tuner doesn't support mode %d. "
				  "Putting tuner to sleep\n", mode);
			t->standby = true;
			if (analog_ops->standby)
				analog_ops->standby(&t->fe);
			return -EINVAL;
		}
		t->mode = mode;
		tuner_dbg("Changing to mode %d\n", mode);
	}
	return 0;
}

/**
 * set_freq - Set the tuner to the desired frequency.
 * @t:		a pointer to the module's internal struct_tuner
 * @freq:	frequency to set (0 means to use the current frequency)
 */
static void set_freq(struct tuner *t, unsigned int freq)
{
	struct i2c_client *client = v4l2_get_subdevdata(&t->sd);

	if (t->mode == V4L2_TUNER_RADIO) {
		if (!freq)
			freq = t->radio_freq;
		set_radio_freq(client, freq);
	} else {
		if (!freq)
			freq = t->tv_freq;
		set_tv_freq(client, freq);
	}
}

/*
 * Functions that are specific for TV mode
 */

/**
 * set_tv_freq - Set tuner frequency,  freq in Units of 62.5 kHz = 1/16MHz
 *
 * @c:	i2c_client descriptor
 * @freq: frequency
 */
static void set_tv_freq(struct i2c_client *c, unsigned int freq)
{
	struct tuner *t = to_tuner(i2c_get_clientdata(c));
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	struct analog_parameters params = {
		.mode      = t->mode,
		.audmode   = t->audmode,
		.std       = t->std
	};

	if (t->type == UNSET) {
		tuner_warn("tuner type not set\n");
		return;
	}
	if (NULL == analog_ops->set_params) {
		tuner_warn("Tuner has no way to set tv freq\n");
		return;
	}
	if (freq < tv_range[0] * 16 || freq > tv_range[1] * 16) {
		tuner_dbg("TV freq (%d.%02d) out of range (%d-%d)\n",
			   freq / 16, freq % 16 * 100 / 16, tv_range[0],
			   tv_range[1]);
		/* V4L2 spec: if the freq is not possible then the closest
		   possible value should be selected */
		if (freq < tv_range[0] * 16)
			freq = tv_range[0] * 16;
		else
			freq = tv_range[1] * 16;
	}
	params.frequency = freq;
	tuner_dbg("tv freq set to %d.%02d\n",
			freq / 16, freq % 16 * 100 / 16);
	t->tv_freq = freq;
	t->standby = false;

	analog_ops->set_params(&t->fe, &params);
}

/**
 * tuner_fixup_std - force a given video standard variant
 *
 * @t:	tuner internal struct
 *
 * A few devices or drivers have problem to detect some standard variations.
 * On other operational systems, the drivers generally have a per-country
 * code, and some logic to apply per-country hacks. V4L2 API doesn't provide
 * such hacks. Instead, it relies on a proper video standard selection from
 * the userspace application. However, as some apps are buggy, not allowing
 * to distinguish all video standard variations, a modprobe parameter can
 * be used to force a video standard match.
 */
static int tuner_fixup_std(struct tuner *t)
{
	if ((t->std & V4L2_STD_PAL) == V4L2_STD_PAL) {
		switch (pal[0]) {
		case '6':
			tuner_dbg("insmod fixup: PAL => PAL-60\n");
			t->std = V4L2_STD_PAL_60;
			break;
		case 'b':
		case 'B':
		case 'g':
		case 'G':
			tuner_dbg("insmod fixup: PAL => PAL-BG\n");
			t->std = V4L2_STD_PAL_BG;
			break;
		case 'i':
		case 'I':
			tuner_dbg("insmod fixup: PAL => PAL-I\n");
			t->std = V4L2_STD_PAL_I;
			break;
		case 'd':
		case 'D':
		case 'k':
		case 'K':
			tuner_dbg("insmod fixup: PAL => PAL-DK\n");
			t->std = V4L2_STD_PAL_DK;
			break;
		case 'M':
		case 'm':
			tuner_dbg("insmod fixup: PAL => PAL-M\n");
			t->std = V4L2_STD_PAL_M;
			break;
		case 'N':
		case 'n':
			if (pal[1] == 'c' || pal[1] == 'C') {
				tuner_dbg("insmod fixup: PAL => PAL-Nc\n");
				t->std = V4L2_STD_PAL_Nc;
			} else {
				tuner_dbg("insmod fixup: PAL => PAL-N\n");
				t->std = V4L2_STD_PAL_N;
			}
			break;
		case '-':
			/* default parameter, do nothing */
			break;
		default:
			tuner_warn("pal= argument not recognised\n");
			break;
		}
	}
	if ((t->std & V4L2_STD_SECAM) == V4L2_STD_SECAM) {
		switch (secam[0]) {
		case 'b':
		case 'B':
		case 'g':
		case 'G':
		case 'h':
		case 'H':
			tuner_dbg("insmod fixup: SECAM => SECAM-BGH\n");
			t->std = V4L2_STD_SECAM_B |
				 V4L2_STD_SECAM_G |
				 V4L2_STD_SECAM_H;
			break;
		case 'd':
		case 'D':
		case 'k':
		case 'K':
			tuner_dbg("insmod fixup: SECAM => SECAM-DK\n");
			t->std = V4L2_STD_SECAM_DK;
			break;
		case 'l':
		case 'L':
			if ((secam[1] == 'C') || (secam[1] == 'c')) {
				tuner_dbg("insmod fixup: SECAM => SECAM-L'\n");
				t->std = V4L2_STD_SECAM_LC;
			} else {
				tuner_dbg("insmod fixup: SECAM => SECAM-L\n");
				t->std = V4L2_STD_SECAM_L;
			}
			break;
		case '-':
			/* default parameter, do nothing */
			break;
		default:
			tuner_warn("secam= argument not recognised\n");
			break;
		}
	}

	if ((t->std & V4L2_STD_NTSC) == V4L2_STD_NTSC) {
		switch (ntsc[0]) {
		case 'm':
		case 'M':
			tuner_dbg("insmod fixup: NTSC => NTSC-M\n");
			t->std = V4L2_STD_NTSC_M;
			break;
		case 'j':
		case 'J':
			tuner_dbg("insmod fixup: NTSC => NTSC_M_JP\n");
			t->std = V4L2_STD_NTSC_M_JP;
			break;
		case 'k':
		case 'K':
			tuner_dbg("insmod fixup: NTSC => NTSC_M_KR\n");
			t->std = V4L2_STD_NTSC_M_KR;
			break;
		case '-':
			/* default parameter, do nothing */
			break;
		default:
			tuner_info("ntsc= argument not recognised\n");
			break;
		}
	}
	return 0;
}

/*
 * Functions that are specific for Radio mode
 */

/**
 * set_radio_freq - Set tuner frequency,  freq in Units of 62.5 Hz  = 1/16kHz
 *
 * @c:	i2c_client descriptor
 * @freq: frequency
 */
static void set_radio_freq(struct i2c_client *c, unsigned int freq)
{
	struct tuner *t = to_tuner(i2c_get_clientdata(c));
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	struct analog_parameters params = {
		.mode      = t->mode,
		.audmode   = t->audmode,
		.std       = t->std
	};

	if (t->type == UNSET) {
		tuner_warn("tuner type not set\n");
		return;
	}
	if (NULL == analog_ops->set_params) {
		tuner_warn("tuner has no way to set radio frequency\n");
		return;
	}
	if (freq < radio_range[0] * 16000 || freq > radio_range[1] * 16000) {
		tuner_dbg("radio freq (%d.%02d) out of range (%d-%d)\n",
			   freq / 16000, freq % 16000 * 100 / 16000,
			   radio_range[0], radio_range[1]);
		/* V4L2 spec: if the freq is not possible then the closest
		   possible value should be selected */
		if (freq < radio_range[0] * 16000)
			freq = radio_range[0] * 16000;
		else
			freq = radio_range[1] * 16000;
	}
	params.frequency = freq;
	tuner_dbg("radio freq set to %d.%02d\n",
			freq / 16000, freq % 16000 * 100 / 16000);
	t->radio_freq = freq;
	t->standby = false;

	analog_ops->set_params(&t->fe, &params);
}

/*
 * Debug function for reporting tuner status to userspace
 */

/**
 * tuner_status - Dumps the current tuner status at dmesg
 * @fe: pointer to struct dvb_frontend
 *
 * This callback is used only for driver debug purposes, answering to
 * VIDIOC_LOG_STATUS. No changes should happen on this call.
 */
static void tuner_status(struct dvb_frontend *fe)
{
	struct tuner *t = fe->analog_demod_priv;
	unsigned long freq, freq_fraction;
	struct dvb_tuner_ops *fe_tuner_ops = &fe->ops.tuner_ops;
	struct analog_demod_ops *analog_ops = &fe->ops.analog_ops;
	const char *p;

	switch (t->mode) {
	case V4L2_TUNER_RADIO:
		p = "radio";
		break;
	case V4L2_TUNER_DIGITAL_TV:
		p = "digital TV";
		break;
	case V4L2_TUNER_ANALOG_TV:
	default:
		p = "analog TV";
		break;
	}
	if (t->mode == V4L2_TUNER_RADIO) {
		freq = t->radio_freq / 16000;
		freq_fraction = (t->radio_freq % 16000) * 100 / 16000;
	} else {
		freq = t->tv_freq / 16;
		freq_fraction = (t->tv_freq % 16) * 100 / 16;
	}
	tuner_info("Tuner mode:      %s%s\n", p,
		   t->standby ? " on standby mode" : "");
	tuner_info("Frequency:       %lu.%02lu MHz\n", freq, freq_fraction);
	tuner_info("Standard:        0x%08lx\n", (unsigned long)t->std);
	if (t->mode != V4L2_TUNER_RADIO)
		return;
	if (fe_tuner_ops->get_status) {
		u32 tuner_status;

		fe_tuner_ops->get_status(&t->fe, &tuner_status);
		if (tuner_status & TUNER_STATUS_LOCKED)
			tuner_info("Tuner is locked.\n");
		if (tuner_status & TUNER_STATUS_STEREO)
			tuner_info("Stereo:          yes\n");
	}
	if (analog_ops->has_signal)
		tuner_info("Signal strength: %d\n",
			   analog_ops->has_signal(fe));
}

/*
 * Function to splicitly change mode to radio. Probably not needed anymore
 */

static int tuner_s_radio(struct v4l2_subdev *sd)
{
	struct tuner *t = to_tuner(sd);

	if (set_mode(t, V4L2_TUNER_RADIO) == 0)
		set_freq(t, 0);
	return 0;
}

/*
 * Tuner callbacks to handle userspace ioctl's
 */

/**
 * tuner_s_power - controls the power state of the tuner
 * @sd: pointer to struct v4l2_subdev
 * @on: a zero value puts the tuner to sleep
 */
static int tuner_s_power(struct v4l2_subdev *sd, int on)
{
	struct tuner *t = to_tuner(sd);
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	/* FIXME: Why this function don't wake the tuner if on != 0 ? */
	if (on)
		return 0;

	tuner_dbg("Putting tuner to sleep\n");
	t->standby = true;
	if (analog_ops->standby)
		analog_ops->standby(&t->fe);
	return 0;
}

static int tuner_s_std(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct tuner *t = to_tuner(sd);

	if (set_mode(t, V4L2_TUNER_ANALOG_TV))
		return 0;

	t->std = std;
	tuner_fixup_std(t);
	set_freq(t, 0);
	return 0;
}

static int tuner_s_frequency(struct v4l2_subdev *sd, struct v4l2_frequency *f)
{
	struct tuner *t = to_tuner(sd);

	if (set_mode(t, f->type) == 0)
		set_freq(t, f->frequency);
	return 0;
}

static int tuner_g_frequency(struct v4l2_subdev *sd, struct v4l2_frequency *f)
{
	struct tuner *t = to_tuner(sd);
	struct dvb_tuner_ops *fe_tuner_ops = &t->fe.ops.tuner_ops;

	if (check_mode(t, f->type) == -EINVAL)
		return 0;
	f->type = t->mode;
	if (fe_tuner_ops->get_frequency && !t->standby) {
		u32 abs_freq;

		fe_tuner_ops->get_frequency(&t->fe, &abs_freq);
		f->frequency = (V4L2_TUNER_RADIO == t->mode) ?
			DIV_ROUND_CLOSEST(abs_freq * 2, 125) :
			DIV_ROUND_CLOSEST(abs_freq, 62500);
	} else {
		f->frequency = (V4L2_TUNER_RADIO == t->mode) ?
			t->radio_freq : t->tv_freq;
	}
	return 0;
}

static int tuner_g_tuner(struct v4l2_subdev *sd, struct v4l2_tuner *vt)
{
	struct tuner *t = to_tuner(sd);
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;
	struct dvb_tuner_ops *fe_tuner_ops = &t->fe.ops.tuner_ops;

	if (check_mode(t, vt->type) == -EINVAL)
		return 0;
	vt->type = t->mode;
	if (analog_ops->get_afc)
		vt->afc = analog_ops->get_afc(&t->fe);
	if (t->mode == V4L2_TUNER_ANALOG_TV)
		vt->capability |= V4L2_TUNER_CAP_NORM;
	if (t->mode != V4L2_TUNER_RADIO) {
		vt->rangelow = tv_range[0] * 16;
		vt->rangehigh = tv_range[1] * 16;
		return 0;
	}

	/* radio mode */
	vt->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	if (fe_tuner_ops->get_status) {
		u32 tuner_status;

		fe_tuner_ops->get_status(&t->fe, &tuner_status);
		vt->rxsubchans =
			(tuner_status & TUNER_STATUS_STEREO) ?
			V4L2_TUNER_SUB_STEREO :
			V4L2_TUNER_SUB_MONO;
	}
	if (analog_ops->has_signal)
		vt->signal = analog_ops->has_signal(&t->fe);
	vt->capability |= V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO;
	vt->audmode = t->audmode;
	vt->rangelow = radio_range[0] * 16000;
	vt->rangehigh = radio_range[1] * 16000;

	return 0;
}

static int tuner_s_tuner(struct v4l2_subdev *sd, struct v4l2_tuner *vt)
{
	struct tuner *t = to_tuner(sd);

	if (set_mode(t, vt->type))
		return 0;

	if (t->mode == V4L2_TUNER_RADIO)
		t->audmode = vt->audmode;
	set_freq(t, 0);

	return 0;
}

static int tuner_log_status(struct v4l2_subdev *sd)
{
	struct tuner *t = to_tuner(sd);
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	if (analog_ops->tuner_status)
		analog_ops->tuner_status(&t->fe);
	return 0;
}

static int tuner_suspend(struct i2c_client *c, pm_message_t state)
{
	struct tuner *t = to_tuner(i2c_get_clientdata(c));
	struct analog_demod_ops *analog_ops = &t->fe.ops.analog_ops;

	tuner_dbg("suspend\n");

	if (!t->standby && analog_ops->standby)
		analog_ops->standby(&t->fe);

	return 0;
}

static int tuner_resume(struct i2c_client *c)
{
	struct tuner *t = to_tuner(i2c_get_clientdata(c));

	tuner_dbg("resume\n");

	if (!t->standby)
		if (set_mode(t, t->mode) == 0)
			set_freq(t, 0);

	return 0;
}

static int tuner_command(struct i2c_client *client, unsigned cmd, void *arg)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	/* TUNER_SET_CONFIG is still called by tuner-simple.c, so we have
	   to handle it here.
	   There must be a better way of doing this... */
	switch (cmd) {
	case TUNER_SET_CONFIG:
		return tuner_s_config(sd, arg);
	}
	return -ENOIOCTLCMD;
}

/*
 * Callback structs
 */

static const struct v4l2_subdev_core_ops tuner_core_ops = {
	.log_status = tuner_log_status,
	.s_std = tuner_s_std,
	.s_power = tuner_s_power,
};

static const struct v4l2_subdev_tuner_ops tuner_tuner_ops = {
	.s_radio = tuner_s_radio,
	.g_tuner = tuner_g_tuner,
	.s_tuner = tuner_s_tuner,
	.s_frequency = tuner_s_frequency,
	.g_frequency = tuner_g_frequency,
	.s_type_addr = tuner_s_type_addr,
	.s_config = tuner_s_config,
};

static const struct v4l2_subdev_ops tuner_ops = {
	.core = &tuner_core_ops,
	.tuner = &tuner_tuner_ops,
};

/*
 * I2C structs and module init functions
 */

static const struct i2c_device_id tuner_id[] = {
	{ "tuner", }, /* autodetect */
	{ }
};
MODULE_DEVICE_TABLE(i2c, tuner_id);

static struct i2c_driver tuner_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tuner",
	},
	.probe		= tuner_probe,
	.remove		= tuner_remove,
	.command	= tuner_command,
	.suspend	= tuner_suspend,
	.resume		= tuner_resume,
	.id_table	= tuner_id,
};

static __init int init_tuner(void)
{
	return i2c_add_driver(&tuner_driver);
}

static __exit void exit_tuner(void)
{
	i2c_del_driver(&tuner_driver);
}

module_init(init_tuner);
module_exit(exit_tuner);

MODULE_DESCRIPTION("device driver for various TV and TV+FM radio tuners");
MODULE_AUTHOR("Ralph Metzler, Gerd Knorr, Gunther Mayer");
MODULE_LICENSE("GPL");
