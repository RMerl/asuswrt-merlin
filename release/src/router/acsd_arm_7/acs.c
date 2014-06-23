/*
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acs.c 452750 2014-01-31 23:12:13Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmendian.h>

#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <wlutils.h>
#include <shutils.h>
#include <proto/ethernet.h>

#include "acsd_svr.h"
#include "chan_utils.h"

#define PREFIX_LEN 32

/* some channel bounds */
#define ACS_CS_MIN_2G_CHAN	1	/* min channel # in 2G band */
#define ACS_CS_MAX_2G_CHAN	CH_MAX_2G_CHANNEL	/* max channel # in 2G band */
#define ACS_CS_MIN_5G_CHAN	36	/* min channel # in 5G band */
#define ACS_CS_MAX_5G_CHAN	MAXCHANNEL	/* max channel # in 5G band */

/* possible min channel # in the band */
#define ACS_CS_MIN_CHAN(band)	((band == WLC_BAND_5G) ? ACS_CS_MIN_5G_CHAN : \
			(band == WLC_BAND_2G) ? ACS_CS_MIN_2G_CHAN : 0)
/* possible max channel # in the band */
#define ACS_CS_MAX_CHAN(band)	((band == WLC_BAND_5G) ? ACS_CS_MAX_5G_CHAN : \
			(band == WLC_BAND_2G) ? ACS_CS_MAX_2G_CHAN : 0)

#define BAND_2G(band) (band == WLC_BAND_2G)
#define BAND_5G(band) (band == WLC_BAND_5G)

#define ACS_SM_BUF_LEN  1024
#define ACS_SRSLT_BUF_LEN (32*1024)

#define ACS_CHANNEL_1 1
#define ACS_CHANNEL_6 6
#define ACS_CHANNEL_11 11

/*
 * channel_pick_t: Return value from the channel pick (preference comparison) functions
 *	PICK_NONE	: Function made no choice, someone else is to decide.
 *	PICK_CANDIDATE	: Candidate chanspec preferred over current chanspec
 *	PICK_CURRENT	: Current chanspec is preferred over candidate.
 */
typedef enum { PICK_NONE = 0, PICK_CURRENT, PICK_CANDIDATE } channel_pick_t;

#define ACS_DFLT_FLAGS ACS_FLAGS_LASTUSED_CHK

acs_policy_t predefined_policy[ACS_POLICY_MAX] = {
/* threshld    Channel score weigths values                                 chan */
/* bgn  itf  {  BSS  BUSY  INTF I-ADJ   FCS TXPWR NOISE TOTAL   CNS   ADJ}  pick */
/* --- ----   ----- ----- ----- ----- ----- ----- ----- ----- ----- -----   func */
{    0, 100, {   -1,    0,    0,    0,    0,    0,    0,    1,    1,    0}, NULL}, /* 0=DEFAULT   */
{    0, 100, { -100,    0,    0,    0,    0,    0,    0,    0,    1,    0}, NULL}, /* 1=LEGACY    */
{  -65,  40, {   -1,    0, -100,   -1,    0,    0,    0,    0,    1,    0}, NULL}, /* 2=INTF      */
{  -65,  40, {   -1, -100, -100,   -1,    0,    0, -100,    0,    1,    0}, NULL}, /* 3=INTF_BUSY */
{  -65,  40, {   -1, -100, -100,   -1, -100,    0, -100,    0,    1,    0}, NULL}, /* 4=OPTIMIZED */
{  -55,  45, { -200,    0, -100,  -50,    0,    0,  -50,    0,    1,    0}, NULL}, /* 5=CUSTOM1   */
{  -70,  45, {   -1,  -50, -100,  -10,  -10,    0,  -50,    0,    1,    0}, NULL}, /* 6=CUSTOM2   */
{    0, 100, {    0,    0,    0,    0,    0,    0,    0,    0,    1,    1}, NULL}, /* 7=FCS       */
};

acs_info_t *acs_info;

int CHANNEL_NUM_2G = 11;
char chanspecbuf[32];

static void
acs_ci_scan_update_idx(acs_scan_chspec_t *chspec_q, uint8 increment)
{
	uint8 idx = chspec_q->idx + increment;
	uint32 chan_flags;

	if (idx >= chspec_q->count)
		idx = 0;

	do {
		chan_flags = chspec_q->chspec_list[idx].flags;

		/* check if it is preffered channel and pref scanning requested */
		if ((chspec_q->ci_scan_running == ACS_CI_SCAN_RUNNING_PREF)) {
			if ((chan_flags & ACS_CI_SCAN_CHAN_PREF))
				break;
		} else if (!(chan_flags & ACS_CI_SCAN_CHAN_EXCL))
			break;

		/* increment index */
		if (++idx == chspec_q->count)
			idx = 0;

	} while (idx != chspec_q->idx);

	chspec_q->idx = idx;
}

/*
 * Function to set a channel table by parsing a list consisting
 * of a comma-separated channel numbers.
 */
static int
acs_set_chan_table(char *channel_list, chanspec_t *chspec_list,
                      unsigned int vector_size)
{
	int chan_index;
	int channel;
	int chan_count = 0;
	char *chan_str;
	char *delim = ",";
	char chan_buf[ACS_MAX_VECTOR_LEN + 2];
	int list_length;

	/*
	* NULL list means no channels are set. Return without
	* modifying the vector.
	*/
	if (channel_list == NULL)
		return 0;

	/*
	* A non-null list means that we must set the vector.
	*  Clear it first.
	* Then parse a list of <chan>,<chan>,...<chan>
	*/
	memset(chan_buf, 0, sizeof(chan_buf));
	list_length = strlen(channel_list);
	list_length = MIN(list_length, ACS_MAX_VECTOR_LEN);
	strncpy(chan_buf, channel_list, list_length);
	strncat(chan_buf, ",", list_length);

	chan_str = strtok(chan_buf, delim);

	for (chan_index = 0; chan_index < vector_size; chan_index++)
	{
		if (chan_str == NULL)
			break;
		channel = strtoul(chan_str, NULL, 16);
		if (channel == 0)
			break;
		chspec_list[chan_count++] = channel;
		chan_str = strtok(NULL, delim);
	}
	return chan_count;
}

#ifdef DEBUG
static void
acs_dump_config_extra(acs_chaninfo_t *c_info)
{
	acs_fcs_t *fcs_info = &c_info->acs_fcs;

	printf("acs_dump_config_extra:\n");
	printf("\t acs_txdelay_period: %d\n", fcs_info->acs_txdelay_period);
	printf("\t acs_txdelay_cnt: %d\n", fcs_info->acs_txdelay_cnt);
	printf("\t acs_txdelay_ratio: %d\n", fcs_info->acs_txdelay_ratio);
	printf("\t acs_dfs: %d\n", fcs_info->acs_dfs);
	printf("\t acs_lowband_least_rssi: %d\n", fcs_info->acs_lowband_least_rssi);
	printf("\t acs_nofcs_least_rssi: %d\n", fcs_info->acs_nofcs_least_rssi);
	printf("\t acs_chan_dwell_time: %d\n", fcs_info->acs_chan_dwell_time);
	printf("\t acs_chan_flop_period: %d\n", fcs_info->acs_chan_flop_period);
	printf("\t acs_tx_idle_cnt: %d\n", fcs_info->acs_tx_idle_cnt);
	printf("\t acs_cs_scan_timer: %d\n", c_info->acs_cs_scan_timer);
	printf("\t acs_ci_scan_timeout: %d\n", fcs_info->acs_ci_scan_timeout);
	printf("\t acs_ci_scan_timer: %d\n", c_info->acs_ci_scan_timer);
	printf("\t acs_scan_chanim_stats: %d\n", fcs_info->acs_scan_chanim_stats);
	printf("\t acs_fcs_chanim_stats: %d\n", fcs_info->acs_fcs_chanim_stats);
	printf("\t acs_fcs_mode: %d\n", c_info->acs_fcs_mode);
}
#endif /* DEBUG */


static void
acs_fcs_retrieve_config(acs_chaninfo_t *c_info, char * prefix)
{
	/* retrieve policy related configuration from nvram */
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	char tmp[100], *str;
	uint8 chan_count;

	ACSD_INFO("retrieve FCS config from nvram ...\n");

	if ((str = nvram_get(strcat_r(prefix, "acs_txdelay_period", tmp))) == NULL)
		fcs_info->acs_txdelay_period = ACS_TXDELAY_PERIOD;
	else
		fcs_info->acs_txdelay_period = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_txdelay_cnt", tmp))) == NULL)
		fcs_info->acs_txdelay_cnt = ACS_TXDELAY_CNT;
	else
		fcs_info->acs_txdelay_cnt = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_txdelay_ratio", tmp))) == NULL)
		fcs_info->acs_txdelay_ratio = ACS_TXDELAY_RATIO;
	else
		fcs_info->acs_txdelay_ratio = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_lowband_least_rssi", tmp))) == NULL)
		fcs_info->acs_lowband_least_rssi = ACS_LOWBAND_LEAST_RSSI;
	else
		fcs_info->acs_lowband_least_rssi = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_nofcs_least_rssi", tmp))) == NULL)
		fcs_info->acs_nofcs_least_rssi = ACS_NOFCS_LEAST_RSSI;
	else
		fcs_info->acs_nofcs_least_rssi = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_scan_chanim_stats", tmp))) == NULL)
		fcs_info->acs_scan_chanim_stats = ACS_SCAN_CHANIM_STATS;
	else
		fcs_info->acs_scan_chanim_stats = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_fcs_chanim_stats", tmp))) == NULL)
		fcs_info->acs_fcs_chanim_stats = ACS_FCS_CHANIM_STATS;
	else
		fcs_info->acs_fcs_chanim_stats = atoi(str);

	memset(&fcs_info->pref_chans, 0, sizeof(fcs_conf_chspec_t));
	if ((str = nvram_get(strcat_r(prefix, "acs_pref_chans", tmp))) == NULL)	{
		fcs_info->pref_chans.count = 0;
	} else {
		chan_count = acs_set_chan_table(str, fcs_info->pref_chans.clist, ACS_MAX_LIST_LEN);
		fcs_info->pref_chans.count = chan_count;
	}

	memset(&fcs_info->excl_chans, 0, sizeof(fcs_conf_chspec_t));
	if ((str = nvram_get(strcat_r(prefix, "acs_excl_chans", tmp))) == NULL)	{
		fcs_info->excl_chans.count = 0;
	} else {
		chan_count = acs_set_chan_table(str, fcs_info->excl_chans.clist, ACS_MAX_LIST_LEN);
		fcs_info->excl_chans.count = chan_count;
	}

	if ((str = nvram_get(strcat_r(prefix, "acs_dfs", tmp))) == NULL)
		fcs_info->acs_dfs = ACS_DFS_ENABLED;
	else
		fcs_info->acs_dfs = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_chan_dwell_time", tmp))) == NULL)
		fcs_info->acs_chan_dwell_time = ACS_CHAN_DWELL_TIME;
	else
		fcs_info->acs_chan_dwell_time = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_chan_flop_period", tmp))) == NULL)
		fcs_info->acs_chan_flop_period = ACS_CHAN_FLOP_PERIOD;
	else
		fcs_info->acs_chan_flop_period = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_tx_idle_cnt", tmp))) == NULL)
		fcs_info->acs_tx_idle_cnt = ACS_TX_IDLE_CNT;
	else
		fcs_info->acs_tx_idle_cnt = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_ci_scan_timeout", tmp))) == NULL)
		fcs_info->acs_ci_scan_timeout = ACS_CI_SCAN_TIMEOUT;
	else
		fcs_info->acs_ci_scan_timeout = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_cs_scan_timer", tmp))) == NULL)
		c_info->acs_cs_scan_timer = ACS_DFLT_CS_SCAN_TIMER;
	else
		c_info->acs_cs_scan_timer = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_ci_scan_timer", tmp))) == NULL)
		c_info->acs_ci_scan_timer = ACS_DFLT_CI_SCAN_TIMER;
	else
		c_info->acs_ci_scan_timer = atoi(str);

	fcs_info->intfparams.version = INTFER_VERSION;
	if ((str = nvram_get(strcat_r(prefix, "intfer_period", tmp))) == NULL)
		fcs_info->intfparams.period = ACS_INTFER_SAMPLE_PERIOD;
	else
		fcs_info->intfparams.period = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "intfer_cnt", tmp))) == NULL)
		fcs_info->intfparams.cnt = ACS_INTFER_SAMPLE_COUNT;
	else
		fcs_info->intfparams.cnt = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "intfer_txfail", tmp))) == NULL)
		fcs_info->intfparams.txfail_thresh = ACS_INTFER_TXFAIL_THRESH;
	else
		fcs_info->intfparams.txfail_thresh = strtoul(str, NULL, 0);

	if ((str = nvram_get(strcat_r(prefix, "intfer_tcptxfail", tmp))) == NULL)
		fcs_info->intfparams.txfail_thresh = ACS_INTFER_TCPTXFAIL_THRESH;
	else
		fcs_info->intfparams.tcptxfail_thresh = strtoul(str, NULL, 0);

	if (nvram_match(strcat_r(prefix, "dcs_csa_unicast", tmp), "1"))
		fcs_info->acs_dcs_csa = CSA_UNICAST_ACTION_FRAME;
	else
		fcs_info->acs_dcs_csa = CSA_BROADCAST_ACTION_FRAME;

#ifdef DEBUG
	acs_dump_config_extra(c_info);
#endif /* DEBUG */
}

static int
cmpchspec(const void *p1, const void *p2)
{
	return (wf_chspec_ctlchan((*(ch_candidate_t *) p1).chspec) - wf_chspec_ctlchan((*(ch_candidate_t *) p2).chspec));
}

/*
 * acs_pick_better_dfsr() - pick one of two candidates based on DFS Re-Entry settings.
 *
 * candidate:	The candidate to compare against the currently selected candidate.
 * current:	The currently selected candidate.
 * score_type:	CS_SCORE_xxx (index into the candidate score array)
 *
 * This function selects the preferred of the two passed candidates based on DFS Re-Entry settings.
 *
 * Returned value:
 *	PICK_CANDIDATE	: Candidate chanspec preferred over current chanspec
 *	PICK_CURRENT	: Current chanspec is preferred over candidate.
 *	PICK_NONE	: We made no choice, someone else please decide.
 *
 * If DFS Re-Entry is enabled, we prefer DFS channels over any others. If both candidates are DFS,
 * we try to decide based on the score. If the score is the same, we prefer the higher channel. If
 * that is the same as well, we keep what chanspec we have.
 *
 * Note:	This only comes into play for initial channel selection or DFS Re-Entry, where we
 *		want to try and get onto a DFS channel which is supposedly less crowded.
 *		At other times, DFS channels will have been disabled in the candidates.
 */
