#ifndef _chan_utils_h_
#define _chan_utils_h_

/* 40MHz channels in 5GHz band */
static const uint8 wf_5g_40m_chans[] =
{38, 46, 54, 62, 102, 110, 118, 126, 134, 142, 151, 159};
#define WF_NUM_5G_40M_CHANS \
	(sizeof(wf_5g_40m_chans)/sizeof(uint8))

/* 80MHz channels in 5GHz band */
static const uint8 wf_5g_80m_chans[] =
{42, 58, 106, 122, 138, 155};
#define WF_NUM_5G_80M_CHANS \
	(sizeof(wf_5g_80m_chans)/sizeof(uint8))

/* 160MHz channels in 5GHz band */
static const uint8 wf_5g_160m_chans[] =
{50, 114};
#define WF_NUM_5G_160M_CHANS \
	(sizeof(wf_5g_160m_chans)/sizeof(uint8))

static const uint8 wf_chspec_bw_mhz[] =
{5, 10, 20, 40, 80, 160, 160};

#define WF_NUM_BW \
	(sizeof(wf_chspec_bw_mhz)/sizeof(uint8))

static const uint8 wf_5g_80m_ctl_chans[] =
{36, 40, 44, 48, 149, 153, 157, 161};
#define WF_NUM_5G_80M_CTL_CHANS \
        (sizeof(wf_5g_80m_ctl_chans)/sizeof(uint8))

static const uint8 wf_5g_80m_ctl_chans_band2[] =
{36, 40, 44, 48};
#define WF_NUM_5G_80M_CTL_CHANS_BAND2 \
	(sizeof(wf_5g_80m_ctl_chans_band2)/sizeof(uint8))

static const uint8 wf_5g_80m_ctl_chans_band4[] =
{149, 153, 157, 161};
#define WF_NUM_5G_80M_CTL_CHANS_BAND4 \
	(sizeof(wf_5g_80m_ctl_chans_band4)/sizeof(uint8))

void logmessage(char *logheader, char *fmt, ...);

int CTLCH_IS_BAND2(chanspec_t chspec);

int CTLCH_IS_BAND4(chanspec_t chspec);

chanspec_t select_80M_chspec(chanspec_t chanspec);

#endif  /* _chan_utils_h_ */

