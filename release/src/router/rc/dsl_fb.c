#include <rc.h>
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <dirent.h>
#include <disk_io_tools.h>	

#define SMTP_SERVER "mymail.asus.com"
#define SMTP_PORT "25"
#define MY_NAME "Administrator"
#define MY_EMAIL "xdsl_feedback@asus.com"
#define SMTP_AUTH "login"
#define SMTP_AUTH_USER "xdsl_feedback"
#define SMTP_AUTH_PASS "asus#1234"
#define FB_FILE "/tmp/xdslissuestracking"
#define	MAIL_CONF "/tmp/var/tmp/data"
#define TOP_FILE "/tmp/top.txt"
#define FREE_FILE "/tmp/free.txt"
#define IPTABLES_FILE "/tmp/fb_iptables.txt"

/*
 * Reads file and returns contents
 * @param	fd	file descriptor
 * @return	contents of file or NULL if an error occurred
 */
char *
fd2str(int fd)
{
	char *buf = NULL;
	size_t count = 0, n;

	do {
		buf = realloc(buf, count + 512);
		n = read(fd, buf + count, 512);
		if (n < 0) {
			free(buf);
			buf = NULL;
		}
		count += n;
	} while (n == 512);

	close(fd);
	if (buf)
		buf[count] = '\0';
	return buf;
}

/*
 * Reads file and returns contents
 * @param	path	path to file
 * @return	contents of file or NULL if an error occurred
 */
char *
file2str(const char *path)
{
	int fd;

	if ((fd = open(path, O_RDONLY)) == -1) {
		perror(path);
		return NULL;
	}

	return fd2str(fd);
}