static channel_pick_t
acs_pick_better_dfsr(ch_candidate_t *candidate, ch_candidate_t *current, int score_type)
{
	ACSD_DFSR("Current: %sDFS channel #%d (0x%x) score[%d] %d,"
		" candidate: %sDFS channel #%d (0x%x) score %d.\n",
		current->is_dfs ? "" : "non-",
		CHSPEC_CHANNEL(current->chspec), current->chspec,
		score_type,
		current->chscore[score_type].score,
		candidate->is_dfs ? "" : "non-",
		CHSPEC_CHANNEL(candidate->chspec), candidate->chspec,
		candidate->chscore[score_type].score);

	if (candidate->is_dfs) { /* candidate is DFS */

		/* DFS wins over non-DFS */
		if (!current->is_dfs) { /* current winner is not DFS, replace */
			ACSD_DFSR("-- selecting candidate: DFS\n");
			return PICK_CANDIDATE;
		}

		/* Both are DFS - lower score wins */
		if (candidate->chscore[score_type].score < current->chscore[score_type].score) {
			ACSD_DFSR("-- selecting candidate: better score.\n");
			return PICK_CANDIDATE;
		}

		/* Both DFS, same score - higher channel number wins. */
		if (CHSPEC_CHANNEL(candidate->chspec) > CHSPEC_CHANNEL(current->chspec)) {
			ACSD_DFSR("-- selecting candidate: higher DFS channel #.\n");
			return PICK_CANDIDATE;
		}

		/* Both DFS, same or worse score, same or lower channel - keep what we have. */
		/* We could randomize the channel selected here */
		ACSD_DFSR("-- keeping current DFS channel.\n");
		return PICK_CURRENT;
	}
	/* Candidate is not DFS, if the current is DFS, keep it. */

	ACSD_DFSR("-- DSFR %s\n", (current->is_dfs) ? "prefers current channel":"does not care");

	return (current->is_dfs) ? PICK_CURRENT : PICK_NONE;
}

/*
 * acs_pick_better_score() - pick one of two candidates based on a specific score.
 *
 * candidate:	The candidate to compare against the currently selected candidate.
 * current:	The currently selected candidate.
 * score_type:	CS_SCORE_xxx (index into the candidate score array)
 *
 * This function selects the preferred of the two passed candidates based on the score.
 *
 * Returned value:
 *	PICK_CANDIDATE	: Candidate chanspec preferred over current chanspec
 *	PICK_CURRENT	: Current chanspec is preferred over candidate.
 */
static channel_pick_t
acs_pick_better_score(ch_candidate_t *candidate, ch_candidate_t *current, int score_type)
{
	/* if number of bss is equal, pick based of distance from interfer */
	if (candidate->chscore[score_type].score == current->chscore[score_type].score) {
		/* for 2G, pick one far away from interferer */
		if (CHSPEC_IS2G(candidate->chspec)) {
			int intadj_score = current->chscore[CH_SCORE_INTFADJ].score;
			if (candidate->chscore[CH_SCORE_INTFADJ].score < intadj_score) {
				ACSD_INFO("-- same score, selected quieter 2G channel.\n");
				return PICK_CANDIDATE;
			}
		} else {
			/* for 5G, pick high power channels if possible */
			uint channel = CHSPEC_CHANNEL(current->chspec);
			if (CHSPEC_CHANNEL(candidate->chspec) > channel) {
				ACSD_INFO("-- same score, selected higher 5G channel.\n");
				return PICK_CANDIDATE;
			}
		}
	} else if (candidate->chscore[score_type].score < current->chscore[score_type].score) {
		ACSD_INFO("-- selected lower (=better) score channel.\n");
		return PICK_CANDIDATE;
	}
	ACSD_INFO("-- keeping current channel.\n");
	return PICK_CURRENT;
}

/*
 * acs_remove_noisy_cns() - Find candidate with best CNS score and disable too distant candidates.
 *
 * candi:	pointer to the candidate array
 * c_count:	number of candidates in the array
 * distance:	value of configuration parameter acs_trigger_var
 *
 * This function looks up the candidate with the best (lowest) CNS score, and removes all
 * other candidates whose CNS score is further than a certain distance by disabling them.
 */
static void
acs_remove_noisy_cns(ch_candidate_t *candi, int c_count, int distance)
{
	ch_score_t *best_score_p = NULL;
	int i;

	/* Determine candidate with the best (lowest) CNS score */
	for (i = 0; i < c_count; i++) {
		if (!candi[i].valid)
			continue;
		if (!best_score_p) {
			best_score_p = candi[i].chscore;
			continue;
		}
		if ((candi[i].chscore[CH_SCORE_CNS].score
			< best_score_p[CH_SCORE_CNS].score)) {
			best_score_p = candi[i].chscore;
		}
	}

	/* ban chanspec that are too far away from best figure */
	for (i = 0; i < c_count; i++) {
		if (candi[i].valid &&
			(candi[i].chscore[CH_SCORE_CNS].score >=
			(best_score_p[CH_SCORE_CNS].score + distance))) {
			ACSD_INFO("banning chanspec %x because of interference \n",
				candi[i].chspec);
			candi[i].valid = FALSE;
			candi[i].reason |= ACS_INVALID_NOISE;
		}
	}
}

/*
 * acs_pick_chanspec_common() - shared function to pick a chanspec to switch to.
 *
 * c_info:	pointer to the acs_chaninfo_t for this interface.
 * bw:		bandwidth to chose from
 * score_type:	CS_SCORE_xxx (index into the candidate score array)
 *
 * Returned value:
 *	The returned value is the most preferred valid chanspec from the candidate array.
 *
 * This function starts by eliminating all candidates whose CNS is too far away from the best
 * CNS score, and then selects a chanspec to use by walking the list of valid candidates and
 * selecting the most preferred one. This selection is currently based on the score_type only,
 * further selection mechanisms are in the works.
 */
static chanspec_t
acs_pick_chanspec_common(acs_chaninfo_t *c_info, int bw, int score_type)
{
	chanspec_t chspec = 0;
	int i, index = -1;
	ch_candidate_t *candi = c_info->candidate[bw];
        uint ctrl_ch;

        qsort(candi, c_info->c_count[bw], sizeof(ch_candidate_t), cmpchspec);

	ACSD_INFO("Selecting channel, score type %d...\n", score_type);
	/* find the chanspec with best figure (cns) */
	acs_remove_noisy_cns(candi, c_info->c_count[bw],
		c_info->chanim_info->config.acs_trigger_var);

	/* Walk all candidate chanspecs and select the best one to use. */
	for (i = 0; i < c_info->c_count[bw]; i++) {
		channel_pick_t choice;

		if (!candi[i].valid)
			continue;

		ctrl_ch = wf_chspec_ctlchan(candi[i].chspec);
		if (CHSPEC_IS2G(candi[i].chspec)
			&& ((ctrl_ch == 1) || (ctrl_ch == 11)))
		{
			candi[i].chscore[score_type].score += 20;
#if 0
			ACSD_PRINT("score++ for chanspec 0x%x (%s): %d\n",
				candi[i].chspec, wf_chspec_ntoa(candi[i].chspec, chanspecbuf), candi[i].chscore[score_type].score);
#endif
		}

		if (index < 0) { /* No previous candi, avoid comparing against random memory */
			index = i; /* Select first valid candidate as a starting point */
			ACSD_INFO("[%d] Default: %s channel #%d (0x%x) with score %d\n",
				i, (candi[i].is_dfs) ? "DFS" : "non-DFS",
				CHSPEC_CHANNEL(candi[i].chspec), candi[i].chspec,
				candi[i].chscore[score_type].score);
			continue;
		}

		ACSD_INFO("[%d] Checking %s channel #%d (0x%x) with score %d\n",
			i, (candi[i].is_dfs) ? "DFS" : "non-DFS",
			CHSPEC_CHANNEL(candi[i].chspec), candi[i].chspec,
			candi[i].chscore[score_type].score);

		/*
		 * See if one of our choice mechanisms has a preferred candidate. Whoever picks
		 * a chanspec first wins.
		 */
		choice = PICK_NONE;
		if (ACS_FCS_MODE(c_info) && (c_info->acs_fcs.acs_dfs != ACS_DFS_DISABLED)) {
			choice = acs_pick_better_dfsr(&candi[i], &candi[index], score_type);
		}
		if (choice == PICK_NONE) {
			choice = acs_pick_better_score(&candi[i], &candi[index], score_type);
		}
		if (choice == PICK_CANDIDATE) {
			index = i;
		}
	}

	/* reset monitoring state machine */
	chanim_mark(c_info->chanim_info).best_score = 0;

	if (index >= 0) {
		chspec = candi[index].chspec;
		ACSD_INFO("Selected Channel #%d (0x%x)\n", CHSPEC_CHANNEL(chspec), chspec);
	}
	return chspec;
}

/*
 * acs_pick_chanspec_fcs_policy() - FCS policy specific function to pick a chanspec to switch to.
 *
 * c_info:	pointer to the acs_chaninfo_t for this interface.
 * bw:		bandwidth to chose from
 *
 * Returned value:
 *	The returned value is the most preferred valid chanspec from the candidate array.
 *
 * This function picks the most preferred chanspec according to the FCS policy. At the time of
 * this writing, this is a selection based on the CH_SCORE_ADJ score.
 */
static chanspec_t
acs_pick_chanspec_fcs_policy(acs_chaninfo_t *c_info, int bw)
{
	return acs_pick_chanspec_common(c_info, bw, CH_SCORE_ADJ);
}

static int
wl_chanspec_list_with_band_4(int unit, int bw)
{
	chanspec_t c = 0, *chanspec;
	char *buf = (char *)malloc(WLC_IOCTL_MAXLEN);
	int buflen;
	wl_uint32_list_t *list;
	int ret = 0, i = 0;
	uint16 bandwidth = 0;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *country_code;
	char *name;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	country_code = nvram_safe_get(strcat_r(prefix, "country_code", tmp));
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (buf == NULL) {
		ret = -1;
		goto exit;
	}

	strcpy(buf, "chanspecs");
	buflen = strlen(buf) + 1;

	if (!unit)
		c |= WL_CHANSPEC_BAND_2G;
	else
		c |= WL_CHANSPEC_BAND_5G;

	if (bw == 80) {
		c |= WL_CHANSPEC_BW_80;
		bandwidth = WL_CHANSPEC_BW_80;
	} else if (bw == 40) {
		c |= WL_CHANSPEC_BW_40;
		bandwidth = WL_CHANSPEC_BW_40;
	} else {
		c |= WL_CHANSPEC_BW_20;
		bandwidth = WL_CHANSPEC_BW_20;
	}
	
	chanspec = (chanspec_t *)(buf + buflen);
	*chanspec = c;
	buflen += (sizeof(chanspec_t));
	strncpy(buf + buflen, country_code, WLC_CNTRY_BUF_SZ);
	buflen += WLC_CNTRY_BUF_SZ;

	/* Add list */
	list = (wl_uint32_list_t *)(buf + buflen);
	list->count = WL_NUMCHANSPECS;
	buflen += sizeof(uint32)*(WL_NUMCHANSPECS + 1);

	ret = wl_ioctl(name, WLC_GET_VAR, buf, buflen);
	if (ret < 0) {
		goto exit;
	}

	list = (wl_uint32_list_t *)buf;

	if (list->count > 0)
	{
		for (i = 0; i < list->count; i++) {
			c = (chanspec_t) list->element[i];

			if (bandwidth && CHSPEC_BW(c) != bandwidth)
				continue;

			if (CTLCH_IS_BAND4(c))
			{
				ret = 1;
				goto exit;
			}
		}
	}

exit:
	if (buf)
		free(buf);

	return ret;
}

/*
 * This module retrieves the following information from the wl driver before
 * deciding on the best channel:
 * 1) scan result (wl_scan_result_t)
 * 2) channel interference stats (wl_chanim_stats_t)
 * 3) scan channel spec list
 * 4) channel spec candidate (all valid channel spec for the current band, bw, locale)
 * 5) band type, coex_enable, bw_cap.
 *
 * The facts which could be weighted in the channel scoring systems are:
 * 1) Number of BSS's detected during the scan process (from scan result)
 * 2) Channel Occupancy (percentage of time the channel is occupied by other BSS's)
 * 3) Channel Interference (from CCA stats)
 * 4) Channel FCS (from CCA stats)
 * 5) Channel MAX tx power
 * 6) Adjacent Channel Interference
 * The scoring algorithm for each factor is subject to update based on testing results.
 * The weight for each factor can be customized based on different channel eval policies.
 */

static int
acs_build_scanlist(acs_chaninfo_t *c_info)
{
	wl_uint32_list_t *list;
	chanspec_t input = 0, c = 0;
	int ret = 0, i, j;
	int count = 0;
	scan_chspec_elemt_t *ch_list;
	acs_rsi_t *rsi = &c_info->rs_info;
	fcs_conf_chspec_t *pref_chans = &(c_info->acs_fcs.pref_chans);
	fcs_conf_chspec_t *excl_chans = &(c_info->acs_fcs.excl_chans);

	char *data_buf, *data_buf1 = NULL;
	data_buf = acsd_malloc(ACS_SM_BUF_LEN);

	input |= WL_CHANSPEC_BW_20;

	if (BAND_5G(rsi->band_type))
		input |= WL_CHANSPEC_BAND_5G;
	else
		input |= WL_CHANSPEC_BAND_2G;

	ret = wl_iovar_getbuf(c_info->name, "chanspecs", &input, sizeof(chanspec_t),
		data_buf, ACS_SM_BUF_LEN);
	if (ret < 0)
		ACS_FREE(data_buf);
	ACS_ERR(ret, "failed to get valid chanspec lists");

	list = (wl_uint32_list_t *)data_buf;
	count = dtoh32(list->count);

	if (BAND_2G(rsi->band_type))
		CHANNEL_NUM_2G = count;

	c_info->scan_chspec_list.count = count;
	c_info->scan_chspec_list.idx = 0;
	c_info->scan_chspec_list.pref_count = 0;
	c_info->scan_chspec_list.excl_count = 0;

	if (!count) {
		ACSD_ERROR("number of valid chanspec is 0\n");
		ret = -1;
		goto cleanup_sl;
	}

	ACS_FREE(c_info->scan_chspec_list.chspec_list);

	ch_list = c_info->scan_chspec_list.chspec_list =
		(scan_chspec_elemt_t *)acsd_malloc(count * sizeof(scan_chspec_elemt_t));

	data_buf1 = acsd_malloc(ACS_SM_BUF_LEN);

	for (i = 0; i < count; i++) {
		c = (chanspec_t)dtoh32(list->element[i]);

		ch_list[i].chspec = c;

		if (BAND_5G(rsi->band_type)) {
			input = c;
			ret = wl_iovar_getbuf(c_info->name, "per_chan_info", &input,
				sizeof(chanspec_t), data_buf1, ACS_SM_BUF_LEN);
			if (ret < 0) {
				ACS_FREE(data_buf);
				ACS_FREE(data_buf1);
			}
			ACS_ERR(ret, "failed to get per_chan_info");

			ch_list[i].chspec_info = dtoh32(*(uint32 *)data_buf1);

			/* Exclude DFS channels if 802.11h spectrum management is off */
			if (!rsi->reg_11h && (ch_list[i].chspec_info & WL_CHAN_RADAR)) {
				ch_list[i].flags |= ACS_CI_SCAN_CHAN_EXCL;
				c_info->scan_chspec_list.excl_count++;
			}
		}

		/* Update preffered channel attribute */
		if (pref_chans && pref_chans->count) {
			for (j = 0; j < pref_chans->count; j++) {
				if (c == pref_chans->clist[j]) {
					ch_list[i].flags |= ACS_CI_SCAN_CHAN_PREF;
					c_info->scan_chspec_list.pref_count++;
					break;
				}
			}
		}

		/* Update exclude channel attribute */
		if (excl_chans && excl_chans->count) {
			for (j = 0; j < excl_chans->count; j++) {
				if (c == excl_chans->clist[j]) {
					ch_list[i].flags |= ACS_CI_SCAN_CHAN_EXCL;
					c_info->scan_chspec_list.excl_count++;
					break;
				}
			}
		}
		ACSD_INFO("chanspec: (0x%04x), chspec_info: 0x%x  pref_chan: 0x%x\n", c,
			ch_list[i].chspec_info, ch_list[i].flags);
	}
	acs_ci_scan_update_idx(&c_info->scan_chspec_list, 0);

cleanup_sl:
	ACS_FREE(data_buf);
	ACS_FREE(data_buf1);

	return ret;
}

