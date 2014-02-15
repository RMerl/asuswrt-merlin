#ifndef _ATE_H_
#define _ATE_H_

extern int Get_USB_Port_Info(const char *port_x);
extern int Get_USB_Port_Folder(const char *port_x);
extern int Get_USB_Port_DataRate(const char *port_x);

#if defined (RTCONFIG_USB_XHCI)
static inline int Get_USB3_Port_Info(const char *port_x)
{
	return Get_USB_Port_Info(port_x);
}

static inline int Get_USB3_Port_Folder(const char *port_x)
{
	return Get_USB_Port_Folder(port_x);
}

static inline int Get_USB3_Port_DataRate(const char *port_x)
{
	return Get_USB_Port_DataRate(port_x);
}
#else
static inline int Get_USB3_Port_Info(const char *port_x)	{ return 0; }
static inline int Get_USB3_Port_Folder(const char *port_x)	{ return 0; }
static inline int Get_USB3_Port_DataRate(const char *port_x)	{ return 0; }
#endif

#endif
