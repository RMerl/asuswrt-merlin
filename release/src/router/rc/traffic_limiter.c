/*
	ASUS features:
		- traffic limiter for wan interface

	Copyright (C) ASUSTek. Computer Inc.
*/

#include <rc.h>
#include <math.h>

#ifdef RTCONFIG_TRAFFIC_LIMITER

#define IFPATH_MAX 64

static int traffic_limiter_is_first = 1;

void traffic_limiter_sendSMS(const char *type, int unit)
{
	char phone[32], message[IFPATH_MAX];
	char *at_cmd[] = {"/usr/sbin/modem_status.sh", "send_sms", phone, message, NULL};
	char buf[32];

	snprintf(buf, sizeof(buf), "tl%d_%s_max", unit, type);
	snprintf(phone, sizeof(phone), "%s", nvram_safe_get("modem_sms_phone"));

	if (strcmp(type, "alert") == 0)
		snprintf(message, sizeof(message), "%s %s bytes.", nvram_safe_get("modem_sms_message1"), nvram_safe_get(buf));
	else if (strcmp(type, "limit") == 0)
		snprintf(message, sizeof(message), "%s %s bytes.", nvram_safe_get("modem_sms_message2"), nvram_safe_get(buf));

#ifdef RTCONFIG_INTERNAL_GOBI
	stop_lteled();
#endif
	_eval(at_cmd, ">/tmp/modem_action.ret", 0, NULL);
#ifdef RTCONFIG_INTERNAL_GOBI
	start_lteled();
#endif
}

static double traffic_limiter_get_max(const char *type, int unit)
{
	char path[IFPATH_MAX];
	char buf[32];

	snprintf(path, sizeof(path), "/tmp/tl%d_%s_max", unit, type);
	if (f_read_string(path, buf, sizeof(buf)) > 0)
		return atof(buf);

	return 0;
}

static void traffic_limiter_set_max(const char *type, int unit, double value)
{
	char path[IFPATH_MAX];
	char buf[32];

	snprintf(path, sizeof(path), "/tmp/tl%d_%s_max", unit, type);
	snprintf(buf, sizeof(buf), "%.9g", value);

	f_write_string(path, buf, 0, 0);
}

static void _traffic_limiter_recover_connect(char *wan_if, int unit)
{
	TL_DBG("RECOVER wan%d: %s connection\n", unit, wan_if);

	/* recover wan connection function */
	update_wan_state(wan_if, WAN_STATE_CONNECTED, 0);
	start_wan_if(unit);
	traffic_limiter_clear_bit("limit", unit);
}

void reset_traffic_limiter_counter(int force)
{
	// force = 0 : check limit line
	// force = 1 : not to check limit line

	/* recover connection when changing limit line or disable limit line */
	char tmp1[32], tmp2[32], tmp3[32];
	char prefix[sizeof("tlXXXXXXXX_")], wan_if[sizeof("wanXXXXXXXX_")];
	int unit;
	double val;

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
	{
		snprintf(prefix, sizeof(prefix), "tl%d_", unit);
		snprintf(wan_if, sizeof(wan_if), "wan%d_", unit);
		strcat_r(prefix, "limit_max", tmp1);
		strcat_r(prefix, "alert_max", tmp2);
		strcat_r(prefix, "limit_enable", tmp3);

		if (force == 1) {
			/* if cycle detect, not to check limit line */
			if (!is_wan_connect(unit))
				_traffic_limiter_recover_connect(wan_if, unit);
		} else if (force == 0) {
			/* if traffic limit disable limit line and wan disconnect */
			if (nvram_get_int(tmp3) == 0 && !is_wan_connect(unit))
				_traffic_limiter_recover_connect(wan_if, unit);
		}

		// when limit line changes
		val = nvram_get_double(tmp1);
		if (fabs(val - traffic_limiter_get_max("limit", unit)) > 1.0/1024) {
			traffic_limiter_set_max("limit", unit, val);
			_traffic_limiter_recover_connect(wan_if, unit);
		}

		// reset counter when alert line changes
		val = nvram_get_double(tmp2);
		if (fabs(val - traffic_limiter_get_max("alert", unit)) > 1.0/1024) {
			traffic_limiter_set_max("alert", unit, val);
			// clear bit
			traffic_limiter_clear_bit("alert", unit);
			traffic_limiter_clear_bit("count", unit);
		}
	}
}

static void traffic_limiter_cycle_detect(void)
{
	struct tm t1, t2;
	time_t date_start = nvram_get_int("tl_date_start");
	time_t now, new_date;
	char buf[32];

	/* current timestamp */
	time(&now);
	
	/* transfer format into tm */
	localtime_r(&now, &t1);
	localtime_r(&date_start, &t2);

	TL_DBG("t1: %d-%d-%d, %d:%d:%d\n", t1.tm_year+1900, t1.tm_mon+1, t1.tm_mday, t1.tm_hour, t1.tm_min, t1.tm_sec);
	TL_DBG("t2: %d-%d-%d, %d:%d:%d\n", t2.tm_year+1900, t2.tm_mon+1, t2.tm_mday, t2.tm_hour, t2.tm_min, t2.tm_sec);

	if ((t1.tm_mon == (t2.tm_mon+1)) && (t1.tm_mday == t2.tm_mday)) {
		// step1. save database
		hm_traffic_limiter_save();

		// setp2. update tl_date_start
		t1.tm_hour = 0;
		t1.tm_min = 0;
		t1.tm_sec = 0;
		new_date = mktime(&t1);

		snprintf(buf, sizeof(buf), "%ld", new_date);
		nvram_set("tl_date_start", buf);
		TL_DBG("buf=%s\n", buf);

		// step3. recover connection
		reset_traffic_limiter_counter(1);
	}
}

void traffic_limiter_limitdata_check(void)
{
	if (nvram_get_int("tl_enable") == 0)
		return;

	/* wanduck to upadte traffic limiter data into /jffs/tld/xxx/tmp */
	if (traffic_limiter_is_first) {
		/* after hardware or software reboot, save final data last time */
		eval("traffic_limiter", "-c");
		traffic_limiter_is_first = 0;
	} else {
		/* normal query and upate data */
		eval("traffic_limiter", "-q");
	}

	/* when tl_cycle is reached, upadte timestamp and start_wan_if() */
	traffic_limiter_cycle_detect();
}

int traffic_limiter_wanduck_check(int unit)
{
	/* wanduck to check which interface's function and limit are enabled */
	int ret = 0, save = 0;
	unsigned int val;

	val = traffic_limiter_read_bit("limit");
	
	/* primary wan */
	if (unit == 0 && nvram_get_int("tl0_limit_enable") && (val & 0x1)) {
		ret = 1;
		save = 1;
	}

	/* secondary wan */
	if (unit == 1 && nvram_get_int("tl1_limit_enable") && (val & 0x2)) {
		ret = 1;
		save = 1;
	}

	/* save database */
	if (save)
		eval("traffic_limiter", "-w");

	return ret;
}

void init_traffic_limiter(void)
{
	traffic_limiter_is_first = 1;
	traffic_limiter_set_max("limit", 0, nvram_get_double("tl0_limit_max"));
	traffic_limiter_set_max("alert", 0, nvram_get_double("tl0_alert_max"));
	traffic_limiter_set_max("limit", 1, nvram_get_double("tl1_limit_max"));
	traffic_limiter_set_max("alert", 1, nvram_get_double("tl1_alert_max"));
}
#endif