static int
acs_scan_prep(acs_chaninfo_t *c_info, wl_scan_params_t *params, int *params_size)
{
	int ret = 0;
	int i, scount = 0;
	acs_scan_chspec_t* scan_chspec_p = &c_info->scan_chspec_list;

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = WL_SCANFLAGS_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = ACS_CS_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 0;

	ret = acs_build_scanlist(c_info);
	ACS_ERR(ret, "failed to build scan chanspec list");

	for (i = 0; i < scan_chspec_p->count; i++) {
		if (scan_chspec_p->chspec_list[i].flags & ACS_CI_SCAN_CHAN_EXCL)
			continue;
		params->channel_list[scount++] = htodchanspec(scan_chspec_p->chspec_list[i].chspec);
	}
	params->channel_num = htod32(scount & WL_SCAN_PARAMS_COUNT_MASK);
	ACSD_INFO("scan channel number: %d\n", params->channel_num);

	*params_size = WL_SCAN_PARAMS_FIXED_SIZE + scount * sizeof(uint16);
	ACSD_INFO("params size: %d\n", *params_size);

	return ret;
}

/* channel information (quick) scan at run time */
int
acs_run_ci_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	int i;
	wl_scan_params_t *params = NULL;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + sizeof(uint16);
	acs_scan_chspec_t* scan_chspec_q = &c_info->scan_chspec_list;
	scan_chspec_elemt_t *scan_elemt = NULL;
	bool is_dfs = FALSE;
	channel_info_t ci;

	if ((scan_chspec_q->count - scan_chspec_q->excl_count) == 0) {
		ACSD_INFO("scan chanspec queue is empty.\n");
		return ret;
	}

	scan_elemt = &scan_chspec_q->chspec_list[scan_chspec_q->idx];
	if (scan_elemt->chspec_info & WL_CHAN_RADAR)
		is_dfs = TRUE;

	params = (wl_scan_params_t*)acsd_malloc(params_size);

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = is_dfs ? WL_SCANFLAGS_PASSIVE : 0;
	params->nprobes = -1;
	params->active_time = ACS_CI_SCAN_DWELL;
	params->passive_time = ACS_CI_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 1; /* 1 channel for each ci scan */

	params->channel_list[0] = htodchanspec(scan_elemt->chspec);

	ret = wl_ioctl(c_info->name, WLC_SCAN, params, params_size);
	if (ret < 0)
		ACS_FREE(params);
	ACS_ERR(ret, "WLC_SCAN failed");

	if (!ret) {
		acs_ci_scan_update_idx(scan_chspec_q, 1);
		if (ACS_FCS_MODE(c_info))
			c_info->acs_fcs.timestamp_acs_scan = uptime();
		sleep_ms(ACS_CI_SCAN_DWELL * 5);
		for (i = 0; i < 10; i++) {
			ret = wl_ioctl(c_info->name, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t));
			if (ret < 0)
				ACS_FREE(params);
			ACS_ERR(ret, "WLC_GET_CHANNEL failed");

			ci.scan_channel = dtoh32(ci.scan_channel);
			if (!ci.scan_channel)
				break;

			ACSD_PRINT("scan in progress ...\n");
			sleep_ms(2);
		}
	}
	ACSD_INFO("ci scan on chspec: 0x%x\n", scan_elemt->chspec);
	ACS_FREE(params);
	return ret;
}

/* channel selection (full) scan at init/reset time */
int
acs_run_cs_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	int i;
	wl_scan_params_t *params = NULL;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + ACS_NUMCHANNELS * sizeof(uint16);
	channel_info_t ci;

	params = (wl_scan_params_t*)acsd_malloc(params_size);
	ret = acs_scan_prep(c_info, params, &params_size);
	if (ret < 0) {
		ACS_FREE(params);
		ACS_ERR(ret, "failed to do scan prep");
	}

	ret = wl_ioctl(c_info->name, WLC_SCAN, params, params_size);
	if (ret < 0) {
		ACS_FREE(params);
		ACS_ERR(ret, "WLC_SCAN failed");
	}

	memset(&ci, 0, sizeof(channel_info_t));
	/* loop to check if cs scan is done, check for scan in progress */
	if (!ret) {
		if (ACS_FCS_MODE(c_info)) {
			c_info->acs_fcs.timestamp_acs_scan = uptime();
			c_info->acs_fcs.timestamp_tx_idle = c_info->acs_fcs.timestamp_acs_scan;
		}
		/* this time needs to be < 1000 to prevent mpc kicking in for 2nd radio */
		sleep_ms(ACS_CS_SCAN_DWELL);
		for (i = 0; i < 100; i++) {
			ret = wl_ioctl(c_info->name, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t));
			if (ret < 0) {
				ACS_FREE(params);
				ACS_ERR(ret, "WLC_GET_CHANNEL failed");
			}

			ci.scan_channel = dtoh32(ci.scan_channel);
			if (!ci.scan_channel)
				break;

			ACSD_PRINT("scan in progress ...\n");
			sleep_ms(ACS_CS_SCAN_DWELL);
		}
	}
	ACS_FREE(params);
	return ret;
}

static int
acs_get_scan(char* name, char *scan_buf, uint buf_len)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_buf;
	int ret = 0;

	list->buflen = htod32(buf_len);
	ret = wl_ioctl(name, WLC_SCAN_RESULTS, scan_buf, buf_len);
	if (ret)
		ACSD_ERROR("err from WLC_SCAN_RESULTS: %d\n", ret);

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_BSS_INFO_VERSION &&
	           list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	           list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
	}
	ACSD_INFO("list->count: %d, list->buflen: %d\n", list->count, list->buflen);

	return ret;
}

#ifdef ACS_DEBUG
static void
acs_dump_map(void)
{
	int i;
	ifname_idx_map_t* cur_map;

	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		cur_map = &acs_info->acs_ifmap[i];
		ACSD_PRINT("i: %d, name: %s, idx: %d, in_use: %d\n",
			i, cur_map->name, cur_map->idx, cur_map->in_use);
	}
}
#endif /* ACS_DEBUG */

static void
acs_add_map(char *name)
{
	int i;
	ifname_idx_map_t* cur_map = acs_info->acs_ifmap;
	size_t length = strlen(name);

	ACSD_DEBUG("add map entry for ifname: %s\n", name);

	if (length >= sizeof(cur_map->name)) {
		ACSD_ERROR("Interface Name Length Exceeded\n");
	} else {
		for (i = 0; i < ACS_MAX_IF_NUM; cur_map++, i++) {
			if (!cur_map->in_use) {
				memcpy(cur_map->name, name, length + 1);
				cur_map->idx = i;
				cur_map->in_use = TRUE;
				break;
			}
		}
	}
#ifdef ACS_DEBUG
	acs_dump_map();
#endif
}

int
acs_idx_from_map(char *name)
{
	int i;
	ifname_idx_map_t *cur_map;

#ifdef ACS_DEBUG
	acs_dump_map();
#endif
	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		cur_map = &acs_info->acs_ifmap[i];
		if (cur_map->in_use && !strcmp(name, cur_map->name)) {
			ACSD_DEBUG("name: %s, cur_map->name: %s idx: %d\n",
				name, cur_map->name, cur_map->idx);
			return cur_map->idx;
		}
	}
	ACSD_ERROR("cannot find the mapped entry for ifname: %s\n", name);
	return -1;
}

/* maybe we do not care about 11b anymore */
static bool
acs_bss_is_11b(wl_bss_info_t* bi)
{
	uint i;
	bool b = TRUE;

	for (i = 0; i < bi->rateset.count; i++) {
		b = bi->rateset.rates[i] & 0x80;
		if (!b)
			break;
	}
	return b;
}

