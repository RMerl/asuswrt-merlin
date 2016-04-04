 /*
 * Copyright 2015, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#ifndef __nt_center_h__
#define __nt_center_h__

#include <libnt.h>

/* NOTIFY CENTER CHECK EVENT ACTION STRUCTURE
---------------------------------*/

typedef struct __nt_check_switch__t_
{
	int	event;		//Refer NOTIFY CLIENT EVENT DEFINE
	int	notify_sw;	//Notify switch refer NOTIFY_XXX define
	char	msg[100];	//Info

}NT_CHECK_SWITCH_T;



#endif
