#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bcmnvram.h>
#include <bcmdevs.h>
#include "shared.h"

struct model_s {
	char *pid;
	int model;
};

static const struct model_s model_list[] = {
#if !defined(RTCONFIG_RALINK)
	{ "RT-N66U",	MODEL_RTN66U	},
	{ "RT-AC56U",	MODEL_RTAC56U	},
	{ "RT-AC66U",	MODEL_RTAC66U	},
	{ "RT-AC68U",	MODEL_RTAC68U	},
	{ "RT-N53",	MODEL_RTN53	},
	{ "RT-N16",	MODEL_RTN16	},
	{ "RT-N18UHP",	MODEL_RTN18UHP	},
	{ "RT-N15U",	MODEL_RTN15U	},
	{ "RT-N12",	MODEL_RTN12	},
	{ "RT-N12B1",	MODEL_RTN12B1	},
	{ "RT-N12C1",	MODEL_RTN12C1	},
	{ "RT-N12D1",	MODEL_RTN12D1	},
	{ "RT-N12HP",	MODEL_RTN12HP	},
	{ "AP-N12",	MODEL_APN12	},
	{ "AP-N12HP",	MODEL_APN12HP	},
	{ "RT-N10U",	MODEL_RTN10U	},
	{ "RT-N14UHP",	MODEL_RTN14UHP	},
	{ "RT-N10+",	MODEL_RTN10P	},
	{ "RT-N10P",	MODEL_RTN10P	},
	{ "RT-N10D1",	MODEL_RTN10D1	},
#else	/* RTCONFIG_RALINK */
#ifdef RTCONFIG_DSL
	{ "DSL-N55U",	MODEL_DSLN55U	},
	{ "DSL-N55U-B",	MODEL_DSLN55U	},
#endif
	{ "EA-N66",	MODEL_EAN66	},
	{ "RT-N13U",	MODEL_RTN13U	},
	{ "RT-N14U",	MODEL_RTN14U	},
	{ "RT-AC52U",	MODEL_RTAC52U	},
	{ "RT-N36U3",	MODEL_RTN36U3	},
	{ "RT-N56U",	MODEL_RTN56U	},
	{ "RT-N65U",	MODEL_RTN65U	},
	{ "RT-N67U",	MODEL_RTN67U	},
#endif	/* !RTCONFIG_RALINK */
	{ NULL, 0 },
};

#if !defined(RTCONFIG_RALINK)
static int get_model_by_hw(void)
{
	char *hw_version = nvram_safe_get("hardware_version");

	if (strncmp(hw_version, "RTN12", 5) == 0) {
		if (strncmp(hw_version, "RTN12B1", 7) == 0)
			return MODEL_RTN12B1;
		if (strncmp(hw_version, "RTN12C1", 7) == 0)
			return MODEL_RTN12C1;
		if (strncmp(hw_version, "RTN12D1", 7) == 0)
			return MODEL_RTN12D1;
		if (strncmp(hw_version, "RTN12HP", 7) == 0)
			return MODEL_RTN12HP;
		if (strncmp(hw_version, "APN12HP", 7) == 0)
			return MODEL_APN12HP;
		return MODEL_RTN12;
	} else if (strncmp(hw_version, "APN12", 5) == 0) {
		if (strncmp(hw_version, "APN12HP", 7) == 0)
			return MODEL_APN12HP;
		return MODEL_APN12;
	}
	return MODEL_UNKNOWN;
}
#endif

/* returns MODEL ID
 * result is cached for safe multiple use */
int get_model(void)
{
	static int model = MODEL_UNKNOWN;
	char *pid;
	const struct model_s *p;

	if (model != MODEL_UNKNOWN)
		return model;

	pid = nvram_safe_get("productid");
	for (p = &model_list[0]; p->pid; ++p) {
		if (!strcmp(pid, p->pid)) {
			model = p->model;
			break;
		}
	}
#if !defined(RTCONFIG_RALINK)
	if (model == MODEL_RTN12)
		model = get_model_by_hw();
	if (model == MODEL_APN12)
		model = get_model_by_hw();
#endif
	return model;
}

#if !defined(RTCONFIG_RALINK)
/* returns product id */
char *get_modelid(int model)
{
	char *pid = "unknown";
	const struct model_s *p;

	for (p = &model_list[0]; p->pid; ++p) {
		if (model == p->model) {
			pid = p->pid;
			break;
		}
	}
	return pid;
}
#endif

/* returns SWITCH ID
 * result is cached for safe multiple use */
int get_switch(void)
{
	static int sw_model = SWITCH_UNKNOWN;

	if (sw_model != SWITCH_UNKNOWN)
		return sw_model;

#ifdef BCM5301X
	sw_model = SWITCH_BCM5301x;
#else
	sw_model = get_switch_model();
#endif
	return sw_model;
}