static void
acs_parse_chanspec(chanspec_t chanspec, acs_channel_t* chan_ptr)
{

	bzero(chan_ptr, sizeof(acs_channel_t));
	chan_ptr->control = CHSPEC_CHANNEL(chanspec);

	if (CHSPEC_IS80(chanspec)) {
		chan_ptr->ext20 = CHSPEC_CHANNEL(chanspec);
		chan_ptr->ext40[0] = chan_ptr->ext40[1] = CHSPEC_CHANNEL(chanspec);
		if ((chanspec & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_LL) {
			chan_ptr->control -= CH_20MHZ_APART + CH_10MHZ_APART;
			chan_ptr->ext20 -= CH_10MHZ_APART;
			chan_ptr->ext40[0] += CH_10MHZ_APART;
			chan_ptr->ext40[1] += CH_10MHZ_APART + CH_20MHZ_APART;
		}
		else if ((chanspec & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_LU) {
			chan_ptr->control -=  CH_10MHZ_APART;
			chan_ptr->ext20 -= CH_20MHZ_APART + CH_10MHZ_APART;
			chan_ptr->ext40[0] += CH_10MHZ_APART;
			chan_ptr->ext40[1] += CH_10MHZ_APART + CH_20MHZ_APART;
		}
		else if ((chanspec & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_UL) {
			chan_ptr->control +=  CH_10MHZ_APART;
			chan_ptr->ext20 += CH_20MHZ_APART + CH_10MHZ_APART;
			chan_ptr->ext40[0] -= CH_10MHZ_APART + CH_20MHZ_APART;
			chan_ptr->ext40[1] -= CH_10MHZ_APART;
		}
		else if ((chanspec & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_UU) {
			chan_ptr->control +=  CH_20MHZ_APART + CH_10MHZ_APART;
			chan_ptr->ext20 +=  CH_10MHZ_APART;
			chan_ptr->ext40[1] -= CH_10MHZ_APART;
			chan_ptr->ext40[0] -= CH_10MHZ_APART + CH_20MHZ_APART;
		}
	}
	else if (CHSPEC_IS40(chanspec)) {
		chan_ptr->ext20 = CHSPEC_CHANNEL(chanspec);
		if (CHSPEC_SB_LOWER(chanspec)) {
			chan_ptr->control -= CH_10MHZ_APART;
			chan_ptr->ext20 += CH_10MHZ_APART;
		}
		else if (CHSPEC_SB_UPPER(chanspec)) {
			chan_ptr->control += CH_10MHZ_APART;
			chan_ptr->ext20 -= CH_10MHZ_APART;
		}
	}
}

#ifdef ACS_DEBUG
static void
acs_dump_chan_bss(acs_chan_bssinfo_t* bssinfo, int ncis)
{
	int c;
	acs_chan_bssinfo_t *cur;

	printf("channel    aAPs bAPs gAPs lSBs uSBs nEXs\n");

	for (c = 0; c < ncis; c ++) {
		cur = &bssinfo[c];
		printf("%2d\t%5d%5d%5d%5d%5d%5d\n", cur->channel, cur->aAPs,
			cur->bAPs, cur->gAPs, cur->lSBs, cur->uSBs, cur->nEXs);
	}
}
#endif /* ACS_DEBUG */

void
acs_expire_scan_entry(acs_chaninfo_t *c_info, time_t limit)
{
	time_t now;
	acs_bss_info_entry_t *curptr, *previous = NULL, *past;
	acs_bss_info_entry_t **rootp = &c_info->acs_bss_info_q;

	curptr = *rootp;
	now = uptime();

	while (curptr) {
		time_t diff = now - curptr->timestamp;
		if (diff > limit) {
			ACSD_FCS("Scan expire: diff %dsec chanspec 0x%x, SSID %s\n",
				(int)diff, curptr->binfo_local.chanspec, curptr->binfo_local.SSID);
			if (previous == NULL)
				*rootp = curptr->next;
			else
				previous->next = curptr->next;

			past = curptr;
			curptr = curptr->next;
			ACS_FREE(past);
			continue;
		}
		previous = curptr;
		curptr = curptr->next;
	}
}

void
acs_cleanup_scan_entry(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *headptr = c_info->acs_bss_info_q;
	acs_bss_info_entry_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		ACS_FREE(curptr);
	}
	c_info->acs_bss_info_q = NULL;
}

static void
display_scan_entry_local(acs_bss_info_sm_t * bsm)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_format_ssid(ssidbuf, bsm->SSID, bsm->SSID_len);

	printf("SSID: \"%s\"\n", ssidbuf);
	printf("BSSID: %s\t", wl_ether_etoa(&bsm->BSSID));
	printf("chanspec: 0x%x\n", bsm->chanspec);
	printf("RSSI: %d dBm\t", (int16)bsm->RSSI);
	printf("Type: %s", ((bsm->type == ACS_BSS_TYPE_11A) ? "802.11A" :
		((bsm->type == ACS_BSS_TYPE_11G) ? "802.11G" : "802.11B")));
	printf("\n");
}

void
acs_dump_scan_entry(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *curptr = c_info->acs_bss_info_q;

	while (curptr) {
		display_scan_entry_local(&curptr->binfo_local);
		printf("timestamp: %u\n", (uint32)curptr->timestamp);
		curptr = curptr->next;
	}
}

static int
acs_insert_scan_entry(acs_chaninfo_t *c_info, acs_bss_info_entry_t * new)
{
	acs_bss_info_entry_t *curptr, *previous = NULL;
	acs_bss_info_entry_t **rootp = &c_info->acs_bss_info_q;

	curptr = *rootp;
	previous = curptr;

	while (curptr &&
	   memcmp(&curptr->binfo_local.BSSID, &new->binfo_local.BSSID, sizeof(struct ether_addr))) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;
	if (previous == NULL)
		*rootp = new;
	else {
		if (curptr == NULL)
			previous->next = new;
		else /* find an existing entry */ {
			curptr->timestamp = new->timestamp;
			memcpy(&new->binfo_local, &curptr->binfo_local, sizeof(acs_bss_info_sm_t));
			ACS_FREE(new);
		}
	}
	return 0;
}

static int
acs_update_scanresult_queue(acs_chaninfo_t *c_info)
{
	wl_scan_results_t* s_result = c_info->scan_results;
	wl_bss_info_t *bi = s_result->bss_info;
	int b, len = 0;
	acs_bss_info_entry_t * new_entry = NULL;
	acs_channel_t chan;
	chanspec_t cur_chspec;

	for (b = 0; b < s_result->count; b ++, bi = (wl_bss_info_t*)((int8*)bi + len)) {

		len = dtoh32(bi->length);
		new_entry = (acs_bss_info_entry_t*)acsd_malloc(sizeof(acs_bss_info_entry_t));

		new_entry->binfo_local.chanspec = cur_chspec = dtoh16(bi->chanspec);
		new_entry->binfo_local.RSSI = dtoh16(bi->RSSI);
		new_entry->binfo_local.SSID_len = bi->SSID_len;
		memcpy(new_entry->binfo_local.SSID, bi->SSID, bi->SSID_len);
		memcpy(&new_entry->binfo_local.BSSID, &bi->BSSID, sizeof(struct ether_addr));
		new_entry->timestamp = uptime();
		acs_parse_chanspec(cur_chspec, &chan);
		ACSD_FCS("Scan: chanspec 0x%x, control %x SSID %s\n", cur_chspec,
			chan.control, new_entry->binfo_local.SSID);
		/* BSS type in 2.4G band */
		if (chan.control <= ACS_CS_MAX_2G_CHAN) {
			if (acs_bss_is_11b(bi))
				new_entry->binfo_local.type = ACS_BSS_TYPE_11B;
			else
				new_entry->binfo_local.type = ACS_BSS_TYPE_11G;
		}
		else
			new_entry->binfo_local.type = ACS_BSS_TYPE_11A;
		acs_insert_scan_entry(c_info, new_entry);
	}
	return 0;
}

static int
acs_update_chan_bssinfo(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *biq;
	scan_chspec_elemt_t* chspec_list;
	char * new_buf = NULL;
	acs_channel_t chan;
	acs_channel_t *chan_p = &chan;
	chanspec_t cur_chspec;
	int count = 0, buf_size, c;
	acs_chan_bssinfo_t *bss_info;

	count = c_info->scan_chspec_list.count;
	chspec_list = c_info->scan_chspec_list.chspec_list;

	if (count == 0)
		return 0;

	buf_size = sizeof(acs_chan_bssinfo_t) * count;
	new_buf = acsd_malloc(buf_size);

	bss_info = (acs_chan_bssinfo_t *) new_buf;

	for (c = 0; c < count; c ++) {
		bzero(&bss_info[c], sizeof(acs_chan_bssinfo_t));

		biq = c_info->acs_bss_info_q;
		/* set channel range centered by the scan channel */
		bss_info[c].channel = CHSPEC_CHANNEL(chspec_list[c].chspec);
		ACSD_DEBUG("count: %d, channel: %d\n", c, bss_info[c].channel);

		while (biq) {
			assert(biq);
			cur_chspec = biq->binfo_local.chanspec;
			acs_parse_chanspec(cur_chspec, chan_p);

			/* skip bss not on the scan channel or adjacent channels */
			if ((chan_p->control != bss_info[c].channel) &&
				(chan_p->ext20 != bss_info[c].channel) &&
				(chan_p->ext40[0] != bss_info[c].channel) &&
				(chan_p->ext40[1] != bss_info[c].channel)) {
				goto next_entry;
			}

			/* count vht80MHz ctl sidebands */
			if (CHSPEC_IS80(cur_chspec) && chan_p->control == bss_info[c].channel) {
				if ((cur_chspec & WL_CHANSPEC_CTL_SB_MASK) ==
					WL_CHANSPEC_CTL_SB_LL) {
					if (bss_info[c].llSBs < MAXNBVAL(sizeof(bss_info[c].llSBs)))
						bss_info[c].llSBs ++;
				}
				else if ((cur_chspec & WL_CHANSPEC_CTL_SB_MASK) ==
					WL_CHANSPEC_CTL_SB_LU) {
					if (bss_info[c].luSBs < MAXNBVAL(sizeof(bss_info[c].luSBs)))
						bss_info[c].luSBs ++;
				}
				else if ((cur_chspec & WL_CHANSPEC_CTL_SB_MASK) ==
					WL_CHANSPEC_CTL_SB_UL) {
					if (bss_info[c].ulSBs < MAXNBVAL(sizeof(bss_info[c].ulSBs)))
						bss_info[c].ulSBs ++;
				}
				else if ((cur_chspec & WL_CHANSPEC_CTL_SB_MASK) ==
					WL_CHANSPEC_CTL_SB_UU) {
					if (bss_info[c].uuSBs < MAXNBVAL(sizeof(bss_info[c].uuSBs)))
						bss_info[c].uuSBs ++;
				}
				goto next_entry;
			}

			/* count VHT80 20 extensions */
			if (CHSPEC_IS80(cur_chspec) && chan_p->ext20 == bss_info[c].channel) {
				if (bss_info[c].wEX20s < MAXNBVAL(sizeof(bss_info[c].wEX20s)))
					bss_info[c].wEX20s ++;
				goto next_entry;
			}

			/* count VHT80 40 extensions */
			if (CHSPEC_IS80(cur_chspec) && (chan_p->ext40[0] == bss_info[c].channel ||
				chan_p->ext40[1] == bss_info[c].channel)) {
				if (bss_info[c].wEX40s < MAXNBVAL(sizeof(bss_info[c].wEX40s)))
					bss_info[c].wEX40s ++;
				goto next_entry;
			}

			/* count 11n ctl sidebands */
			if (CHSPEC_IS40(cur_chspec) && chan_p->control == bss_info[c].channel) {
				if (CHSPEC_SB_LOWER(cur_chspec)) {
					if (bss_info[c].lSBs < MAXNBVAL(sizeof(bss_info[c].lSBs)))
						bss_info[c].lSBs ++;
				}
				else if (CHSPEC_SB_UPPER(cur_chspec)) {
					if (bss_info[c].uSBs < MAXNBVAL(sizeof(bss_info[c].uSBs)))
						bss_info[c].uSBs ++;
				}
				goto next_entry;
			}
			/* count 11n extensions */
			if (CHSPEC_IS40(cur_chspec) && chan_p->ext20 == bss_info[c].channel) {
				if (bss_info[c].nEXs < MAXNBVAL(sizeof(bss_info[c].nEXs)))
					bss_info[c].nEXs ++;
				goto next_entry;
			}

			/* count BSSs in 2.4G band */
			if (bss_info[c].channel <= ACS_CS_MAX_2G_CHAN) {
				if (biq->binfo_local.type == ACS_BSS_TYPE_11B) {
					if (bss_info[c].bAPs < MAXNBVAL(sizeof(bss_info[c].bAPs)))
						bss_info[c].bAPs ++;
				}
				else {
					if (bss_info[c].gAPs < MAXNBVAL(sizeof(bss_info[c].gAPs)))
						bss_info[c].gAPs ++;
				}
			}
			/* count BSSs in 5G band */
			else {
				if (bss_info[c].aAPs < MAXNBVAL(sizeof(bss_info[c].aAPs)))
					bss_info[c].aAPs ++;
			}
next_entry:
			biq = biq->next;
		}
		ACSD_DEBUG(" channel %u: %u aAPs %u bAPs %u gAPs %u lSBs %u uSBs %u nEXs\n",
			bss_info[c].channel, bss_info[c].aAPs, bss_info[c].bAPs,
			bss_info[c].gAPs, bss_info[c].lSBs, bss_info[c].uSBs, bss_info[c].nEXs);
	}

	ACS_FREE(c_info->ch_bssinfo);
	c_info->ch_bssinfo = (acs_chan_bssinfo_t *) new_buf;

#ifdef ACS_DEBUG
	acs_dump_chan_bss(c_info->ch_bssinfo, c_info->scan_chspec_list.count);
	acs_dump_scan_entry(c_info);
#endif /* ACS_DEBUG */

	return 0;
}

/* radio setting information needed from the driver */
static int
acs_get_rs_info(acs_chaninfo_t * c_info, char* prefix)
{
	int ret = 0;
	char tmp[100];
	int band, pref_chspec, coex;
	acs_rsi_t *rsi = &c_info->rs_info;
	char *str;
	char data_buf[100];
	struct {
		uint32 band;
		uint32 bw_cap;
	} param = { 0, 0 };

	/*
	 * Check if the user set the "chanspec" nvram. If not, check if
	 * the "channel" nvram is set for backward compatibility.
	 */
	if ((str = nvram_get(strcat_r(prefix, "chanspec", tmp))) == NULL) {
		str = nvram_get(strcat_r(prefix, "channel", tmp));
	}

	if (str && strcmp(str, "0")) {
		ret = wl_iovar_getint(c_info->name, "chanspec", &pref_chspec);
		ACS_ERR(ret, "failed to get chanspec");

		ret = wl_iovar_setint(c_info->name, "pref_chanspec", pref_chspec);
		ACS_ERR(ret, "failed to set perf_chanspec");

		rsi->pref_chspec = dtoh32(pref_chspec);
	}
	else {
		ret = wl_iovar_setint(c_info->name, "pref_chanspec", 0);
		ACS_ERR(ret, "failed to set perf_chanspec");
	}

	ret = wl_iovar_getint(c_info->name, "obss_coex", &coex);
	ACS_ERR(ret, "failed to get obss_coex");

	rsi->coex_enb = dtoh32(coex);
	ACSD_INFO("coex_enb: %d\n",  rsi->coex_enb);

	ret = wl_ioctl(c_info->name, WLC_GET_BAND, &band, sizeof(band));
	ACS_ERR(ret, "failed to get band info");

	rsi->band_type = dtoh32(band);
	ACSD_INFO("band_type: %d\n",  rsi->band_type);

	param.band = band;

	ret = wl_iovar_getbuf(c_info->name, "bw_cap", &param, sizeof(param),
		data_buf, sizeof(data_buf));
	ACS_ERR(ret, "failed to get bw_cap");

	rsi->bw_cap = *((uint32 *)data_buf);
	ACSD_INFO("bw_cap: %d\n",  rsi->bw_cap);

	return ret;
}

int
acs_request_data(acs_chaninfo_t *c_info)
{
	int ret = 0;

	char *dump_buf = acsd_malloc(ACS_SRSLT_BUF_LEN);

	ret = acs_get_scan(c_info->name, dump_buf, ACS_SRSLT_BUF_LEN);

	ACS_FREE(c_info->scan_results);
	c_info->scan_results = (wl_scan_results_t *)dump_buf;

	acs_update_scanresult_queue(c_info);
	acs_update_chan_bssinfo(c_info);

	acsd_chanim_query(c_info, WL_CHANIM_COUNT_ALL, 0);

	return ret;
}

/*
 * acs_pick_chanspec_default() - default policy function to pick a chanspec to switch to.
 *
 * c_info:	pointer to the acs_chaninfo_t for this interface.
 * bw:		bandwidth to chose from
 *
 * Returned value:
 *	The returned value is the most preferred valid chanspec from the candidate array.
 *
 * This function picks the most preferred chanspec according to the default policy. At the time of
 * this writing, this is a selection based on the CH_SCORE_BSS score.
 */
static chanspec_t
acs_pick_chanspec_default(acs_chaninfo_t* c_info, int bw)
{
	return acs_pick_chanspec_common(c_info, bw, CH_SCORE_BSS);
}

static chanspec_t
acs_pick_chanspec(acs_chaninfo_t* c_info, int bw)
{
	chanspec_t chspec = 0;
	int i, index = -1;
	int score_type = CH_SCORE_TOTAL;
	ch_candidate_t *candi = c_info->candidate[bw];

	qsort(candi, c_info->c_count[bw], sizeof(ch_candidate_t), cmpchspec);

	/* pick the chanspec with the highest total score */
	for (i = 0; i < c_info->c_count[bw]; i++) {
		if (!candi[i].valid)
			continue;

		if (index < 0) { /* No previous candi, avoid comparing against random memory */
			index = i; /* Select first valid candidate as a starting point */
			ACSD_INFO("[%d] Default: %s channel #%d (0x%x) with score %d\n",
				i, (candi[i].is_dfs) ? "DFS" : "non-DFS",
				CHSPEC_CHANNEL(candi[i].chspec), candi[i].chspec,
				candi[i].chscore[score_type].score);
			continue;
		}

		ACSD_INFO("[%d] Checking %s channel #%d (0x%x) with score %d\n",
			i, (candi[i].is_dfs) ? "DFS" : "non-DFS",
			CHSPEC_CHANNEL(candi[i].chspec), candi[i].chspec,
			candi[i].chscore[score_type].score);

		uint ctrl_ch = wf_chspec_ctlchan(candi[i].chspec);
		if (CHSPEC_IS2G(candi[i].chspec) && (ctrl_ch == 6))
		{
			candi[i].chscore[score_type].score += 20;
#if 0
			ACSD_PRINT("score++ for chanspec 0x%x (%s)\n",
				candi[i].chspec, wf_chspec_ntoa(candi[i].chspec, chanspecbuf));
#endif
		}

		if (candi[i].chscore[score_type].score >
		    candi[index].chscore[score_type].score) {
			ACSD_INFO("-- selected higher (=better) score channel.\n");
			index = i;
		}
	}

	if (index < 0) {
		ACSD_ERROR("No valid chanspec found\n");
	} else {
		chspec = candi[index].chspec;
		ACSD_INFO("The highest score: %d, chspec: 0x%x\n",
			candi[index].chscore[score_type].score,
			chspec);
	}
	return chspec;
}

void
acs_default_policy(acs_policy_t *a_pol, acs_policy_index index)
{
	if (index >= ACS_POLICY_MAX) {
		ACSD_ERROR("Invalid acs policy index %d, reverting to default (%d).\n",
			index, ACS_POLICY_DEFAULT);
		index = ACS_POLICY_DEFAULT;
	}

	memcpy(a_pol, &predefined_policy[index], sizeof(acs_policy_t));

	if (index == ACS_POLICY_DEFAULT)
		a_pol->chan_selector = acs_pick_chanspec_default;
	else if (index == ACS_POLICY_FCS)
		a_pol->chan_selector = acs_pick_chanspec_fcs_policy;
	else
		a_pol->chan_selector = acs_pick_chanspec;
}

#ifdef DEBUG
static void
acs_dump_policy(acs_policy_t *a_pol)
{
	printf("ACS Policy:\n");
	printf("Bg Noise threshold: %d\n", a_pol->bgnoise_thres);
	printf("Interference threshold: %d\n", a_pol->intf_threshold);
	printf("Channel Scoring Weights: \n");
	printf("\t BSS: %d\n", a_pol->acs_weight[CH_SCORE_BSS]);
	printf("\t BUSY: %d\n", a_pol->acs_weight[CH_SCORE_BUSY]);
	printf("\t INTF: %d\n", a_pol->acs_weight[CH_SCORE_INTF]);
	printf("\t INTFADJ: %d\n", a_pol->acs_weight[CH_SCORE_INTFADJ]);
	printf("\t FCS: %d\n", a_pol->acs_weight[CH_SCORE_FCS]);
	printf("\t TXPWR: %d\n", a_pol->acs_weight[CH_SCORE_TXPWR]);
	printf("\t BGNOISE: %d\n", a_pol->acs_weight[CH_SCORE_BGNOISE]);
	printf("\t CNS: %d\n", a_pol->acs_weight[CH_SCORE_CNS]);

}
#endif /* DEBUG */

static void
acs_retrieve_config(acs_chaninfo_t *c_info, char * prefix)
{
	/* retrieve policy related configuration from nvram */
	char conf_word[128], conf_var[16], tmp[100];
	char *next;
	int i = 0, val;
	acs_policy_index index;
	acs_policy_t *a_pol = &c_info->acs_policy;
	uint32 flags;

	/* the current layout of config */
	ACSD_INFO("retrieve config from nvram ...\n");

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_scan_entry_expire", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_scan_entry_expire set. Retrieve default.\n");
		c_info->acs_scan_entry_expire = ACS_CI_SCAN_EXPIRE;
	}
	else {
		char *endptr = NULL;
		c_info->acs_scan_entry_expire = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_scan_entry_expire: 0x%x\n", c_info->acs_scan_entry_expire);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_fcs_mode", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_fcs_mode is set. Retrieve default.\n");
		c_info->acs_fcs_mode = ACS_FCS_MODE_DEFAULT;
	}
	else {
		char *endptr = NULL;
		c_info->acs_fcs_mode = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_fcs_mode: 0x%x\n", c_info->acs_fcs_mode);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_flags", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs flag set. Retrieve default.\n");
		flags = ACS_DFLT_FLAGS;
	}
	else {
		char *endptr = NULL;
		flags = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs flags: 0x%x\n", flags);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_pol", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs policy set. Retrieve default.\n");

		acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_pol_idx", tmp));

		if (!strcmp(conf_word, "")) {
			if (ACS_FCS_MODE(c_info))
				index = ACS_POLICY_FCS;
			else
				index = ACS_POLICY_DEFAULT;
		} else {
			index = atoi(conf_word);
		}
		acs_default_policy(a_pol, index);

	} else {

		index = ACS_POLICY_USER;
		memset(a_pol, 0, sizeof(*a_pol));	/* Initialise policy values to all zeroes */
		foreach(conf_var, conf_word, next) {
			val = atoi(conf_var);
			ACSD_DEBUG("i: %d conf_var: %s val: %d\n", i, conf_var, val);

			if (i == 0)
				a_pol->bgnoise_thres = val;
			else if (i == 1)
				a_pol->intf_threshold = val;
			else {
				if ((i - 2) >= CH_SCORE_MAX) {
					ACSD_ERROR("Ignoring excess values in %sacs_pol=\"%s\"\n",
						prefix, conf_word);
					break; /* Prevent overwriting innocent memory */
				}
				a_pol->acs_weight[i - 2] = val;
				ACSD_DEBUG("weight No. %d, value: %d\n", i-2, val);
			}
			i++;
		}
		a_pol->chan_selector = acs_pick_chanspec;
	}

	acs_fcs_retrieve_config(c_info, prefix);

	c_info->flags = flags;
	c_info->policy_index = index;
#ifdef DEBUG
	acs_dump_policy(a_pol);
#endif
}

