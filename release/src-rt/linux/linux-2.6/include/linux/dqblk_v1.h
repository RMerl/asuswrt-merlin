/*
 *	File with in-memory structures of old quota format
 */

#ifndef _LINUX_DQBLK_V1_H
#define _LINUX_DQBLK_V1_H

/* Root squash turned on */
#define V1_DQF_RSQUASH 1

/* Numbers of blocks needed for updates */
#define V1_INIT_ALLOC 1
#define V1_INIT_REWRITE 1
#define V1_DEL_ALLOC 0
#define V1_DEL_REWRITE 2

#endif	/* _LINUX_DQBLK_V1_H */