void start_DSLsendmail(void)
{
	FILE *fp;
	char cmd[512] = {0};
	char buf[1024] = {0};
	char tmp_val[70] = {0};
	char tmpContent[70] = {0};
	char attach_syslog_cmd[70] = {0};
	char attach_cfgfile_cmd[70] = {0};
	char attach_iptables_cmd[256] = {0};
	char up_time[128];
	char *str = file2str("/proc/uptime");
	unsigned int up = atoi(str);
	unsigned int dsl_up_time = 0, current_up=0;
	int days=0, hours=0, minutes=0;
	int nValue=0;

	/* write the configuration file.*/
	if (!(fp = fopen(MAIL_CONF, "w"))) {
		logmessage("email", "Failed to send mail!\n");
		return;
	}

	nvram_set("fb_state", "0");

	sprintf(buf,
		"SMTP_SERVER = '%s'\n"
		"SMTP_PORT = '%s'\n"
		"MY_NAME = '%s'\n"
		"MY_EMAIL = '%s'\n"
		"USE_TLS = '%s'\n"
		"SMTP_AUTH = '%s'\n"
		"SMTP_AUTH_USER = '%s'\n"
		"SMTP_AUTH_PASS = '%s'\n"
		, SMTP_SERVER
		, SMTP_PORT
		, MY_NAME
		, MY_EMAIL
		, "true"
		, SMTP_AUTH
		, SMTP_AUTH_USER
		, SMTP_AUTH_PASS
	);
	fputs(buf,fp);
	fclose(fp);
	current_up = up;
	if (up > 60*60*24) {
		days = up / (60*60*24);
		up %= 60*60*24;
	}
	if (up > 60*60) {
		hours = up / (60*60);
		up %= 60*60;
	}
	if (up > 60) {
		minutes = up / 60;
		up %= 60;
	}
	sprintf(up_time, "%d days, %d hours, %d minutes, %d seconds", days, hours, minutes, up);

	//add some system information
	sprintf(cmd, "free > %s", FREE_FILE);
	system(cmd);
	sprintf(cmd, "top -n 1 > %s", TOP_FILE);
	system(cmd);

	if(nvram_match("PM_attach_syslog", "1")){
		sprintf(attach_syslog_cmd,"-a /tmp/syslog.log ");
		if(check_if_file_exist("/tmp/syslog.log-1"))
			strcat(attach_syslog_cmd, "-a /tmp/syslog.log-1 ");
		eval("req_dsl_drv", "getdmesg");
		if(check_if_file_exist("/tmp/adsl/dmesg.txt"))
			strcat(attach_syslog_cmd, "-a /tmp/adsl/dmesg.txt");
	}

	if(nvram_match("PM_attach_cfgfile", "1")){
		nvram_commit();
		eval("nvram", "save", "/tmp/settings");
		sprintf(attach_cfgfile_cmd, "-a /tmp/settings");
	}

	if(nvram_match("PM_attach_iptables", "1")){
		sprintf(cmd, "iptables-save > %s", IPTABLES_FILE);
		system(cmd);
		sprintf(attach_iptables_cmd, "-a %s", IPTABLES_FILE);
	}

	fp = fopen(FB_FILE, "w");
	if(fp) {
		fputs("CUSTOMER FEEDBACK\n----------------------------------------------------------------------------------------------------------------\n\n", fp);
		fputs("Model: ", fp);
		fputs(get_productid(), fp);

		fputs("\nFirmware Version: ", fp);
		sprintf(tmp_val, "%s.%s_%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno"));
		fputs(tmp_val, fp);
		fputs("\nInner Version: ", fp);
		fputs(nvram_safe_get("innerver"), fp);

		fputs("\nDSL Firmware Version: ", fp);
		fputs(nvram_safe_get("dsllog_fwver"), fp);

		fputs("\nDSL Driver Version: ", fp);
		fputs(nvram_safe_get("dsllog_drvver"), fp);

		fputs("\nPIN Code: ", fp);
		fputs(nvram_safe_get("wps_device_pin"), fp);

		fputs("\nMAC Address: ", fp);
		fputs(nvram_safe_get("wl0_hwaddr"), fp);

		fputs("\nBrowser: ", fp);
		fputs(nvram_safe_get("fb_browserInfo"), fp);

		fputs("\nConfigured DSL Modulation: ", fp);
		switch(nvram_get_int("dslx_modulation")) {
			case 0:
				fputs("T1.413", fp);
				break;
			case 1:
				fputs("G.lite", fp);
				break;
			case 2:
				fputs("G.Dmt", fp);
				break;
			case 3:
				fputs("ADSL2", fp);
				break;
			case 4:
				fputs("ADSL2+", fp);
				break;
			case 5:
				fputs("Multiple Mode", fp);
				break;
#ifdef RTCONFIG_VDSL
			case 6:
				fputs("VDSL2", fp);
				break;
#endif
		}

		fputs("\nConfigured Annex Mode: ", fp);
			switch(nvram_get_int("dslx_annex")) {
				case 0:
					fputs("Annex A", fp);
					break;
				case 1:
					fputs("Annex I", fp);
					break;
				case 2:
					fputs("Annex A/L", fp);
					break;
				case 3:
					fputs("Annex M", fp);
					break;
				case 4:
					fputs("Annex A/I/J/L/M", fp);
					break;
				case 5:
					fputs("Annex B", fp);
					break;
				case 6:
					fputs("Annex B/J", fp);
					break;
			}

		fputs("\nStability Adjustment(ADSL): ", fp);
		nValue = nvram_get_int("dslx_snrm_offset");
		if(nValue == 0)
			fprintf(fp, "%d (Disabled)", nValue);
		else
			fprintf(fp, "%d (%d dB)", nValue, nValue/512);

#ifdef RTCONFIG_VDSL
		fputs("\nStability Adjustment(VDSL): ", fp);
		nValue = nvram_get_int("dslx_vdsl_target_snrm");
		if(nValue == 32767)
			fprintf(fp, "%d (Disabled)", nValue);
		else
			fprintf(fp, "%d (%d dB)", nValue, nValue/512);

		fputs("\nTx Power Control (VDSL): ", fp);
		nValue = nvram_get_int("dslx_vdsl_tx_gain_off");
		if(nValue == 32767)
			fprintf(fp, "%d (Disabled)", nValue);
		else
			fprintf(fp, "%d (%d dB)", nValue, nValue/10);

		fputs("\nRx AGC GAIN Adjustment (VDSL): ", fp);
		nValue = nvram_get_int("dslx_vdsl_rx_agc");
		if(nValue == 65535)
			fprintf(fp, "%d (Disabled)", nValue);
		else if(nValue == 394)
			fprintf(fp, "%d (Stable)", nValue);
		else if(nValue == 476)
			fprintf(fp, "%d (Balance)", nValue);
		else if(nValue == 550)
			fprintf(fp, "%d (High Performance)", nValue);
		else
			fprintf(fp, "%d", nValue);

		fputs("\nUPBO - upstream power back off (VDSL): ", fp);
		fputs(nvram_safe_get("dslx_vdsl_upbo"), fp);

		fputs("\nVDSL Profile: ", fp);
		nValue = nvram_get_int("dslx_vdsl_profile");
		if(nValue == 0)
			fprintf(fp, "%d (30a multi mode)", nValue);
		else if(nValue == 1)
			fprintf(fp, "%d (17a multi mode)", nValue);
		else
			fprintf(fp, "%d", nValue);
#endif

		fputs("\nSRA (Seamless Rate Adaptation): ", fp);
		fputs(nvram_safe_get("dslx_sra"), fp);

		fputs("\nBitswap: ", fp);
		fputs(nvram_safe_get("dslx_bitswap"), fp);

		fputs("\nG.INP: ", fp);
		fputs(nvram_safe_get("dslx_ginp"), fp);

		fputs("\nMonitor line stability: ", fp);
		if(nvram_match("dsltmp_syncloss", "1")){
			nvram_set("dsltmp_syncloss", "2");
			fputs("2", fp);
		}
		else{
			fputs(nvram_safe_get("dsltmp_syncloss"), fp);
		}

		fputs("\nSystem Up time: ", fp);
		fputs(up_time, fp);

		fputs("\nDSL Up time: ", fp);
		sprintf(tmp_val, "%s", nvram_safe_get("adsl_timestamp"));
		if( nvram_match("dsltmp_adslsyncsts", "up") && atoi(tmp_val) > 0){
			days = 0;
			hours = 0;
			minutes = 0;
			dsl_up_time = current_up - atoi(tmp_val);
			if (dsl_up_time > 60*60*24) {
				days = dsl_up_time / (60*60*24);
				dsl_up_time %= 60*60*24;
			}
			if (dsl_up_time > 60*60) {
				hours = dsl_up_time / (60*60);
				dsl_up_time %= 60*60;
			}
			if (dsl_up_time > 60) {
				minutes = dsl_up_time / 60;
				dsl_up_time %= 60;
			}
			sprintf(up_time, "%d days, %d hours, %d minutes, %d seconds", days, hours, minutes, dsl_up_time);
			fputs(up_time, fp);
		}

		fputs("\n", fp);

		fputs("\nDSL Link Status: ", fp);
		fputs(nvram_safe_get("dsltmp_adslsyncsts"), fp);

		if(nvram_match("dsltmp_adslsyncsts", "up"))
		{
			fputs("\nCurrent DSL Modulation: ", fp);
			fputs(nvram_safe_get("dsllog_opmode"), fp);

			fputs("\nCurrent Annex Mode: ", fp);
			fputs(nvram_safe_get("dsllog_adsltype"), fp);

			fputs("\nSNR Down: ", fp);
			fputs(nvram_safe_get("dsllog_snrmargindown"), fp);

			fputs("\nSNR Up: ", fp);
			fputs(nvram_safe_get("dsllog_snrmarginup"), fp);

			fputs("\nLine Attenuation Down: ", fp);
			fputs(nvram_safe_get("dsllog_attendown"), fp);

			fputs("\nLine Attenuation Up: ", fp);
			fputs(nvram_safe_get("dsllog_attenup"), fp);

			fputs("\nData Rate Down: ", fp);
			fputs(nvram_safe_get("dsllog_dataratedown"), fp);

			fputs("\nData Rate Up: ", fp);
			fputs(nvram_safe_get("dsllog_datarateup"), fp);

			fputs("\nMAX Rate Down: ", fp);
			fputs(nvram_safe_get("dsllog_attaindown"), fp);

			fputs("\nMAX Rate Up: ", fp);
			fputs(nvram_safe_get("dsllog_attainup"), fp);

			fputs("\nCRC Down: ", fp);
			fputs(nvram_safe_get("dsllog_crcdown"), fp);

			fputs("\nCRC Up: ", fp);
			fputs(nvram_safe_get("dsllog_crcup"), fp);

			fputs("\nPower Down: ", fp);
			fputs(nvram_safe_get("dsllog_powerdown"), fp);

			fputs("\nPower Up: ", fp);
			fputs(nvram_safe_get("dsllog_powerup"), fp);

			fputs("\n", fp);
		}

		if(nvram_safe_get("fb_country") != 0){
			sprintf(tmpContent, "\nCountry: %s\n", nvram_safe_get("fb_country"));
			fputs(tmpContent, fp);
		}

		sprintf(tmpContent, "Time Zone: %s\n", nvram_safe_get("time_zone_x"));
		fputs(tmpContent, fp);

		if(nvram_invmatch("fb_ISP", "")){
			sprintf(tmpContent, "ISP: %s\n", nvram_safe_get("fb_ISP"));
			fputs(tmpContent, fp);
		}
		if(nvram_invmatch("fb_Subscribed_Info", "")){
			sprintf(tmpContent, "Subscribed Package: %s\n", nvram_safe_get("fb_Subscribed_Info"));
			fputs(tmpContent, fp);
		}
		if(nvram_invmatch("fb_email", "")){
			sprintf(tmpContent, "E-mail: %s\n", nvram_safe_get("fb_email"));
			fputs(tmpContent, fp);
		}
		if(nvram_match("dslx_transmode", "atm")){
			fprintf(fp, "VPI/VCI: %s/%s\n", nvram_safe_get("dsl0_vpi"), nvram_safe_get("dsl0_vci"));
			fprintf(fp, "WAN Connection Type: %s\n", nvram_safe_get("dsl0_proto"));
			nValue = nvram_get_int("dsl0_encap");
			if(nValue)
				fputs("Encapsulation Mode: VC-Mux\n", fp);
			else
				fputs("Encapsulation Mode: LLC\n", fp);
		}
		else{
			fprintf(fp, "WAN Connection Type: %s\n", nvram_safe_get("dsl8_proto"));
			fprintf(fp, "VLAN ID: %s\n", nvram_safe_get("dsl8_vid"));
		}
		if(nvram_invmatch("fb_availability", "")){
			sprintf(tmpContent, "DSL connection: %s\n", nvram_safe_get("fb_availability"));
			fputs(tmpContent, fp);
		}
		if(nvram_invmatch("fb_comment", "")){
			fputs("Comments:\n", fp);
			fputs(nvram_safe_get("fb_comment"), fp);
			fputs("\n\n", fp);
		}
		fclose(fp);
	}
	sprintf(cmd, "cat %s | /usr/sbin/email -c %s -s \"%s feedback from %s\" -a %s -a %s %s %s %s %s"
		, FB_FILE
		, MAIL_CONF
		, get_productid(), nvram_safe_get("fb_email")
		, TOP_FILE
		, FREE_FILE
		, attach_iptables_cmd
		, attach_cfgfile_cmd
		, attach_syslog_cmd
		, (!nvram_match("fb_email_dbg", ""))? nvram_safe_get("fb_email_dbg"): MY_EMAIL
		);

	system(cmd);

	if(nvram_match("fb_state", "2")) {
		memset(cmd, 0, sizeof(cmd));
		sprintf(cmd, "cd /tmp; tar zcf fb_data.tgz");
		strcat(cmd, " email.log");
		if(check_if_file_exist("/tmp/adsl/info_adsl.txt"))
			strcat(cmd, " adsl/info_adsl.txt");
		if(nvram_match("PM_attach_syslog", "1")) {
			if(check_if_file_exist("/tmp/syslog.log"))
				strcat(cmd, " syslog.log");
			if(check_if_file_exist("/tmp/syslog.log-1"))
				strcat(cmd, " syslog.log-1");
			if(check_if_file_exist("/tmp/adsl/dmesg.txt"))
				strcat(cmd, " adsl/dmesg.txt");
		}
		if(nvram_match("PM_attach_cfgfile", "1")) {
			if(check_if_file_exist("/tmp/settings"))
				strcat(cmd, " settings");
		}
		if(nvram_match("PM_attach_iptables", "1")) {
			if(check_if_file_exist("/tmp/fb_iptables.txt"))
				strcat(cmd, " fb_iptables.txt");
		}
		system(cmd);
	}
	else {
		nvram_set("fb_state", "1");
		unlink(FB_FILE);
	}

	unlink(TOP_FILE);
	unlink(FREE_FILE);
	unlink(IPTABLES_FILE);
}
