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

#ifdef RTCONFIG_QCA
extern int setAllLedOn2(void);
#else
static inline int setAllLedOn2(void)
{
	puts("0");
	return 0;
}
#endif

#if defined(RTCONFIG_TCODE)
extern int getTerritoryCode(void);
extern int setTerritoryCode(const char *tcode);
#else
static inline int getTerritoryCode(void) { return -1; }
static inline int setTerritoryCode(const char *tcode) { return -1; }
#endif

/* fail log */
#define FAIL_LOG_MAX 100
struct FAIL_LOG
{
	unsigned char num;
	unsigned char bits[15];
};

extern void Get_fail_log(char *buf, int size, unsigned int offset);
extern void Gen_fail_log(const char *logStr, int max, struct FAIL_LOG *log);


#endif
