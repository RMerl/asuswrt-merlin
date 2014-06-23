/*
 * ACS deamon chanim module (Linux)
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acsd_chanim.c 451091 2014-01-24 01:57:01Z $
 */

#include "acsd_svr.h"

#define ACS_CHANIM_BUF_LEN (2*1024)

static void
chanim_config_init(chanim_info_t * ch_info)
{
	char* intf_threshold;
	int trigger;

	ch_info->config.flags = CHANIM_FLAGS_RELATIVE_THRES;

	ch_info->config.sample_period = CHANIM_DFLT_SAMPLE_PERIOD;
	ch_info->config.threshold_time = CHANIM_DFLT_THRESHOLD_TIME;
	ch_info->config.max_acs = CHANIM_DFLT_MAX_ACS;
	ch_info->config.lockout_period = CHANIM_DFLT_LOCKOUT_PERIOD;

	ch_info->config.scb_max_probe = CHANIM_DFLT_SCB_MAX_PROBE;
	ch_info->config.scb_timeout = CHANIM_DFLT_SCB_TIMEOUT;
	ch_info->config.scb_activity_time = CHANIM_DFLT_SCB_ACTIVITY_TIME;

	ch_info->config.acs_trigger_var = 40;

	intf_threshold = nvram_get("acs_def_plcy_intf_threshold");
	if (intf_threshold) {
		trigger = atoi(intf_threshold);
		if (trigger >= 0)
			ch_info->config.acs_trigger_var = trigger;
	}

	/* range set up */
	chanim_range(ch_info).sample_period.min_val =
		CHANIM_SAMPLE_PERIOD_MIN;
	chanim_range(ch_info).sample_period.max_val =
		CHANIM_SAMPLE_PERIOD_MAX;
	chanim_range(ch_info).threshold_time.min_val =
		CHANIM_THRESHOLD_TIME_MIN;
	chanim_range(ch_info).threshold_time.max_val =
		CHANIM_THRESHOLD_TIME_MAX;
	chanim_range(ch_info).max_acs.min_val =
		CHANIM_MAX_ACS_MIN;
	chanim_range(ch_info).max_acs.max_val =
		CHANIM_MAX_ACS_MAX;
	chanim_range(ch_info).lockout_period.min_val =
		CHANIM_LOCKOUT_PERIOD_MIN;
	chanim_range(ch_info).lockout_period.max_val =
		CHANIM_LOCKOUT_PERIOD_MAX;

	chanim_range(ch_info).acs_trigger_var.min_val = 1;
	chanim_range(ch_info).acs_trigger_var.max_val =	50;

	/* if some driver configeration is needed, add here */
}

int
acsd_chanim_init(acs_chaninfo_t *c_info)
{
	chanim_info_t * ch_info;
	int ret;

	ch_info = (chanim_info_t *) acsd_malloc(sizeof(chanim_info_t));
	chanim_config_init(ch_info);

	ret = wl_iovar_setint(c_info->name, "chanim_sample_period",
		chanim_config(ch_info).sample_period);

	if (ret < 0) {
		ACSD_ERROR("failed to set chanim_sample_period");
	}

	chanim_mark(ch_info).wl_sample_period =
		chanim_config(ch_info).sample_period;

	ret = wl_iovar_setint(c_info->name, "noise_metric",
		NOISE_MEASURE_KNOISE);

	if (ret < 0) {
		ACSD_ERROR("failed to set noise metric");
	}

	ACS_FREE(c_info->chanim_info);
	c_info->chanim_info = ch_info;

	return ret;
}

#ifdef DEBUG
static void
acsd_dump_chanim(wl_chanim_stats_t* chanim_stats)
{
	int count = dtoh32(chanim_stats->count);
	int i, j;
	chanim_stats_t *stats;

	printf("Chanim Stats Dump: count: %d\n", count);
	printf("chanspec tx   inbss   obss   nocat   nopkt   doze     txop     "
		   "goodtx  badtx   glitch   badplcp  knoise  timestamp\n");

	for (i = 0; i < count; i++) {
		stats = &chanim_stats->stats[i];
		printf("0x%4x\t", stats->chanspec);
		for (j = 0; j < CCASTATS_MAX; j++)
			printf("%d\t", stats->ccastats[j]);
		printf("%d\t%d\t%d\t%d", dtoh32(stats->glitchcnt), dtoh32(stats->badplcp),
			stats->bgnoise, dtoh32(stats->timestamp));
		printf("\n");
	}
}
#endif /* DEBUG */