/* set intfer trigger params */
int acs_intfer_config(acs_chaninfo_t *c_info)
{
	wl_intfer_params_t *params = &(c_info->acs_fcs.intfparams);
	int err;

	err = wl_iovar_set(c_info->name, "intfer_params", (void *)params,
		sizeof(wl_intfer_params_t));
	if (err < 0) {
		ACSD_ERROR("intfer_params error! ret code: %d\n", err);
	}
	return err;
}

static int
acs_start(char *name, acs_chaninfo_t ** c_info_ptr)
{
	int unit, length;
	char prefix[PREFIX_LEN], tmp[100];
	int index, ret = 0;
	acs_chaninfo_t *c_info;
	acs_rsi_t* rsi;

	ACSD_INFO("acs_start for interface %s\n", name);

	length = strlen(name);
	if (length >= sizeof(c_info->name)) {
		ret = ACSD_FAIL;
		ACS_ERR(ret, "Interface Name Length Exceeded\n");
	}

	index = acs_idx_from_map(name);

	if (index < 0) {
		ret = ACSD_FAIL;
		ACS_ERR(ret, "Mapped entry not present for interface");
	}

	/* allocate core data structure for this if */
	c_info = acs_info->chan_info[index] =
		(acs_chaninfo_t*)acsd_malloc(sizeof(acs_chaninfo_t));

	memcpy(c_info->name, name, length + 1);

	ret = wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	/* check radio */
	if (nvram_match(strcat_r(prefix, "radio", tmp), "0")) {
		ACSD_INFO("ifname %s: radio is off\n", name);
		c_info->mode = ACS_MODE_DISABLE;
		goto acs_start_done;
	}

	acs_retrieve_config(c_info, prefix);

	rsi = &c_info->rs_info;
	acs_get_rs_info(c_info, prefix);

	if (rsi->pref_chspec == 0) {
		c_info->mode = ACS_MODE_SELECT;
	}
	else if (rsi->coex_enb &&
		nvram_match(strcat_r(prefix, "nmode", tmp), "-1")) {
		c_info->mode = ACS_MODE_COEXCHECK;
	}
	else
		c_info->mode = ACS_MODE_MONITOR; /* default mode */

	if ((c_info->mode == ACS_MODE_SELECT) && BAND_5G(rsi->band_type) &&
		(nvram_match(strcat_r(prefix, "reg_mode", tmp), "h") ||
		nvram_match(strcat_r(prefix, "reg_mode", tmp), "strict_h"))) {
		rsi->reg_11h = TRUE;
	}

	ret = acsd_chanim_init(c_info);
	if (ret < 0)
		ACS_FREE(c_info);
	ACS_ERR(ret, "chanim init failed\n");


	if (AUTOCHANNEL(c_info) && ACS_FCS_MODE(c_info))
		acs_intfer_config(c_info);

	/* Do not even allocate a DFS Reentry context on 2.4GHz which does not have DFS channels */
	/* or if 802.11h spectrum management is not enabled. */
	if (BAND_2G(c_info->rs_info.band_type) || (rsi->reg_11h == FALSE)) {
		ACSD_DFSR("DFS Reentry disabled %s\n", (BAND_2G(c_info->rs_info.band_type)) ?
			"on 2.4GHz band" : "as 802.11h is not enabled");
	} else {
		ACS_DFSR_CTX(c_info) = acs_dfsr_init(prefix,
			(ACS_FCS_MODE(c_info) && (c_info->acs_fcs.acs_dfs == ACS_DFS_REENTRY)));
		ret = (ACS_DFSR_CTX(c_info) == NULL) ? -1 : 0;
		ACS_ERR(ret, "Failed to allocate DFS Reentry context\n");
	}

	if (!AUTOCHANNEL(c_info) && !COEXCHECK(c_info))
		goto acs_start_done;

	ret = acs_run_cs_scan(c_info);
	ACS_ERR(ret, "cs scan failed\n");

	ACS_FREE(c_info->acs_bss_info_q);

	ret = acs_request_data(c_info);
	ACS_ERR(ret, "request data failed\n");

acs_start_done:
	*c_info_ptr = c_info;
	return ret;
}

static int
acs_build_candidates(acs_chaninfo_t *c_info, int bw)
{
	wl_uint32_list_t *list;
	chanspec_t input = 0, c = 0;
	int ret = 0, i, j, k;
	int count = 0;
	ch_candidate_t *candi;
	acs_rsi_t *rsi = &c_info->rs_info;
	acs_scan_chspec_t* scan_chspec_p = &c_info->scan_chspec_list;
	acs_channel_t chan, candi_chan;

	char *data_buf;
	data_buf = acsd_malloc(ACS_SM_BUF_LEN);

	if (bw == ACS_BW_80)
		input |= WL_CHANSPEC_BW_80;
	else if (bw == ACS_BW_40)
		input |= WL_CHANSPEC_BW_40;
	else
		input |= WL_CHANSPEC_BW_20;

	if (BAND_5G(rsi->band_type))
		input |= WL_CHANSPEC_BAND_5G;
	else
		input |= WL_CHANSPEC_BAND_2G;

	ret = wl_iovar_getbuf(c_info->name, "chanspecs", &input, sizeof(chanspec_t),
		data_buf, ACS_SM_BUF_LEN);
	if (ret < 0)
		ACS_FREE(data_buf);
	ACS_ERR(ret, "failed to get valid chanspec lists");

	list = (wl_uint32_list_t *)data_buf;
	count = dtoh32(list->count);

	if (!count) {
		ACSD_ERROR("number of valid chanspec is 0\n");
		ret = -1;
		goto cleanup;
	}

	ACS_FREE(c_info->candidate[bw]);
	c_info->candidate[bw] = (ch_candidate_t*)acsd_malloc(count * sizeof(ch_candidate_t));
	candi = c_info->candidate[bw];

	ACSD_DEBUG("address of candi: 0x%x\n", (uint32)candi);
	for (i = 0; i < count; i++) {
		c = (chanspec_t)dtoh32(list->element[i]);
		candi[i].chspec = c;
		candi[i].valid = TRUE;

		acs_parse_chanspec(candi[i].chspec, &candi_chan);
		for (k = 0; k < scan_chspec_p->count; k++) {
			acs_parse_chanspec(scan_chspec_p->chspec_list[k].chspec, &chan);
			if ((scan_chspec_p->chspec_list[k].chspec_info & WL_CHAN_RADAR) &&
				(chan.control == candi_chan.control)) {
				candi[i].is_dfs = TRUE;
				if (!rsi->reg_11h) {
					/* DFS Channels can only be used if 802.11h is enabled. */
					candi[i].valid = FALSE;
					candi[i].reason |= ACS_INVALID_DFS_NO_11H;
				}
			}
		}

		/* assign weight based on config */
		for (j = 0; j < CH_SCORE_MAX; j++) {
			candi[i].chscore[j].weight = c_info->acs_policy.acs_weight[j];
			ACSD_DEBUG("chanspec: (0x%04x) score: %d, weight: %d\n",
				c, candi[i].chscore[j].score, candi[i].chscore[j].weight);
		}
	}
	c_info->c_count[bw] = count;

cleanup:
	free(data_buf);
	return ret;
}

/*
 * 20/40 Coex compliance check:
 * Return a 20/40 Coex compatible chanspec based on the scan data.
 * Verify that the 40MHz input_chspec passes 20/40 Coex rules.
 * If so, return the same chanspec.
 * Otherwise return a 20MHz chanspec which is centered on the
 * input_chspec's control channel.
 */
static chanspec_t
acs_coex_check(acs_chaninfo_t* c_info, chanspec_t input_chspec)
{
	int forty_center;
	uint ctrl_ch, ext_ch;
	acs_channel_t chan;
	bool upperSB;
	chanspec_t chspec_out;
	int ci_index, ninfo = c_info->scan_chspec_list.count;
	bool conflict = FALSE;
	acs_chan_bssinfo_t *ci = c_info->ch_bssinfo;
	char err_msg[128];

	if (!CHSPEC_IS40(input_chspec))
		ACSD_ERROR("input channel spec is not 40MHz!");

	/* this will get us the center of the input 40MHz channel */
	forty_center = CHSPEC_CHANNEL(input_chspec);
	upperSB = CHSPEC_SB_UPPER(input_chspec);

	acs_parse_chanspec(input_chspec, &chan);

	ctrl_ch = chan.control;
	ext_ch = chan.ext20;

	ACSD_DEBUG("InputChanspec:  40Center %d, CtrlCenter %d, ExtCenter %d\n",
	          forty_center, ctrl_ch, ext_ch);

	/* Loop over scan data looking for interferance based on 20/40 Coex Rules. */
	for (ci_index = 0; ci_index < ninfo; ci_index++) {
		ACSD_DEBUG("Examining ci[%d].channel = %d, forty_center-5 = %d, "
		          "forty_center+5 = %d\n",
		          ci_index, ci[ci_index].channel, forty_center - WLC_2G_25MHZ_OFFSET,
		          forty_center+WLC_2G_25MHZ_OFFSET);

		/* Ignore any channels not within the range we care about.
		 * 20/40 Coex rules for 2.4GHz:
		 * Must look at all channels where a 20MHz BSS would overlap with our
		 * 40MHz BW + 5MHz on each side.  This means that we must inspect any channel
		 * within 5 5MHz channels of the center of our 40MHz chanspec.
		 *
		 * Example:
		 * 40MHz Chanspec centered on Ch.8
		 *              +5 ----------40MHz-------------  +5
		 *              |  |           |              |   |
		 * -1  0  1  2  3  4  5  6  7  8  9  10  11  12  13  14
		 *
		 * Existing 20MHz BSS on Ch. 1 (Doesn't interfere with our 40MHz AP)
		 *  -----20MHz---
		 *  |     |     |
		 * -1  0  1  2  3  4  5  6  7  8  9  10  11  12  13  14
		 *
		 * Existing 20MHz BSS on Ch. 3 (Does interfere our 40MHz AP)
		 *        -----20MHz---
		 *        |     |     |
		 * -1  0  1  2  3  4  5  6  7  8  9  10  11  12  13  14
		 *
		 *  In this example, we only pay attention to channels in the range of 3 thru 13.
		 */

		if (ci[ci_index].channel < forty_center - WLC_2G_25MHZ_OFFSET ||
		    ci[ci_index].channel > forty_center + WLC_2G_25MHZ_OFFSET) {
			ACSD_DEBUG("Not in range, continue.\n");
			continue;
		}

		ACSD_DEBUG("In range.\n");

		/* Is there an existing BSS? */
		if (ci[ci_index].bAPs || ci[ci_index].gAPs || ci[ci_index].lSBs ||
		    ci[ci_index].uSBs || ci[ci_index].nEXs) {
			ACSD_DEBUG("Existing BSSs on channel %d\n", ci[ci_index].channel);

			/* Existing BSS is ONLY okay if:
			 * Our control channel is aligned with existing 20 or Control Channel
			 * Our extension channel is aligned with an existing extension channel
			 */
			if (ci[ci_index].channel == ctrl_ch) {
				ACSD_DEBUG("Examining ctrl_ch\n");

				/* Two problems that we need to detect here:
				 *
				 * 1:  If this channel is being used as a 40MHz extension.
				 * 2:  If this channel is being used as a control channel for an
				 *     existing 40MHz, we must both use the same CTRL sideband
				 */
				if (ci[ci_index].nEXs) {
					snprintf(err_msg, sizeof(err_msg), "ctrl channel: %d"
						" existing ext. channel", ctrl_ch);
					conflict = TRUE;
					break;
				} else if ((upperSB && ci[ci_index].lSBs) ||
				           (!upperSB && ci[ci_index].uSBs)) {
					snprintf(err_msg, sizeof(err_msg), "ctrl channel %d"
						" SB not aligned with existing 40BSS", ctrl_ch);
					conflict = TRUE;
					break;
				}
			} else if (ci[ci_index].channel == ext_ch) {
				ACSD_DEBUG("Examining ext_ch\n");

				/* Any BSS using this as it's center is an interferance */
				if (ci[ci_index].bAPs || ci[ci_index].gAPs || ci[ci_index].lSBs ||
				    ci[ci_index].uSBs) {
					snprintf(err_msg, sizeof(err_msg), "ext channel %d"
						" used as ctrl channel by existing BSSs", ext_ch);
					conflict = TRUE;
					break;
				}
			} else {
				/* If anyone is using this channel, it's an conflict */
				conflict = TRUE;
				snprintf(err_msg, sizeof(err_msg),
					"channel %d used by exiting BSSs ", ci[ci_index].channel);
				break;
			}
		}
	}

	if (conflict) {
		chspec_out = CH20MHZ_CHSPEC(ctrl_ch);
		if (c_info->rs_info.pref_chspec)
			ACSD_PRINT("COEX: downgraded chanspec 0x%x to 0x%x: %s\n",
				input_chspec, chspec_out, err_msg);
	} else {
		chspec_out = input_chspec;
		ACSD_DEBUG("No conflict found, returning 40MHz chanspec 0x%x\n",
		          chspec_out);
	}
	return chspec_out;
}

