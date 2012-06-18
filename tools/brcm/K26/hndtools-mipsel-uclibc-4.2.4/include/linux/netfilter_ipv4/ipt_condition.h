#ifndef __IPT_CONDITION_MATCH__
#define __IPT_CONDITION_MATCH__

#define CONDITION_NAME_LEN  32

struct condition_info {
	char name[CONDITION_NAME_LEN];
	int  invert;
};

#endif
