#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#ifndef ASSERT
#define ASSERT(exp)
#endif
#include <stdarg.h>
#include <syslog.h>
#include <bcmwifi_channels.h>

#include "chan_utils.h"
#if 0
/* bw in MHz, return the channel count from the center channel to the
 * the channel at the edge of the band
 */
static uint8
center_chan_to_edge(uint bw)
{
	/* edge channels separated by BW - 10MHz on each side
	 * delta from cf to edge is half of that,
	 * MHz to channel num conversion is 5MHz/channel
	 */
        return (uint8)(((bw - 20) / 2) / 5);
}

/* return channel number of the low edge of the band
 * given the center channel and BW
 */
static uint8
channel_low_edge(uint center_ch, uint bw)
{
	return (uint8)(center_ch - center_chan_to_edge(bw));
}
/* return control channel given center channel and side band */
static uint8
channel_to_ctl_chan(uint center_ch, uint bw, uint sb)
{
	return (uint8)(channel_low_edge(center_ch, bw) + sb * 4);
}

/* return side band number given center channel and control channel
 * return -1 on error
 */
static int
channel_to_sb(uint center_ch, uint ctl_ch, uint bw)
{
	uint lowest = channel_low_edge(center_ch, bw);
	uint sb;

	if ((ctl_ch - lowest) % 4) {
		/* bad ctl channel, not mult 4 */
		return -1;
	}

	sb = ((ctl_ch - lowest) / 4);

	/* sb must be a index to a 20MHz channel in range */
	if (sb >= (bw / 20)) {
		/* ctl_ch must have been too high for the center_ch */
		return -1;
	}

	return sb;
}