static void
acsd_display_chanim(chanim_stats_t* stats)
{
	int j;
	printf("chanspec tx   inbss   obss   nocat   nopkt   doze     txop     "
		   "goodtx  badtx   glitch   badplcp  knoise  timestamp\n");
	printf("0x%4x\t", stats->chanspec);
	for (j = 0; j < CCASTATS_MAX; j++)
		printf("%d\t", stats->ccastats[j]);
	printf("%d\t%d\t%d\t%d", dtoh32(stats->glitchcnt), dtoh32(stats->badplcp),
		stats->bgnoise, dtoh32(stats->timestamp));
	printf("\n");
}

static int
chanim_update_state(acs_chaninfo_t *c_info, bool state)
{
	int ret;

	ret = wl_iovar_setint(c_info->name, "chanim_state", (uint)state);
	ACSD_CHANIM("set chanim_state: %d\n", state);

	if (ret < 0) {
		ACSD_ERROR("failed to set chanim_state");
	}
	return ret;
}

static bool
chanim_intf_detected(chanim_info_t *ch_info)
{
	bool detected = FALSE;
	chanim_stats_t* latest = &ch_info->stats[chanim_mark(ch_info).stats_idx];
	chanim_config_t* config = &ch_info->config;

	bool noise_detect = FALSE;
	int score;

	/* if the detection is not supported, return false */
	if (latest->bgnoise == 0)
		return detected;

	score = latest->bgnoise;
	score += chanim_txop_to_noise(latest->chan_idle);

	if (score < chanim_mark(ch_info).best_score)
		chanim_mark(ch_info).best_score = score;

	if (acsd_debug_level & ACSD_DEBUG_CHANIM)
		printf("acs monitor: score = %d (ref %d)\n",
			score, chanim_mark(ch_info).best_score);

	if ((score - chanim_mark(ch_info).best_score) >= config->acs_trigger_var)
		noise_detect = TRUE;

	if (noise_detect && (latest->ccastats[CCASTATS_INBSS] < 2)) {
		detected = TRUE;
	}

	return detected;
}

/* start to poll the scbs frequently */
static int
chanim_speedup_scbprobe(acs_chaninfo_t * c_info)
{
	chanim_info_t * ch_info = c_info->chanim_info;
	wl_scb_probe_t scb_probe;
	int ret = 0;

	/* get the current setting from the driver */
	ret = wl_iovar_get(c_info->name, "scb_probe", &scb_probe,
		sizeof(wl_scb_probe_t));
	ACS_ERR(ret, "failed to get scb_probe results");

	ACSD_DEBUG("scb_probe: scb_timeout: %d, scb_max_probe: %d, scb_activity_time: %d\n",
		scb_probe.scb_timeout, scb_probe.scb_max_probe, scb_probe.scb_activity_time);

	chanim_mark(ch_info).scb_timeout = dtoh32(scb_probe.scb_timeout);
	chanim_mark(ch_info).scb_max_probe = dtoh32(scb_probe.scb_max_probe);
	chanim_mark(ch_info).scb_activity_time = dtoh32(scb_probe.scb_activity_time);

	/* set with the chanim setting */
	scb_probe.scb_timeout = htod32(chanim_config(ch_info).scb_timeout);
	scb_probe.scb_max_probe = htod32(chanim_config(ch_info).scb_max_probe);
	scb_probe.scb_activity_time = htod32(chanim_config(ch_info).scb_activity_time);

	/* get the current setting from the driver */
	ret = wl_iovar_set(c_info->name, "scb_probe", &scb_probe,
		sizeof(wl_scb_probe_t));
	ACS_ERR(ret, "failed to set scb_probe results");

	return ret;
}

