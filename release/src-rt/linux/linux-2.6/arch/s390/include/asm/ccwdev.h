/*
 * Copyright  IBM Corp. 2002, 2009
 *
 * Author(s): Arnd Bergmann <arndb@de.ibm.com>
 *
 * Interface for CCW device drivers
 */
#ifndef _S390_CCWDEV_H_
#define _S390_CCWDEV_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <asm/fcx.h>

/* structs from asm/cio.h */
struct irb;
struct ccw1;
struct ccw_dev_id;

/* simplified initializers for struct ccw_device:
 * CCW_DEVICE and CCW_DEVICE_DEVTYPE initialize one
 * entry in your MODULE_DEVICE_TABLE and set the match_flag correctly */
#define CCW_DEVICE(cu, cum) 						\
	.cu_type=(cu), .cu_model=(cum),					\
	.match_flags=(CCW_DEVICE_ID_MATCH_CU_TYPE			\
		   | (cum ? CCW_DEVICE_ID_MATCH_CU_MODEL : 0))

#define CCW_DEVICE_DEVTYPE(cu, cum, dev, devm)				\
	.cu_type=(cu), .cu_model=(cum), .dev_type=(dev), .dev_model=(devm),\
	.match_flags=CCW_DEVICE_ID_MATCH_CU_TYPE			\
		   | ((cum) ? CCW_DEVICE_ID_MATCH_CU_MODEL : 0) 	\
		   | CCW_DEVICE_ID_MATCH_DEVICE_TYPE			\
		   | ((devm) ? CCW_DEVICE_ID_MATCH_DEVICE_MODEL : 0)

/* scan through an array of device ids and return the first
 * entry that matches the device.
 *
 * the array must end with an entry containing zero match_flags
 */
static inline const struct ccw_device_id *
ccw_device_id_match(const struct ccw_device_id *array,
			const struct ccw_device_id *match)
{
	const struct ccw_device_id *id = array;

	for (id = array; id->match_flags; id++) {
		if ((id->match_flags & CCW_DEVICE_ID_MATCH_CU_TYPE)
		    && (id->cu_type != match->cu_type))
			continue;

		if ((id->match_flags & CCW_DEVICE_ID_MATCH_CU_MODEL)
		    && (id->cu_model != match->cu_model))
			continue;

		if ((id->match_flags & CCW_DEVICE_ID_MATCH_DEVICE_TYPE)
		    && (id->dev_type != match->dev_type))
			continue;

		if ((id->match_flags & CCW_DEVICE_ID_MATCH_DEVICE_MODEL)
		    && (id->dev_model != match->dev_model))
			continue;

		return id;
	}

	return NULL;
}

/**
 * struct ccw_device - channel attached device
 * @ccwlock: pointer to device lock
 * @id: id of this device
 * @drv: ccw driver for this device
 * @dev: embedded device structure
 * @online: online status of device
 * @handler: interrupt handler
 *
 * @handler is a member of the device rather than the driver since a driver
 * can have different interrupt handlers for different ccw devices
 * (multi-subchannel drivers).
 */
struct ccw_device {
	spinlock_t *ccwlock;
/* private: */
	struct ccw_device_private *private;	/* cio private information */
/* public: */
	struct ccw_device_id id;
	struct ccw_driver *drv;
	struct device dev;
	int online;
	void (*handler) (struct ccw_device *, unsigned long, struct irb *);
};

/*
 * Possible events used by the path_event notifier.
 */
#define PE_NONE				0x0
#define PE_PATH_GONE			0x1 /* A path is no longer available. */
#define PE_PATH_AVAILABLE		0x2 /* A path has become available and
					       was successfully verified. */
#define PE_PATHGROUP_ESTABLISHED	0x4 /* A pathgroup was reset and had
					       to be established again. */

/*
 * Possible CIO actions triggered by the unit check handler.
 */
enum uc_todo {
	UC_TODO_RETRY,
	UC_TODO_RETRY_ON_NEW_PATH,
	UC_TODO_STOP
};

/**
 * struct ccw driver - device driver for channel attached devices
 * @ids: ids supported by this driver
 * @probe: function called on probe
 * @remove: function called on remove
 * @set_online: called when setting device online
 * @set_offline: called when setting device offline
 * @notify: notify driver of device state changes
 * @path_event: notify driver of channel path events
 * @shutdown: called at device shutdown
 * @prepare: prepare for pm state transition
 * @complete: undo work done in @prepare
 * @freeze: callback for freezing during hibernation snapshotting
 * @thaw: undo work done in @freeze
 * @restore: callback for restoring after hibernation
 * @uc_handler: callback for unit check handler
 * @driver: embedded device driver structure
 */
