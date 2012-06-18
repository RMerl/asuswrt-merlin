#ifndef _XT_CONDITION_H
#define _XT_CONDITION_H

#define CONDITION_NAME_LEN  32

struct condition_info {
	char name[CONDITION_NAME_LEN];
	int  invert;
};

#endif /* _XT_CONDITION_H */