static int
chanim_restore_scbprobe(acs_chaninfo_t * c_info)
{
	chanim_info_t * ch_info = c_info->chanim_info;
	wl_scb_probe_t scb_probe;
	int ret = 0;

	/* set with the stored driver setting */
	scb_probe.scb_timeout = htod32(chanim_mark(ch_info).scb_timeout);
	scb_probe.scb_max_probe = htod32(chanim_mark(ch_info).scb_max_probe);
	scb_probe.scb_activity_time = htod32(chanim_mark(ch_info).scb_activity_time);

	/* get the current setting from the driver */
	ret = wl_iovar_set(c_info->name, "scb_probe", &scb_probe,
		sizeof(wl_scb_probe_t));
	ACS_ERR(ret, "failed to get chanim results");

	return ret;
}

void
chanim_upd_acs_record(chanim_info_t *ch_info, chanspec_t selected, uint8 trigger)
{
	chanim_acs_record_t* cur_record = &ch_info->record[chanim_mark(ch_info).record_idx];
	chanim_stats_t *cur_stats = &ch_info->stats[chanim_mark(ch_info).stats_idx];
	time_t now = uptime();

	bzero(cur_record, sizeof(chanim_acs_record_t));

	cur_record->trigger = trigger;
	cur_record->timestamp = now;
	cur_record->selected_chspc = selected;
	cur_record->valid = TRUE;

	if (cur_stats) {
		cur_record->bgnoise = cur_stats->bgnoise;
		cur_record->glitch_cnt = cur_stats->glitchcnt;
		cur_record->ccastats = cur_stats->ccastats[CCASTATS_NOPKT];
	}

	chanim_mark(ch_info).record_idx ++;
	if (chanim_mark(ch_info).record_idx == CHANIM_ACS_RECORD)
		chanim_mark(ch_info).record_idx = 0;
}

static bool
chanim_chk_lockout(chanim_info_t *ch_info)
{
	uint8 cur_idx = chanim_mark(ch_info).record_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	time_t now = uptime();
	time_t passed = 0;
	int i, j;

	if (!chanim_config(ch_info).max_acs)
		return TRUE;

	for (i = 0, j = 0; i < CHANIM_STATS_RECORD; i++) {

		start_idx = MODSUB(cur_idx, i+1, CHANIM_ACS_RECORD);
		start_record = &ch_info->record[start_idx];

		if (start_record->trigger == APCS_INIT)
			return FALSE;

		if (start_record->trigger == APCS_CHANIM)
			j++;

		if (j == chanim_config(ch_info).max_acs)
			break;
	}

	if (j != chanim_config(ch_info).max_acs)
		return FALSE;

	passed = now - start_record->timestamp;

	if (start_record->valid && (passed <
			chanim_config(ch_info).lockout_period)) {
		ACSD_ERROR("chanim lockout true\n");
		return TRUE;
	}
	return FALSE;
}

uint
chanim_scb_lastused(acs_chaninfo_t* c_info)
{
	uint lastused = 0;
	int ret;

	ret = wl_iovar_getint(c_info->name, "scb_lastused", (int *)&lastused);

	if (ret < 0) {
		ACSD_ERROR("failed to get scb_lastused");
		return 0;
	}

	ACSD_DEBUG("lastused: %d\n", lastused);
	return lastused;
}


