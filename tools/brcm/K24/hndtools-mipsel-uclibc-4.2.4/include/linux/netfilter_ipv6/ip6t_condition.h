#ifndef __IP6T_CONDITION_MATCH__
#define __IP6T_CONDITION_MATCH__

#define CONDITION6_NAME_LEN  32

struct condition6_info {
	char name[CONDITION6_NAME_LEN];
	int  invert;
};

#endif
