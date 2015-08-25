#ifndef __AT_CMD_H__
#define __AT_CMD_H__


extern char *Gobi_SimCard(char *line, int size);	//get sim card status line
extern int Gobi_SimCardReady(const char *line);		//check status via status line

extern char *Gobi_IMEI(char *line, int size);		//get device IMEI number

extern char *Gobi_ConnectISP(char *line, int size);	//get current connect ISP name

extern int Gobi_ConnectStatus_Int(void);		//get current connect status number (GSM/…/CDMA/LTE)
extern char *Gobi_ConnectStatus_Str(int status);	//convert status number to string

extern int Gobi_SignalQuality_Int(void);		//get signal quality value (0~31)
extern int Gobi_SignalQuality_Percent(int value);	//convert quality value (0~31) to percentage

extern int Gobi_SignalLevel_Int(void);			//get signal quality (level) in dBm

char * Gobi_FwVersion(char *line, int size);		//get FW version string
char * Gobi_QcnVersion(char *line, int size);		//get Qcn version string

char * Gobi_SelectBand(const char *band, char *line, int size);	//set LTE band. could be B3/B7/B20/B38 .
char * Gobi_BandChannel(char *line, int size);		//get Band and Channel

#endif	/* ! __AT_CMD_H__ */