/* update the chanim state machine */
static int
chanim_upd_state(acs_chaninfo_t * c_info)
{
	int ret = 0;
	chanim_info_t * ch_info = c_info->chanim_info;
	uint8 cur_state = chanim_mark(ch_info).state;
	time_t now = uptime();
	bool detected = chanim_intf_detected(ch_info);

	ACSD_DEBUG("current time: %ld\n", now);

	if (chanim_mark(ch_info).detected != detected) {
		chanim_update_state(c_info, detected);
		chanim_mark(ch_info).detected = detected;
	}

	switch (cur_state) {
	case CHANIM_STATE_DETECTING:
		if (detected) {
			cur_state = CHANIM_STATE_DETECTED;
			ACSD_CHANIM("state changed: from %d to %d\n",
				chanim_mark(ch_info).state, cur_state);
			chanim_mark(ch_info).detecttime = now;
		}
		break;

	case CHANIM_STATE_DETECTED:

		if (!detected) {
			cur_state = CHANIM_STATE_DETECTING;
			ACSD_CHANIM("state changed: from %d to %d\n",
				chanim_mark(ch_info).state, cur_state);
			chanim_mark(ch_info).detecttime = 0;
			break;
		}
		/* if detect only, break */
		if (AUTOCHANNEL(c_info)) {
			time_t passed = now - chanim_mark(ch_info).detecttime;

			if ((uint8)passed > chanim_act_delay(ch_info)) {
				cur_state = CHANIM_STATE_ACTON;
				ACSD_CHANIM("state changed: from %d to %d\n",
					chanim_mark(ch_info).state, cur_state);
				chanim_speedup_scbprobe(c_info);
			}
		}
		break;

	case CHANIM_STATE_ACTON: {

		if (!detected) {
			cur_state = CHANIM_STATE_DETECTING;
			ACSD_CHANIM("state changed: from %d to %d\n",
				chanim_mark(ch_info).state, cur_state);
			goto post_act;
		}
			/* check for lockout */
		if (chanim_chk_lockout(ch_info)) {
			cur_state = CHANIM_STATE_LOCKOUT;
			ACSD_CHANIM("state changed: from %d to %d\n",
			   chanim_mark(ch_info).state, cur_state);
			goto post_act;
		}

		if (c_info->flags & ACS_FLAGS_LASTUSED_CHK) {
			uint lastused = 0;
			lastused = chanim_scb_lastused(c_info);

			ACSD_DEBUG("lastused: %d\n", lastused);

			if (lastused < (uint)chanim_act_delay(ch_info)) {
				cur_state = CHANIM_STATE_DETECTING;
				ACSD_DEBUG("state changed: from %d to %d\n",
					chanim_mark(ch_info).state, cur_state);
				goto post_act;
			}
		}

		/* taking action */
		ret = acs_run_cs_scan(c_info);
		ACS_ERR(ret, "cs scan failed\n");

		ret = acs_request_data(c_info);
		ACS_ERR(ret, "request data failed\n");

		acs_select_chspec(c_info);
		acs_set_chspec(c_info);
		chanim_upd_acs_record(ch_info, c_info->selected_chspec, APCS_CHANIM);

		ret = acs_update_driver(c_info);
		ACS_ERR(ret, "update driver failed\n");

		cur_state = CHANIM_STATE_DETECTING;
		ACSD_CHANIM("state changed: from %d to %d\n",
			chanim_mark(ch_info).state, cur_state);

post_act:
		chanim_mark(ch_info).detecttime = 0;
		chanim_restore_scbprobe(c_info);

		break;
	}

	case CHANIM_STATE_LOCKOUT:
		if (!detected) {
			cur_state = CHANIM_STATE_DETECTING;
			ACSD_CHANIM("state changed: from %d to %d\n",
				chanim_mark(ch_info).state, cur_state);
			break;
		}

		if (!chanim_chk_lockout(ch_info)) {
			cur_state = CHANIM_STATE_DETECTED;
			ACSD_CHANIM("state changed: from %d to %d\n",
			   chanim_mark(ch_info).state, cur_state);
			chanim_mark(ch_info).detecttime = now;
		}
		break;

	default:
		ACSD_ERROR("Invalid chanim state: %d\n", cur_state);
		break;
	}

	chanim_mark(ch_info).state = cur_state;
	return ret;
}

static void
chanim_update_base(chanim_stats_t * base, chanim_stats_t tmp)
{
	if (base->glitchcnt > tmp.glitchcnt)
		base->glitchcnt = tmp.glitchcnt;
	if (base->badplcp > tmp.badplcp)
		base->badplcp = tmp.badplcp;
	if (base->ccastats[CCASTATS_NOPKT] > tmp.ccastats[CCASTATS_NOPKT])
		base->ccastats[CCASTATS_NOPKT] = tmp.ccastats[CCASTATS_NOPKT];
	if (base->bgnoise > tmp.bgnoise)
		base->bgnoise = tmp.bgnoise;
}