static bool
acs_has_valid_candidate(acs_chaninfo_t* c_info, int bw)
{
	int i;
	bool result = FALSE;
	ch_candidate_t* candi = c_info->candidate[bw];

	for (i = 0; i < c_info->c_count[bw]; i++) {
		if (candi[i].valid) {
			result = TRUE;
			break;
		}
	}
	ACSD_DEBUG("result: %d\n", result);
	return result;
}

/*
 * Individual scoring algorithm. It is subject to tuning or customization based on
 * testing results or customer requirement.
 *
 */
static int
acs_chan_score_bss(ch_candidate_t* candi, acs_chan_bssinfo_t* bss_info, int ncis)
{
	acs_channel_t chan;
	int score = 0, tmp_score = 0;
	int i, min, max;
	int ch;
	bool ovlp = FALSE;

	if (CHSPEC_IS2G(candi->chspec) && (!nvram_match("acs_2g_ch_no_ovlp", "1"))) {
		ovlp = TRUE;
	}

	acs_parse_chanspec(candi->chspec, &chan);

	for (i = 0; i < ncis; i++) {
		ch = (int)bss_info[i].channel;

		/* control channel */
		min = max = (int)chan.control;
		if (ovlp) {
			min -= CH_10MHZ_APART;
			max += CH_10MHZ_APART;
		}

		ACSD_DEBUG("ch: %d, min: %d, max: %d\n", ch, min, max);

		/* ban channel 144 as primary or secondary 20 */
		if (chan.control == 144 || chan.ext20 == 144) {
			candi->valid = FALSE;
			candi->reason = ACS_INVALID_144;
			break;
		}

		if (ch < min || ch > max)
			goto ext20;

		tmp_score = bss_info[i].nEXs + bss_info[i].wEX20s;
		if (tmp_score > 0) {
			if ((candi->in_use == 0 || CHSPEC_IS80(candi->chspec)) &&
				(!nvram_match("acs_no_restrict_align", "1"))) {
				/* mark this candidate invalid if it is not used or is 80MHz */
				candi->valid = FALSE;
				candi->reason = ACS_INVALID_ALIGN;
				break;
			}
			else
				/* in_use and not 80MHz, allowed */
				score += ACS_NOT_ALIGN_WT * tmp_score;
		}

		score += bss_info[i].aAPs + bss_info[i].bAPs + bss_info[i].gAPs;
		score += bss_info[i].lSBs + bss_info[i].uSBs;
		score += bss_info[i].llSBs + bss_info[i].luSBs + bss_info[i].ulSBs +
			bss_info[i].uuSBs;
		score += bss_info[i].wEX40s;

ext20:
		/* this is to calculate bss score for ext20 channel */
		if (chan.ext20 == 0)
			continue;

		min = max = (int)chan.ext20;
		if (ovlp) {
			min -= CH_10MHZ_APART;
			max += CH_10MHZ_APART;
		}

		if (ch < min || ch > max)
			goto ext40;

		tmp_score = bss_info[i].aAPs + bss_info[i].bAPs + bss_info[i].gAPs;
		tmp_score += bss_info[i].lSBs + bss_info[i].uSBs;
		tmp_score += bss_info[i].llSBs + bss_info[i].luSBs + bss_info[i].ulSBs +
			bss_info[i].uuSBs;

		if (tmp_score > 0) {
			if ((candi->in_use == 0 || CHSPEC_IS80(candi->chspec)) &&
				(!nvram_match("acs_no_restrict_align", "1"))) {
				/* mark this candidate invalid if it is not used or is 80MHz */
				candi->valid = FALSE;
				candi->reason = ACS_INVALID_ALIGN;
				break;
			}
			else
				/* in_use and not 80MHz, allowed */
				score += ACS_NOT_ALIGN_WT * tmp_score;
		}

		score += bss_info[i].nEXs + bss_info[i].wEX20s;
		score += bss_info[i].wEX40s;

ext40:
		/* to calculate bss scroe for ext40 channel */
		if (chan.ext40[0] == 0)
			continue;

		/* no ovlp in 5G */
		if (ch != chan.ext40[0] && ch != chan.ext40[1])
			continue;

		score += bss_info[i].aAPs + bss_info[i].bAPs + bss_info[i].gAPs;
		score += bss_info[i].lSBs + bss_info[i].uSBs;
		score += bss_info[i].llSBs + bss_info[i].luSBs + bss_info[i].ulSBs +
			bss_info[i].uuSBs;
		score += bss_info[i].wEX40s;
		score += bss_info[i].nEXs + bss_info[i].wEX20s;

		ACSD_DEBUG("i: %d, score: %d\n", i, score);
	}

	ACSD_INFO("candidate: %x, score_bss: %d\n", candi->chspec, score);
	return score;
}

static void
acs_candidate_score_bss(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	acs_chan_bssinfo_t* bss_info = c_info->ch_bssinfo;
	int score = 0;
	int ncis = c_info->scan_chspec_list.count;
	acs_bss_info_entry_t *biq;
	uint8 count = 0;
	chanspec_t cur_chspec;

	biq = c_info->acs_bss_info_q;

	while (biq) {
		assert(biq);
		cur_chspec = biq->binfo_local.chanspec;

		if (candi->chspec == cur_chspec)
			count++;
		biq = biq->next;
	}
	candi->in_use = count;

	score = acs_chan_score_bss(candi, bss_info, ncis);

	candi->chscore[CH_SCORE_BSS].score = score;
	ACSD_DEBUG("bss score: %d for chanspec 0x%x\n", score, candi->chspec);
}

static void
acs_candidate_score_busy(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(candi->chspec, &chan);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if ((ch == chan.control) || (ch == chan.ext20)) {
			score += stats->ccastats[CCASTATS_OBSS];
			hits ++;
		}

		if (hits == 2 || (hits && chan.control == chan.ext20)) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("busy check failed for chanspec: 0x%x\n", chspec);
		return;
	}

	if (hits)
		candi->chscore[CH_SCORE_BUSY].score = score/hits;
	ACSD_DEBUG("busy score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_intf(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &chan);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == chan.control || ch == chan.ext20) {
			score += stats->ccastats[CCASTATS_NOPKT];
			hits ++;
		}

		if (hits == 2 || (hits && chan.control == chan.ext20)) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("intf check failed for chanspec: 0x%x\n", chspec);
		return;
	}

	if (hits)
		candi->chscore[CH_SCORE_INTF].score = score/hits;
	ACSD_DEBUG("intf score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_intfadj(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, score = 0;
	int dist, d_weight = 10;

	acs_parse_chanspec(chspec, &chan);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);
		ACSD_DEBUG("channel: %d, ch: %d\n", chan.control, ch);

		if (ch != chan.control) {
			dist = ch - chan.control;
			dist = dist > 0 ? dist : dist * -1;
			score += stats->ccastats[CCASTATS_NOPKT] * d_weight / dist;

			ACSD_DEBUG("dist: %d, count: %d, score: %d\n",
				dist, stats->ccastats[CCASTATS_NOPKT], score);
			if (chan.ext20 != 0 && ch != chan.ext20) {
				dist = ABS(ch - chan.ext20);
				score += stats->ccastats[CCASTATS_NOPKT] * d_weight / dist;
			}

			ACSD_DEBUG("channel: %d, ch: %d score: %d\n", chan.control, ch, score);
		}
	}

	candi->chscore[CH_SCORE_INTFADJ].score = score / d_weight;
	ACSD_DEBUG("intf_adj score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_fcs(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &chan);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == chan.control || ch == chan.ext20) {
			score += stats->ccastats[CCASTATS_NOCTG];
			hits ++;
		}
		if (hits == 2 || (hits && (chan.ext20 == 0))) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("fcs check failed for chanspec: 0x%x\n", chspec);
		return;
	}

	if (hits)
		candi->chscore[CH_SCORE_FCS].score = score/hits;
	ACSD_DEBUG("fcs score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_txpwr(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	/* TBD */
}

static void
acs_candidate_score_bgnoise(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &chan);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == chan.control || ch == chan.ext20) {
			if (stats->bgnoise && stats->bgnoise > ACS_BGNOISE_BASE) {
				score += stats->bgnoise - ACS_BGNOISE_BASE;
			}
			hits ++;
		}
		if (hits == 2 || (hits && (chan.ext20 == 0))) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("bgnoise check failed for chanspec: 0x%x\n", chspec);
		return;
	}
	if (hits)
		candi->chscore[CH_SCORE_BGNOISE].score = score/hits;
	ACSD_DEBUG("fcs score: %d for chanspec 0x%x\n", score, chspec);
}

static void
acs_candidate_score_total(ch_candidate_t *candi)
{
	int i, total;
	ch_score_t *score_p;

	total = 0;
	score_p = candi->chscore;

	for (i = 0; i < CH_SCORE_TOTAL; i++)
		total += score_p[i].score * score_p[i].weight;
	score_p[CH_SCORE_TOTAL].score = total;
}

static void
acs_candidate_score_cns(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, score = 0, min_score = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &chan);

	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == chan.control || ch == chan.ext20 || ch == chan.ext40[0] ||
			ch == chan.ext40[1]) {
			score = stats->bgnoise;
			score += chanim_txop_to_noise(stats->chan_idle);
			hits ++;
			min_score = MIN(min_score, score);
		}
		if (hits == 4 || (hits == 2 && 0 == chan.ext40[0]) || (hits && 0 == chan.ext20)) {
			done = TRUE;
			break;
		}
	}

	if (!done) {
		ACSD_ERROR("knoise check failed for chanspec: 0x%x\n", chspec);
		return;
	}
	candi->chscore[CH_SCORE_CNS].score = min_score;
	ACSD_INFO("Composite Noise Score (CNS): %d for chanspec 0x%x\n",
		min_score, chspec);
}

static void
acs_candidate_adjacent_bss(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	acs_chan_bssinfo_t* bss_info = c_info->ch_bssinfo;
	int i, ch, adjbss = 0;
	int ncis = c_info->scan_chspec_list.count;
	acs_channel_t chan;
	uint8 channel_sb_low, channel_sb_upper;

	acs_parse_chanspec(candi->chspec, &chan);

	if (CHSPEC_IS80(candi->chspec)) {
		channel_sb_low = CHSPEC_CHANNEL(candi->chspec) - CH_10MHZ_APART - CH_20MHZ_APART;
		channel_sb_upper = CHSPEC_CHANNEL(candi->chspec) + CH_10MHZ_APART + CH_20MHZ_APART;
	} else if (CHSPEC_IS40(candi->chspec)) {
		channel_sb_low = CHSPEC_CHANNEL(candi->chspec) - CH_10MHZ_APART;
		channel_sb_upper = CHSPEC_CHANNEL(candi->chspec) + CH_10MHZ_APART;
	} else {
		channel_sb_low = channel_sb_upper = CHSPEC_CHANNEL(candi->chspec);
	}

	for (i = 0; i < ncis; i++) {
		ch = (int)bss_info[i].channel;
		if ((ch == (channel_sb_low - CH_20MHZ_APART)) ||
			(ch == (channel_sb_upper + CH_20MHZ_APART))) {
			adjbss += bss_info[i].aAPs + bss_info[i].bAPs + bss_info[i].gAPs;
			adjbss += bss_info[i].lSBs + bss_info[i].uSBs;
			adjbss += bss_info[i].llSBs + bss_info[i].luSBs + bss_info[i].ulSBs +
				bss_info[i].uuSBs;
			adjbss += bss_info[i].wEX40s;
			adjbss += bss_info[i].nEXs + bss_info[i].wEX20s;
		}
	}

	candi->chscore[CH_SCORE_ADJ].score = adjbss;
	ACSD_FCS("adjacent bss score: %d for chanspec 0x%x\n", adjbss, candi->chspec);
}

static void
acs_candidate_score(acs_chaninfo_t* c_info, int bw)
{
	ch_score_t *score_p;
	ch_candidate_t* candi;
	int i;

	for (i = 0; i < c_info->c_count[bw]; i++) {
		candi = &c_info->candidate[bw][i];
		if (!candi->valid)
			continue;
		score_p = candi->chscore;

		ACSD_DEBUG("calc score for candidate chanspec: 0x%x\n",
			candi->chspec);

		/* calculate the score for each factor */
		if (score_p[CH_SCORE_BSS].weight)
			acs_candidate_score_bss(candi, c_info);

		if (score_p[CH_SCORE_BUSY].weight)
			acs_candidate_score_busy(candi, c_info);

		if (score_p[CH_SCORE_INTF].weight)
			acs_candidate_score_intf(candi, c_info);

		if (score_p[CH_SCORE_INTFADJ].weight)
			acs_candidate_score_intfadj(candi, c_info);

		if (score_p[CH_SCORE_FCS].weight)
			acs_candidate_score_fcs(candi, c_info);

		if (score_p[CH_SCORE_TXPWR].weight)
			acs_candidate_score_txpwr(candi, c_info);

		if (score_p[CH_SCORE_BGNOISE].weight)
			acs_candidate_score_bgnoise(candi, c_info);

		acs_candidate_score_cns(candi, c_info);

		if (score_p[CH_SCORE_ADJ].weight)
			acs_candidate_adjacent_bss(candi, c_info);

		acs_candidate_score_total(candi);
#ifdef ACS_DEBUG
		acs_dump_score(score_p);
#endif
	}
}

