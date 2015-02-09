/* $Id: isdn_divertif.h,v 1.4.6.1 2001/09/23 22:25:05 Exp $
 *
 * Header for the diversion supplementary interface for i4l.
 *
 * Author    Werner Cornelius (werner@titro.de)
 * Copyright by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */


/***********************************************************/
/* magic value is also used to control version information */
/***********************************************************/
#define DIVERT_IF_MAGIC 0x25873401
#define DIVERT_CMD_REG  0x00  /* register command */
#define DIVERT_CMD_REL  0x01  /* release command */
#define DIVERT_NO_ERR   0x00  /* return value no error */
#define DIVERT_CMD_ERR  0x01  /* invalid cmd */
#define DIVERT_VER_ERR  0x02  /* magic/version invalid */
#define DIVERT_REG_ERR  0x03  /* module already registered */
#define DIVERT_REL_ERR  0x04  /* module not registered */
#define DIVERT_REG_NAME isdn_register_divert

#ifdef __KERNEL__
#include <linux/isdnif.h>
#include <linux/types.h>

/***************************************************************/
/* structure exchanging data between isdn hl and divert module */
/***************************************************************/ 
typedef struct
  { ulong if_magic; /* magic info and version */
    int cmd; /* command */
    int (*stat_callback)(isdn_ctrl *); /* supplied by divert module when calling */
    int (*ll_cmd)(isdn_ctrl *); /* supplied by hl on return */
    char * (*drv_to_name)(int); /* map a driver id to name, supplied by hl */
    int (*name_to_drv)(char *); /* map a driver id to name, supplied by hl */
  } isdn_divert_if;

/*********************/
/* function register */
/*********************/
extern int DIVERT_REG_NAME(isdn_divert_if *);
#endif