struct ccw_driver {
	struct ccw_device_id *ids;
	int (*probe) (struct ccw_device *);
	void (*remove) (struct ccw_device *);
	int (*set_online) (struct ccw_device *);
	int (*set_offline) (struct ccw_device *);
	int (*notify) (struct ccw_device *, int);
	void (*path_event) (struct ccw_device *, int *);
	void (*shutdown) (struct ccw_device *);
	int (*prepare) (struct ccw_device *);
	void (*complete) (struct ccw_device *);
	int (*freeze)(struct ccw_device *);
	int (*thaw) (struct ccw_device *);
	int (*restore)(struct ccw_device *);
	enum uc_todo (*uc_handler) (struct ccw_device *, struct irb *);
	struct device_driver driver;
};

extern struct ccw_device *get_ccwdev_by_busid(struct ccw_driver *cdrv,
					      const char *bus_id);

/* devices drivers call these during module load and unload.
 * When a driver is registered, its probe method is called
 * when new devices for its type pop up */
extern int  ccw_driver_register   (struct ccw_driver *driver);
extern void ccw_driver_unregister (struct ccw_driver *driver);

struct ccw1;

extern int ccw_device_set_options_mask(struct ccw_device *, unsigned long);
extern int ccw_device_set_options(struct ccw_device *, unsigned long);
extern void ccw_device_clear_options(struct ccw_device *, unsigned long);
int ccw_device_is_pathgroup(struct ccw_device *cdev);
int ccw_device_is_multipath(struct ccw_device *cdev);

/* Allow for i/o completion notification after primary interrupt status. */
#define CCWDEV_EARLY_NOTIFICATION	0x0001
/* Report all interrupt conditions. */
#define CCWDEV_REPORT_ALL	 	0x0002
/* Try to perform path grouping. */
#define CCWDEV_DO_PATHGROUP             0x0004
/* Allow forced onlining of boxed devices. */
#define CCWDEV_ALLOW_FORCE              0x0008
/* Try to use multipath mode. */
#define CCWDEV_DO_MULTIPATH		0x0010

extern int ccw_device_start(struct ccw_device *, struct ccw1 *,
			    unsigned long, __u8, unsigned long);
extern int ccw_device_start_timeout(struct ccw_device *, struct ccw1 *,
				    unsigned long, __u8, unsigned long, int);
extern int ccw_device_start_key(struct ccw_device *, struct ccw1 *,
				unsigned long, __u8, __u8, unsigned long);
extern int ccw_device_start_timeout_key(struct ccw_device *, struct ccw1 *,
					unsigned long, __u8, __u8,
					unsigned long, int);


extern int ccw_device_resume(struct ccw_device *);
extern int ccw_device_halt(struct ccw_device *, unsigned long);
extern int ccw_device_clear(struct ccw_device *, unsigned long);
int ccw_device_tm_start_key(struct ccw_device *cdev, struct tcw *tcw,
			    unsigned long intparm, u8 lpm, u8 key);
int ccw_device_tm_start_key(struct ccw_device *, struct tcw *,
			    unsigned long, u8, u8);
int ccw_device_tm_start_timeout_key(struct ccw_device *, struct tcw *,
			    unsigned long, u8, u8, int);
int ccw_device_tm_start(struct ccw_device *, struct tcw *,
			    unsigned long, u8);
int ccw_device_tm_start_timeout(struct ccw_device *, struct tcw *,
			    unsigned long, u8, int);
int ccw_device_tm_intrg(struct ccw_device *cdev);

int ccw_device_get_mdc(struct ccw_device *cdev, u8 mask);

extern int ccw_device_set_online(struct ccw_device *cdev);
extern int ccw_device_set_offline(struct ccw_device *cdev);


extern struct ciw *ccw_device_get_ciw(struct ccw_device *, __u32 cmd);
extern __u8 ccw_device_get_path_mask(struct ccw_device *);
extern void ccw_device_get_id(struct ccw_device *, struct ccw_dev_id *);

#define get_ccwdev_lock(x) (x)->ccwlock

#define to_ccwdev(n) container_of(n, struct ccw_device, dev)
#define to_ccwdrv(n) container_of(n, struct ccw_driver, driver)

extern struct ccw_device *ccw_device_probe_console(void);
extern int ccw_device_force_console(void);

int ccw_device_siosl(struct ccw_device *);

// FIXME: these have to go
extern int _ccw_device_get_subchannel_number(struct ccw_device *);

extern void *ccw_device_get_chp_desc(struct ccw_device *, int);
#endif /* _S390_CCWDEV_H_ */
