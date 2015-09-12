/*
	alert mail for all types of mail information
		- depends on EMAIL=y

	the call function needs to depend on his own flag

	NOTE: if possible, not to add any dynamic library, you can use a new binary for excute process
*/

#include <rc.h>

// define function bit, you can define more functions as below
#define TM_WRS_MAIL		0x01
#define TRAFFIC_CONTROL_MAIL	0x02
#define CONFIRM_MAIL		0x04

#define MAIL_CONTENT	"/tmp/push_mail"
#define MAIL_CONF	"/etc/email/email.conf"

static int debug = 0;
static int value = 0;

/*
	type :
		as define function TM_WRS_MAIL, TRAFFIC_CONTROL_MAIL, etc.

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
#ifdef RTCONFIG_TRAFFIC_CONTROL
	if(type == TRAFFIC_CONTROL_MAIL)
		snprintf(title, sizeof(title), "Traffic control alert! %s", date);
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
		logmessage("alert mail", "AiProtection send mail");
	}
#endif
#ifdef RTCONFIG_TRAFFIC_CONTROL
	if(type == TRAFFIC_CONTROL_MAIL)
	{
		int count = nvram_get_int("traffic_control_count");
		fprintf(fp, "Dear user,\n\n");
		fprintf(fp, "Traffic Control configuration as below:\n\n");
		if((count & 1) == 1){
			fprintf(fp, "\t<Primary WAN>\n");
			fprintf(fp, "\tRouter Current Traffic: %.2f GB\n", atof(nvram_safe_get("traffic_control_realtime_0")));
			fprintf(fp, "\tAlert Traffic: %.2f GB\n", atof(nvram_safe_get("traffic_control_alert_max")));
			fprintf(fp, "\tMAX Traffic: %.2f GB\n\n", atof(nvram_safe_get("traffic_control_limit_max")));
		}
		if((count & 2) == 2){
			fprintf(fp, "\t<Secondary WAN>\n");
			fprintf(fp, "\tRouter Current Traffic: %.2f GB\n", atof(nvram_safe_get("traffic_control_realtime_1")));
			fprintf(fp, "\tAlert Traffic: %.2f GB\n", atof(nvram_safe_get("traffic_control_alert_max")));
			fprintf(fp, "\tMAX Traffic: %.2f GB\n\n", atof(nvram_safe_get("traffic_control_limit_max")));
		}
		fprintf(fp, "Your internet traffic usage have reached the alert vlaue. If you are in limited traffic usage, please get more attention.\n\n");
		fprintf(fp, "Thanks,\n");
		fprintf(fp, "ASUSTeK Computer Inc.\n");
		logmessage("alert mail", "Traffic control send mail");
	}
#endif
	if(type == CONFIRM_MAIL)
	{
		fprintf(fp, "Dear user,\n\n");
		fprintf(fp, "This is for your mail address confirmation and please click below link to go back firmware page for configuration.\n\n");
		fprintf(fp, "http://router.asus.com/\n\n");
		fprintf(fp, "Thanks,\n");
		fprintf(fp, "ASUSTeK Computer Inc.\n");
		logmessage("alert mail", "Confirm Mail");
	}
	status = pclose(fp);
	if(status == -1) _dprintf("%s : errno %d (%s)\n", __FUNCTION__, errno, strerror(errno));
	file_unlock(lock);
}

#ifdef RTCONFIG_BWDPI
static
void am_tm_wrs_mail()
{
	debug = nvram_get_int("alert_mail_debug");

	char path[33];
	memset(path, 0, sizeof(path));

	// AiProtection wrs_vp.txt
	int flag = merge_log(path, sizeof(path));

	if(debug)
	{
		dbg("%s : flag = %d, path = %s\n", __FUNCTION__, flag, path);
		logmessage("alert mail", "AiProtection - flag = %d, path = %s", flag, path);
	}

	// check flag and path
	if(flag && strcmp(path, "")) am_send_mail(TM_WRS_MAIL , path);
}
#endif

#ifdef RTCONFIG_TRAFFIC_CONTROL
static
int traffic_control_realtime_check()
{
	int val = 0;
	FILE *fp;
	char buf[3];

	memset(buf, 0, sizeof(buf));

	if((fp = fopen("/tmp/traffic_control_alert_mail", "r")) != NULL)
	{
		if(fgets(buf, sizeof(buf), fp) != NULL)
		{
			if(strstr(buf, "1"))
				val = 1;
			else
				val = 0;

		}
		fclose(fp);
	}

	return val;
}

static
void am_traffic_control_mail()
{
	debug = nvram_get_int("alert_mail_debug");

	char *path = NULL;
	int flag = 0;

	// realtime traffic (database + realtime in both wan interface)
	if(nvram_get_int("traffic_control_alert_enable"))
		flag = traffic_control_realtime_check();

	if(debug)
	{
		dbg("%s : flag = %d, path = %s\n", __FUNCTION__, flag, path);
		logmessage("alert mail", "traffic control - flag = %d, path = %s", flag, path);
	}

	// check flag only
	if(flag){
		// send alert SMS
		if(nvram_get_int("modem_sms_limit")) traffic_control_sendSMS(0);
		am_send_mail(TRAFFIC_CONTROL_MAIL , path);
	}
}
#endif

static
int alert_mail_function_check()
{
	debug = nvram_get_int("alert_mail_debug");

	// intial global variable
	value = 0;

	// TrendMicro wrs mail
	if(nvram_get_int("wrs_enable") || nvram_get_int("wrs_cc_enable") || nvram_get_int("wrs_vp_enable"))
		value |= TM_WRS_MAIL;

	// traffic control mail
	if(nvram_get_int("traffic_control_alert_enable") || nvram_get_int("traffic_control_limit_enable"))
		value |= TRAFFIC_CONTROL_MAIL;

	if(debug)
	{
		dbg("%s : value = %d(0x%x)\n", __FUNCTION__, value, value);
		logmessage("alert mail", "value = %d(0x%x)", value, value);
	}

	return value;
}

void alert_mail_service()
{
	// check function enable or not
	if(!alert_mail_function_check()) return;

#ifdef RTCONFIG_BWDPI
	if((value & TM_WRS_MAIL) != 0)
		am_tm_wrs_mail();
#endif

#ifdef RTCONFIG_TRAFFIC_CONTROL
	if((value & TRAFFIC_CONTROL_MAIL) != 0)
		am_traffic_control_mail();
#endif
}
