/*
	alert mail for all types of mail information
		- depends on EMAIL=y

	the call function needs to depend on his own flag

	NOTE: if possible, not to add any dynamic library, you can use a new binary for excute process
*/

#include <rc.h>
#include <disk_io_tools.h>

// define function bit, you can define more functions as below
#define TM_WRS_MAIL		0x01
#define TRAFFIC_LIMITER_MAIL	0x02
#define CONFIRM_MAIL		0x04

#define MAIL_CONTENT		"/tmp/push_mail"
#define MAIL_CONF		"/etc/email/email.conf"

static int value = 0;

/* debug message */
#define AM_DEBUG		"/tmp/AM_DEBUG"
#define MyDBG(fmt,args...) \
	if(isFileExist(AM_DEBUG) > 0) { \
		dbg("[ALERT MAIL][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

static int isFileExist(char *fname)
{
	struct stat fstat;
	
	if (lstat(fname,&fstat)==-1)
		return 0;
	if (S_ISREG(fstat.st_mode))
		return 1;
	
	return 0;
}

#ifdef RTCONFIG_TRAFFIC_LIMITER
static
void am_traffic_limiter_mail()
{
	unsigned int flag = traffic_limiter_read_bit("alert");
	unsigned int val = traffic_limiter_read_bit("count"); // have sent
	char *path = NULL;
	int send = 0;

	if((flag & 0x1) && !(val & 0x01)){
		// send alert SMS
		if(nvram_get_int("modem_sms_limit")) traffic_limiter_sendSMS("alert", 0);
		send = 1;
	}
	if((flag & 0x2) && !(val & 0x02)){
		// send alert SMS
		if(nvram_get_int("modem_sms_limit")) traffic_limiter_sendSMS("alert", 1);
		send = 1;
	}

	// send alert mail
	if(send) am_send_mail(TRAFFIC_LIMITER_MAIL , path);
}
#endif


/*
	type :
		as define function TM_WRS_MAIL, TRAFFIC_LIMITER_MAIL, etc.

	path :
		attachment file or some information in file, if no file, path will be NULL
*/
void am_send_mail(int type, char *path)
{
	int lock; // file lock
	FILE *fp;
	char title[64], smtp_auth_pass[256];
	char date[30];
	time_t now;
	int status = 0;
	
	lock = file_lock("email");
	memset(title, 0, sizeof(title));
	memset(date, 0, sizeof(date));
	memset(smtp_auth_pass, 0, sizeof(smtp_auth_pass));

	MyDBG("type=%d, path=%s\n", type, path);

	// get current date
	time(&now);
	StampToDate(now, date);

	// email server conf setting
	mkdir_if_none("/etc/email");
	if((fp = fopen(MAIL_CONF, "w")) == NULL){
		_dprintf("fail to open %s\n", MAIL_CONF);
		return;
	}

#ifdef RTCONFIG_HTTPS
	strncpy(smtp_auth_pass,pwdec(nvram_get("PM_SMTP_AUTH_PASS")),256);
#else
	strncpy(smtp_auth_pass,nvram_get("PM_SMTP_AUTH_PASS"),256);
#endif

	fprintf(fp,"SMTP_SERVER = '%s'\n", nvram_safe_get("PM_SMTP_SERVER"));
	fprintf(fp,"SMTP_PORT = '%s'\n", nvram_safe_get("PM_SMTP_PORT"));
	fprintf(fp,"MY_NAME = 'Administrator'\n");
	fprintf(fp,"MY_EMAIL = '%s'\n", nvram_safe_get("PM_MY_EMAIL"));
	fprintf(fp,"USE_TLS = 'true'\n");
	fprintf(fp,"SMTP_AUTH = 'LOGIN'\n");
	fprintf(fp,"SMTP_AUTH_USER = '%s'\n", nvram_safe_get("PM_SMTP_AUTH_USER"));
	fprintf(fp,"SMTP_AUTH_PASS = '%s'\n", smtp_auth_pass);
	fclose(fp);

	// mail title
#ifdef RTCONFIG_BWDPI
	if(type == TM_WRS_MAIL)
		snprintf(title, sizeof(title), "AiProtection alert! %s", date);
#endif
#ifdef RTCONFIG_TRAFFIC_LIMITER
	if(type == TRAFFIC_LIMITER_MAIL)
		snprintf(title, sizeof(title), "Traffic Limiter alert! %s", date);
#endif
	if(type == CONFIRM_MAIL)
		snprintf(title, sizeof(title), "%s Mail Alert Verify", nvram_safe_get("productid"));

	// create command (attachment or not)
	char cmd[512];
	memset(cmd, 0, sizeof(cmd));
	if (path != NULL){
		snprintf(cmd, sizeof(cmd), "email -V -s \"%s\" %s -a \"%s\"", title, nvram_safe_get("PM_MY_EMAIL"), path);
	}
	else{
		snprintf(cmd, sizeof(cmd), "email -V -s \"%s\" %s", title, nvram_safe_get("PM_MY_EMAIL"));
	}

	// create popen
	if((fp = popen(cmd, "w")) == NULL){
		_dprintf("%s : popen() error\n", __FUNCTION__);
		return;
	}

	// email content use popen (pipe open) ... 
	// write mail content
#ifdef RTCONFIG_BWDPI
	if(type == TM_WRS_MAIL)
	{
		// extract mail log into seperated event information
		extract_data(path, fp);

		fprintf(fp, "%s\'s AiProtection detected suspicious networking behavior and prevented your device making a connection to a malicious website (see above and the attached log for details).", nvram_safe_get("productid"));
		fprintf(fp, "Suggested actions:\n");
		fprintf(fp, "1. If you know that the cause of this attempted connection was a proprietary app or program and not a web browser, we recommand that you uninstall it from your device.\n");
		fprintf(fp, "2. Ensure your device is up to time in all its operating system patches as well as updates to all apps or programs.\n");
		fprintf(fp, "3. You should take this opportunity to check for, and install, any router firmware updates.\n");
		fprintf(fp, "4. If you continue to receive such alerts for similar attempted connections, investigation into the cause will be necessary.\n");
		fprintf(fp, "Meanwhile, rest assured that AiProtection continues to help keep you safe on the Internet.\n");
		fprintf(fp, "\n");
		fprintf(fp, "You also can link to trend micro website to download security trial software for your client device protection.\n");
		fprintf(fp, "http://www.trendmicro.com/\n");
		fprintf(fp, "\n");
		fprintf(fp, "ASUS AiProtection FAQ:\n");
		fprintf(fp, "http://www.asus.com/support/FAQ/1012070/\n");
		logmessage("ALERT MAIL", "AiProtection send mail");
		MyDBG("AiProtection send mail\n");
	}
#endif
#ifdef RTCONFIG_TRAFFIC_LIMITER
	if(type == TRAFFIC_LIMITER_MAIL)
	{
		unsigned int flag = traffic_limiter_read_bit("alert");
		unsigned int val = traffic_limiter_read_bit("count"); // have sent
		
		fprintf(fp, "Dear user,\n\n");
		fprintf(fp, "Traffic limiter configuration as below:\n\n");
		if((flag & 0x1) && !(val & 0x1)){
			fprintf(fp, "\t<Primary WAN>\n");
			fprintf(fp, "\tRouter Current Traffic: %.2f GB\n", traffic_limiter_get_realtime(0));
			fprintf(fp, "\tAlert Traffic: %.2f GB\n", nvram_get_double("tl0_alert_max"));
			fprintf(fp, "\tMAX Traffic: %.2f GB\n\n", nvram_get_double("tl0_limit_max"));
			traffic_limiter_set_bit("count", 0);
			MyDBG("Traffic limiter send mail - primary\n");
		}
		if((flag & 0x2) && !(val & 0x2)){
			fprintf(fp, "\t<Secondary WAN>\n");
			fprintf(fp, "\tRouter Current Traffic: %.2f GB\n", traffic_limiter_get_realtime(1));
			fprintf(fp, "\tAlert Traffic: %.2f GB\n", nvram_get_double("tl1_alert_max"));
			fprintf(fp, "\tMAX Traffic: %.2f GB\n\n", nvram_get_double("tl1_limit_max"));
			traffic_limiter_set_bit("count", 1);
			MyDBG("Traffic limiter send mail - secondary\n");
		}
		fprintf(fp, "Your internet traffic usage have reached the alert vlaue. If you are in limited traffic usage, please get more attention.\n\n");
		fprintf(fp, "Thanks,\n");
		fprintf(fp, "ASUSTeK Computer Inc.\n");
		logmessage("ALERT MAIL", "Traffic limter send mail");
	}
#endif
	if(type == CONFIRM_MAIL)
	{
		fprintf(fp, "Dear user,\n\n");
		fprintf(fp, "This is for your mail address confirmation and please click below link to go back firmware page for configuration.\n\n");
		fprintf(fp, "http://router.asus.com/\n\n");
		fprintf(fp, "Thanks,\n");
		fprintf(fp, "ASUSTeK Computer Inc.\n");
		logmessage("ALERT MAIL", "Confirm Mail");
		MyDBG("Confirm Mail\n");
	}
	status = pclose(fp);
	if(status == -1) _dprintf("%s : errno %d (%s)\n", __FUNCTION__, errno, strerror(errno));
	file_unlock(lock);
}

#ifdef RTCONFIG_BWDPI
static
void am_tm_wrs_mail()
{
	char path[33];
	memset(path, 0, sizeof(path));

	// AiProtection wrs_vp.txt
	int flag = merge_log(path, sizeof(path));

	// check flag and path
	if(flag && strcmp(path, "")) am_send_mail(TM_WRS_MAIL , path);
}
#endif

static
int alert_mail_function_check()
{
	// intial global variable
	value = 0;

	// TrendMicro wrs mail
	if(nvram_get_int("wrs_enable") || nvram_get_int("wrs_cc_enable") || nvram_get_int("wrs_vp_enable"))
		value |= TM_WRS_MAIL;

	// traffic limiter mail
	if(nvram_get_int("tl_enable") && (nvram_get_int("tl0_alert_enable") || nvram_get_int("tl1_alert_enable")))
		value |= TRAFFIC_LIMITER_MAIL;

	MyDBG("value = %d(0x%x)\n", value, value);

	return value;
}

void alert_mail_service()
{
	// check function enable or not
	if(!alert_mail_function_check()) return;

#ifdef RTCONFIG_BWDPI
	if(value & TM_WRS_MAIL)
		am_tm_wrs_mail();
#endif

#ifdef RTCONFIG_TRAFFIC_LIMITER
	if(value & TRAFFIC_LIMITER_MAIL)
		am_traffic_limiter_mail();
#endif
}
