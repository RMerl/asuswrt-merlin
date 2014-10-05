#ifndef __LINUX_SHAPER_H
#define __LINUX_SHAPER_H


#define SHAPER_SET_DEV		0x0001
#define SHAPER_SET_SPEED	0x0002
#define SHAPER_GET_DEV		0x0003
#define SHAPER_GET_SPEED	0x0004

struct shaperconf
{
	__u16	ss_cmd;
	union
	{
		char 	ssu_name[14];
		__u32	ssu_speed;
	} ss_u;
#define ss_speed ss_u.ssu_speed
#define ss_name ss_u.ssu_name
};

#endif