/* convert bandwidth from chanspec to MHz */
static uint
bw_chspec_to_mhz(chanspec_t chspec)
{
	uint bw;

	bw = (chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT;
	return (bw >= WF_NUM_BW ? 0 : wf_chspec_bw_mhz[bw]);
}
#endif

/*
 * This function returns the channel number that control traffic is being sent on, for 20MHz
 * channels this is just the channel number, for 40MHZ, 80MHz, 160MHz channels it is the 20MHZ
 * sideband depending on the chanspec selected
 */
#if 0
uint8
wf_chspec_ctlchan(chanspec_t chspec)
{
	uint center_chan;
	uint bw_mhz;
	uint sb;

	if (wf_chspec_malformed(chspec))
		ASSERT(!wf_chspec_malformed(chspec));

	/* Is there a sideband ? */
	if (CHSPEC_IS20(chspec)) {
		return CHSPEC_CHANNEL(chspec);
	} else {
		sb = CHSPEC_CTL_SB(chspec) >> WL_CHANSPEC_CTL_SB_SHIFT;

		if (CHSPEC_IS8080(chspec)) {
			bw_mhz = 80;

			if (sb < 4) {
				center_chan = CHSPEC_CHAN1(chspec);
			}
			else {
				center_chan = CHSPEC_CHAN2(chspec);
				sb -= 4;
			}

			/* convert from channel index to channel number */
			center_chan = wf_5g_80m_chans[center_chan];
		}
		else {
			bw_mhz = bw_chspec_to_mhz(chspec);
			center_chan = CHSPEC_CHANNEL(chspec) >> WL_CHANSPEC_CHAN_SHIFT;
		}

		return (channel_to_ctl_chan(center_chan, bw_mhz, sb));
	}
}
/* return index of 80MHz channel from channel number
 * return -1 on error
 */
static int
channel_80mhz_to_id(uint ch)
{
	uint i;
	for (i = 0; i < WF_NUM_5G_80M_CHANS; i ++) {
		if (ch == wf_5g_80m_chans[i])
			return i;
	}

	return -1;
}

static int
read_uint(const char **p, unsigned int *num)
{
	unsigned long val;
	char *endp = NULL;

	val = strtoul(*p, &endp, 10);
	/* if endp is the initial pointer value, then a number was not read */
	if (endp == *p)
		return 0;

	/* advance the buffer pointer to the end of the integer string */
	*p = endp;
	/* return the parsed integer */
	*num = (unsigned int)val;

	return 1;
}
#endif

/* given a chanspec and a string buffer, format the chanspec as a
 * string, and return the original pointer a.
 * Min buffer length must be CHANSPEC_STR_LEN.
 * On error return NULL
 */
#if 0
char *
wf_chspec_ntoa(chanspec_t chspec, char *buf)
{
	const char *band;
	uint ctl_chan;

	if (wf_chspec_malformed(chspec))
		return NULL;

	band = "";

	/* check for non-default band spec */
	if ((CHSPEC_IS2G(chspec) && CHSPEC_CHANNEL(chspec) > CH_MAX_2G_CHANNEL) ||
	    (CHSPEC_IS5G(chspec) && CHSPEC_CHANNEL(chspec) <= CH_MAX_2G_CHANNEL))
		band = (CHSPEC_IS2G(chspec)) ? "2g" : "5g";

	/* ctl channel */
	ctl_chan = wf_chspec_ctlchan(chspec);

	/* bandwidth and ctl sideband */
	if (CHSPEC_IS20(chspec)) {
		snprintf(buf, CHANSPEC_STR_LEN, "%s%d", band, ctl_chan);
	} else if (!CHSPEC_IS8080(chspec)) {
		const char *bw;
		const char *sb = "";

		bw = wf_chspec_bw_str[(chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT];

#ifdef CHANSPEC_NEW_40MHZ_FORMAT
		/* ctl sideband string if needed for 2g 40MHz */
		if (CHSPEC_IS40(chspec) && CHSPEC_IS2G(chspec)) {
			sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
		}

		snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s%s", band, ctl_chan, bw, sb);
#else
		/* ctl sideband string instead of BW for 40MHz */
		if (CHSPEC_IS40(chspec)) {
			sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
			snprintf(buf, CHANSPEC_STR_LEN, "%s%d%s", band, ctl_chan, sb);
		} else {
			snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s", band, ctl_chan, bw);
		}
#endif /* CHANSPEC_NEW_40MHZ_FORMAT */

	} else {
		/* 80+80 */
		uint chan1 = (chspec & WL_CHANSPEC_CHAN1_MASK) >> WL_CHANSPEC_CHAN1_SHIFT;
		uint chan2 = (chspec & WL_CHANSPEC_CHAN2_MASK) >> WL_CHANSPEC_CHAN2_SHIFT;

		/* convert to channel number */
		chan1 = (chan1 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan1] : 0;
		chan2 = (chan2 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan2] : 0;

		/* Outputs a max of CHANSPEC_STR_LEN chars including '\0'  */
		snprintf(buf, CHANSPEC_STR_LEN, "%d/80+80/%d-%d", ctl_chan, chan1, chan2);
	}

	return (buf);
}
#endif
/* given a chanspec string, convert to a chanspec.
 * On error return 0
 */
#if 0
chanspec_t
wf_chspec_aton(const char *a)
{
	chanspec_t chspec;
	uint chspec_ch, chspec_band, bw, chspec_bw, chspec_sb;
	uint num, ctl_ch;
	uint ch1, ch2;
	char c, sb_ul = '\0';
	int i;

	bw = 20;
	chspec_sb = 0;
	chspec_ch = ch1 = ch2 = 0;

	/* parse channel num or band */
	if (!read_uint(&a, &num))
		return 0;

	/* if we are looking at a 'g', then the first number was a band */
	c = tolower(a[0]);
	if (c == 'g') {
		a ++; /* consume the char */

		/* band must be "2" or "5" */
		if (num == 2)
			chspec_band = WL_CHANSPEC_BAND_2G;
		else if (num == 5)
			chspec_band = WL_CHANSPEC_BAND_5G;
		else
			return 0;

		/* read the channel number */
		if (!read_uint(&a, &ctl_ch))
			return 0;

		c = tolower(a[0]);
	}
	else {
		/* first number is channel, use default for band */
		ctl_ch = num;
		chspec_band = ((ctl_ch <= CH_MAX_2G_CHANNEL) ?
		               WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G);
	}

	if (c == '\0') {
		/* default BW of 20MHz */
		chspec_bw = WL_CHANSPEC_BW_20;
		goto done_read;
	}

	a ++; /* consume the 'u','l', or '/' */

	/* check 'u'/'l' */
	if (c == 'u' || c == 'l') {
		sb_ul = c;
		chspec_bw = WL_CHANSPEC_BW_40;
		goto done_read;
	}

	/* next letter must be '/' */
	if (c != '/')
		return 0;

	/* read bandwidth */
	if (!read_uint(&a, &bw))
		return 0;

	/* convert to chspec value */
	if (bw == 20) {
		chspec_bw = WL_CHANSPEC_BW_20;
	} else if (bw == 40) {
		chspec_bw = WL_CHANSPEC_BW_40;
	} else if (bw == 80) {
		chspec_bw = WL_CHANSPEC_BW_80;
	} else if (bw == 160) {
		chspec_bw = WL_CHANSPEC_BW_160;
	} else {
		return 0;
	}

	/* So far we have <band>g<chan>/<bw>
	 * Can now be followed by u/l if bw = 40,
	 * or '+80' if bw = 80, to make '80+80' bw.
	 */

	c = tolower(a[0]);

	/* if we have a 2g/40 channel, we should have a l/u spec now */
	if (chspec_band == WL_CHANSPEC_BAND_2G && bw == 40) {
		if (c == 'u' || c == 'l') {
			a ++; /* consume the u/l char */
			sb_ul = c;
			goto done_read;
		}
	}

	/* check for 80+80 */
	if (c == '+') {
		/* 80+80 */
		static const char *plus80 = "80/";

		/* must be looking at '+80/'
 		 * check and consume this string.
 		 */
		chspec_bw = WL_CHANSPEC_BW_8080;

		a ++; /* consume the char '+' */

		/* consume the '80/' string */
		for (i = 0; i < 3; i++) {
			if (*a++ != *plus80++) {
				return 0;
			}
		}

		/* read primary 80MHz channel */
		if (!read_uint(&a, &ch1))
			return 0;

		/* must followed by '-' */
		if (a[0] != '-')
			return 0;
		a ++; /* consume the char */

		/* read secondary 80MHz channel */
		if (!read_uint(&a, &ch2))
			return 0;
	}

done_read:
	/* skip trailing white space */
	while (a[0] == ' ') {
		a ++;
	}

	/* must be end of string */
	if (a[0] != '\0')
		return 0;

	/* Now have all the chanspec string parts read;
 	 * chspec_band, ctl_ch, chspec_bw, sb_ul, ch1, ch2.
 	 * chspec_band and chspec_bw are chanspec values.
	 * Need to convert ctl_ch, sb_ul, and ch1,ch2 into
	 * a center channel (or two) and sideband.
 	 */

	/* if a sb u/l string was given, just use that,
	 * guaranteed to be bw = 40 by sting parse.
	 */
	if (sb_ul != '\0') {
		if (sb_ul == 'l') {
			chspec_ch = UPPER_20_SB(ctl_ch);
			chspec_sb = WL_CHANSPEC_CTL_SB_LLL;
		} else if (sb_ul == 'u') {
			chspec_ch = LOWER_20_SB(ctl_ch);
			chspec_sb = WL_CHANSPEC_CTL_SB_LLU;
		}
	}
	/* if the bw is 20, center and sideband are trivial */
	else if (chspec_bw == WL_CHANSPEC_BW_20) {
		chspec_ch = ctl_ch;
		chspec_sb = 0;
	}
	/* if the bw is 40/80/160, not 80+80, a single method
	 * can be used to to find the center and sideband
	 */
	else if (chspec_bw != WL_CHANSPEC_BW_8080) {
		/* figure out ctl sideband based on ctl channel and bandwidth */
		const uint8 *center_ch = NULL;
		int num_ch = 0;
		int sb = -1;

		if (chspec_bw == WL_CHANSPEC_BW_40) {
			center_ch = wf_5g_40m_chans;
			num_ch = WF_NUM_5G_40M_CHANS;
		} else if (chspec_bw == WL_CHANSPEC_BW_80) {
			center_ch = wf_5g_80m_chans;
			num_ch = WF_NUM_5G_80M_CHANS;
		} else if (chspec_bw == WL_CHANSPEC_BW_160) {
			center_ch = wf_5g_160m_chans;
			num_ch = WF_NUM_5G_160M_CHANS;
		} else {
			return 0;
		}

		for (i = 0; i < num_ch; i ++) {
			sb = channel_to_sb(center_ch[i], ctl_ch, bw);
			if (sb >= 0) {
				chspec_ch = center_ch[i];
				chspec_sb = sb << WL_CHANSPEC_CTL_SB_SHIFT;
				break;
			}
		}

		/* check for no matching sb/center */
		if (sb < 0) {
			return 0;
		}
	}
	/* Otherwise, bw is 80+80. Figure out channel pair and sb */
	else {
		int ch1_id = 0, ch2_id = 0;
		int sb;

		ch1_id = channel_80mhz_to_id(ch1);
		ch2_id = channel_80mhz_to_id(ch2);

		/* validate channels */
		if (ch1 >= ch2 || ch1_id < 0 || ch2_id < 0)
			return 0;

		/* combined channel in chspec */
		chspec_ch = (((uint16)ch1_id << WL_CHANSPEC_CHAN1_SHIFT) |
			((uint16)ch2_id << WL_CHANSPEC_CHAN2_SHIFT));

		/* figure out ctl sideband */

		/* does the primary channel fit with the 1st 80MHz channel ? */
		sb = channel_to_sb(ch1, ctl_ch, bw);
		if (sb < 0) {
			/* no, so does the primary channel fit with the 2nd 80MHz channel ? */
			sb = channel_to_sb(ch2, ctl_ch, bw);
			if (sb < 0) {
				/* no match for ctl_ch to either 80MHz center channel */
				return 0;
			}
			/* sb index is 0-3 for the low 80MHz channel, and 4-7 for
			 * the high 80MHz channel. Add 4 to to shift to high set.
			 */
			sb += 4;
		}

		chspec_sb = sb << WL_CHANSPEC_CTL_SB_SHIFT;
	}

	chspec = (chspec_ch | chspec_band | chspec_bw | chspec_sb);

	if (wf_chspec_malformed(chspec))
		return 0;

	return chspec;
}
#endif
/*
 * Verify the chanspec is using a legal set of parameters, i.e. that the
 * chanspec specified a band, bw, ctl_sb and channel and that the
 * combination could be legal given any set of circumstances.
 * RETURNS: TRUE is the chanspec is malformed, false if it looks good.
 */
#if 0
bool
wf_chspec_malformed(chanspec_t chanspec)
{
	uint chspec_bw = CHSPEC_BW(chanspec);
	uint chspec_ch = CHSPEC_CHANNEL(chanspec);

	/* must be 2G or 5G band */
	if (CHSPEC_IS2G(chanspec)) {
		/* must be valid bandwidth */
		if (chspec_bw != WL_CHANSPEC_BW_20 &&
		    chspec_bw != WL_CHANSPEC_BW_40) {
			return TRUE;
		}
	} else if (CHSPEC_IS5G(chanspec)) {
		if (chspec_bw == WL_CHANSPEC_BW_8080) {
			uint ch1_id, ch2_id;

			/* channel number in 80+80 must be in range */
			ch1_id = CHSPEC_CHAN1(chanspec);
			ch2_id = CHSPEC_CHAN2(chanspec);
			if (ch1_id >= WF_NUM_5G_80M_CHANS || ch2_id >= WF_NUM_5G_80M_CHANS)
				return TRUE;

			/* ch2 must be above ch1 for the chanspec */
			if (ch2_id <= ch1_id)
				return TRUE;
		} else if (chspec_bw == WL_CHANSPEC_BW_20 || chspec_bw == WL_CHANSPEC_BW_40 ||
		           chspec_bw == WL_CHANSPEC_BW_80 || chspec_bw == WL_CHANSPEC_BW_160) {

			if (chspec_ch > MAXCHANNEL) {
				return TRUE;
			}
		} else {
			/* invalid bandwidth */
			return TRUE;
		}
	} else {
		/* must be 2G or 5G band */
		return TRUE;
	}

	/* side band needs to be consistent with bandwidth */
	if (chspec_bw == WL_CHANSPEC_BW_20) {
		if (CHSPEC_CTL_SB(chanspec) != WL_CHANSPEC_CTL_SB_LLL)
			return TRUE;
	} else if (chspec_bw == WL_CHANSPEC_BW_40) {
		if (CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_LLU)
			return TRUE;
	} else if (chspec_bw == WL_CHANSPEC_BW_80) {
		if (CHSPEC_CTL_SB(chanspec) > WL_CHANSPEC_CTL_SB_LUU)
			return TRUE;
	}

	return FALSE;
}
#endif
chanspec_t
select_80M_chspec(chanspec_t chanspec)
{
	uint ctl_ch;
	char buf[32];
	const uint8 *ctl_chs = wf_5g_80m_ctl_chans;
	int num_ch = WF_NUM_5G_80M_CTL_CHANS;
	int i, found = 0;
	chanspec_t chspec;

	if (CHSPEC_IS5G(chanspec) && CHSPEC_IS40(chanspec))
	{
		ctl_ch = wf_chspec_ctlchan(chanspec);

		for (i = 0; i < num_ch; i ++) {
			if (ctl_ch == ctl_chs[i])
			{
				found = 1;
				break;
			}
		}

		if (!found)
			return 0;

		sprintf(buf, "%d/80", ctl_ch);
		chspec = wf_chspec_aton(buf);

		if (!wf_chspec_malformed(chspec))
			return chspec;
	}

	return 0;
}

int
CTLCH_IS_BAND2(chanspec_t chspec)
{               
	uint ctl_ch;
	const uint8 *ctl_chs = wf_5g_80m_ctl_chans_band2;
	int num_ch = WF_NUM_5G_80M_CTL_CHANS_BAND2;
	int i, match = 0;

	ctl_ch = wf_chspec_ctlchan(chspec);
	for (i = 0; i < num_ch; i ++) {
		if (ctl_ch == ctl_chs[i])
		{
			match = 1;
			break;
		}
	}

	return match;
}

int
CTLCH_IS_BAND4(chanspec_t chspec)
{
	uint ctl_ch;
	const uint8 *ctl_chs = wf_5g_80m_ctl_chans_band4;
	int num_ch = WF_NUM_5G_80M_CTL_CHANS_BAND4;
	int i, match = 0;

	ctl_ch = wf_chspec_ctlchan(chspec);
	for (i = 0; i < num_ch; i ++) {
		if (ctl_ch == ctl_chs[i])
		{
			match = 1;
			break;
		}
	}

	return match;
}

void logmessage(char *logheader, char *fmt, ...)
{
	va_list args;
	char buf[512];

	va_start(args, fmt);

	vsnprintf(buf, sizeof(buf), fmt, args);
	openlog(logheader, 0, 0);
	syslog(0, buf);
	closelog();
	va_end(args);
}
