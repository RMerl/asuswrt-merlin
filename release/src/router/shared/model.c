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
	{ "RT-AC66U",	MODEL_RTAC66U	},
	{ "RT-N53",	MODEL_RTN53	},
	{ "RT-N16",	MODEL_RTN16	},
	{ "RT-N15U",	MODEL_RTN15U	},
	{ "RT-N12",	MODEL_RTN12	},
	{ "RT-N12B1",	MODEL_RTN12B1	},
	{ "RT-N12C1",	MODEL_RTN12C1	},
	{ "RT-N12D1",	MODEL_RTN12D1	},
	{ "RT-N12HP",	MODEL_RTN12HP	},
	{ "RT-N10U",	MODEL_RTN10U	},
	{ "RT-N10D1",	MODEL_RTN10D1	},
#else	/* RTCONFIG_RALINK */
#ifdef RTCONFIG_DSL
	{ "DSL-N55U",	MODEL_DSLN55U	},
	{ "DSL-N55U-B",	MODEL_DSLN55U	},
#endif
	{ "EA-N66",	MODEL_EAN66	},
	{ "RT-N13U",	MODEL_RTN13U	},
	{ "RT-N36U3",	MODEL_RTN36U3	},
	{ "RT-N56U",	MODEL_RTN56U	},
	{ "RT-N65U",	MODEL_RTN65U	},
	{ "RT-N67U",	MODEL_RTN67U	},
#endif	/* !RTCONFIG_RALINK */
	{ NULL, 0 },
};

#if !defined(RTCONFIG_RALINK)
int get_model_by_hw_rtn12(void)
{
	char *hw_version = nvram_safe_get("hardware_version");

	if (strncmp(hw_version, "RTN12", 5) != 0)
		return MODEL_UNKNOWN;
	else if (strncmp(hw_version, "RTN12B1", 7) == 0)
		return MODEL_RTN12B1;
	else if (strncmp(hw_version, "RTN12C1", 7) == 0)
		return MODEL_RTN12C1;
	else if (strncmp(hw_version, "RTN12D1", 7) == 0)
		return MODEL_RTN12D1;
	else if (strncmp(hw_version, "RTN12HP", 7) == 0)
		return MODEL_RTN12HP;

	return MODEL_RTN12;
}
#endif

int get_model(void)
{
	int model = MODEL_UNKNOWN;
	char *pid = nvram_safe_get("productid");
	const struct model_s *p;

	for (p = &model_list[0]; p->pid; ++p) {
		if (!strcmp(pid, p->pid)) {
			model = p->model;
			break;
		}
	}
#if !defined(RTCONFIG_RALINK)
	if (model == MODEL_RTN12)
		model = get_model_by_hw_rtn12();
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