static int
acsd_update_chanim(acs_chaninfo_t * c_info, chanim_stats_t * stats, uint ticks)
{
	int ret = 0;
	chanim_info_t * ch_info = c_info->chanim_info;
	chanim_stats_t tmp;

	memcpy(&tmp, stats, sizeof(chanim_stats_t));

	tmp.glitchcnt = htod32(tmp.glitchcnt);
	tmp.badplcp = htod32(tmp.badplcp);
	tmp.chanspec = htod16(tmp.chanspec);
	tmp.timestamp = htod32(tmp.timestamp);

	memcpy(&ch_info->stats[chanim_mark(ch_info).stats_idx], &tmp,
		sizeof(chanim_stats_t));

	if (acsd_debug_level & ACSD_DEBUG_CHANIM)
		acsd_display_chanim(&tmp);

	if (chanim_base(ch_info).chanspec != tmp.chanspec) {
		if (chanim_base(ch_info).chanspec != 0)
			ch_info->ticks = ticks;
		memcpy(&ch_info->base, &tmp, sizeof(chanim_stats_t));
	}
	else
		chanim_update_base(&ch_info->base, tmp);

	/* Skip default chan switch SM for fast cs mode */
	if (ACS_FCS_MODE(c_info)) {
		/* check if perf ci scna is needed */
		if (stats->chan_idle < c_info->acs_fcs.acs_scan_chanim_stats)
			c_info->scan_chspec_list.ci_pref_scan_request = TRUE;
	}
	else
		chanim_upd_state(c_info);

	ch_info->mark.stats_idx ++;
	if (chanim_mark(ch_info).stats_idx == CHANIM_STATS_RECORD)
		chanim_mark(ch_info).stats_idx = 0;

	ACSD_DEBUG("stats_idx: %d\n", chanim_mark(ch_info).stats_idx);

	return ret;
}

int
acsd_chanim_query(acs_chaninfo_t * c_info, uint32 count, uint32 ticks)
{
	int ret = 0;
	char *data_buf;
	wl_chanim_stats_t *list;
	wl_chanim_stats_t param;
	int buflen = ACS_CHANIM_BUF_LEN;

	data_buf = acsd_malloc(ACS_CHANIM_BUF_LEN);
	list = (wl_chanim_stats_t *) data_buf;

	param.buflen = htod32(buflen);
	param.count = htod32(count);

	ret = wl_iovar_getbuf(c_info->name, "chanim_stats", &param,
		sizeof(wl_chanim_stats_t), data_buf, buflen);
	if (ret < 0)
		ACS_FREE(data_buf);
	ACS_ERR(ret, "failed to get chanim results");

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);

	ACSD_DEBUG("buflen: %d, version: %d count: %d\n",
		list->buflen, list->version, list->count);

	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_CHANIM_STATS_VERSION) {
		fprintf(stderr, "Sorry, your driver has wl_chanim_stats version %d "
			"but this program supports only version %d.\n",
				list->version, WL_CHANIM_STATS_VERSION);
		list->buflen = 0;
		list->count = 0;
	}

	if (count == WL_CHANIM_COUNT_ALL) {
		ACS_FREE(c_info->chanim_stats);
		c_info->chanim_stats = list;
#ifdef DEBUG
		acsd_dump_chanim(c_info->chanim_stats);
#endif
	}
	else {
		ret = acsd_update_chanim(c_info, list->stats, ticks);
		ACS_FREE(list);
	}

	return ret;
}

void
acsd_chanim_check(uint ticks, acs_chaninfo_t *c_info)
{
	chanim_info_t* ch_info = c_info->chanim_info;
	uint8 period = chanim_config(ch_info).sample_period;
	int ret;

	/* sync the sample period in driver if it is changed */
	if (period != chanim_mark(ch_info).wl_sample_period) {
		ret = wl_iovar_setint(c_info->name, "chanim_sample_period", (uint)period);
		if (ret < 0) {
			ACSD_ERROR("failed to set chanim_sample_period");
			return;
		}
		chanim_mark(ch_info).wl_sample_period = period;
	}

	ACSD_DEBUG("ticks: %d, period: %d\n", ticks, period);

	/* start the query after a number of  ticks (since start or channel change,
	 * since bgnoise is  not accurate before that.
	 */
	if (period && (ticks - ch_info->ticks > CHANIM_CHECK_START) &&
		(ticks % period == 0)) {
		acsd_chanim_query(c_info, WL_CHANIM_COUNT_ONE, ticks);
	}
}

int
chanim_txop_to_noise(uint8 txop)
{
	int noise = 0;
	uint8 val = 0;

	if (txop > CHANIM_TXOP_BASE)
		return noise;

	val = CHANIM_TXOP_BASE - txop;
	noise = (int)val * val * val * val / 51200;

	return noise;
}