static void
acs_candidate_check_intf(ch_candidate_t *candi, acs_chaninfo_t* c_info)
{
	wl_chanim_stats_t *ch_stats = c_info->chanim_stats;
	chanim_stats_t *stats;
	chanspec_t chspec = candi->chspec;
	acs_channel_t chan;
	int i, ch, intf = 0, hits = 0;
	bool done = FALSE;

	acs_parse_chanspec(chspec, &chan);
	for (i = 0; i < ch_stats->count; i++) {
		stats = &ch_stats->stats[i];
		ch = CHSPEC_CHANNEL(stats->chanspec);

		if (ch == chan.control || ch == chan.ext20) {
			if (c_info->flags & ACS_FLAGS_INTF_THRES_CCA) {
				intf = stats->ccastats[CCASTATS_NOPKT];
				if (intf > c_info->acs_policy.intf_threshold) {
					candi->valid = FALSE;
					candi->reason |= ACS_INVALID_INTF_CCA;
					break;
				}
			}
			if (c_info->flags & ACS_FLAGS_INTF_THRES_BGN) {
				intf = stats->bgnoise;
				if (intf && intf > c_info->acs_policy.bgnoise_thres) {
					candi->valid = FALSE;
					candi->reason |= ACS_INVALID_INTF_BGN;
					break;
				}
			}
			hits ++;
		}
		if (hits == 2 || (hits && (chan.ext20 == 0))) {
			done = TRUE;
			break;
		}
	}
	if (!done) {
		ACSD_ERROR("intf check failed for chanspec: 0x%x\n", chspec);
		return;
	}
}

static bool
acs_is_initial_selection(acs_chaninfo_t* c_info)
{
	bool initial_selection = FALSE;
	chanim_info_t * ch_info = c_info->chanim_info;
	uint8 cur_idx = chanim_mark(ch_info).record_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;

	start_idx = MODSUB(cur_idx, 1, CHANIM_ACS_RECORD);
	start_record = &ch_info->record[start_idx];
	if ((start_idx == CHANIM_ACS_RECORD - 1) && (start_record->timestamp == 0))
		initial_selection = TRUE;

	ACSD_DFSR("Initial selection is %d\n", initial_selection);
	return initial_selection;
}

static uint32 acs_channel_info(acs_chaninfo_t *c_info, chanspec_t chspec)
{
#define RES_SZ 20 /* Need 13, strlen("per_chan_info"), +4, sizeof(uint32). Rounded to 20. */
	char resbuf[RES_SZ];
	int ret;

	/* Figure out the control chanspec. */
	chspec = wf_chspec_ctlchspec(chspec);

	if ((ret = wl_iovar_getbuf(c_info->name, "per_chan_info", &chspec, sizeof(chanspec_t),
		resbuf, RES_SZ)) == BCME_OK) {
		uint32 channel_status;

		channel_status = dtoh32(*(uint32 *)resbuf);
		ACSD_DFSR("BW20 Channel 0x%x (#%d) Status %08x (%sactive, %d minutes)\n",
			chspec, CHSPEC_CHANNEL(chspec),
			channel_status,
			(channel_status & WL_CHAN_INACTIVE) ? "in" :"",
			(channel_status >> 24) & 0xff);
		return channel_status;
	}
	ACSD_ERROR("Failed to get channel status: %d\n", ret);
	return 0;
}

/*
 * acs_dfs_channel_is_usable() - check whether a specific DFS channel is usable right now.
 *
 * Returns TRUE if so, FALSE if not (ie because the channel is currently out of service).
 *
 */
static bool
acs_dfs_channel_is_usable(acs_chaninfo_t *c_info, chanspec_t chspec)
{
	return (acs_channel_info(c_info, chspec) & WL_CHAN_INACTIVE) ? FALSE : TRUE;
}

/* is chanspec DFS channel */
static bool
acs_is_dfs_chanspec(acs_chaninfo_t *c_info, chanspec_t chspec)
{
	return (acs_channel_info(c_info, chspec) & WL_CHAN_RADAR);
}

/* for 80M/40 bw chanspec, select control chan with max AP number
 * to co-exist with neighbor friendly
 */
