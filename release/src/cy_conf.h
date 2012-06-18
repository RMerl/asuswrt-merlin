/*

	This should be used by the kernel only	-- zzz

*/	

// port_based_qos.c
/*
#define	HW_QOS_SUPPORT	1
#define	CONFIG_HW_QOS	y
#define	__CONFIG_HW_QOS__	1
*/

// port_based_qos.c, dev.c
//#define	PERFORMANCE_SUPPORT	0	<remove the line>
//#define	CONFIG_PERFORMANCE		<remove the line>
#undef	__CONFIG_PERFORMANCE__
