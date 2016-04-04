
#define MAIL_CONTENT_INFO_PATH		"/tmp/MailContentInfo"

void exec_send_mail(MAIL_INFO_T *mInfo, char *contentPath, char *attrPath1, char *attrPath2)
{
	char Cmdbuf[512];
	char attrtmp1[32];
	char attrtmp2[32];
	
	memset(&Cmdbuf, 0, sizeof(Cmdbuf));
	memset(&attrtmp1, 0, sizeof(attrtmp1));
	memset(&attrtmp2, 0, sizeof(attrtmp2));
	
	snprintf(Cmdbuf, sizeof(Cmdbuf), "cat %s | email -V -z %d -s \"ASUS [%s] Alert - %s\" \"%s\"",
		contentPath,
		mInfo->MsendId,
		mInfo->modelName,
		mInfo->subject,
		mInfo->toMail);
	
	if(attrPath1 != NULL) {
		snprintf(attrtmp1, sizeof(attrtmp1), "-a %s", attrPath1);
		strcat(Cmdbuf, attrtmp1);
	}
	if(attrPath2 != NULL) {
		snprintf(attrtmp2, sizeof(attrtmp2), "-a %s", attrPath2);
		strcat(Cmdbuf, attrtmp2);
	}
	system(Cmdbuf);
}

/* ------------------------------
    ### RESERVATION EVENT ###
---------------------------------*/
MAIL_INFO_T *RESERVATION_MAIL_CONFIRM_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		fputs("This is for your mail address confirmation and please click below link to go back firmware page for configuration.\n\n", fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	
	strcpy(mInfo->subject, "Notify Mail Verify");
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;

}

/* ------------------------------
    ### WAN EVENT ###
---------------------------------*/

MAIL_INFO_T *WAN_DISCONN_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		snprintf(buf, sizeof(buf), "You receive %s from Asus Router.\n\n\n", eInfo_get_eName(mInfo->event));
		fputs(buf, fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}

MAIL_INFO_T *WAN_BLOCK_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		snprintf(buf, sizeof(buf), "You receive %s from Asus Router.\n\n\n", eInfo_get_eName(mInfo->event));
		fputs(buf, fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}
/* ------------------------------
    ### PASSWORD EVENT ###
---------------------------------*/
MAIL_INFO_T *PASSWORD_SAME_WITH_LOGIN_WIFI_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		snprintf(buf, sizeof(buf), "You receive %s from Asus Router.\n\n\n", eInfo_get_eName(mInfo->event));
		fputs(buf, fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}

MAIL_INFO_T *PASSWORD_WIFI_WEAK_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		snprintf(buf, sizeof(buf), "You receive %s from Asus Router.\n\n\n", eInfo_get_eName(mInfo->event));
		fputs(buf, fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}

MAIL_INFO_T *PASSWORD_LOGIN_STRENGTH_CHECK_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		snprintf(buf, sizeof(buf), "You receive %s from Asus Router.\n\n\n", eInfo_get_eName(mInfo->event));
		fputs(buf, fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}
/* ------------------------------
    ### GUEST NETWORK EVENT ###
---------------------------------*/
/* ------------------------------
    ### RSSI EVENT ###
---------------------------------*/
/* ------------------------------
    ### DUALWAN EVENT ###
---------------------------------*/
/* ------------------------------
    ### LOGIN EVENT ###
---------------------------------*/
MAIL_INFO_T *LOGIN_FAIL_LAN_WEB_FUNC(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	char date[30] = {0};
	
	StampToDate(mInfo->tstamp, date);

	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		snprintf(buf, sizeof(buf), "[%s] Detect abnormal logins many times from [%s] via WEB\n\n\n", date, mInfo->msg);
		fputs(buf, fp);
		fputs("http://router.asus.com\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	strcpy(mInfo->subject, "Failed to login");
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}
/* ------------------------------
    ### PROTECTION EVENT ###
---------------------------------*/
MAIL_INFO_T *PROTECTION_INTO_MONITORMODE_FUN(MAIL_INFO_T *mInfo)
{
	return mInfo;
}

MAIL_INFO_T *PROTECTION_VULNERABILITY_FUN(MAIL_INFO_T *mInfo)
{
	return mInfo;
}

MAIL_INFO_T *PROTECTION_CC_FUN(MAIL_INFO_T *mInfo)
{
	return mInfo;
}

MAIL_INFO_T *PROTECTION_DOS_FUN(MAIL_INFO_T *mInfo)
{
	return mInfo;
}
/* ------------------------------
    ### PARENTAL CONTROL EVENT ###
---------------------------------*/
/* ------------------------------
    ### TRAFFIC METER EVENT ###
---------------------------------*/
MAIL_INFO_T *TRAFFICMETER_CUTOFF_FUN(MAIL_INFO_T *mInfo)
{
	return mInfo;
}

MAIL_INFO_T *TRAFFICMETER_ALERT_FUN(MAIL_INFO_T *mInfo)
{
	FILE *fp;
	char buf[128] = {0};
	int  flag = 0, val = 0;

	if(f_read_string("/tmp/tl_alert", buf, sizeof(buf)) > 0)
		flag = atoi(buf);
	if(f_read_string("/tmp/tl_count", buf, sizeof(buf)) > 0)
		val = atoi(buf);

	fp = fopen(MAIL_CONTENT_INFO_PATH, "w");
	if(fp) {
		fputs("Dear user,\n\n", fp);
		fputs("Traffic limiter configuration as below:\n\n", fp);
		if((flag & 0x1) && !(val & 0x1)){
			fputs("\t<Primary WAN>\n", fp);
			f_read_string("/tmp/tl0_realtime", buf, sizeof(buf));
			fprintf(fp, "\tRouter Current Traffic: %s GB\n", buf);
			f_read_string("/tmp/tl0_alert_max", buf, sizeof(buf));
			fprintf(fp, "\tAlert Traffic: %s GB\n", buf) ;
			f_read_string("/tmp/tl0_limit_max", buf, sizeof(buf));
			fprintf(fp, "\tMAX Traffic: %s GB\n\n", buf);
		}
		if((flag & 0x2) && !(val & 0x2)){
			fputs("\t<Secpmdary WAN>\n", fp);
			f_read_string("/tmp/tl1_realtime", buf, sizeof(buf));
			fprintf(fp, "\tRouter Current Traffic: %s GB\n", buf);
			f_read_string("/tmp/tl1_alert_max", buf, sizeof(buf));
			fprintf(fp, "\tAlert Traffic: %s GB\n", buf) ;
			f_read_string("/tmp/tl1_limit_max", buf, sizeof(buf));
			fprintf(fp, "\tMAX Traffic: %s GB\n\n", buf);
		}
		fputs("Your internet traffic usage have reached the alert vlaue. If you are in limited traffic usage, please get more attention.\n\n", fp);
		fputs("Thanks, \n", fp);
		fputs("ASUSTeK Computer Inc.\n", fp);
		fclose(fp);
	} else {
		ErrorMsg("Error, Cant open %s ,send mail fail.\n", MAIL_CONTENT_INFO_PATH);
		return mInfo;
	}
	exec_send_mail(mInfo, MAIL_CONTENT_INFO_PATH, NULL, NULL);
	return mInfo;
}
/* ------------------------------
    ### FIRMWARE EVENT ###
---------------------------------*/
/* ------------------------------
    ### USB EVENT ###
---------------------------------*/
/* ------------------------------
    ### HINT EVENT ###
---------------------------------*/