chanspec_t
acs_adjust_ctrl_chan(acs_chaninfo_t *c_info, chanspec_t chspec)
{
	chanspec_t selected = chspec;
	acs_chan_bssinfo_t* bss_info = c_info->ch_bssinfo;
	int i, j, max_sb;
	int ch;
	uint8 ctrl_sb[4] = {0}, num_sb[4] = {0};

	if (nvram_match("acs_ctrl_chan_adjust", "0"))
		return selected;

	if (!CHSPEC_IS80(selected) && !CHSPEC_IS40(selected))
		return selected;

	max_sb = (CHSPEC_IS80(selected))? 4 : 2;

	if (CHSPEC_IS80(selected)) {
		ctrl_sb[(WL_CHANSPEC_CTL_SB_UL >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3] =
			UL_20_SB(CHSPEC_CHANNEL(selected));
		ctrl_sb[(WL_CHANSPEC_CTL_SB_UU >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3] =
			UU_20_SB(CHSPEC_CHANNEL(selected));
		ctrl_sb[(WL_CHANSPEC_CTL_SB_LL >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3] =
			LL_20_SB(CHSPEC_CHANNEL(selected));
		ctrl_sb[(WL_CHANSPEC_CTL_SB_LU >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3] =
			LU_20_SB(CHSPEC_CHANNEL(selected));

		ACSD_INFO("sb:%d %d=%d %d=%d %d=%d %d=%d\n", max_sb,
			(WL_CHANSPEC_CTL_SB_LL >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3,
			LL_20_SB(CHSPEC_CHANNEL(selected)),
			(WL_CHANSPEC_CTL_SB_LU >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3,
			LU_20_SB(CHSPEC_CHANNEL(selected)),
			(WL_CHANSPEC_CTL_SB_UL >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3,
			UL_20_SB(CHSPEC_CHANNEL(selected)),
			(WL_CHANSPEC_CTL_SB_UU >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3,
			UU_20_SB(CHSPEC_CHANNEL(selected)));
	}
	else {
		ctrl_sb[(WL_CHANSPEC_CTL_SB_L >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3] =
			LOWER_20_SB(CHSPEC_CHANNEL(selected));
		ctrl_sb[(WL_CHANSPEC_CTL_SB_U >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3] =
			UPPER_20_SB(CHSPEC_CHANNEL(selected));

		ACSD_INFO("sb:%d %d=%d %d=%d\n", max_sb,
			(WL_CHANSPEC_CTL_SB_L >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3,
			LOWER_20_SB(CHSPEC_CHANNEL(selected)),
			(WL_CHANSPEC_CTL_SB_U >> WL_CHANSPEC_CTL_SB_SHIFT)&0x3,
			UPPER_20_SB(CHSPEC_CHANNEL(selected)));
	}

	/* calulate no. APs for all 20M side bands */
	for (i = 0; i < c_info->scan_chspec_list.count; i++) {
		ch = (int)bss_info[i].channel;

		for (j = 0; j < max_sb; j++) {
			if (ch == ctrl_sb[j]) {
				num_sb[j] = bss_info[i].aAPs + bss_info[i].bAPs + bss_info[i].gAPs +
					bss_info[i].lSBs + bss_info[i].uSBs +
					bss_info[i].llSBs + bss_info[i].luSBs + bss_info[i].ulSBs +
					bss_info[i].uuSBs;
				ACSD_INFO("i=%d j=%d chan=%d num_sb=%d\n", i, j, ch, num_sb[j]);
			}
		}
	}

	/* find which sideband has max no. APs */
	for (i = 1, j = 0; i < max_sb; i++) {
		if (num_sb[i] > num_sb[j])
			j = i;
	}

	ACSD_INFO("selected sb: %d\n", j);
	selected &=  ~(WL_CHANSPEC_CTL_SB_MASK);
	selected |= (j << WL_CHANSPEC_CTL_SB_SHIFT);
	ACSD_INFO("Final selected chanspec: 0x%4x\n", selected);
	return selected;
}

static int rand_seed_by_time(void)
{
	time_t atime;

	time(&atime);
	srand((unsigned long)atime);

	return rand();
}

void
acs_select_chspec(acs_chaninfo_t *c_info)
{
	bool need_coex_check = FALSE;
	chanspec_t selected = 0, in_chspec = 0, out_chspec = 0;
	ch_candidate_t *candi;
	int bw = ACS_BW_20;
	int ret = 0, i, j;
	acs_rsi_t *rsi = &c_info->rs_info;
	chanim_info_t * ch_info = c_info->chanim_info;
	time_t now = uptime();
	fcs_conf_chspec_t *excl_chans;
	chanspec_t cur_chspec;
	int tmp_chspec;
	bool dfsr_disable = !(acs_dfsr_reentry_type(ACS_DFSR_CTX(c_info)) == DFS_REENTRY_IMMEDIATE);
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *wl_bw;

	/* if given a chanspec but just need to pass coex check */

	need_coex_check = BAND_2G(rsi->band_type) &&
		(rsi->bw_cap == WLC_BW_CAP_40MHZ) &&
		rsi->coex_enb;

	if (CHSPEC_IS2G(rsi->pref_chspec) && CHSPEC_IS40(rsi->pref_chspec) &&
		need_coex_check) {
		selected = acs_coex_check(c_info, rsi->pref_chspec);
		goto done;
	}

	if (WL_BW_CAP_80MHZ(rsi->bw_cap)) {
			bw = ACS_BW_80;
	}
	else if (WL_BW_CAP_40MHZ(rsi->bw_cap)) {
			bw = ACS_BW_40;
	}

recheck:
	/* build the candidate chanspec list */
	ret = acs_build_candidates(c_info, bw);
	candi = c_info->candidate[bw];

	/* going through the  coex check if needed */
	if ((bw == ACS_BW_40) && need_coex_check) {
		for (i = 0; i < c_info->c_count[bw]; i++) {
			in_chspec = candi[i].chspec;
			out_chspec = acs_coex_check(c_info, in_chspec);
			if (in_chspec != out_chspec) {
				candi[i].valid = FALSE;
				candi[i].reason |= ACS_INVALID_COEX;
			}
		}
	}

	/* going through the interference check if needed */

	if (c_info->flags & ACS_FLAGS_INTF_THRES_CCA ||
		c_info->flags & ACS_FLAGS_INTF_THRES_BGN) {
		for (i = 0; i < c_info->c_count[bw]; i++)
			acs_candidate_check_intf(&candi[i], c_info);
	}

	/*
	 * For 20MHz channels, only pick from 1, 6, 11
	 * For 40MHz channels, only pick from control channel being 1, 6, 11 (BT doc)
	 * Mark all the other candidates invalid
	 */

	if (BAND_2G(rsi->band_type)) {
		acs_channel_t chan;
		ACSD_DEBUG("Filter chanspecs for 40/20 MHz channels, count: %d\n",
			c_info->c_count[bw]);
		for (i = 0; i < c_info->c_count[bw]; i++) {
			acs_parse_chanspec(candi[i].chspec, &chan);

			ACSD_DEBUG("channel: %d, ext: %d\n", chan.control, chan.ext20);

			if ((!nvram_match("acs_2g_ch_no_restrict", "1")) &&
				(chan.control != ACS_CHANNEL_1) &&
				(chan.control != ACS_CHANNEL_6) &&
				(chan.control != ACS_CHANNEL_11))  {
				candi[i].valid = FALSE;
				candi[i].reason |= ACS_INVALID_OVLP;
				continue;
			}
			ACSD_DEBUG("valid channel: %d\n", chan.control);
		}
	}

	excl_chans = &(c_info->acs_fcs.excl_chans);
	for (i = 0; i < c_info->c_count[bw]; i++) {
		/* Exclude channels build candidate */
		if (excl_chans && excl_chans->count) {
			for (j = 0; j < excl_chans->count; j++) {
				if (candi[i].chspec == excl_chans->clist[j]) {
					candi[i].valid = FALSE;
					candi[i].reason |= ACS_INVALID_EXCL;
					break;
				}
			}
		}
	}

	if (ACS_FCS_MODE(c_info)) {
		excl_chans = &(c_info->acs_fcs.excl_chans);
		if (wl_iovar_getint(c_info->name, "chanspec", &tmp_chspec) < 0)
			cur_chspec = c_info->selected_chspec;
		else
			cur_chspec = (chanspec_t)tmp_chspec;

		for (i = 0; i < c_info->c_count[bw]; i++) {
			/* Exclude channels build candidate */
			if (excl_chans && excl_chans->count) {
				for (j = 0; j < excl_chans->count; j++) {
					if (candi[i].chspec == excl_chans->clist[j]) {
						candi[i].valid = FALSE;
						candi[i].reason |= ACS_INVALID_EXCL;
						break;
					}
				}
			}

			if (!acs_is_initial_selection(c_info)) {
				/* Check for same sideband */
				if (CHSPEC_CHANNEL(cur_chspec) &&
					(CHSPEC_CTL_SB(cur_chspec) !=
					CHSPEC_CTL_SB(candi[i].chspec))) {
					candi[i].reason |= ACS_INVALID_MISMATCH_SB;
				}

				/* avoid select same channel */
				if (cur_chspec == candi[i].chspec) {
					ACSD_FCS("Skipping current channel %x for candidate \n",
						cur_chspec);
					candi[i].reason |= ACS_INVALID_SAMECHAN;
				}

				/* Use DFS channels if DFS reentry is OK */
				if (candi[i].is_dfs && (dfsr_disable ||
					(!acs_dfs_channel_is_usable(c_info, candi[i].chspec))))
					candi[i].reason |= ACS_INVALID_DFS;
			}
			else {
				/* Use DFS channels if we are just coming up */
				if (candi[i].is_dfs &&
					((c_info->acs_fcs.acs_dfs == ACS_DFS_DISABLED) ||
					(!acs_dfs_channel_is_usable(c_info, candi[i].chspec))))
						candi[i].reason |= ACS_INVALID_DFS;
			}
			candi[i].valid = (candi[i].reason == 0);
		}
		acs_dfsr_reentry_done(ACS_DFSR_CTX(c_info)); /* DFS Re-entry has been done */
		/* avoid channel flip and flop, skip the channel which selected in
		 * some amount of time
		 */
		for (i = 0; i < c_info->c_count[bw]; i++) {
			for (j = 0; j < CHANIM_ACS_RECORD; j++) {
				if (candi[i].chspec == ch_info->record[j].selected_chspc) {
					if (now - ch_info->record[j].timestamp <
							c_info->acs_fcs.acs_chan_flop_period) {
						candi[i].valid = FALSE;
						candi[i].reason |= ACS_INVALID_CHAN_FLOP_PERIOD;
						j = CHANIM_ACS_RECORD;
					}
				}
			}
		}
	}

	acs_candidate_score(c_info, bw);

	/* if there is at least one valid chanspec */
	if (acs_has_valid_candidate(c_info, bw)) {
		acs_policy_t *a_pol = &c_info->acs_policy;
		if (a_pol->chan_selector)
			selected = a_pol->chan_selector(c_info, bw);
		else
			ACSD_ERROR("chan_selector is null for the selected policy");
		goto done;
	} else if (ACS_FCS_MODE(c_info)) {
		ACSD_DEBUG("FCS: no valid channel to select. BW is not adjust. \n");
		/* FCS doesn't downgrade bandwidth if there is no valid channel can be selected */
		return;
	}

	/* if we failed to pick a 80 chanspec, we fall back to 40 */
	if (bw == ACS_BW_80) {
		ACSD_DEBUG("Failed to find a valid 80 MHz chanspec\n");
		bw = ACS_BW_40;
		goto recheck;
	} else if (bw == ACS_BW_40) {
	/* if we failed to pick a 40 chanspec, we fall back to 20 */
		ACSD_DEBUG("Failed to find a valid 40 MHz chanspec\n");
		bw = ACS_BW_20;
		goto recheck;
	}
	/* pick a chanspec if we are here */
	else {
		if (BAND_5G(rsi->band_type) && c_info->c_count[bw])
			selected = candi[c_info->c_count[bw]-1].chspec;
		else
			selected = candi[0].chspec;
	}

done:
#if 0
        ACSD_PRINT("selected channel spec: 0x%4x\n", selected);
#else
        snprintf(prefix, sizeof(prefix), "wl%d_", 1);           // 5GHz
        wl_bw = nvram_safe_get(strcat_r(prefix, "bw", tmp));
        if (CHSPEC_IS5G(selected) && CHSPEC_IS40(selected)
                && (atoi(wl_bw) == 3))
        {       // force to select 80M chanspec
                chanspec_t chspec_tmp = select_80M_chspec(selected);
                if (chspec_tmp) selected = chspec_tmp;
        }
        else if ((selected == 0) && BAND_5G(rsi->band_type)
                && wl_chanspec_list_with_band_4(1, 20))
        {
                char buf[32];

                i = rand_seed_by_time() % WF_NUM_5G_80M_CTL_CHANS_BAND4;
                i = wf_5g_80m_ctl_chans_band4[i];
                sprintf(buf, "%d", i);
                selected = wf_chspec_aton(buf);
        }
        ACSD_PRINT("selected channel spec: 0x%4x (%s)\n", selected,
                wf_chspec_ntoa(selected, chanspecbuf));
#endif
        selected = acs_adjust_ctrl_chan(c_info, selected);
#if 0
        ACSD_PRINT("Adjusted channel spec: 0x%4x\n", selected);
#else
        ACSD_PRINT("Adjusted channel spec: 0x%4x (%s)\n", selected,
                wf_chspec_ntoa(selected, chanspecbuf));
#endif

	ACSD_PRINT("selected channel spec: 0x%4x (%s)\n", selected,
		wf_chspec_ntoa(selected, chanspecbuf));

	c_info->selected_chspec = selected;
}

void
acs_set_chspec(acs_chaninfo_t * c_info)
{
	int ret = 0;
	chanspec_t chspec = c_info->selected_chspec;


	if (chspec) {
		ret = wl_iovar_setint(c_info->name, "chanspec", htod32(chspec));
		if (ret == 0) {
			acs_dfsr_chanspec_update(ACS_DFSR_CTX(c_info), chspec, __FUNCTION__);
			c_info->cur_is_dfs = acs_is_dfs_chanspec(c_info, chspec);
		}
	}
	if (ret)
		ACSD_ERROR("set chanspec 0x%x failed!\n", chspec);
}

static void
acs_init_info(acs_info_t ** acs_info_p)
{
	acs_info = (acs_info_t*)acsd_malloc(sizeof(acs_info_t));

	* acs_info_p = acs_info;
}

void
acs_init_run(acs_info_t ** acs_info_p)
{
	char name[16], *next;
	acs_chaninfo_t * c_info;
	int ret = 0;

	acs_init_info(acs_info_p);

	foreach(name, nvram_safe_get("acs_ifnames"), next) {
		acs_add_map(name);
		ret = acs_start(name, &c_info);

		if (ret) {
			ACSD_ERROR("acs_start failed for ifname: %s\n", name);
			break;
		}

		if (AUTOCHANNEL(c_info) || COEXCHECK(c_info)) {
			acs_select_chspec(c_info);
			acs_set_chspec(c_info);

			ACSD_DEBUG("ifname %s - mode: %s\n", name,
			   AUTOCHANNEL(c_info)? "SELECT" :
			   COEXCHECK(c_info)? "COEXCHECK" :
			   ACS11H(c_info)? "11H" : "MONITOR");

			chanim_upd_acs_record(c_info->chanim_info,
				c_info->selected_chspec, APCS_INIT);
		}
	}
}

/* check if there is still associated scbs. reture value: TRUE if yes. */
static bool
acs_check_assoc_scb(acs_chaninfo_t * c_info)
{
	bool connected = TRUE;
	int result = 0;
	int ret = 0;

	ret = wl_iovar_getint(c_info->name, "scb_assoced", &result);
	if (ret) {
		ACSD_ERROR("failed to get scb_assoced\n");
		return connected;
	}

	connected = dtoh32(result) ? TRUE : FALSE;
	ACSD_DEBUG("connected: %d\n",  connected);

	return connected;
}

int
acs_update_driver(acs_chaninfo_t * c_info)
{
	int ret = 0;
	bool param = TRUE;
	/* if we are already beaconing, after the acs scan and new chanspec selection,
	   we need to ask the driver to do some updates (beacon, probes, etc..).
	*/
	ret = wl_iovar_setint(c_info->name, "acs_update", htod32((uint)param));
	ACS_ERR(ret, "acs update failed\n");

	return ret;
}

int
acs_fcs_ci_scan_check(acs_chaninfo_t *c_info)
{
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	acs_scan_chspec_t* chspec_q = &c_info->scan_chspec_list;
	time_t now = uptime();

	/* return for non fcs mode or no chan to scan */
	if (!ACS_FCS_MODE(c_info) || (chspec_q->count <= chspec_q->excl_count))
		return 0;

	/* start ci scan:
	1. when txop is less than thld, start ci scan for pref chan
	2. if no scan for a long period, start ci scan
	*/

	/* scan pref chan: when txop < thld, start ci scan for pref chan */
	if (c_info->scan_chspec_list.ci_pref_scan_request && (chspec_q->pref_count > 0)) {
		c_info->scan_chspec_list.ci_pref_scan_request = FALSE;

		if (chspec_q->ci_scan_running != ACS_CI_SCAN_RUNNING_PREF) {
			ACSD_FCS("acs_ci_scan_timeout start CI pref scan: scan_count %d\n",
				chspec_q->pref_count);
			chspec_q->ci_scan_running = ACS_CI_SCAN_RUNNING_PREF;
			fcs_info->acs_ci_scan_count = chspec_q->pref_count;
			acs_ci_scan_update_idx(&c_info->scan_chspec_list, 0);
		}
	}

	/* check for current scanning status */
	if (chspec_q->ci_scan_running)
		return 1;

	/* check scan timeout, and trigger CI scan if timeout happened */
	if ((now - fcs_info->timestamp_acs_scan) >= fcs_info->acs_ci_scan_timeout) {
		fcs_info->acs_ci_scan_count = chspec_q->count - chspec_q->excl_count;
		chspec_q->ci_scan_running = ACS_CI_SCAN_RUNNING_NORM;
		acs_ci_scan_update_idx(&c_info->scan_chspec_list, 0);
		ACSD_FCS("acs_ci_scan_timeout start CI scan: now %u(%u), scan_count %d\n",
			(uint)now, fcs_info->timestamp_acs_scan,
			chspec_q->count - chspec_q->excl_count);
		return 1;
	}

	return 0;
}

static int
acs_fcs_ci_scan_finish_check(acs_chaninfo_t * c_info)
{
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	acs_scan_chspec_t* chspec_q = &c_info->scan_chspec_list;

	/* do nothing for fcs mode or scanning not active  */
	if ((!ACS_FCS_MODE(c_info)) || (!chspec_q->ci_scan_running))
		return 0;

	/* Check for end of scan: scanned all channels once */
	if ((fcs_info->acs_ci_scan_count) && (!(--fcs_info->acs_ci_scan_count))) {
		ACSD_FCS("acs_ci_scan_timeout stop CI scan: now %u \n", (uint)uptime());
		chspec_q->ci_scan_running = 0;
	}

	return 0;
}

static int
acs_fcs_tx_idle_check(acs_chaninfo_t *c_info)
{
	uint timer = c_info->acs_cs_scan_timer;
	time_t now = uptime();
	wl_cnt_t wlcnts;
	int full_scan = 0;
	int ret = 0;
	uint32 acs_txframe;
	acs_fcs_t *fcs_info = &c_info->acs_fcs;

	if (!ACS_FCS_MODE(c_info))
		return full_scan;

	/* Check for idle period "acs_cs_scan_timer" */
	if ((now - fcs_info->timestamp_tx_idle) < timer)
		return full_scan;

	ACSD_FCS("acs_fcs_tx_idle: now %u(%u)\n", (uint)now, fcs_info->timestamp_tx_idle);

	/* Check wl transmit activity and trigger full scan if it is idle */
	ret = wl_iovar_get(c_info->name, "counters", &wlcnts, sizeof(wl_cnt_t));
	if (ret < 0) {
		ACSD_ERROR("wl counters failed (%d)\n", ret);
		return full_scan;
	}

	ACSD_FCS("acs_fcs_tx_idle: txframe %d(%d)\n", wlcnts.txframe, fcs_info->acs_txframe);

	if (wlcnts.txframe > fcs_info->acs_txframe)
		acs_txframe = wlcnts.txframe - fcs_info->acs_txframe;
	else
		acs_txframe = wlcnts.txframe + ((uint32)0xFFFFFFFF - fcs_info->acs_txframe);

	if (acs_txframe < (fcs_info->acs_tx_idle_cnt * (now - fcs_info->timestamp_tx_idle))) {
		ACSD_FCS("acs_fcs_tx_idle fullscan: %d\n",	fcs_info->acs_txframe);
		full_scan = 1;
	}

	fcs_info->acs_txframe = wlcnts.txframe;
	fcs_info->timestamp_tx_idle = now;
	return full_scan;
}

/*
 * acs_scan_timer_or_dfsr_check() - check for scan timer or dfs reentry, change channel if needed.
 *
 * This function checks whether we need to change channels because of CS scan timer expiration or
 * DFS Reentry, and does the channel switch if so.
 */
int
acs_scan_timer_or_dfsr_check(acs_chaninfo_t * c_info)
{
	uint cs_scan_timer;
	chanim_info_t * ch_info;
	int ret = 0;
	uint8 cur_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	int switch_reason = APCS_INIT; /* Abuse APCS_INIT as undefined switch reason (no switch) */

	ch_info = c_info->chanim_info;
	cur_idx = chanim_mark(ch_info).record_idx;
	start_idx = MODSUB(cur_idx, 1, CHANIM_ACS_RECORD);
	start_record = &ch_info->record[start_idx];

	if (AUTOCHANNEL(c_info)) {
		/* Check whether we should switch now because of the CS scan timer */
		cs_scan_timer = c_info->acs_cs_scan_timer;

		if (SCAN_TIMER_ON(c_info) && cs_scan_timer) {
			time_t passed;

			ACSD_DEBUG(" timer: %d\n", cs_scan_timer);

			passed = uptime() - start_record->timestamp;

			if (acs_fcs_tx_idle_check(c_info) ||
				((passed > cs_scan_timer) && (!acs_check_assoc_scb(c_info)))) {
				switch_reason = APCS_CSTIMER;
			}
		}
	}

	/* If not switching because of CS scan timer, see if DFS Reentry switch is needed */
	if ((switch_reason == APCS_INIT) &&
		ACS_FCS_MODE(c_info) &&
		acs_dfsr_reentry_type(ACS_DFSR_CTX(c_info)) == DFS_REENTRY_IMMEDIATE) {
			ACSD_DFSR("Switching Channels for DFS Reentry.\n");
			switch_reason = APCS_DFS_REENTRY;
	}

	switch (switch_reason) {
	case APCS_CSTIMER:
		/* start scan */
		ret = acs_run_cs_scan(c_info);
		ACS_ERR(ret, "cs scan failed\n");
		acs_cleanup_scan_entry(c_info);


		ret = acs_request_data(c_info);
		ACS_ERR(ret, "request data failed\n");

		/* Fall through to common case */

	case APCS_DFS_REENTRY:

		acs_select_chspec(c_info);
		acs_set_chspec(c_info);

		/* for consecutive timer based trigger, replace the previous one */
		if ((switch_reason == APCS_CSTIMER) && start_record->trigger == APCS_CSTIMER)
			chanim_mark(ch_info).record_idx = start_idx;

		chanim_upd_acs_record(ch_info, c_info->selected_chspec, switch_reason);

		ret = acs_update_driver(c_info);

		ACS_ERR(ret, "update driver failed\n");
		break;

	default:
		break;
	}
	return ret;
}

int
acs_do_ci_update(uint ticks, acs_chaninfo_t * c_info)
{
	int ret = 0;

	if (ticks % c_info->acs_ci_scan_timer)
		return ret;

	acs_expire_scan_entry(c_info, (time_t)c_info->acs_scan_entry_expire);

	if (ACS_FCS_MODE(c_info) && (!(c_info->scan_chspec_list.ci_scan_running)))
		return ret;

	if (c_info->cur_is_dfs) {
		ACSD_INFO("No CI scan when running in DFS chan:%x\n", c_info->cur_chspec);
	}
	else {
		ret = acs_run_ci_scan(c_info);
		ACS_ERR(ret, "ci scan failed\n");

		ret = acs_request_data(c_info);
		ACS_ERR(ret, "request data failed\n");
	}

	acs_fcs_ci_scan_finish_check(c_info);

	return ret;
}

int
acs_update_status(acs_chaninfo_t * c_info)
{
	int ret = 0;
	int cur_chspec;

	ret = wl_iovar_getint(c_info->name, "chanspec", &cur_chspec);
	ACS_ERR(ret, "acs get chanspec failed\n");

	/* To add a acs_record when finding out channel change isn't make by ACSD */
	if ((chanspec_t)dtoh32(cur_chspec) != c_info->cur_chspec) {
		c_info->cur_chspec = (chanspec_t)dtoh32(cur_chspec);
		c_info->cur_is_dfs = acs_is_dfs_chanspec(c_info, cur_chspec);
		chanim_upd_acs_record(c_info->chanim_info,
			c_info->cur_chspec, APCS_NONACSD);
		acs_dfsr_chanspec_update(ACS_DFSR_CTX(c_info), c_info->cur_chspec,
			__FUNCTION__);

		ACSD_INFO("%s: chanspec: 0x%x\n", c_info->name, c_info->cur_chspec);
	}

	return ret;
}

void
acs_cleanup(acs_info_t ** acs_info_p)
{
	int i;

	for (i = 0; i < ACS_MAX_IF_NUM; i++) {
		acs_chaninfo_t* c_info = (*acs_info_p)->chan_info[i];

		if (!c_info)
			continue;

		ACS_FREE(c_info->scan_results);
		acs_cleanup_scan_entry(c_info);
		ACS_FREE(c_info->ch_bssinfo);
		ACS_FREE(c_info->chanim_stats);
		ACS_FREE(c_info->scan_chspec_list.chspec_list);
		ACS_FREE(c_info->candidate[ACS_BW_20]);
		ACS_FREE(c_info->candidate[ACS_BW_40]);
		ACS_FREE(c_info->chanim_info);

		acs_dfsr_exit(ACS_DFSR_CTX(c_info));

		ACS_FREE(c_info);
	}
	ACS_FREE(acs_info);
	*acs_info_p = NULL;
}
