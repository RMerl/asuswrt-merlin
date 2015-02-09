/*

  Broadcom B43 wireless driver
  IEEE 802.11n PHY support

  Copyright (c) 2008 Michael Buesch <mb@bu3sch.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "b43.h"
#include "phy_n.h"
#include "tables_nphy.h"
#include "main.h"

struct nphy_txgains {
	u16 txgm[2];
	u16 pga[2];
	u16 pad[2];
	u16 ipa[2];
};

struct nphy_iqcal_params {
	u16 txgm;
	u16 pga;
	u16 pad;
	u16 ipa;
	u16 cal_gain;
	u16 ncorr[5];
};

struct nphy_iq_est {
	s32 iq0_prod;
	u32 i0_pwr;
	u32 q0_pwr;
	s32 iq1_prod;
	u32 i1_pwr;
	u32 q1_pwr;
};

enum b43_nphy_rf_sequence {
	B43_RFSEQ_RX2TX,
	B43_RFSEQ_TX2RX,
	B43_RFSEQ_RESET2RX,
	B43_RFSEQ_UPDATE_GAINH,
	B43_RFSEQ_UPDATE_GAINL,
	B43_RFSEQ_UPDATE_GAINU,
};

static void b43_nphy_set_rf_sequence(struct b43_wldev *dev, u8 cmd,
					u8 *events, u8 *delays, u8 length);
static void b43_nphy_force_rf_sequence(struct b43_wldev *dev,
				       enum b43_nphy_rf_sequence seq);
static void b43_nphy_rf_control_override(struct b43_wldev *dev, u16 field,
						u16 value, u8 core, bool off);
static void b43_nphy_rf_control_intc_override(struct b43_wldev *dev, u8 field,
						u16 value, u8 core);
static int nphy_channel_switch(struct b43_wldev *dev, unsigned int channel);

static inline bool b43_empty_chanspec(struct b43_chanspec *chanspec)
{
	return !chanspec->channel && !chanspec->sideband &&
		!chanspec->b_width && !chanspec->b_freq;
}

static inline bool b43_eq_chanspecs(struct b43_chanspec *chanspec1,
					struct b43_chanspec *chanspec2)
{
	return (chanspec1->channel == chanspec2->channel &&
		chanspec1->sideband == chanspec2->sideband &&
		chanspec1->b_width == chanspec2->b_width &&
		chanspec1->b_freq == chanspec2->b_freq);
}

void b43_nphy_set_rxantenna(struct b43_wldev *dev, int antenna)
{//TODO
}

static void b43_nphy_op_adjust_txpower(struct b43_wldev *dev)
{//TODO
}

static enum b43_txpwr_result b43_nphy_op_recalc_txpower(struct b43_wldev *dev,
							bool ignore_tssi)
{//TODO
	return B43_TXPWR_RES_DONE;
}

static void b43_chantab_radio_upload(struct b43_wldev *dev,
				const struct b43_nphy_channeltab_entry_rev2 *e)
{
	b43_radio_write(dev, B2055_PLL_REF, e->radio_pll_ref);
	b43_radio_write(dev, B2055_RF_PLLMOD0, e->radio_rf_pllmod0);
	b43_radio_write(dev, B2055_RF_PLLMOD1, e->radio_rf_pllmod1);
	b43_radio_write(dev, B2055_VCO_CAPTAIL, e->radio_vco_captail);
	b43_read32(dev, B43_MMIO_MACCTL); /* flush writes */

	b43_radio_write(dev, B2055_VCO_CAL1, e->radio_vco_cal1);
	b43_radio_write(dev, B2055_VCO_CAL2, e->radio_vco_cal2);
	b43_radio_write(dev, B2055_PLL_LFC1, e->radio_pll_lfc1);
	b43_radio_write(dev, B2055_PLL_LFR1, e->radio_pll_lfr1);
	b43_read32(dev, B43_MMIO_MACCTL); /* flush writes */

	b43_radio_write(dev, B2055_PLL_LFC2, e->radio_pll_lfc2);
	b43_radio_write(dev, B2055_LGBUF_CENBUF, e->radio_lgbuf_cenbuf);
	b43_radio_write(dev, B2055_LGEN_TUNE1, e->radio_lgen_tune1);
	b43_radio_write(dev, B2055_LGEN_TUNE2, e->radio_lgen_tune2);
	b43_read32(dev, B43_MMIO_MACCTL); /* flush writes */

	b43_radio_write(dev, B2055_C1_LGBUF_ATUNE, e->radio_c1_lgbuf_atune);
	b43_radio_write(dev, B2055_C1_LGBUF_GTUNE, e->radio_c1_lgbuf_gtune);
	b43_radio_write(dev, B2055_C1_RX_RFR1, e->radio_c1_rx_rfr1);
	b43_radio_write(dev, B2055_C1_TX_PGAPADTN, e->radio_c1_tx_pgapadtn);
	b43_read32(dev, B43_MMIO_MACCTL); /* flush writes */

	b43_radio_write(dev, B2055_C1_TX_MXBGTRIM, e->radio_c1_tx_mxbgtrim);
	b43_radio_write(dev, B2055_C2_LGBUF_ATUNE, e->radio_c2_lgbuf_atune);
	b43_radio_write(dev, B2055_C2_LGBUF_GTUNE, e->radio_c2_lgbuf_gtune);
	b43_radio_write(dev, B2055_C2_RX_RFR1, e->radio_c2_rx_rfr1);
	b43_read32(dev, B43_MMIO_MACCTL); /* flush writes */

	b43_radio_write(dev, B2055_C2_TX_PGAPADTN, e->radio_c2_tx_pgapadtn);
	b43_radio_write(dev, B2055_C2_TX_MXBGTRIM, e->radio_c2_tx_mxbgtrim);
}

static void b43_chantab_phy_upload(struct b43_wldev *dev,
				   const struct b43_phy_n_sfo_cfg *e)
{
	b43_phy_write(dev, B43_NPHY_BW1A, e->phy_bw1a);
	b43_phy_write(dev, B43_NPHY_BW2, e->phy_bw2);
	b43_phy_write(dev, B43_NPHY_BW3, e->phy_bw3);
	b43_phy_write(dev, B43_NPHY_BW4, e->phy_bw4);
	b43_phy_write(dev, B43_NPHY_BW5, e->phy_bw5);
	b43_phy_write(dev, B43_NPHY_BW6, e->phy_bw6);
}

static void b43_nphy_tx_power_fix(struct b43_wldev *dev)
{
	//TODO
}


/* http://bcm-v4.sipsolutions.net/802.11/PHY/Radio/2055Setup */
static void b43_radio_2055_setup(struct b43_wldev *dev,
				const struct b43_nphy_channeltab_entry_rev2 *e)
{
	B43_WARN_ON(dev->phy.rev >= 3);

	b43_chantab_radio_upload(dev, e);
	udelay(50);
	b43_radio_write(dev, B2055_VCO_CAL10, 0x05);
	b43_radio_write(dev, B2055_VCO_CAL10, 0x45);
	b43_read32(dev, B43_MMIO_MACCTL); /* flush writes */
	b43_radio_write(dev, B2055_VCO_CAL10, 0x65);
	udelay(300);
}

static void b43_radio_init2055_pre(struct b43_wldev *dev)
{
	b43_phy_mask(dev, B43_NPHY_RFCTL_CMD,
		     ~B43_NPHY_RFCTL_CMD_PORFORCE);
	b43_phy_set(dev, B43_NPHY_RFCTL_CMD,
		    B43_NPHY_RFCTL_CMD_CHIP0PU |
		    B43_NPHY_RFCTL_CMD_OEPORFORCE);
	b43_phy_set(dev, B43_NPHY_RFCTL_CMD,
		    B43_NPHY_RFCTL_CMD_PORFORCE);
}

static void b43_radio_init2055_post(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	struct ssb_sprom *sprom = &(dev->dev->bus->sprom);
	struct ssb_boardinfo *binfo = &(dev->dev->bus->boardinfo);
	int i;
	u16 val;
	bool workaround = false;

	if (sprom->revision < 4)
		workaround = (binfo->vendor != PCI_VENDOR_ID_BROADCOM ||
				binfo->type != 0x46D ||
				binfo->rev < 0x41);
	else
		workaround = ((sprom->boardflags_hi & B43_BFH_NOPA) == 0);

	b43_radio_mask(dev, B2055_MASTER1, 0xFFF3);
	if (workaround) {
		b43_radio_mask(dev, B2055_C1_RX_BB_REG, 0x7F);
		b43_radio_mask(dev, B2055_C2_RX_BB_REG, 0x7F);
	}
	b43_radio_maskset(dev, B2055_RRCCAL_NOPTSEL, 0xFFC0, 0x2C);
	b43_radio_write(dev, B2055_CAL_MISC, 0x3C);
	b43_radio_mask(dev, B2055_CAL_MISC, 0xFFBE);
	b43_radio_set(dev, B2055_CAL_LPOCTL, 0x80);
	b43_radio_set(dev, B2055_CAL_MISC, 0x1);
	msleep(1);
	b43_radio_set(dev, B2055_CAL_MISC, 0x40);
	for (i = 0; i < 200; i++) {
		val = b43_radio_read(dev, B2055_CAL_COUT2);
		if (val & 0x80) {
			i = 0;
			break;
		}
		udelay(10);
	}
	if (i)
		b43err(dev->wl, "radio post init timeout\n");
	b43_radio_mask(dev, B2055_CAL_LPOCTL, 0xFF7F);
	nphy_channel_switch(dev, dev->phy.channel);
	b43_radio_write(dev, B2055_C1_RX_BB_LPF, 0x9);
	b43_radio_write(dev, B2055_C2_RX_BB_LPF, 0x9);
	b43_radio_write(dev, B2055_C1_RX_BB_MIDACHP, 0x83);
	b43_radio_write(dev, B2055_C2_RX_BB_MIDACHP, 0x83);
	b43_radio_maskset(dev, B2055_C1_LNA_GAINBST, 0xFFF8, 0x6);
	b43_radio_maskset(dev, B2055_C2_LNA_GAINBST, 0xFFF8, 0x6);
	if (!nphy->gain_boost) {
		b43_radio_set(dev, B2055_C1_RX_RFSPC1, 0x2);
		b43_radio_set(dev, B2055_C2_RX_RFSPC1, 0x2);
	} else {
		b43_radio_mask(dev, B2055_C1_RX_RFSPC1, 0xFFFD);
		b43_radio_mask(dev, B2055_C2_RX_RFSPC1, 0xFFFD);
	}
	udelay(2);
}

/*
 * Initialize a Broadcom 2055 N-radio
 * http://bcm-v4.sipsolutions.net/802.11/Radio/2055/Init
 */
static void b43_radio_init2055(struct b43_wldev *dev)
{
	b43_radio_init2055_pre(dev);
	if (b43_status(dev) < B43_STAT_INITIALIZED)
		b2055_upload_inittab(dev, 0, 1);
	else
		b2055_upload_inittab(dev, 0, 0);
	b43_radio_init2055_post(dev);
}

/*
 * Initialize a Broadcom 2056 N-radio
 * http://bcm-v4.sipsolutions.net/802.11/Radio/2056/Init
 */
static void b43_radio_init2056(struct b43_wldev *dev)
{
	/* TODO */
}


/*
 * Upload the N-PHY tables.
 * http://bcm-v4.sipsolutions.net/802.11/PHY/N/InitTables
 */
static void b43_nphy_tables_init(struct b43_wldev *dev)
{
	if (dev->phy.rev < 3)
		b43_nphy_rev0_1_2_tables_init(dev);
	else
		b43_nphy_rev3plus_tables_init(dev);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/PA%20override */
static void b43_nphy_pa_override(struct b43_wldev *dev, bool enable)
{
	struct b43_phy_n *nphy = dev->phy.n;
	enum ieee80211_band band;
	u16 tmp;

	if (!enable) {
		nphy->rfctrl_intc1_save = b43_phy_read(dev,
						       B43_NPHY_RFCTL_INTC1);
		nphy->rfctrl_intc2_save = b43_phy_read(dev,
						       B43_NPHY_RFCTL_INTC2);
		band = b43_current_band(dev->wl);
		if (dev->phy.rev >= 3) {
			if (band == IEEE80211_BAND_5GHZ)
				tmp = 0x600;
			else
				tmp = 0x480;
		} else {
			if (band == IEEE80211_BAND_5GHZ)
				tmp = 0x180;
			else
				tmp = 0x120;
		}
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, tmp);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, tmp);
	} else {
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC1,
				nphy->rfctrl_intc1_save);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC2,
				nphy->rfctrl_intc2_save);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/TxLpFbw */
static void b43_nphy_tx_lp_fbw(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u16 tmp;
	enum ieee80211_band band = b43_current_band(dev->wl);
	bool ipa = (nphy->ipa2g_on && band == IEEE80211_BAND_2GHZ) ||
			(nphy->ipa5g_on && band == IEEE80211_BAND_5GHZ);

	if (dev->phy.rev >= 3) {
		if (ipa) {
			tmp = 4;
			b43_phy_write(dev, B43_NPHY_TXF_40CO_B32S2,
			      (((((tmp << 3) | tmp) << 3) | tmp) << 3) | tmp);
		}

		tmp = 1;
		b43_phy_write(dev, B43_NPHY_TXF_40CO_B1S2,
			      (((((tmp << 3) | tmp) << 3) | tmp) << 3) | tmp);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/BmacPhyClkFgc */
static void b43_nphy_bmac_clock_fgc(struct b43_wldev *dev, bool force)
{
	u32 tmslow;

	if (dev->phy.type != B43_PHYTYPE_N)
		return;

	tmslow = ssb_read32(dev->dev, SSB_TMSLOW);
	if (force)
		tmslow |= SSB_TMSLOW_FGC;
	else
		tmslow &= ~SSB_TMSLOW_FGC;
	ssb_write32(dev->dev, SSB_TMSLOW, tmslow);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/CCA */
static void b43_nphy_reset_cca(struct b43_wldev *dev)
{
	u16 bbcfg;

	b43_nphy_bmac_clock_fgc(dev, 1);
	bbcfg = b43_phy_read(dev, B43_NPHY_BBCFG);
	b43_phy_write(dev, B43_NPHY_BBCFG, bbcfg | B43_NPHY_BBCFG_RSTCCA);
	udelay(1);
	b43_phy_write(dev, B43_NPHY_BBCFG, bbcfg & ~B43_NPHY_BBCFG_RSTCCA);
	b43_nphy_bmac_clock_fgc(dev, 0);
	b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RESET2RX);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/MIMOConfig */
static void b43_nphy_update_mimo_config(struct b43_wldev *dev, s32 preamble)
{
	u16 mimocfg = b43_phy_read(dev, B43_NPHY_MIMOCFG);

	mimocfg |= B43_NPHY_MIMOCFG_AUTO;
	if (preamble == 1)
		mimocfg |= B43_NPHY_MIMOCFG_GFMIX;
	else
		mimocfg &= ~B43_NPHY_MIMOCFG_GFMIX;

	b43_phy_write(dev, B43_NPHY_MIMOCFG, mimocfg);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/Chains */
static void b43_nphy_update_txrx_chain(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	bool override = false;
	u16 chain = 0x33;

	if (nphy->txrx_chain == 0) {
		chain = 0x11;
		override = true;
	} else if (nphy->txrx_chain == 1) {
		chain = 0x22;
		override = true;
	}

	b43_phy_maskset(dev, B43_NPHY_RFSEQCA,
			~(B43_NPHY_RFSEQCA_TXEN | B43_NPHY_RFSEQCA_RXEN),
			chain);

	if (override)
		b43_phy_set(dev, B43_NPHY_RFSEQMODE,
				B43_NPHY_RFSEQMODE_CAOVER);
	else
		b43_phy_mask(dev, B43_NPHY_RFSEQMODE,
				~B43_NPHY_RFSEQMODE_CAOVER);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RxIqEst */
static void b43_nphy_rx_iq_est(struct b43_wldev *dev, struct nphy_iq_est *est,
				u16 samps, u8 time, bool wait)
{
	int i;
	u16 tmp;

	b43_phy_write(dev, B43_NPHY_IQEST_SAMCNT, samps);
	b43_phy_maskset(dev, B43_NPHY_IQEST_WT, ~B43_NPHY_IQEST_WT_VAL, time);
	if (wait)
		b43_phy_set(dev, B43_NPHY_IQEST_CMD, B43_NPHY_IQEST_CMD_MODE);
	else
		b43_phy_mask(dev, B43_NPHY_IQEST_CMD, ~B43_NPHY_IQEST_CMD_MODE);

	b43_phy_set(dev, B43_NPHY_IQEST_CMD, B43_NPHY_IQEST_CMD_START);

	for (i = 1000; i; i--) {
		tmp = b43_phy_read(dev, B43_NPHY_IQEST_CMD);
		if (!(tmp & B43_NPHY_IQEST_CMD_START)) {
			est->i0_pwr = (b43_phy_read(dev, B43_NPHY_IQEST_IPACC_HI0) << 16) |
					b43_phy_read(dev, B43_NPHY_IQEST_IPACC_LO0);
			est->q0_pwr = (b43_phy_read(dev, B43_NPHY_IQEST_QPACC_HI0) << 16) |
					b43_phy_read(dev, B43_NPHY_IQEST_QPACC_LO0);
			est->iq0_prod = (b43_phy_read(dev, B43_NPHY_IQEST_IQACC_HI0) << 16) |
					b43_phy_read(dev, B43_NPHY_IQEST_IQACC_LO0);

			est->i1_pwr = (b43_phy_read(dev, B43_NPHY_IQEST_IPACC_HI1) << 16) |
					b43_phy_read(dev, B43_NPHY_IQEST_IPACC_LO1);
			est->q1_pwr = (b43_phy_read(dev, B43_NPHY_IQEST_QPACC_HI1) << 16) |
					b43_phy_read(dev, B43_NPHY_IQEST_QPACC_LO1);
			est->iq1_prod = (b43_phy_read(dev, B43_NPHY_IQEST_IQACC_HI1) << 16) |
					b43_phy_read(dev, B43_NPHY_IQEST_IQACC_LO1);
			return;
		}
		udelay(10);
	}
	memset(est, 0, sizeof(*est));
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RxIqCoeffs */
static void b43_nphy_rx_iq_coeffs(struct b43_wldev *dev, bool write,
					struct b43_phy_n_iq_comp *pcomp)
{
	if (write) {
		b43_phy_write(dev, B43_NPHY_C1_RXIQ_COMPA0, pcomp->a0);
		b43_phy_write(dev, B43_NPHY_C1_RXIQ_COMPB0, pcomp->b0);
		b43_phy_write(dev, B43_NPHY_C2_RXIQ_COMPA1, pcomp->a1);
		b43_phy_write(dev, B43_NPHY_C2_RXIQ_COMPB1, pcomp->b1);
	} else {
		pcomp->a0 = b43_phy_read(dev, B43_NPHY_C1_RXIQ_COMPA0);
		pcomp->b0 = b43_phy_read(dev, B43_NPHY_C1_RXIQ_COMPB0);
		pcomp->a1 = b43_phy_read(dev, B43_NPHY_C2_RXIQ_COMPA1);
		pcomp->b1 = b43_phy_read(dev, B43_NPHY_C2_RXIQ_COMPB1);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RxCalPhyCleanup */
static void b43_nphy_rx_cal_phy_cleanup(struct b43_wldev *dev, u8 core)
{
	u16 *regs = dev->phy.n->tx_rx_cal_phy_saveregs;

	b43_phy_write(dev, B43_NPHY_RFSEQCA, regs[0]);
	if (core == 0) {
		b43_phy_write(dev, B43_NPHY_AFECTL_C1, regs[1]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER1, regs[2]);
	} else {
		b43_phy_write(dev, B43_NPHY_AFECTL_C2, regs[1]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, regs[2]);
	}
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, regs[3]);
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, regs[4]);
	b43_phy_write(dev, B43_NPHY_RFCTL_RSSIO1, regs[5]);
	b43_phy_write(dev, B43_NPHY_RFCTL_RSSIO2, regs[6]);
	b43_phy_write(dev, B43_NPHY_TXF_40CO_B1S1, regs[7]);
	b43_phy_write(dev, B43_NPHY_RFCTL_OVER, regs[8]);
	b43_phy_write(dev, B43_NPHY_PAPD_EN0, regs[9]);
	b43_phy_write(dev, B43_NPHY_PAPD_EN1, regs[10]);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RxCalPhySetup */
static void b43_nphy_rx_cal_phy_setup(struct b43_wldev *dev, u8 core)
{
	u8 rxval, txval;
	u16 *regs = dev->phy.n->tx_rx_cal_phy_saveregs;

	regs[0] = b43_phy_read(dev, B43_NPHY_RFSEQCA);
	if (core == 0) {
		regs[1] = b43_phy_read(dev, B43_NPHY_AFECTL_C1);
		regs[2] = b43_phy_read(dev, B43_NPHY_AFECTL_OVER1);
	} else {
		regs[1] = b43_phy_read(dev, B43_NPHY_AFECTL_C2);
		regs[2] = b43_phy_read(dev, B43_NPHY_AFECTL_OVER);
	}
	regs[3] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC1);
	regs[4] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC2);
	regs[5] = b43_phy_read(dev, B43_NPHY_RFCTL_RSSIO1);
	regs[6] = b43_phy_read(dev, B43_NPHY_RFCTL_RSSIO2);
	regs[7] = b43_phy_read(dev, B43_NPHY_TXF_40CO_B1S1);
	regs[8] = b43_phy_read(dev, B43_NPHY_RFCTL_OVER);
	regs[9] = b43_phy_read(dev, B43_NPHY_PAPD_EN0);
	regs[10] = b43_phy_read(dev, B43_NPHY_PAPD_EN1);

	b43_phy_mask(dev, B43_NPHY_PAPD_EN0, ~0x0001);
	b43_phy_mask(dev, B43_NPHY_PAPD_EN1, ~0x0001);

	b43_phy_maskset(dev, B43_NPHY_RFSEQCA,
			~B43_NPHY_RFSEQCA_RXDIS & 0xFFFF,
			((1 - core) << B43_NPHY_RFSEQCA_RXDIS_SHIFT));
	b43_phy_maskset(dev, B43_NPHY_RFSEQCA, ~B43_NPHY_RFSEQCA_TXEN,
			((1 - core) << B43_NPHY_RFSEQCA_TXEN_SHIFT));
	b43_phy_maskset(dev, B43_NPHY_RFSEQCA, ~B43_NPHY_RFSEQCA_RXEN,
			(core << B43_NPHY_RFSEQCA_RXEN_SHIFT));
	b43_phy_maskset(dev, B43_NPHY_RFSEQCA, ~B43_NPHY_RFSEQCA_TXDIS,
			(core << B43_NPHY_RFSEQCA_TXDIS_SHIFT));

	if (core == 0) {
		b43_phy_mask(dev, B43_NPHY_AFECTL_C1, ~0x0007);
		b43_phy_set(dev, B43_NPHY_AFECTL_OVER1, 0x0007);
	} else {
		b43_phy_mask(dev, B43_NPHY_AFECTL_C2, ~0x0007);
		b43_phy_set(dev, B43_NPHY_AFECTL_OVER, 0x0007);
	}

	b43_nphy_rf_control_intc_override(dev, 2, 0, 3);
	b43_nphy_rf_control_override(dev, 8, 0, 3, false);
	b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RX2TX);

	if (core == 0) {
		rxval = 1;
		txval = 8;
	} else {
		rxval = 4;
		txval = 2;
	}
	b43_nphy_rf_control_intc_override(dev, 1, rxval, (core + 1));
	b43_nphy_rf_control_intc_override(dev, 1, txval, (2 - core));
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/CalcRxIqComp */
static void b43_nphy_calc_rx_iq_comp(struct b43_wldev *dev, u8 mask)
{
	int i;
	s32 iq;
	u32 ii;
	u32 qq;
	int iq_nbits, qq_nbits;
	int arsh, brsh;
	u16 tmp, a, b;

	struct nphy_iq_est est;
	struct b43_phy_n_iq_comp old;
	struct b43_phy_n_iq_comp new = { };
	bool error = false;

	if (mask == 0)
		return;

	b43_nphy_rx_iq_coeffs(dev, false, &old);
	b43_nphy_rx_iq_coeffs(dev, true, &new);
	b43_nphy_rx_iq_est(dev, &est, 0x4000, 32, false);
	new = old;

	for (i = 0; i < 2; i++) {
		if (i == 0 && (mask & 1)) {
			iq = est.iq0_prod;
			ii = est.i0_pwr;
			qq = est.q0_pwr;
		} else if (i == 1 && (mask & 2)) {
			iq = est.iq1_prod;
			ii = est.i1_pwr;
			qq = est.q1_pwr;
		} else {
			B43_WARN_ON(1);
			continue;
		}

		if (ii + qq < 2) {
			error = true;
			break;
		}

		iq_nbits = fls(abs(iq));
		qq_nbits = fls(qq);

		arsh = iq_nbits - 20;
		if (arsh >= 0) {
			a = -((iq << (30 - iq_nbits)) + (ii >> (1 + arsh)));
			tmp = ii >> arsh;
		} else {
			a = -((iq << (30 - iq_nbits)) + (ii << (-1 - arsh)));
			tmp = ii << -arsh;
		}
		if (tmp == 0) {
			error = true;
			break;
		}
		a /= tmp;

		brsh = qq_nbits - 11;
		if (brsh >= 0) {
			b = (qq << (31 - qq_nbits));
			tmp = ii >> brsh;
		} else {
			b = (qq << (31 - qq_nbits));
			tmp = ii << -brsh;
		}
		if (tmp == 0) {
			error = true;
			break;
		}
		b = int_sqrt(b / tmp - a * a) - (1 << 10);

		if (i == 0 && (mask & 0x1)) {
			if (dev->phy.rev >= 3) {
				new.a0 = a & 0x3FF;
				new.b0 = b & 0x3FF;
			} else {
				new.a0 = b & 0x3FF;
				new.b0 = a & 0x3FF;
			}
		} else if (i == 1 && (mask & 0x2)) {
			if (dev->phy.rev >= 3) {
				new.a1 = a & 0x3FF;
				new.b1 = b & 0x3FF;
			} else {
				new.a1 = b & 0x3FF;
				new.b1 = a & 0x3FF;
			}
		}
	}

	if (error)
		new = old;

	b43_nphy_rx_iq_coeffs(dev, true, &new);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/TxIqWar */
static void b43_nphy_tx_iq_workaround(struct b43_wldev *dev)
{
	u16 array[4];
	int i;

	b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x3C50);
	for (i = 0; i < 4; i++)
		array[i] = b43_phy_read(dev, B43_NPHY_TABLE_DATALO);

	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_NPHY_TXIQW0, array[0]);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_NPHY_TXIQW1, array[1]);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_NPHY_TXIQW2, array[2]);
	b43_shm_write16(dev, B43_SHM_SHARED, B43_SHM_SH_NPHY_TXIQW3, array[3]);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/clip-detection */
static void b43_nphy_write_clip_detection(struct b43_wldev *dev, u16 *clip_st)
{
	b43_phy_write(dev, B43_NPHY_C1_CLIP1THRES, clip_st[0]);
	b43_phy_write(dev, B43_NPHY_C2_CLIP1THRES, clip_st[1]);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/clip-detection */
static void b43_nphy_read_clip_detection(struct b43_wldev *dev, u16 *clip_st)
{
	clip_st[0] = b43_phy_read(dev, B43_NPHY_C1_CLIP1THRES);
	clip_st[1] = b43_phy_read(dev, B43_NPHY_C2_CLIP1THRES);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/SuperSwitchInit */
static void b43_nphy_superswitch_init(struct b43_wldev *dev, bool init)
{
	if (dev->phy.rev >= 3) {
		if (!init)
			return;
		if (0) {
			b43_ntab_write(dev, B43_NTAB16(9, 2), 0x211);
			b43_ntab_write(dev, B43_NTAB16(9, 3), 0x222);
			b43_ntab_write(dev, B43_NTAB16(9, 8), 0x144);
			b43_ntab_write(dev, B43_NTAB16(9, 12), 0x188);
		}
	} else {
		b43_phy_write(dev, B43_NPHY_GPIO_LOOEN, 0);
		b43_phy_write(dev, B43_NPHY_GPIO_HIOEN, 0);

		ssb_chipco_gpio_control(&dev->dev->bus->chipco, 0xFC00,
					0xFC00);
		b43_write32(dev, B43_MMIO_MACCTL,
			b43_read32(dev, B43_MMIO_MACCTL) &
			~B43_MACCTL_GPOUTSMSK);
		b43_write16(dev, B43_MMIO_GPIO_MASK,
			b43_read16(dev, B43_MMIO_GPIO_MASK) | 0xFC00);
		b43_write16(dev, B43_MMIO_GPIO_CONTROL,
			b43_read16(dev, B43_MMIO_GPIO_CONTROL) & ~0xFC00);

		if (init) {
			b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_LO1, 0x2D8);
			b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_UP1, 0x301);
			b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_LO2, 0x2D8);
			b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_UP2, 0x301);
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/classifier */
static u16 b43_nphy_classifier(struct b43_wldev *dev, u16 mask, u16 val)
{
	u16 tmp;

	if (dev->dev->id.revision == 16)
		b43_mac_suspend(dev);

	tmp = b43_phy_read(dev, B43_NPHY_CLASSCTL);
	tmp &= (B43_NPHY_CLASSCTL_CCKEN | B43_NPHY_CLASSCTL_OFDMEN |
		B43_NPHY_CLASSCTL_WAITEDEN);
	tmp &= ~mask;
	tmp |= (val & mask);
	b43_phy_maskset(dev, B43_NPHY_CLASSCTL, 0xFFF8, tmp);

	if (dev->dev->id.revision == 16)
		b43_mac_enable(dev);

	return tmp;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/carriersearch */
static void b43_nphy_stay_in_carrier_search(struct b43_wldev *dev, bool enable)
{
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_n *nphy = phy->n;

	if (enable) {
		u16 clip[] = { 0xFFFF, 0xFFFF };
		if (nphy->deaf_count++ == 0) {
			nphy->classifier_state = b43_nphy_classifier(dev, 0, 0);
			b43_nphy_classifier(dev, 0x7, 0);
			b43_nphy_read_clip_detection(dev, nphy->clip_state);
			b43_nphy_write_clip_detection(dev, clip);
		}
		b43_nphy_reset_cca(dev);
	} else {
		if (--nphy->deaf_count == 0) {
			b43_nphy_classifier(dev, 0x7, nphy->classifier_state);
			b43_nphy_write_clip_detection(dev, nphy->clip_state);
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/stop-playback */
static void b43_nphy_stop_playback(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u16 tmp;

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 1);

	tmp = b43_phy_read(dev, B43_NPHY_SAMP_STAT);
	if (tmp & 0x1)
		b43_phy_set(dev, B43_NPHY_SAMP_CMD, B43_NPHY_SAMP_CMD_STOP);
	else if (tmp & 0x2)
		b43_phy_mask(dev, B43_NPHY_IQLOCAL_CMDGCTL, 0x7FFF);

	b43_phy_mask(dev, B43_NPHY_SAMP_CMD, ~0x0004);

	if (nphy->bb_mult_save & 0x80000000) {
		tmp = nphy->bb_mult_save & 0xFFFF;
		b43_ntab_write(dev, B43_NTAB16(15, 87), tmp);
		nphy->bb_mult_save = 0;
	}

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 0);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/SpurWar */
static void b43_nphy_spur_workaround(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	u8 channel = nphy->radio_chanspec.channel;
	int tone[2] = { 57, 58 };
	u32 noise[2] = { 0x3FF, 0x3FF };

	B43_WARN_ON(dev->phy.rev < 3);

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 1);

	if (nphy->gband_spurwar_en) {
		/* TODO: N PHY Adjust Analog Pfbw (7) */
		if (channel == 11 && dev->phy.is_40mhz)
			; /* TODO: N PHY Adjust Min Noise Var(2, tone, noise)*/
		else
			; /* TODO: N PHY Adjust Min Noise Var(0, NULL, NULL)*/
		/* TODO: N PHY Adjust CRS Min Power (0x1E) */
	}

	if (nphy->aband_spurwar_en) {
		if (channel == 54) {
			tone[0] = 0x20;
			noise[0] = 0x25F;
		} else if (channel == 38 || channel == 102 || channel == 118) {
			if (0) {
				tone[0] = 0x20;
				noise[0] = 0x21F;
			} else {
				tone[0] = 0;
				noise[0] = 0;
			}
		} else if (channel == 134) {
			tone[0] = 0x20;
			noise[0] = 0x21F;
		} else if (channel == 151) {
			tone[0] = 0x10;
			noise[0] = 0x23F;
		} else if (channel == 153 || channel == 161) {
			tone[0] = 0x30;
			noise[0] = 0x23F;
		} else {
			tone[0] = 0;
			noise[0] = 0;
		}

		if (!tone[0] && !noise[0])
			; /* TODO: N PHY Adjust Min Noise Var(1, tone, noise)*/
		else
			; /* TODO: N PHY Adjust Min Noise Var(0, NULL, NULL)*/
	}

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 0);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/AdjustLnaGainTbl */
static void b43_nphy_adjust_lna_gain_table(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	u8 i;
	s16 tmp;
	u16 data[4];
	s16 gain[2];
	u16 minmax[2];
	u16 lna_gain[4] = { -2, 10, 19, 25 };

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 1);

	if (nphy->gain_boost) {
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
			gain[0] = 6;
			gain[1] = 6;
		} else {
			tmp = 40370 - 315 * nphy->radio_chanspec.channel;
			gain[0] = ((tmp >> 13) + ((tmp >> 12) & 1));
			tmp = 23242 - 224 * nphy->radio_chanspec.channel;
			gain[1] = ((tmp >> 13) + ((tmp >> 12) & 1));
		}
	} else {
		gain[0] = 0;
		gain[1] = 0;
	}

	for (i = 0; i < 2; i++) {
		if (nphy->elna_gain_config) {
			data[0] = 19 + gain[i];
			data[1] = 25 + gain[i];
			data[2] = 25 + gain[i];
			data[3] = 25 + gain[i];
		} else {
			data[0] = lna_gain[0] + gain[i];
			data[1] = lna_gain[1] + gain[i];
			data[2] = lna_gain[2] + gain[i];
			data[3] = lna_gain[3] + gain[i];
		}
		b43_ntab_write_bulk(dev, B43_NTAB16(10, 8), 4, data);

		minmax[i] = 23 + gain[i];
	}

	b43_phy_maskset(dev, B43_NPHY_C1_MINMAX_GAIN, ~B43_NPHY_C1_MINGAIN,
				minmax[0] << B43_NPHY_C1_MINGAIN_SHIFT);
	b43_phy_maskset(dev, B43_NPHY_C2_MINMAX_GAIN, ~B43_NPHY_C2_MINGAIN,
				minmax[1] << B43_NPHY_C2_MINGAIN_SHIFT);

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 0);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/WorkaroundsGainCtrl */
static void b43_nphy_gain_crtl_workarounds(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u8 i, j;
	u8 code;

	/* TODO: for PHY >= 3
	s8 *lna1_gain, *lna2_gain;
	u8 *gain_db, *gain_bits;
	u16 *rfseq_init;
	u8 lpf_gain[6] = { 0x00, 0x06, 0x0C, 0x12, 0x12, 0x12 };
	u8 lpf_bits[6] = { 0, 1, 2, 3, 3, 3 };
	*/

	u8 rfseq_events[3] = { 6, 8, 7 };
	u8 rfseq_delays[3] = { 10, 30, 1 };

	if (dev->phy.rev >= 3) {
		/* TODO */
	} else {
		/* Set Clip 2 detect */
		b43_phy_set(dev, B43_NPHY_C1_CGAINI,
				B43_NPHY_C1_CGAINI_CL2DETECT);
		b43_phy_set(dev, B43_NPHY_C2_CGAINI,
				B43_NPHY_C2_CGAINI_CL2DETECT);

		/* Set narrowband clip threshold */
		b43_phy_set(dev, B43_NPHY_C1_NBCLIPTHRES, 0x84);
		b43_phy_set(dev, B43_NPHY_C2_NBCLIPTHRES, 0x84);

		if (!dev->phy.is_40mhz) {
			/* Set dwell lengths */
			b43_phy_set(dev, B43_NPHY_CLIP1_NBDWELL_LEN, 0x002B);
			b43_phy_set(dev, B43_NPHY_CLIP2_NBDWELL_LEN, 0x002B);
			b43_phy_set(dev, B43_NPHY_W1CLIP1_DWELL_LEN, 0x0009);
			b43_phy_set(dev, B43_NPHY_W1CLIP2_DWELL_LEN, 0x0009);
		}

		/* Set wideband clip 2 threshold */
		b43_phy_maskset(dev, B43_NPHY_C1_CLIPWBTHRES,
				~B43_NPHY_C1_CLIPWBTHRES_CLIP2,
				21);
		b43_phy_maskset(dev, B43_NPHY_C2_CLIPWBTHRES,
				~B43_NPHY_C2_CLIPWBTHRES_CLIP2,
				21);

		if (!dev->phy.is_40mhz) {
			b43_phy_maskset(dev, B43_NPHY_C1_CGAINI,
				~B43_NPHY_C1_CGAINI_GAINBKOFF, 0x1);
			b43_phy_maskset(dev, B43_NPHY_C2_CGAINI,
				~B43_NPHY_C2_CGAINI_GAINBKOFF, 0x1);
			b43_phy_maskset(dev, B43_NPHY_C1_CCK_CGAINI,
				~B43_NPHY_C1_CCK_CGAINI_GAINBKOFF, 0x1);
			b43_phy_maskset(dev, B43_NPHY_C2_CCK_CGAINI,
				~B43_NPHY_C2_CCK_CGAINI_GAINBKOFF, 0x1);
		}

		b43_phy_set(dev, B43_NPHY_CCK_SHIFTB_REF, 0x809C);

		if (nphy->gain_boost) {
			if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ &&
			    dev->phy.is_40mhz)
				code = 4;
			else
				code = 5;
		} else {
			code = dev->phy.is_40mhz ? 6 : 7;
		}

		/* Set HPVGA2 index */
		b43_phy_maskset(dev, B43_NPHY_C1_INITGAIN,
				~B43_NPHY_C1_INITGAIN_HPVGA2,
				code << B43_NPHY_C1_INITGAIN_HPVGA2_SHIFT);
		b43_phy_maskset(dev, B43_NPHY_C2_INITGAIN,
				~B43_NPHY_C2_INITGAIN_HPVGA2,
				code << B43_NPHY_C2_INITGAIN_HPVGA2_SHIFT);

		b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x1D06);
		b43_phy_write(dev, B43_NPHY_TABLE_DATALO,
					(code << 8 | 0x7C));
		b43_phy_write(dev, B43_NPHY_TABLE_DATALO,
					(code << 8 | 0x7C));

		b43_nphy_adjust_lna_gain_table(dev);

		if (nphy->elna_gain_config) {
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x0808);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x0);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x1);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x1);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x1);

			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x0C08);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x0);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x1);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x1);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x1);

			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x1D06);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO,
					(code << 8 | 0x74));
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO,
					(code << 8 | 0x74));
		}

		if (dev->phy.rev == 2) {
			for (i = 0; i < 4; i++) {
				b43_phy_write(dev, B43_NPHY_TABLE_ADDR,
						(0x0400 * i) + 0x0020);
				for (j = 0; j < 21; j++)
					b43_phy_write(dev,
						B43_NPHY_TABLE_DATALO, 3 * j);
			}

			b43_nphy_set_rf_sequence(dev, 5,
					rfseq_events, rfseq_delays, 3);
			b43_phy_maskset(dev, B43_NPHY_OVER_DGAIN1,
				~B43_NPHY_OVER_DGAIN_CCKDGECV & 0xFFFF,
				0x5A << B43_NPHY_OVER_DGAIN_CCKDGECV_SHIFT);

			if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
				b43_phy_maskset(dev, B43_PHY_N(0xC5D),
						0xFF80, 4);
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/Workarounds */
static void b43_nphy_workarounds(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_n *nphy = phy->n;

	u8 events1[7] = { 0x0, 0x1, 0x2, 0x8, 0x4, 0x5, 0x3 };
	u8 delays1[7] = { 0x8, 0x6, 0x6, 0x2, 0x4, 0x3C, 0x1 };

	u8 events2[7] = { 0x0, 0x3, 0x5, 0x4, 0x2, 0x1, 0x8 };
	u8 delays2[7] = { 0x8, 0x6, 0x2, 0x4, 0x4, 0x6, 0x1 };

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
		b43_nphy_classifier(dev, 1, 0);
	else
		b43_nphy_classifier(dev, 1, 1);

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 1);

	b43_phy_set(dev, B43_NPHY_IQFLIP,
		    B43_NPHY_IQFLIP_ADC1 | B43_NPHY_IQFLIP_ADC2);

	if (dev->phy.rev >= 3) {
		/* TODO */
	} else {
		if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ &&
		    nphy->band5g_pwrgain) {
			b43_radio_mask(dev, B2055_C1_TX_RF_SPARE, ~0x8);
			b43_radio_mask(dev, B2055_C2_TX_RF_SPARE, ~0x8);
		} else {
			b43_radio_set(dev, B2055_C1_TX_RF_SPARE, 0x8);
			b43_radio_set(dev, B2055_C2_TX_RF_SPARE, 0x8);
		}

		/* TODO: convert to b43_ntab_write? */
		b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2000);
		b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x000A);
		b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2010);
		b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x000A);
		b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2002);
		b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0xCDAA);
		b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2012);
		b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0xCDAA);

		if (dev->phy.rev < 2) {
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2008);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x0000);
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2018);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x0000);
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2007);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x7AAB);
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2017);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x7AAB);
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2006);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x0800);
			b43_phy_write(dev, B43_NPHY_TABLE_ADDR, 0x2016);
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO, 0x0800);
		}

		b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_LO1, 0x2D8);
		b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_UP1, 0x301);
		b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_LO2, 0x2D8);
		b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_UP2, 0x301);

		if (bus->sprom.boardflags2_lo & 0x100 &&
		    bus->boardinfo.type == 0x8B) {
			delays1[0] = 0x1;
			delays1[5] = 0x14;
		}
		b43_nphy_set_rf_sequence(dev, 0, events1, delays1, 7);
		b43_nphy_set_rf_sequence(dev, 1, events2, delays2, 7);

		b43_nphy_gain_crtl_workarounds(dev);

		if (dev->phy.rev < 2) {
			if (b43_phy_read(dev, B43_NPHY_RXCTL) & 0x2)
				; /*TODO: b43_mhf(dev, 2, 0x0010, 0x0010, 3);*/
		} else if (dev->phy.rev == 2) {
			b43_phy_write(dev, B43_NPHY_CRSCHECK2, 0);
			b43_phy_write(dev, B43_NPHY_CRSCHECK3, 0);
		}

		if (dev->phy.rev < 2)
			b43_phy_mask(dev, B43_NPHY_SCRAM_SIGCTL,
					~B43_NPHY_SCRAM_SIGCTL_SCM);

		/* Set phase track alpha and beta */
		b43_phy_write(dev, B43_NPHY_PHASETR_A0, 0x125);
		b43_phy_write(dev, B43_NPHY_PHASETR_A1, 0x1B3);
		b43_phy_write(dev, B43_NPHY_PHASETR_A2, 0x105);
		b43_phy_write(dev, B43_NPHY_PHASETR_B0, 0x16E);
		b43_phy_write(dev, B43_NPHY_PHASETR_B1, 0xCD);
		b43_phy_write(dev, B43_NPHY_PHASETR_B2, 0x20);

		b43_phy_mask(dev, B43_NPHY_PIL_DW1,
				~B43_NPHY_PIL_DW_64QAM & 0xFFFF);
		b43_phy_write(dev, B43_NPHY_TXF_20CO_S2B1, 0xB5);
		b43_phy_write(dev, B43_NPHY_TXF_20CO_S2B2, 0xA4);
		b43_phy_write(dev, B43_NPHY_TXF_20CO_S2B3, 0x00);

		if (dev->phy.rev == 2)
			b43_phy_set(dev, B43_NPHY_FINERX2_CGC,
					B43_NPHY_FINERX2_CGC_DECGC);
	}

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 0);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/LoadSampleTable */
static int b43_nphy_load_samples(struct b43_wldev *dev,
					struct b43_c32 *samples, u16 len) {
	struct b43_phy_n *nphy = dev->phy.n;
	u16 i;
	u32 *data;

	data = kzalloc(len * sizeof(u32), GFP_KERNEL);
	if (!data) {
		b43err(dev->wl, "allocation for samples loading failed\n");
		return -ENOMEM;
	}
	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 1);

	for (i = 0; i < len; i++) {
		data[i] = (samples[i].i & 0x3FF << 10);
		data[i] |= samples[i].q & 0x3FF;
	}
	b43_ntab_write_bulk(dev, B43_NTAB32(17, 0), len, data);

	kfree(data);
	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 0);
	return 0;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/GenLoadSamples */
static u16 b43_nphy_gen_load_samples(struct b43_wldev *dev, u32 freq, u16 max,
					bool test)
{
	int i;
	u16 bw, len, rot, angle;
	struct b43_c32 *samples;


	bw = (dev->phy.is_40mhz) ? 40 : 20;
	len = bw << 3;

	if (test) {
		if (b43_phy_read(dev, B43_NPHY_BBCFG) & B43_NPHY_BBCFG_RSTRX)
			bw = 82;
		else
			bw = 80;

		if (dev->phy.is_40mhz)
			bw <<= 1;

		len = bw << 1;
	}

	samples = kzalloc(len * sizeof(struct b43_c32), GFP_KERNEL);
	if (!samples) {
		b43err(dev->wl, "allocation for samples generation failed\n");
		return 0;
	}
	rot = (((freq * 36) / bw) << 16) / 100;
	angle = 0;

	for (i = 0; i < len; i++) {
		samples[i] = b43_cordic(angle);
		angle += rot;
		samples[i].q = CORDIC_CONVERT(samples[i].q * max);
		samples[i].i = CORDIC_CONVERT(samples[i].i * max);
	}

	i = b43_nphy_load_samples(dev, samples, len);
	kfree(samples);
	return (i < 0) ? 0 : len;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RunSamples */
static void b43_nphy_run_samples(struct b43_wldev *dev, u16 samps, u16 loops,
					u16 wait, bool iqmode, bool dac_test)
{
	struct b43_phy_n *nphy = dev->phy.n;
	int i;
	u16 seq_mode;
	u32 tmp;

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, true);

	if ((nphy->bb_mult_save & 0x80000000) == 0) {
		tmp = b43_ntab_read(dev, B43_NTAB16(15, 87));
		nphy->bb_mult_save = (tmp & 0xFFFF) | 0x80000000;
	}

	if (!dev->phy.is_40mhz)
		tmp = 0x6464;
	else
		tmp = 0x4747;
	b43_ntab_write(dev, B43_NTAB16(15, 87), tmp);

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, false);

	b43_phy_write(dev, B43_NPHY_SAMP_DEPCNT, (samps - 1));

	if (loops != 0xFFFF)
		b43_phy_write(dev, B43_NPHY_SAMP_LOOPCNT, (loops - 1));
	else
		b43_phy_write(dev, B43_NPHY_SAMP_LOOPCNT, loops);

	b43_phy_write(dev, B43_NPHY_SAMP_WAITCNT, wait);

	seq_mode = b43_phy_read(dev, B43_NPHY_RFSEQMODE);

	b43_phy_set(dev, B43_NPHY_RFSEQMODE, B43_NPHY_RFSEQMODE_CAOVER);
	if (iqmode) {
		b43_phy_mask(dev, B43_NPHY_IQLOCAL_CMDGCTL, 0x7FFF);
		b43_phy_set(dev, B43_NPHY_IQLOCAL_CMDGCTL, 0x8000);
	} else {
		if (dac_test)
			b43_phy_write(dev, B43_NPHY_SAMP_CMD, 5);
		else
			b43_phy_write(dev, B43_NPHY_SAMP_CMD, 1);
	}
	for (i = 0; i < 100; i++) {
		if (b43_phy_read(dev, B43_NPHY_RFSEQST) & 1) {
			i = 0;
			break;
		}
		udelay(10);
	}
	if (i)
		b43err(dev->wl, "run samples timeout\n");

	b43_phy_write(dev, B43_NPHY_RFSEQMODE, seq_mode);
}

/*
 * Transmits a known value for LO calibration
 * http://bcm-v4.sipsolutions.net/802.11/PHY/N/TXTone
 */
static int b43_nphy_tx_tone(struct b43_wldev *dev, u32 freq, u16 max_val,
				bool iqmode, bool dac_test)
{
	u16 samp = b43_nphy_gen_load_samples(dev, freq, max_val, dac_test);
	if (samp == 0)
		return -1;
	b43_nphy_run_samples(dev, samp, 0xFFFF, 0, iqmode, dac_test);
	return 0;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/TxPwrCtrlCoefSetup */
static void b43_nphy_tx_pwr_ctrl_coef_setup(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	int i, j;
	u32 tmp;
	u32 cur_real, cur_imag, real_part, imag_part;

	u16 buffer[7];

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, true);

	b43_ntab_read_bulk(dev, B43_NTAB16(15, 80), 7, buffer);

	for (i = 0; i < 2; i++) {
		tmp = ((buffer[i * 2] & 0x3FF) << 10) |
			(buffer[i * 2 + 1] & 0x3FF);
		b43_phy_write(dev, B43_NPHY_TABLE_ADDR,
				(((i + 26) << 10) | 320));
		for (j = 0; j < 128; j++) {
			b43_phy_write(dev, B43_NPHY_TABLE_DATAHI,
					((tmp >> 16) & 0xFFFF));
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO,
					(tmp & 0xFFFF));
		}
	}

	for (i = 0; i < 2; i++) {
		tmp = buffer[5 + i];
		real_part = (tmp >> 8) & 0xFF;
		imag_part = (tmp & 0xFF);
		b43_phy_write(dev, B43_NPHY_TABLE_ADDR,
				(((i + 26) << 10) | 448));

		if (dev->phy.rev >= 3) {
			cur_real = real_part;
			cur_imag = imag_part;
			tmp = ((cur_real & 0xFF) << 8) | (cur_imag & 0xFF);
		}

		for (j = 0; j < 128; j++) {
			if (dev->phy.rev < 3) {
				cur_real = (real_part * loscale[j] + 128) >> 8;
				cur_imag = (imag_part * loscale[j] + 128) >> 8;
				tmp = ((cur_real & 0xFF) << 8) |
					(cur_imag & 0xFF);
			}
			b43_phy_write(dev, B43_NPHY_TABLE_DATAHI,
					((tmp >> 16) & 0xFFFF));
			b43_phy_write(dev, B43_NPHY_TABLE_DATALO,
					(tmp & 0xFFFF));
		}
	}

	if (dev->phy.rev >= 3) {
		b43_shm_write16(dev, B43_SHM_SHARED,
				B43_SHM_SH_NPHY_TXPWR_INDX0, 0xFFFF);
		b43_shm_write16(dev, B43_SHM_SHARED,
				B43_SHM_SH_NPHY_TXPWR_INDX1, 0xFFFF);
	}

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, false);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/SetRfSeq */
static void b43_nphy_set_rf_sequence(struct b43_wldev *dev, u8 cmd,
					u8 *events, u8 *delays, u8 length)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u8 i;
	u8 end = (dev->phy.rev >= 3) ? 0x1F : 0x0F;
	u16 offset1 = cmd << 4;
	u16 offset2 = offset1 + 0x80;

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, true);

	b43_ntab_write_bulk(dev, B43_NTAB8(7, offset1), length, events);
	b43_ntab_write_bulk(dev, B43_NTAB8(7, offset2), length, delays);

	for (i = length; i < 16; i++) {
		b43_ntab_write(dev, B43_NTAB8(7, offset1 + i), end);
		b43_ntab_write(dev, B43_NTAB8(7, offset2 + i), 1);
	}

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, false);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/ForceRFSeq */
static void b43_nphy_force_rf_sequence(struct b43_wldev *dev,
				       enum b43_nphy_rf_sequence seq)
{
	static const u16 trigger[] = {
		[B43_RFSEQ_RX2TX]		= B43_NPHY_RFSEQTR_RX2TX,
		[B43_RFSEQ_TX2RX]		= B43_NPHY_RFSEQTR_TX2RX,
		[B43_RFSEQ_RESET2RX]		= B43_NPHY_RFSEQTR_RST2RX,
		[B43_RFSEQ_UPDATE_GAINH]	= B43_NPHY_RFSEQTR_UPGH,
		[B43_RFSEQ_UPDATE_GAINL]	= B43_NPHY_RFSEQTR_UPGL,
		[B43_RFSEQ_UPDATE_GAINU]	= B43_NPHY_RFSEQTR_UPGU,
	};
	int i;
	u16 seq_mode = b43_phy_read(dev, B43_NPHY_RFSEQMODE);

	B43_WARN_ON(seq >= ARRAY_SIZE(trigger));

	b43_phy_set(dev, B43_NPHY_RFSEQMODE,
		    B43_NPHY_RFSEQMODE_CAOVER | B43_NPHY_RFSEQMODE_TROVER);
	b43_phy_set(dev, B43_NPHY_RFSEQTR, trigger[seq]);
	for (i = 0; i < 200; i++) {
		if (!(b43_phy_read(dev, B43_NPHY_RFSEQST) & trigger[seq]))
			goto ok;
		msleep(1);
	}
	b43err(dev->wl, "RF sequence status timeout\n");
ok:
	b43_phy_write(dev, B43_NPHY_RFSEQMODE, seq_mode);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RFCtrlOverride */
static void b43_nphy_rf_control_override(struct b43_wldev *dev, u16 field,
						u16 value, u8 core, bool off)
{
	int i;
	u8 index = fls(field);
	u8 addr, en_addr, val_addr;
	/* we expect only one bit set */
	B43_WARN_ON(field & (~(1 << (index - 1))));

	if (dev->phy.rev >= 3) {
		const struct nphy_rf_control_override_rev3 *rf_ctrl;
		for (i = 0; i < 2; i++) {
			if (index == 0 || index == 16) {
				b43err(dev->wl,
					"Unsupported RF Ctrl Override call\n");
				return;
			}

			rf_ctrl = &tbl_rf_control_override_rev3[index - 1];
			en_addr = B43_PHY_N((i == 0) ?
				rf_ctrl->en_addr0 : rf_ctrl->en_addr1);
			val_addr = B43_PHY_N((i == 0) ?
				rf_ctrl->val_addr0 : rf_ctrl->val_addr1);

			if (off) {
				b43_phy_mask(dev, en_addr, ~(field));
				b43_phy_mask(dev, val_addr,
						~(rf_ctrl->val_mask));
			} else {
				if (core == 0 || ((1 << core) & i) != 0) {
					b43_phy_set(dev, en_addr, field);
					b43_phy_maskset(dev, val_addr,
						~(rf_ctrl->val_mask),
						(value << rf_ctrl->val_shift));
				}
			}
		}
	} else {
		const struct nphy_rf_control_override_rev2 *rf_ctrl;
		if (off) {
			b43_phy_mask(dev, B43_NPHY_RFCTL_OVER, ~(field));
			value = 0;
		} else {
			b43_phy_set(dev, B43_NPHY_RFCTL_OVER, field);
		}

		for (i = 0; i < 2; i++) {
			if (index <= 1 || index == 16) {
				b43err(dev->wl,
					"Unsupported RF Ctrl Override call\n");
				return;
			}

			if (index == 2 || index == 10 ||
			    (index >= 13 && index <= 15)) {
				core = 1;
			}

			rf_ctrl = &tbl_rf_control_override_rev2[index - 2];
			addr = B43_PHY_N((i == 0) ?
				rf_ctrl->addr0 : rf_ctrl->addr1);

			if ((core & (1 << i)) != 0)
				b43_phy_maskset(dev, addr, ~(rf_ctrl->bmask),
						(value << rf_ctrl->shift));

			b43_phy_set(dev, B43_NPHY_RFCTL_OVER, 0x1);
			b43_phy_set(dev, B43_NPHY_RFCTL_CMD,
					B43_NPHY_RFCTL_CMD_START);
			udelay(1);
			b43_phy_mask(dev, B43_NPHY_RFCTL_OVER, 0xFFFE);
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RFCtrlIntcOverride */
static void b43_nphy_rf_control_intc_override(struct b43_wldev *dev, u8 field,
						u16 value, u8 core)
{
	u8 i, j;
	u16 reg, tmp, val;

	B43_WARN_ON(dev->phy.rev < 3);
	B43_WARN_ON(field > 4);

	for (i = 0; i < 2; i++) {
		if ((core == 1 && i == 1) || (core == 2 && !i))
			continue;

		reg = (i == 0) ?
			B43_NPHY_RFCTL_INTC1 : B43_NPHY_RFCTL_INTC2;
		b43_phy_mask(dev, reg, 0xFBFF);

		switch (field) {
		case 0:
			b43_phy_write(dev, reg, 0);
			b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RESET2RX);
			break;
		case 1:
			if (!i) {
				b43_phy_maskset(dev, B43_NPHY_RFCTL_INTC1,
						0xFC3F, (value << 6));
				b43_phy_maskset(dev, B43_NPHY_TXF_40CO_B1S1,
						0xFFFE, 1);
				b43_phy_set(dev, B43_NPHY_RFCTL_CMD,
						B43_NPHY_RFCTL_CMD_START);
				for (j = 0; j < 100; j++) {
					if (b43_phy_read(dev, B43_NPHY_RFCTL_CMD) & B43_NPHY_RFCTL_CMD_START) {
						j = 0;
						break;
					}
					udelay(10);
				}
				if (j)
					b43err(dev->wl,
						"intc override timeout\n");
				b43_phy_mask(dev, B43_NPHY_TXF_40CO_B1S1,
						0xFFFE);
			} else {
				b43_phy_maskset(dev, B43_NPHY_RFCTL_INTC2,
						0xFC3F, (value << 6));
				b43_phy_maskset(dev, B43_NPHY_RFCTL_OVER,
						0xFFFE, 1);
				b43_phy_set(dev, B43_NPHY_RFCTL_CMD,
						B43_NPHY_RFCTL_CMD_RXTX);
				for (j = 0; j < 100; j++) {
					if (b43_phy_read(dev, B43_NPHY_RFCTL_CMD) & B43_NPHY_RFCTL_CMD_RXTX) {
						j = 0;
						break;
					}
					udelay(10);
				}
				if (j)
					b43err(dev->wl,
						"intc override timeout\n");
				b43_phy_mask(dev, B43_NPHY_RFCTL_OVER,
						0xFFFE);
			}
			break;
		case 2:
			if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) {
				tmp = 0x0020;
				val = value << 5;
			} else {
				tmp = 0x0010;
				val = value << 4;
			}
			b43_phy_maskset(dev, reg, ~tmp, val);
			break;
		case 3:
			if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) {
				tmp = 0x0001;
				val = value;
			} else {
				tmp = 0x0004;
				val = value << 2;
			}
			b43_phy_maskset(dev, reg, ~tmp, val);
			break;
		case 4:
			if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) {
				tmp = 0x0002;
				val = value << 1;
			} else {
				tmp = 0x0008;
				val = value << 3;
			}
			b43_phy_maskset(dev, reg, ~tmp, val);
			break;
		}
	}
}

static void b43_nphy_bphy_init(struct b43_wldev *dev)
{
	unsigned int i;
	u16 val;

	val = 0x1E1F;
	for (i = 0; i < 14; i++) {
		b43_phy_write(dev, B43_PHY_N_BMODE(0x88 + i), val);
		val -= 0x202;
	}
	val = 0x3E3F;
	for (i = 0; i < 16; i++) {
		b43_phy_write(dev, B43_PHY_N_BMODE(0x97 + i), val);
		val -= 0x202;
	}
	b43_phy_write(dev, B43_PHY_N_BMODE(0x38), 0x668);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/ScaleOffsetRssi */
static void b43_nphy_scale_offset_rssi(struct b43_wldev *dev, u16 scale,
				       s8 offset, u8 core, u8 rail, u8 type)
{
	u16 tmp;
	bool core1or5 = (core == 1) || (core == 5);
	bool core2or5 = (core == 2) || (core == 5);

	offset = clamp_val(offset, -32, 31);
	tmp = ((scale & 0x3F) << 8) | (offset & 0x3F);

	if (core1or5 && (rail == 0) && (type == 2))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0I_RSSI_Z, tmp);
	if (core1or5 && (rail == 1) && (type == 2))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_RSSI_Z, tmp);
	if (core2or5 && (rail == 0) && (type == 2))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1I_RSSI_Z, tmp);
	if (core2or5 && (rail == 1) && (type == 2))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_RSSI_Z, tmp);
	if (core1or5 && (rail == 0) && (type == 0))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0I_RSSI_X, tmp);
	if (core1or5 && (rail == 1) && (type == 0))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_RSSI_X, tmp);
	if (core2or5 && (rail == 0) && (type == 0))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1I_RSSI_X, tmp);
	if (core2or5 && (rail == 1) && (type == 0))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_RSSI_X, tmp);
	if (core1or5 && (rail == 0) && (type == 1))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0I_RSSI_Y, tmp);
	if (core1or5 && (rail == 1) && (type == 1))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_RSSI_Y, tmp);
	if (core2or5 && (rail == 0) && (type == 1))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1I_RSSI_Y, tmp);
	if (core2or5 && (rail == 1) && (type == 1))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_RSSI_Y, tmp);
	if (core1or5 && (rail == 0) && (type == 6))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0I_TBD, tmp);
	if (core1or5 && (rail == 1) && (type == 6))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_TBD, tmp);
	if (core2or5 && (rail == 0) && (type == 6))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1I_TBD, tmp);
	if (core2or5 && (rail == 1) && (type == 6))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_TBD, tmp);
	if (core1or5 && (rail == 0) && (type == 3))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0I_PWRDET, tmp);
	if (core1or5 && (rail == 1) && (type == 3))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_PWRDET, tmp);
	if (core2or5 && (rail == 0) && (type == 3))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1I_PWRDET, tmp);
	if (core2or5 && (rail == 1) && (type == 3))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_PWRDET, tmp);
	if (core1or5 && (type == 4))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0I_TSSI, tmp);
	if (core2or5 && (type == 4))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1I_TSSI, tmp);
	if (core1or5 && (type == 5))
		b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_TSSI, tmp);
	if (core2or5 && (type == 5))
		b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_TSSI, tmp);
}

static void b43_nphy_rev2_rssi_select(struct b43_wldev *dev, u8 code, u8 type)
{
	u16 val;

	if (type < 3)
		val = 0;
	else if (type == 6)
		val = 1;
	else if (type == 3)
		val = 2;
	else
		val = 3;

	val = (val << 12) | (val << 14);
	b43_phy_maskset(dev, B43_NPHY_AFECTL_C1, 0x0FFF, val);
	b43_phy_maskset(dev, B43_NPHY_AFECTL_C2, 0x0FFF, val);

	if (type < 3) {
		b43_phy_maskset(dev, B43_NPHY_RFCTL_RSSIO1, 0xFFCF,
				(type + 1) << 4);
		b43_phy_maskset(dev, B43_NPHY_RFCTL_RSSIO2, 0xFFCF,
				(type + 1) << 4);
	}

	/* TODO use some definitions */
	if (code == 0) {
		b43_phy_maskset(dev, B43_NPHY_AFECTL_OVER, 0xCFFF, 0);
		if (type < 3) {
			b43_phy_maskset(dev, B43_NPHY_RFCTL_CMD, 0xFEC7, 0);
			b43_phy_maskset(dev, B43_NPHY_RFCTL_OVER, 0xEFDC, 0);
			b43_phy_maskset(dev, B43_NPHY_RFCTL_CMD, 0xFFFE, 0);
			udelay(20);
			b43_phy_maskset(dev, B43_NPHY_RFCTL_OVER, 0xFFFE, 0);
		}
	} else {
		b43_phy_maskset(dev, B43_NPHY_AFECTL_OVER, 0xCFFF,
				0x3000);
		if (type < 3) {
			b43_phy_maskset(dev, B43_NPHY_RFCTL_CMD,
					0xFEC7, 0x0180);
			b43_phy_maskset(dev, B43_NPHY_RFCTL_OVER,
					0xEFDC, (code << 1 | 0x1021));
			b43_phy_maskset(dev, B43_NPHY_RFCTL_CMD, 0xFFFE, 0x1);
			udelay(20);
			b43_phy_maskset(dev, B43_NPHY_RFCTL_OVER, 0xFFFE, 0);
		}
	}
}

static void b43_nphy_rev3_rssi_select(struct b43_wldev *dev, u8 code, u8 type)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u8 i;
	u16 reg, val;

	if (code == 0) {
		b43_phy_mask(dev, B43_NPHY_AFECTL_OVER1, 0xFDFF);
		b43_phy_mask(dev, B43_NPHY_AFECTL_OVER, 0xFDFF);
		b43_phy_mask(dev, B43_NPHY_AFECTL_C1, 0xFCFF);
		b43_phy_mask(dev, B43_NPHY_AFECTL_C2, 0xFCFF);
		b43_phy_mask(dev, B43_NPHY_TXF_40CO_B1S0, 0xFFDF);
		b43_phy_mask(dev, B43_NPHY_TXF_40CO_B32S1, 0xFFDF);
		b43_phy_mask(dev, B43_NPHY_RFCTL_LUT_TRSW_UP1, 0xFFC3);
		b43_phy_mask(dev, B43_NPHY_RFCTL_LUT_TRSW_UP2, 0xFFC3);
	} else {
		for (i = 0; i < 2; i++) {
			if ((code == 1 && i == 1) || (code == 2 && !i))
				continue;

			reg = (i == 0) ?
				B43_NPHY_AFECTL_OVER1 : B43_NPHY_AFECTL_OVER;
			b43_phy_maskset(dev, reg, 0xFDFF, 0x0200);

			if (type < 3) {
				reg = (i == 0) ?
					B43_NPHY_AFECTL_C1 :
					B43_NPHY_AFECTL_C2;
				b43_phy_maskset(dev, reg, 0xFCFF, 0);

				reg = (i == 0) ?
					B43_NPHY_RFCTL_LUT_TRSW_UP1 :
					B43_NPHY_RFCTL_LUT_TRSW_UP2;
				b43_phy_maskset(dev, reg, 0xFFC3, 0);

				if (type == 0)
					val = (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) ? 4 : 8;
				else if (type == 1)
					val = 16;
				else
					val = 32;
				b43_phy_set(dev, reg, val);

				reg = (i == 0) ?
					B43_NPHY_TXF_40CO_B1S0 :
					B43_NPHY_TXF_40CO_B32S1;
				b43_phy_set(dev, reg, 0x0020);
			} else {
				if (type == 6)
					val = 0x0100;
				else if (type == 3)
					val = 0x0200;
				else
					val = 0x0300;

				reg = (i == 0) ?
					B43_NPHY_AFECTL_C1 :
					B43_NPHY_AFECTL_C2;

				b43_phy_maskset(dev, reg, 0xFCFF, val);
				b43_phy_maskset(dev, reg, 0xF3FF, val << 2);

				if (type != 3 && type != 6) {
					enum ieee80211_band band =
						b43_current_band(dev->wl);

					if ((nphy->ipa2g_on &&
						band == IEEE80211_BAND_2GHZ) ||
						(nphy->ipa5g_on &&
						band == IEEE80211_BAND_5GHZ))
						val = (band == IEEE80211_BAND_5GHZ) ? 0xC : 0xE;
					else
						val = 0x11;
					reg = (i == 0) ? 0x2000 : 0x3000;
					reg |= B2055_PADDRV;
					b43_radio_write16(dev, reg, val);

					reg = (i == 0) ?
						B43_NPHY_AFECTL_OVER1 :
						B43_NPHY_AFECTL_OVER;
					b43_phy_set(dev, reg, 0x0200);
				}
			}
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RSSISel */
static void b43_nphy_rssi_select(struct b43_wldev *dev, u8 code, u8 type)
{
	if (dev->phy.rev >= 3)
		b43_nphy_rev3_rssi_select(dev, code, type);
	else
		b43_nphy_rev2_rssi_select(dev, code, type);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/SetRssi2055Vcm */
static void b43_nphy_set_rssi_2055_vcm(struct b43_wldev *dev, u8 type, u8 *buf)
{
	int i;
	for (i = 0; i < 2; i++) {
		if (type == 2) {
			if (i == 0) {
				b43_radio_maskset(dev, B2055_C1_B0NB_RSSIVCM,
						  0xFC, buf[0]);
				b43_radio_maskset(dev, B2055_C1_RX_BB_RSSICTL5,
						  0xFC, buf[1]);
			} else {
				b43_radio_maskset(dev, B2055_C2_B0NB_RSSIVCM,
						  0xFC, buf[2 * i]);
				b43_radio_maskset(dev, B2055_C2_RX_BB_RSSICTL5,
						  0xFC, buf[2 * i + 1]);
			}
		} else {
			if (i == 0)
				b43_radio_maskset(dev, B2055_C1_RX_BB_RSSICTL5,
						  0xF3, buf[0] << 2);
			else
				b43_radio_maskset(dev, B2055_C2_RX_BB_RSSICTL5,
						  0xF3, buf[2 * i + 1] << 2);
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/PollRssi */
static int b43_nphy_poll_rssi(struct b43_wldev *dev, u8 type, s32 *buf,
				u8 nsamp)
{
	int i;
	int out;
	u16 save_regs_phy[9];
	u16 s[2];

	if (dev->phy.rev >= 3) {
		save_regs_phy[0] = b43_phy_read(dev,
						B43_NPHY_RFCTL_LUT_TRSW_UP1);
		save_regs_phy[1] = b43_phy_read(dev,
						B43_NPHY_RFCTL_LUT_TRSW_UP2);
		save_regs_phy[2] = b43_phy_read(dev, B43_NPHY_AFECTL_C1);
		save_regs_phy[3] = b43_phy_read(dev, B43_NPHY_AFECTL_C2);
		save_regs_phy[4] = b43_phy_read(dev, B43_NPHY_AFECTL_OVER1);
		save_regs_phy[5] = b43_phy_read(dev, B43_NPHY_AFECTL_OVER);
		save_regs_phy[6] = b43_phy_read(dev, B43_NPHY_TXF_40CO_B1S0);
		save_regs_phy[7] = b43_phy_read(dev, B43_NPHY_TXF_40CO_B32S1);
	}

	b43_nphy_rssi_select(dev, 5, type);

	if (dev->phy.rev < 2) {
		save_regs_phy[8] = b43_phy_read(dev, B43_NPHY_GPIO_SEL);
		b43_phy_write(dev, B43_NPHY_GPIO_SEL, 5);
	}

	for (i = 0; i < 4; i++)
		buf[i] = 0;

	for (i = 0; i < nsamp; i++) {
		if (dev->phy.rev < 2) {
			s[0] = b43_phy_read(dev, B43_NPHY_GPIO_LOOUT);
			s[1] = b43_phy_read(dev, B43_NPHY_GPIO_HIOUT);
		} else {
			s[0] = b43_phy_read(dev, B43_NPHY_RSSI1);
			s[1] = b43_phy_read(dev, B43_NPHY_RSSI2);
		}

		buf[0] += ((s8)((s[0] & 0x3F) << 2)) >> 2;
		buf[1] += ((s8)(((s[0] >> 8) & 0x3F) << 2)) >> 2;
		buf[2] += ((s8)((s[1] & 0x3F) << 2)) >> 2;
		buf[3] += ((s8)(((s[1] >> 8) & 0x3F) << 2)) >> 2;
	}
	out = (buf[0] & 0xFF) << 24 | (buf[1] & 0xFF) << 16 |
		(buf[2] & 0xFF) << 8 | (buf[3] & 0xFF);

	if (dev->phy.rev < 2)
		b43_phy_write(dev, B43_NPHY_GPIO_SEL, save_regs_phy[8]);

	if (dev->phy.rev >= 3) {
		b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_UP1,
				save_regs_phy[0]);
		b43_phy_write(dev, B43_NPHY_RFCTL_LUT_TRSW_UP2,
				save_regs_phy[1]);
		b43_phy_write(dev, B43_NPHY_AFECTL_C1, save_regs_phy[2]);
		b43_phy_write(dev, B43_NPHY_AFECTL_C2, save_regs_phy[3]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER1, save_regs_phy[4]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, save_regs_phy[5]);
		b43_phy_write(dev, B43_NPHY_TXF_40CO_B1S0, save_regs_phy[6]);
		b43_phy_write(dev, B43_NPHY_TXF_40CO_B32S1, save_regs_phy[7]);
	}

	return out;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RSSICal */
static void b43_nphy_rev2_rssi_cal(struct b43_wldev *dev, u8 type)
{
	int i, j;
	u8 state[4];
	u8 code, val;
	u16 class, override;
	u8 regs_save_radio[2];
	u16 regs_save_phy[2];
	s8 offset[4];

	u16 clip_state[2];
	u16 clip_off[2] = { 0xFFFF, 0xFFFF };
	s32 results_min[4] = { };
	u8 vcm_final[4] = { };
	s32 results[4][4] = { };
	s32 miniq[4][2] = { };

	if (type == 2) {
		code = 0;
		val = 6;
	} else if (type < 2) {
		code = 25;
		val = 4;
	} else {
		B43_WARN_ON(1);
		return;
	}

	class = b43_nphy_classifier(dev, 0, 0);
	b43_nphy_classifier(dev, 7, 4);
	b43_nphy_read_clip_detection(dev, clip_state);
	b43_nphy_write_clip_detection(dev, clip_off);

	if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ)
		override = 0x140;
	else
		override = 0x110;

	regs_save_phy[0] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC1);
	regs_save_radio[0] = b43_radio_read16(dev, B2055_C1_PD_RXTX);
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, override);
	b43_radio_write16(dev, B2055_C1_PD_RXTX, val);

	regs_save_phy[1] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC2);
	regs_save_radio[1] = b43_radio_read16(dev, B2055_C2_PD_RXTX);
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, override);
	b43_radio_write16(dev, B2055_C2_PD_RXTX, val);

	state[0] = b43_radio_read16(dev, B2055_C1_PD_RSSIMISC) & 0x07;
	state[1] = b43_radio_read16(dev, B2055_C2_PD_RSSIMISC) & 0x07;
	b43_radio_mask(dev, B2055_C1_PD_RSSIMISC, 0xF8);
	b43_radio_mask(dev, B2055_C2_PD_RSSIMISC, 0xF8);
	state[2] = b43_radio_read16(dev, B2055_C1_SP_RSSI) & 0x07;
	state[3] = b43_radio_read16(dev, B2055_C2_SP_RSSI) & 0x07;

	b43_nphy_rssi_select(dev, 5, type);
	b43_nphy_scale_offset_rssi(dev, 0, 0, 5, 0, type);
	b43_nphy_scale_offset_rssi(dev, 0, 0, 5, 1, type);

	for (i = 0; i < 4; i++) {
		u8 tmp[4];
		for (j = 0; j < 4; j++)
			tmp[j] = i;
		if (type != 1)
			b43_nphy_set_rssi_2055_vcm(dev, type, tmp);
		b43_nphy_poll_rssi(dev, type, results[i], 8);
		if (type < 2)
			for (j = 0; j < 2; j++)
				miniq[i][j] = min(results[i][2 * j],
						results[i][2 * j + 1]);
	}

	for (i = 0; i < 4; i++) {
		s32 mind = 40;
		u8 minvcm = 0;
		s32 minpoll = 249;
		s32 curr;
		for (j = 0; j < 4; j++) {
			if (type == 2)
				curr = abs(results[j][i]);
			else
				curr = abs(miniq[j][i / 2] - code * 8);

			if (curr < mind) {
				mind = curr;
				minvcm = j;
			}

			if (results[j][i] < minpoll)
				minpoll = results[j][i];
		}
		results_min[i] = minpoll;
		vcm_final[i] = minvcm;
	}

	if (type != 1)
		b43_nphy_set_rssi_2055_vcm(dev, type, vcm_final);

	for (i = 0; i < 4; i++) {
		offset[i] = (code * 8) - results[vcm_final[i]][i];

		if (offset[i] < 0)
			offset[i] = -((abs(offset[i]) + 4) / 8);
		else
			offset[i] = (offset[i] + 4) / 8;

		if (results_min[i] == 248)
			offset[i] = code - 32;

		if (i % 2 == 0)
			b43_nphy_scale_offset_rssi(dev, 0, offset[i], 1, 0,
							type);
		else
			b43_nphy_scale_offset_rssi(dev, 0, offset[i], 2, 1,
							type);
	}

	b43_radio_maskset(dev, B2055_C1_PD_RSSIMISC, 0xF8, state[0]);
	b43_radio_maskset(dev, B2055_C1_PD_RSSIMISC, 0xF8, state[1]);

	switch (state[2]) {
	case 1:
		b43_nphy_rssi_select(dev, 1, 2);
		break;
	case 4:
		b43_nphy_rssi_select(dev, 1, 0);
		break;
	case 2:
		b43_nphy_rssi_select(dev, 1, 1);
		break;
	default:
		b43_nphy_rssi_select(dev, 1, 1);
		break;
	}

	switch (state[3]) {
	case 1:
		b43_nphy_rssi_select(dev, 2, 2);
		break;
	case 4:
		b43_nphy_rssi_select(dev, 2, 0);
		break;
	default:
		b43_nphy_rssi_select(dev, 2, 1);
		break;
	}

	b43_nphy_rssi_select(dev, 0, type);

	b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, regs_save_phy[0]);
	b43_radio_write16(dev, B2055_C1_PD_RXTX, regs_save_radio[0]);
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, regs_save_phy[1]);
	b43_radio_write16(dev, B2055_C2_PD_RXTX, regs_save_radio[1]);

	b43_nphy_classifier(dev, 7, class);
	b43_nphy_write_clip_detection(dev, clip_state);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RSSICalRev3 */
static void b43_nphy_rev3_rssi_cal(struct b43_wldev *dev)
{
	/* TODO */
}

/*
 * RSSI Calibration
 * http://bcm-v4.sipsolutions.net/802.11/PHY/N/RSSICal
 */
static void b43_nphy_rssi_cal(struct b43_wldev *dev)
{
	if (dev->phy.rev >= 3) {
		b43_nphy_rev3_rssi_cal(dev);
	} else {
		b43_nphy_rev2_rssi_cal(dev, 2);
		b43_nphy_rev2_rssi_cal(dev, 0);
		b43_nphy_rev2_rssi_cal(dev, 1);
	}
}

/*
 * Restore RSSI Calibration
 * http://bcm-v4.sipsolutions.net/802.11/PHY/N/RestoreRssiCal
 */
static void b43_nphy_restore_rssi_cal(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	u16 *rssical_radio_regs = NULL;
	u16 *rssical_phy_regs = NULL;

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		if (b43_empty_chanspec(&nphy->rssical_chanspec_2G))
			return;
		rssical_radio_regs = nphy->rssical_cache.rssical_radio_regs_2G;
		rssical_phy_regs = nphy->rssical_cache.rssical_phy_regs_2G;
	} else {
		if (b43_empty_chanspec(&nphy->rssical_chanspec_5G))
			return;
		rssical_radio_regs = nphy->rssical_cache.rssical_radio_regs_5G;
		rssical_phy_regs = nphy->rssical_cache.rssical_phy_regs_5G;
	}

	/* TODO use some definitions */
	b43_radio_maskset(dev, 0x602B, 0xE3, rssical_radio_regs[0]);
	b43_radio_maskset(dev, 0x702B, 0xE3, rssical_radio_regs[1]);

	b43_phy_write(dev, B43_NPHY_RSSIMC_0I_RSSI_Z, rssical_phy_regs[0]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_RSSI_Z, rssical_phy_regs[1]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_1I_RSSI_Z, rssical_phy_regs[2]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_RSSI_Z, rssical_phy_regs[3]);

	b43_phy_write(dev, B43_NPHY_RSSIMC_0I_RSSI_X, rssical_phy_regs[4]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_RSSI_X, rssical_phy_regs[5]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_1I_RSSI_X, rssical_phy_regs[6]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_RSSI_X, rssical_phy_regs[7]);

	b43_phy_write(dev, B43_NPHY_RSSIMC_0I_RSSI_Y, rssical_phy_regs[8]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_0Q_RSSI_Y, rssical_phy_regs[9]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_1I_RSSI_Y, rssical_phy_regs[10]);
	b43_phy_write(dev, B43_NPHY_RSSIMC_1Q_RSSI_Y, rssical_phy_regs[11]);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/GetIpaGainTbl */
static const u32 *b43_nphy_get_ipa_gain_table(struct b43_wldev *dev)
{
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		if (dev->phy.rev >= 6) {
			/* TODO If the chip is 47162
				return txpwrctrl_tx_gain_ipa_rev5 */
			return txpwrctrl_tx_gain_ipa_rev6;
		} else if (dev->phy.rev >= 5) {
			return txpwrctrl_tx_gain_ipa_rev5;
		} else {
			return txpwrctrl_tx_gain_ipa;
		}
	} else {
		return txpwrctrl_tx_gain_ipa_5g;
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/TxCalRadioSetup */
static void b43_nphy_tx_cal_radio_setup(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u16 *save = nphy->tx_rx_cal_radio_saveregs;
	u16 tmp;
	u8 offset, i;

	if (dev->phy.rev >= 3) {
	    for (i = 0; i < 2; i++) {
		tmp = (i == 0) ? 0x2000 : 0x3000;
		offset = i * 11;

		save[offset + 0] = b43_radio_read16(dev, B2055_CAL_RVARCTL);
		save[offset + 1] = b43_radio_read16(dev, B2055_CAL_LPOCTL);
		save[offset + 2] = b43_radio_read16(dev, B2055_CAL_TS);
		save[offset + 3] = b43_radio_read16(dev, B2055_CAL_RCCALRTS);
		save[offset + 4] = b43_radio_read16(dev, B2055_CAL_RCALRTS);
		save[offset + 5] = b43_radio_read16(dev, B2055_PADDRV);
		save[offset + 6] = b43_radio_read16(dev, B2055_XOCTL1);
		save[offset + 7] = b43_radio_read16(dev, B2055_XOCTL2);
		save[offset + 8] = b43_radio_read16(dev, B2055_XOREGUL);
		save[offset + 9] = b43_radio_read16(dev, B2055_XOMISC);
		save[offset + 10] = b43_radio_read16(dev, B2055_PLL_LFC1);

		if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) {
			b43_radio_write16(dev, tmp | B2055_CAL_RVARCTL, 0x0A);
			b43_radio_write16(dev, tmp | B2055_CAL_LPOCTL, 0x40);
			b43_radio_write16(dev, tmp | B2055_CAL_TS, 0x55);
			b43_radio_write16(dev, tmp | B2055_CAL_RCCALRTS, 0);
			b43_radio_write16(dev, tmp | B2055_CAL_RCALRTS, 0);
			if (nphy->ipa5g_on) {
				b43_radio_write16(dev, tmp | B2055_PADDRV, 4);
				b43_radio_write16(dev, tmp | B2055_XOCTL1, 1);
			} else {
				b43_radio_write16(dev, tmp | B2055_PADDRV, 0);
				b43_radio_write16(dev, tmp | B2055_XOCTL1, 0x2F);
			}
			b43_radio_write16(dev, tmp | B2055_XOCTL2, 0);
		} else {
			b43_radio_write16(dev, tmp | B2055_CAL_RVARCTL, 0x06);
			b43_radio_write16(dev, tmp | B2055_CAL_LPOCTL, 0x40);
			b43_radio_write16(dev, tmp | B2055_CAL_TS, 0x55);
			b43_radio_write16(dev, tmp | B2055_CAL_RCCALRTS, 0);
			b43_radio_write16(dev, tmp | B2055_CAL_RCALRTS, 0);
			b43_radio_write16(dev, tmp | B2055_XOCTL1, 0);
			if (nphy->ipa2g_on) {
				b43_radio_write16(dev, tmp | B2055_PADDRV, 6);
				b43_radio_write16(dev, tmp | B2055_XOCTL2,
					(dev->phy.rev < 5) ? 0x11 : 0x01);
			} else {
				b43_radio_write16(dev, tmp | B2055_PADDRV, 0);
				b43_radio_write16(dev, tmp | B2055_XOCTL2, 0);
			}
		}
		b43_radio_write16(dev, tmp | B2055_XOREGUL, 0);
		b43_radio_write16(dev, tmp | B2055_XOMISC, 0);
		b43_radio_write16(dev, tmp | B2055_PLL_LFC1, 0);
	    }
	} else {
		save[0] = b43_radio_read16(dev, B2055_C1_TX_RF_IQCAL1);
		b43_radio_write16(dev, B2055_C1_TX_RF_IQCAL1, 0x29);

		save[1] = b43_radio_read16(dev, B2055_C1_TX_RF_IQCAL2);
		b43_radio_write16(dev, B2055_C1_TX_RF_IQCAL2, 0x54);

		save[2] = b43_radio_read16(dev, B2055_C2_TX_RF_IQCAL1);
		b43_radio_write16(dev, B2055_C2_TX_RF_IQCAL1, 0x29);

		save[3] = b43_radio_read16(dev, B2055_C2_TX_RF_IQCAL2);
		b43_radio_write16(dev, B2055_C2_TX_RF_IQCAL2, 0x54);

		save[3] = b43_radio_read16(dev, B2055_C1_PWRDET_RXTX);
		save[4] = b43_radio_read16(dev, B2055_C2_PWRDET_RXTX);

		if (!(b43_phy_read(dev, B43_NPHY_BANDCTL) &
		    B43_NPHY_BANDCTL_5GHZ)) {
			b43_radio_write16(dev, B2055_C1_PWRDET_RXTX, 0x04);
			b43_radio_write16(dev, B2055_C2_PWRDET_RXTX, 0x04);
		} else {
			b43_radio_write16(dev, B2055_C1_PWRDET_RXTX, 0x20);
			b43_radio_write16(dev, B2055_C2_PWRDET_RXTX, 0x20);
		}

		if (dev->phy.rev < 2) {
			b43_radio_set(dev, B2055_C1_TX_BB_MXGM, 0x20);
			b43_radio_set(dev, B2055_C2_TX_BB_MXGM, 0x20);
		} else {
			b43_radio_mask(dev, B2055_C1_TX_BB_MXGM, ~0x20);
			b43_radio_mask(dev, B2055_C2_TX_BB_MXGM, ~0x20);
		}
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/IqCalGainParams */
static void b43_nphy_iq_cal_gain_params(struct b43_wldev *dev, u16 core,
					struct nphy_txgains target,
					struct nphy_iqcal_params *params)
{
	int i, j, indx;
	u16 gain;

	if (dev->phy.rev >= 3) {
		params->txgm = target.txgm[core];
		params->pga = target.pga[core];
		params->pad = target.pad[core];
		params->ipa = target.ipa[core];
		params->cal_gain = (params->txgm << 12) | (params->pga << 8) |
					(params->pad << 4) | (params->ipa);
		for (j = 0; j < 5; j++)
			params->ncorr[j] = 0x79;
	} else {
		gain = (target.pad[core]) | (target.pga[core] << 4) |
			(target.txgm[core] << 8);

		indx = (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) ?
			1 : 0;
		for (i = 0; i < 9; i++)
			if (tbl_iqcal_gainparams[indx][i][0] == gain)
				break;
		i = min(i, 8);

		params->txgm = tbl_iqcal_gainparams[indx][i][1];
		params->pga = tbl_iqcal_gainparams[indx][i][2];
		params->pad = tbl_iqcal_gainparams[indx][i][3];
		params->cal_gain = (params->txgm << 7) | (params->pga << 4) |
					(params->pad << 2);
		for (j = 0; j < 4; j++)
			params->ncorr[j] = tbl_iqcal_gainparams[indx][i][4 + j];
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/UpdateTxCalLadder */
static void b43_nphy_update_tx_cal_ladder(struct b43_wldev *dev, u16 core)
{
	struct b43_phy_n *nphy = dev->phy.n;
	int i;
	u16 scale, entry;

	u16 tmp = nphy->txcal_bbmult;
	if (core == 0)
		tmp >>= 8;
	tmp &= 0xff;

	for (i = 0; i < 18; i++) {
		scale = (ladder_lo[i].percent * tmp) / 100;
		entry = ((scale & 0xFF) << 8) | ladder_lo[i].g_env;
		b43_ntab_write(dev, B43_NTAB16(15, i), entry);

		scale = (ladder_iq[i].percent * tmp) / 100;
		entry = ((scale & 0xFF) << 8) | ladder_iq[i].g_env;
		b43_ntab_write(dev, B43_NTAB16(15, i + 32), entry);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/ExtPaSetTxDigiFilts */
static void b43_nphy_ext_pa_set_tx_dig_filters(struct b43_wldev *dev)
{
	int i;
	for (i = 0; i < 15; i++)
		b43_phy_write(dev, B43_PHY_N(0x2C5 + i),
				tbl_tx_filter_coef_rev4[2][i]);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/IpaSetTxDigiFilts */
static void b43_nphy_int_pa_set_tx_dig_filters(struct b43_wldev *dev)
{
	int i, j;
	/* B43_NPHY_TXF_20CO_S0A1, B43_NPHY_TXF_40CO_S0A1, unknown */
	u16 offset[] = { 0x186, 0x195, 0x2C5 };

	for (i = 0; i < 3; i++)
		for (j = 0; j < 15; j++)
			b43_phy_write(dev, B43_PHY_N(offset[i] + j),
					tbl_tx_filter_coef_rev4[i][j]);

	if (dev->phy.is_40mhz) {
		for (j = 0; j < 15; j++)
			b43_phy_write(dev, B43_PHY_N(offset[0] + j),
					tbl_tx_filter_coef_rev4[3][j]);
	} else if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ) {
		for (j = 0; j < 15; j++)
			b43_phy_write(dev, B43_PHY_N(offset[0] + j),
					tbl_tx_filter_coef_rev4[5][j]);
	}

	if (dev->phy.channel == 14)
		for (j = 0; j < 15; j++)
			b43_phy_write(dev, B43_PHY_N(offset[0] + j),
					tbl_tx_filter_coef_rev4[6][j]);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/GetTxGain */
static struct nphy_txgains b43_nphy_get_tx_gains(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	u16 curr_gain[2];
	struct nphy_txgains target;
	const u32 *table = NULL;

	if (nphy->txpwrctrl == 0) {
		int i;

		if (nphy->hang_avoid)
			b43_nphy_stay_in_carrier_search(dev, true);
		b43_ntab_read_bulk(dev, B43_NTAB16(7, 0x110), 2, curr_gain);
		if (nphy->hang_avoid)
			b43_nphy_stay_in_carrier_search(dev, false);

		for (i = 0; i < 2; ++i) {
			if (dev->phy.rev >= 3) {
				target.ipa[i] = curr_gain[i] & 0x000F;
				target.pad[i] = (curr_gain[i] & 0x00F0) >> 4;
				target.pga[i] = (curr_gain[i] & 0x0F00) >> 8;
				target.txgm[i] = (curr_gain[i] & 0x7000) >> 12;
			} else {
				target.ipa[i] = curr_gain[i] & 0x0003;
				target.pad[i] = (curr_gain[i] & 0x000C) >> 2;
				target.pga[i] = (curr_gain[i] & 0x0070) >> 4;
				target.txgm[i] = (curr_gain[i] & 0x0380) >> 7;
			}
		}
	} else {
		int i;
		u16 index[2];
		index[0] = (b43_phy_read(dev, B43_NPHY_C1_TXPCTL_STAT) &
			B43_NPHY_TXPCTL_STAT_BIDX) >>
			B43_NPHY_TXPCTL_STAT_BIDX_SHIFT;
		index[1] = (b43_phy_read(dev, B43_NPHY_C2_TXPCTL_STAT) &
			B43_NPHY_TXPCTL_STAT_BIDX) >>
			B43_NPHY_TXPCTL_STAT_BIDX_SHIFT;

		for (i = 0; i < 2; ++i) {
			if (dev->phy.rev >= 3) {
				enum ieee80211_band band =
					b43_current_band(dev->wl);

				if ((nphy->ipa2g_on &&
				     band == IEEE80211_BAND_2GHZ) ||
				    (nphy->ipa5g_on &&
				     band == IEEE80211_BAND_5GHZ)) {
					table = b43_nphy_get_ipa_gain_table(dev);
				} else {
					if (band == IEEE80211_BAND_5GHZ) {
						if (dev->phy.rev == 3)
							table = b43_ntab_tx_gain_rev3_5ghz;
						else if (dev->phy.rev == 4)
							table = b43_ntab_tx_gain_rev4_5ghz;
						else
							table = b43_ntab_tx_gain_rev5plus_5ghz;
					} else {
						table = b43_ntab_tx_gain_rev3plus_2ghz;
					}
				}

				target.ipa[i] = (table[index[i]] >> 16) & 0xF;
				target.pad[i] = (table[index[i]] >> 20) & 0xF;
				target.pga[i] = (table[index[i]] >> 24) & 0xF;
				target.txgm[i] = (table[index[i]] >> 28) & 0xF;
			} else {
				table = b43_ntab_tx_gain_rev0_1_2;

				target.ipa[i] = (table[index[i]] >> 16) & 0x3;
				target.pad[i] = (table[index[i]] >> 18) & 0x3;
				target.pga[i] = (table[index[i]] >> 20) & 0x7;
				target.txgm[i] = (table[index[i]] >> 23) & 0x7;
			}
		}
	}

	return target;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/TxCalPhyCleanup */
static void b43_nphy_tx_cal_phy_cleanup(struct b43_wldev *dev)
{
	u16 *regs = dev->phy.n->tx_rx_cal_phy_saveregs;

	if (dev->phy.rev >= 3) {
		b43_phy_write(dev, B43_NPHY_AFECTL_C1, regs[0]);
		b43_phy_write(dev, B43_NPHY_AFECTL_C2, regs[1]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER1, regs[2]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, regs[3]);
		b43_phy_write(dev, B43_NPHY_BBCFG, regs[4]);
		b43_ntab_write(dev, B43_NTAB16(8, 3), regs[5]);
		b43_ntab_write(dev, B43_NTAB16(8, 19), regs[6]);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, regs[7]);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, regs[8]);
		b43_phy_write(dev, B43_NPHY_PAPD_EN0, regs[9]);
		b43_phy_write(dev, B43_NPHY_PAPD_EN1, regs[10]);
		b43_nphy_reset_cca(dev);
	} else {
		b43_phy_maskset(dev, B43_NPHY_AFECTL_C1, 0x0FFF, regs[0]);
		b43_phy_maskset(dev, B43_NPHY_AFECTL_C2, 0x0FFF, regs[1]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, regs[2]);
		b43_ntab_write(dev, B43_NTAB16(8, 2), regs[3]);
		b43_ntab_write(dev, B43_NTAB16(8, 18), regs[4]);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, regs[5]);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, regs[6]);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/TxCalPhySetup */
static void b43_nphy_tx_cal_phy_setup(struct b43_wldev *dev)
{
	u16 *regs = dev->phy.n->tx_rx_cal_phy_saveregs;
	u16 tmp;

	regs[0] = b43_phy_read(dev, B43_NPHY_AFECTL_C1);
	regs[1] = b43_phy_read(dev, B43_NPHY_AFECTL_C2);
	if (dev->phy.rev >= 3) {
		b43_phy_maskset(dev, B43_NPHY_AFECTL_C1, 0xF0FF, 0x0A00);
		b43_phy_maskset(dev, B43_NPHY_AFECTL_C2, 0xF0FF, 0x0A00);

		tmp = b43_phy_read(dev, B43_NPHY_AFECTL_OVER1);
		regs[2] = tmp;
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER1, tmp | 0x0600);

		tmp = b43_phy_read(dev, B43_NPHY_AFECTL_OVER);
		regs[3] = tmp;
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, tmp | 0x0600);

		regs[4] = b43_phy_read(dev, B43_NPHY_BBCFG);
		b43_phy_mask(dev, B43_NPHY_BBCFG,
			     ~B43_NPHY_BBCFG_RSTRX & 0xFFFF);

		tmp = b43_ntab_read(dev, B43_NTAB16(8, 3));
		regs[5] = tmp;
		b43_ntab_write(dev, B43_NTAB16(8, 3), 0);

		tmp = b43_ntab_read(dev, B43_NTAB16(8, 19));
		regs[6] = tmp;
		b43_ntab_write(dev, B43_NTAB16(8, 19), 0);
		regs[7] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC1);
		regs[8] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC2);

		b43_nphy_rf_control_intc_override(dev, 2, 1, 3);
		b43_nphy_rf_control_intc_override(dev, 1, 2, 1);
		b43_nphy_rf_control_intc_override(dev, 1, 8, 2);

		regs[9] = b43_phy_read(dev, B43_NPHY_PAPD_EN0);
		regs[10] = b43_phy_read(dev, B43_NPHY_PAPD_EN1);
		b43_phy_mask(dev, B43_NPHY_PAPD_EN0, ~0x0001);
		b43_phy_mask(dev, B43_NPHY_PAPD_EN1, ~0x0001);
	} else {
		b43_phy_maskset(dev, B43_NPHY_AFECTL_C1, 0x0FFF, 0xA000);
		b43_phy_maskset(dev, B43_NPHY_AFECTL_C2, 0x0FFF, 0xA000);
		tmp = b43_phy_read(dev, B43_NPHY_AFECTL_OVER);
		regs[2] = tmp;
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, tmp | 0x3000);
		tmp = b43_ntab_read(dev, B43_NTAB16(8, 2));
		regs[3] = tmp;
		tmp |= 0x2000;
		b43_ntab_write(dev, B43_NTAB16(8, 2), tmp);
		tmp = b43_ntab_read(dev, B43_NTAB16(8, 18));
		regs[4] = tmp;
		tmp |= 0x2000;
		b43_ntab_write(dev, B43_NTAB16(8, 18), tmp);
		regs[5] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC1);
		regs[6] = b43_phy_read(dev, B43_NPHY_RFCTL_INTC2);
		if (b43_current_band(dev->wl) == IEEE80211_BAND_5GHZ)
			tmp = 0x0180;
		else
			tmp = 0x0120;
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, tmp);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, tmp);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/SaveCal */
static void b43_nphy_save_cal(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	struct b43_phy_n_iq_comp *rxcal_coeffs = NULL;
	u16 *txcal_radio_regs = NULL;
	struct b43_chanspec *iqcal_chanspec;
	u16 *table = NULL;

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 1);

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		rxcal_coeffs = &nphy->cal_cache.rxcal_coeffs_2G;
		txcal_radio_regs = nphy->cal_cache.txcal_radio_regs_2G;
		iqcal_chanspec = &nphy->iqcal_chanspec_2G;
		table = nphy->cal_cache.txcal_coeffs_2G;
	} else {
		rxcal_coeffs = &nphy->cal_cache.rxcal_coeffs_5G;
		txcal_radio_regs = nphy->cal_cache.txcal_radio_regs_5G;
		iqcal_chanspec = &nphy->iqcal_chanspec_5G;
		table = nphy->cal_cache.txcal_coeffs_5G;
	}

	b43_nphy_rx_iq_coeffs(dev, false, rxcal_coeffs);
	/* TODO use some definitions */
	if (dev->phy.rev >= 3) {
		txcal_radio_regs[0] = b43_radio_read(dev, 0x2021);
		txcal_radio_regs[1] = b43_radio_read(dev, 0x2022);
		txcal_radio_regs[2] = b43_radio_read(dev, 0x3021);
		txcal_radio_regs[3] = b43_radio_read(dev, 0x3022);
		txcal_radio_regs[4] = b43_radio_read(dev, 0x2023);
		txcal_radio_regs[5] = b43_radio_read(dev, 0x2024);
		txcal_radio_regs[6] = b43_radio_read(dev, 0x3023);
		txcal_radio_regs[7] = b43_radio_read(dev, 0x3024);
	} else {
		txcal_radio_regs[0] = b43_radio_read(dev, 0x8B);
		txcal_radio_regs[1] = b43_radio_read(dev, 0xBA);
		txcal_radio_regs[2] = b43_radio_read(dev, 0x8D);
		txcal_radio_regs[3] = b43_radio_read(dev, 0xBC);
	}
	*iqcal_chanspec = nphy->radio_chanspec;
	b43_ntab_write_bulk(dev, B43_NTAB16(15, 80), 8, table);

	if (nphy->hang_avoid)
		b43_nphy_stay_in_carrier_search(dev, 0);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/RestoreCal */
static void b43_nphy_restore_cal(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;

	u16 coef[4];
	u16 *loft = NULL;
	u16 *table = NULL;

	int i;
	u16 *txcal_radio_regs = NULL;
	struct b43_phy_n_iq_comp *rxcal_coeffs = NULL;

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		if (b43_empty_chanspec(&nphy->iqcal_chanspec_2G))
			return;
		table = nphy->cal_cache.txcal_coeffs_2G;
		loft = &nphy->cal_cache.txcal_coeffs_2G[5];
	} else {
		if (b43_empty_chanspec(&nphy->iqcal_chanspec_5G))
			return;
		table = nphy->cal_cache.txcal_coeffs_5G;
		loft = &nphy->cal_cache.txcal_coeffs_5G[5];
	}

	b43_ntab_write_bulk(dev, B43_NTAB16(15, 80), 4, table);

	for (i = 0; i < 4; i++) {
		if (dev->phy.rev >= 3)
			table[i] = coef[i];
		else
			coef[i] = 0;
	}

	b43_ntab_write_bulk(dev, B43_NTAB16(15, 88), 4, coef);
	b43_ntab_write_bulk(dev, B43_NTAB16(15, 85), 2, loft);
	b43_ntab_write_bulk(dev, B43_NTAB16(15, 93), 2, loft);

	if (dev->phy.rev < 2)
		b43_nphy_tx_iq_workaround(dev);

	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		txcal_radio_regs = nphy->cal_cache.txcal_radio_regs_2G;
		rxcal_coeffs = &nphy->cal_cache.rxcal_coeffs_2G;
	} else {
		txcal_radio_regs = nphy->cal_cache.txcal_radio_regs_5G;
		rxcal_coeffs = &nphy->cal_cache.rxcal_coeffs_5G;
	}

	/* TODO use some definitions */
	if (dev->phy.rev >= 3) {
		b43_radio_write(dev, 0x2021, txcal_radio_regs[0]);
		b43_radio_write(dev, 0x2022, txcal_radio_regs[1]);
		b43_radio_write(dev, 0x3021, txcal_radio_regs[2]);
		b43_radio_write(dev, 0x3022, txcal_radio_regs[3]);
		b43_radio_write(dev, 0x2023, txcal_radio_regs[4]);
		b43_radio_write(dev, 0x2024, txcal_radio_regs[5]);
		b43_radio_write(dev, 0x3023, txcal_radio_regs[6]);
		b43_radio_write(dev, 0x3024, txcal_radio_regs[7]);
	} else {
		b43_radio_write(dev, 0x8B, txcal_radio_regs[0]);
		b43_radio_write(dev, 0xBA, txcal_radio_regs[1]);
		b43_radio_write(dev, 0x8D, txcal_radio_regs[2]);
		b43_radio_write(dev, 0xBC, txcal_radio_regs[3]);
	}
	b43_nphy_rx_iq_coeffs(dev, true, rxcal_coeffs);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/CalTxIqlo */
static int b43_nphy_cal_tx_iq_lo(struct b43_wldev *dev,
				struct nphy_txgains target,
				bool full, bool mphase)
{
	struct b43_phy_n *nphy = dev->phy.n;
	int i;
	int error = 0;
	int freq;
	bool avoid = false;
	u8 length;
	u16 tmp, core, type, count, max, numb, last, cmd;
	const u16 *table;
	bool phy6or5x;

	u16 buffer[11];
	u16 diq_start = 0;
	u16 save[2];
	u16 gain[2];
	struct nphy_iqcal_params params[2];
	bool updated[2] = { };

	b43_nphy_stay_in_carrier_search(dev, true);

	if (dev->phy.rev >= 4) {
		avoid = nphy->hang_avoid;
		nphy->hang_avoid = 0;
	}

	b43_ntab_read_bulk(dev, B43_NTAB16(7, 0x110), 2, save);

	for (i = 0; i < 2; i++) {
		b43_nphy_iq_cal_gain_params(dev, i, target, &params[i]);
		gain[i] = params[i].cal_gain;
	}

	b43_ntab_write_bulk(dev, B43_NTAB16(7, 0x110), 2, gain);

	b43_nphy_tx_cal_radio_setup(dev);
	b43_nphy_tx_cal_phy_setup(dev);

	phy6or5x = dev->phy.rev >= 6 ||
		(dev->phy.rev == 5 && nphy->ipa2g_on &&
		b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ);
	if (phy6or5x) {
		if (dev->phy.is_40mhz) {
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 0), 18,
					tbl_tx_iqlo_cal_loft_ladder_40);
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 32), 18,
					tbl_tx_iqlo_cal_iqimb_ladder_40);
		} else {
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 0), 18,
					tbl_tx_iqlo_cal_loft_ladder_20);
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 32), 18,
					tbl_tx_iqlo_cal_iqimb_ladder_20);
		}
	}

	b43_phy_write(dev, B43_NPHY_IQLOCAL_CMDGCTL, 0x8AA9);

	if (!dev->phy.is_40mhz)
		freq = 2500;
	else
		freq = 5000;

	if (nphy->mphase_cal_phase_id > 2)
		b43_nphy_run_samples(dev, (dev->phy.is_40mhz ? 40 : 20) * 8,
					0xFFFF, 0, true, false);
	else
		error = b43_nphy_tx_tone(dev, freq, 250, true, false);

	if (error == 0) {
		if (nphy->mphase_cal_phase_id > 2) {
			table = nphy->mphase_txcal_bestcoeffs;
			length = 11;
			if (dev->phy.rev < 3)
				length -= 2;
		} else {
			if (!full && nphy->txiqlocal_coeffsvalid) {
				table = nphy->txiqlocal_bestc;
				length = 11;
				if (dev->phy.rev < 3)
					length -= 2;
			} else {
				full = true;
				if (dev->phy.rev >= 3) {
					table = tbl_tx_iqlo_cal_startcoefs_nphyrev3;
					length = B43_NTAB_TX_IQLO_CAL_STARTCOEFS_REV3;
				} else {
					table = tbl_tx_iqlo_cal_startcoefs;
					length = B43_NTAB_TX_IQLO_CAL_STARTCOEFS;
				}
			}
		}

		b43_ntab_write_bulk(dev, B43_NTAB16(15, 64), length, table);

		if (full) {
			if (dev->phy.rev >= 3)
				max = B43_NTAB_TX_IQLO_CAL_CMDS_FULLCAL_REV3;
			else
				max = B43_NTAB_TX_IQLO_CAL_CMDS_FULLCAL;
		} else {
			if (dev->phy.rev >= 3)
				max = B43_NTAB_TX_IQLO_CAL_CMDS_RECAL_REV3;
			else
				max = B43_NTAB_TX_IQLO_CAL_CMDS_RECAL;
		}

		if (mphase) {
			count = nphy->mphase_txcal_cmdidx;
			numb = min(max,
				(u16)(count + nphy->mphase_txcal_numcmds));
		} else {
			count = 0;
			numb = max;
		}

		for (; count < numb; count++) {
			if (full) {
				if (dev->phy.rev >= 3)
					cmd = tbl_tx_iqlo_cal_cmds_fullcal_nphyrev3[count];
				else
					cmd = tbl_tx_iqlo_cal_cmds_fullcal[count];
			} else {
				if (dev->phy.rev >= 3)
					cmd = tbl_tx_iqlo_cal_cmds_recal_nphyrev3[count];
				else
					cmd = tbl_tx_iqlo_cal_cmds_recal[count];
			}

			core = (cmd & 0x3000) >> 12;
			type = (cmd & 0x0F00) >> 8;

			if (phy6or5x && updated[core] == 0) {
				b43_nphy_update_tx_cal_ladder(dev, core);
				updated[core] = 1;
			}

			tmp = (params[core].ncorr[type] << 8) | 0x66;
			b43_phy_write(dev, B43_NPHY_IQLOCAL_CMDNNUM, tmp);

			if (type == 1 || type == 3 || type == 4) {
				buffer[0] = b43_ntab_read(dev,
						B43_NTAB16(15, 69 + core));
				diq_start = buffer[0];
				buffer[0] = 0;
				b43_ntab_write(dev, B43_NTAB16(15, 69 + core),
						0);
			}

			b43_phy_write(dev, B43_NPHY_IQLOCAL_CMD, cmd);
			for (i = 0; i < 2000; i++) {
				tmp = b43_phy_read(dev, B43_NPHY_IQLOCAL_CMD);
				if (tmp & 0xC000)
					break;
				udelay(10);
			}

			b43_ntab_read_bulk(dev, B43_NTAB16(15, 96), length,
						buffer);
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 64), length,
						buffer);

			if (type == 1 || type == 3 || type == 4)
				buffer[0] = diq_start;
		}

		if (mphase)
			nphy->mphase_txcal_cmdidx = (numb >= max) ? 0 : numb;

		last = (dev->phy.rev < 3) ? 6 : 7;

		if (!mphase || nphy->mphase_cal_phase_id == last) {
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 96), 4, buffer);
			b43_ntab_read_bulk(dev, B43_NTAB16(15, 80), 4, buffer);
			if (dev->phy.rev < 3) {
				buffer[0] = 0;
				buffer[1] = 0;
				buffer[2] = 0;
				buffer[3] = 0;
			}
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 88), 4,
						buffer);
			b43_ntab_read_bulk(dev, B43_NTAB16(15, 101), 2,
						buffer);
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 85), 2,
						buffer);
			b43_ntab_write_bulk(dev, B43_NTAB16(15, 93), 2,
						buffer);
			length = 11;
			if (dev->phy.rev < 3)
				length -= 2;
			b43_ntab_read_bulk(dev, B43_NTAB16(15, 96), length,
						nphy->txiqlocal_bestc);
			nphy->txiqlocal_coeffsvalid = true;
			nphy->txiqlocal_chanspec = nphy->radio_chanspec;
		} else {
			length = 11;
			if (dev->phy.rev < 3)
				length -= 2;
			b43_ntab_read_bulk(dev, B43_NTAB16(15, 96), length,
						nphy->mphase_txcal_bestcoeffs);
		}

		b43_nphy_stop_playback(dev);
		b43_phy_write(dev, B43_NPHY_IQLOCAL_CMDGCTL, 0);
	}

	b43_nphy_tx_cal_phy_cleanup(dev);
	b43_ntab_write_bulk(dev, B43_NTAB16(7, 0x110), 2, save);

	if (dev->phy.rev < 2 && (!mphase || nphy->mphase_cal_phase_id == last))
		b43_nphy_tx_iq_workaround(dev);

	if (dev->phy.rev >= 4)
		nphy->hang_avoid = avoid;

	b43_nphy_stay_in_carrier_search(dev, false);

	return error;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/ReapplyTxCalCoeffs */
static void b43_nphy_reapply_tx_cal_coeffs(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy = dev->phy.n;
	u8 i;
	u16 buffer[7];
	bool equal = true;

	if (!nphy->txiqlocal_coeffsvalid ||
	    b43_eq_chanspecs(&nphy->txiqlocal_chanspec, &nphy->radio_chanspec))
		return;

	b43_ntab_read_bulk(dev, B43_NTAB16(15, 80), 7, buffer);
	for (i = 0; i < 4; i++) {
		if (buffer[i] != nphy->txiqlocal_bestc[i]) {
			equal = false;
			break;
		}
	}

	if (!equal) {
		b43_ntab_write_bulk(dev, B43_NTAB16(15, 80), 4,
					nphy->txiqlocal_bestc);
		for (i = 0; i < 4; i++)
			buffer[i] = 0;
		b43_ntab_write_bulk(dev, B43_NTAB16(15, 88), 4,
					buffer);
		b43_ntab_write_bulk(dev, B43_NTAB16(15, 85), 2,
					&nphy->txiqlocal_bestc[5]);
		b43_ntab_write_bulk(dev, B43_NTAB16(15, 93), 2,
					&nphy->txiqlocal_bestc[5]);
	}
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/CalRxIqRev2 */
static int b43_nphy_rev2_cal_rx_iq(struct b43_wldev *dev,
			struct nphy_txgains target, u8 type, bool debug)
{
	struct b43_phy_n *nphy = dev->phy.n;
	int i, j, index;
	u8 rfctl[2];
	u8 afectl_core;
	u16 tmp[6];
	u16 cur_hpf1, cur_hpf2, cur_lna;
	u32 real, imag;
	enum ieee80211_band band;

	u8 use;
	u16 cur_hpf;
	u16 lna[3] = { 3, 3, 1 };
	u16 hpf1[3] = { 7, 2, 0 };
	u16 hpf2[3] = { 2, 0, 0 };
	u32 power[3] = { };
	u16 gain_save[2];
	u16 cal_gain[2];
	struct nphy_iqcal_params cal_params[2];
	struct nphy_iq_est est;
	int ret = 0;
	bool playtone = true;
	int desired = 13;

	b43_nphy_stay_in_carrier_search(dev, 1);

	if (dev->phy.rev < 2)
		b43_nphy_reapply_tx_cal_coeffs(dev);
	b43_ntab_read_bulk(dev, B43_NTAB16(7, 0x110), 2, gain_save);
	for (i = 0; i < 2; i++) {
		b43_nphy_iq_cal_gain_params(dev, i, target, &cal_params[i]);
		cal_gain[i] = cal_params[i].cal_gain;
	}
	b43_ntab_write_bulk(dev, B43_NTAB16(7, 0x110), 2, cal_gain);

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			rfctl[0] = B43_NPHY_RFCTL_INTC1;
			rfctl[1] = B43_NPHY_RFCTL_INTC2;
			afectl_core = B43_NPHY_AFECTL_C1;
		} else {
			rfctl[0] = B43_NPHY_RFCTL_INTC2;
			rfctl[1] = B43_NPHY_RFCTL_INTC1;
			afectl_core = B43_NPHY_AFECTL_C2;
		}

		tmp[1] = b43_phy_read(dev, B43_NPHY_RFSEQCA);
		tmp[2] = b43_phy_read(dev, afectl_core);
		tmp[3] = b43_phy_read(dev, B43_NPHY_AFECTL_OVER);
		tmp[4] = b43_phy_read(dev, rfctl[0]);
		tmp[5] = b43_phy_read(dev, rfctl[1]);

		b43_phy_maskset(dev, B43_NPHY_RFSEQCA,
				~B43_NPHY_RFSEQCA_RXDIS & 0xFFFF,
				((1 - i) << B43_NPHY_RFSEQCA_RXDIS_SHIFT));
		b43_phy_maskset(dev, B43_NPHY_RFSEQCA, ~B43_NPHY_RFSEQCA_TXEN,
				(1 - i));
		b43_phy_set(dev, afectl_core, 0x0006);
		b43_phy_set(dev, B43_NPHY_AFECTL_OVER, 0x0006);

		band = b43_current_band(dev->wl);

		if (nphy->rxcalparams & 0xFF000000) {
			if (band == IEEE80211_BAND_5GHZ)
				b43_phy_write(dev, rfctl[0], 0x140);
			else
				b43_phy_write(dev, rfctl[0], 0x110);
		} else {
			if (band == IEEE80211_BAND_5GHZ)
				b43_phy_write(dev, rfctl[0], 0x180);
			else
				b43_phy_write(dev, rfctl[0], 0x120);
		}

		if (band == IEEE80211_BAND_5GHZ)
			b43_phy_write(dev, rfctl[1], 0x148);
		else
			b43_phy_write(dev, rfctl[1], 0x114);

		if (nphy->rxcalparams & 0x10000) {
			b43_radio_maskset(dev, B2055_C1_GENSPARE2, 0xFC,
					(i + 1));
			b43_radio_maskset(dev, B2055_C2_GENSPARE2, 0xFC,
					(2 - i));
		}

		for (j = 0; i < 4; j++) {
			if (j < 3) {
				cur_lna = lna[j];
				cur_hpf1 = hpf1[j];
				cur_hpf2 = hpf2[j];
			} else {
				if (power[1] > 10000) {
					use = 1;
					cur_hpf = cur_hpf1;
					index = 2;
				} else {
					if (power[0] > 10000) {
						use = 1;
						cur_hpf = cur_hpf1;
						index = 1;
					} else {
						index = 0;
						use = 2;
						cur_hpf = cur_hpf2;
					}
				}
				cur_lna = lna[index];
				cur_hpf1 = hpf1[index];
				cur_hpf2 = hpf2[index];
				cur_hpf += desired - hweight32(power[index]);
				cur_hpf = clamp_val(cur_hpf, 0, 10);
				if (use == 1)
					cur_hpf1 = cur_hpf;
				else
					cur_hpf2 = cur_hpf;
			}

			tmp[0] = ((cur_hpf2 << 8) | (cur_hpf1 << 4) |
					(cur_lna << 2));
			b43_nphy_rf_control_override(dev, 0x400, tmp[0], 3,
									false);
			b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RESET2RX);
			b43_nphy_stop_playback(dev);

			if (playtone) {
				ret = b43_nphy_tx_tone(dev, 4000,
						(nphy->rxcalparams & 0xFFFF),
						false, false);
				playtone = false;
			} else {
				b43_nphy_run_samples(dev, 160, 0xFFFF, 0,
							false, false);
			}

			if (ret == 0) {
				if (j < 3) {
					b43_nphy_rx_iq_est(dev, &est, 1024, 32,
									false);
					if (i == 0) {
						real = est.i0_pwr;
						imag = est.q0_pwr;
					} else {
						real = est.i1_pwr;
						imag = est.q1_pwr;
					}
					power[i] = ((real + imag) / 1024) + 1;
				} else {
					b43_nphy_calc_rx_iq_comp(dev, 1 << i);
				}
				b43_nphy_stop_playback(dev);
			}

			if (ret != 0)
				break;
		}

		b43_radio_mask(dev, B2055_C1_GENSPARE2, 0xFC);
		b43_radio_mask(dev, B2055_C2_GENSPARE2, 0xFC);
		b43_phy_write(dev, rfctl[1], tmp[5]);
		b43_phy_write(dev, rfctl[0], tmp[4]);
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER, tmp[3]);
		b43_phy_write(dev, afectl_core, tmp[2]);
		b43_phy_write(dev, B43_NPHY_RFSEQCA, tmp[1]);

		if (ret != 0)
			break;
	}

	b43_nphy_rf_control_override(dev, 0x400, 0, 3, true);
	b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RESET2RX);
	b43_ntab_write_bulk(dev, B43_NTAB16(7, 0x110), 2, gain_save);

	b43_nphy_stay_in_carrier_search(dev, 0);

	return ret;
}

static int b43_nphy_rev3_cal_rx_iq(struct b43_wldev *dev,
			struct nphy_txgains target, u8 type, bool debug)
{
	return -1;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/CalRxIq */
static int b43_nphy_cal_rx_iq(struct b43_wldev *dev,
			struct nphy_txgains target, u8 type, bool debug)
{
	if (dev->phy.rev >= 3)
		return b43_nphy_rev3_cal_rx_iq(dev, target, type, debug);
	else
		return b43_nphy_rev2_cal_rx_iq(dev, target, type, debug);
}

/*
 * Init N-PHY
 * http://bcm-v4.sipsolutions.net/802.11/PHY/Init/N
 */
int b43_phy_initn(struct b43_wldev *dev)
{
	struct ssb_bus *bus = dev->dev->bus;
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_n *nphy = phy->n;
	u8 tx_pwr_state;
	struct nphy_txgains target;
	u16 tmp;
	enum ieee80211_band tmp2;
	bool do_rssi_cal;

	u16 clip[2];
	bool do_cal = false;

	if ((dev->phy.rev >= 3) &&
	   (bus->sprom.boardflags_lo & B43_BFL_EXTLNA) &&
	   (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)) {
		chipco_set32(&dev->dev->bus->chipco, SSB_CHIPCO_CHIPCTL, 0x40);
	}
	nphy->deaf_count = 0;
	b43_nphy_tables_init(dev);
	nphy->crsminpwr_adjusted = false;
	nphy->noisevars_adjusted = false;

	/* Clear all overrides */
	if (dev->phy.rev >= 3) {
		b43_phy_write(dev, B43_NPHY_TXF_40CO_B1S1, 0);
		b43_phy_write(dev, B43_NPHY_RFCTL_OVER, 0);
		b43_phy_write(dev, B43_NPHY_TXF_40CO_B1S0, 0);
		b43_phy_write(dev, B43_NPHY_TXF_40CO_B32S1, 0);
	} else {
		b43_phy_write(dev, B43_NPHY_RFCTL_OVER, 0);
	}
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC1, 0);
	b43_phy_write(dev, B43_NPHY_RFCTL_INTC2, 0);
	if (dev->phy.rev < 6) {
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC3, 0);
		b43_phy_write(dev, B43_NPHY_RFCTL_INTC4, 0);
	}
	b43_phy_mask(dev, B43_NPHY_RFSEQMODE,
		     ~(B43_NPHY_RFSEQMODE_CAOVER |
		       B43_NPHY_RFSEQMODE_TROVER));
	if (dev->phy.rev >= 3)
		b43_phy_write(dev, B43_NPHY_AFECTL_OVER1, 0);
	b43_phy_write(dev, B43_NPHY_AFECTL_OVER, 0);

	if (dev->phy.rev <= 2) {
		tmp = (dev->phy.rev == 2) ? 0x3B : 0x40;
		b43_phy_maskset(dev, B43_NPHY_BPHY_CTL3,
				~B43_NPHY_BPHY_CTL3_SCALE,
				tmp << B43_NPHY_BPHY_CTL3_SCALE_SHIFT);
	}
	b43_phy_write(dev, B43_NPHY_AFESEQ_TX2RX_PUD_20M, 0x20);
	b43_phy_write(dev, B43_NPHY_AFESEQ_TX2RX_PUD_40M, 0x20);

	if (bus->sprom.boardflags2_lo & 0x100 ||
	    (bus->boardinfo.vendor == PCI_VENDOR_ID_APPLE &&
	     bus->boardinfo.type == 0x8B))
		b43_phy_write(dev, B43_NPHY_TXREALFD, 0xA0);
	else
		b43_phy_write(dev, B43_NPHY_TXREALFD, 0xB8);
	b43_phy_write(dev, B43_NPHY_MIMO_CRSTXEXT, 0xC8);
	b43_phy_write(dev, B43_NPHY_PLOAD_CSENSE_EXTLEN, 0x50);
	b43_phy_write(dev, B43_NPHY_TXRIFS_FRDEL, 0x30);

	b43_nphy_update_mimo_config(dev, nphy->preamble_override);
	b43_nphy_update_txrx_chain(dev);

	if (phy->rev < 2) {
		b43_phy_write(dev, B43_NPHY_DUP40_GFBL, 0xAA8);
		b43_phy_write(dev, B43_NPHY_DUP40_BL, 0x9A4);
	}

	tmp2 = b43_current_band(dev->wl);
	if ((nphy->ipa2g_on && tmp2 == IEEE80211_BAND_2GHZ) ||
	    (nphy->ipa5g_on && tmp2 == IEEE80211_BAND_5GHZ)) {
		b43_phy_set(dev, B43_NPHY_PAPD_EN0, 0x1);
		b43_phy_maskset(dev, B43_NPHY_EPS_TABLE_ADJ0, 0x007F,
				nphy->papd_epsilon_offset[0] << 7);
		b43_phy_set(dev, B43_NPHY_PAPD_EN1, 0x1);
		b43_phy_maskset(dev, B43_NPHY_EPS_TABLE_ADJ1, 0x007F,
				nphy->papd_epsilon_offset[1] << 7);
		b43_nphy_int_pa_set_tx_dig_filters(dev);
	} else if (phy->rev >= 5) {
		b43_nphy_ext_pa_set_tx_dig_filters(dev);
	}

	b43_nphy_workarounds(dev);

	/* Reset CCA, in init code it differs a little from standard way */
	b43_nphy_bmac_clock_fgc(dev, 1);
	tmp = b43_phy_read(dev, B43_NPHY_BBCFG);
	b43_phy_write(dev, B43_NPHY_BBCFG, tmp | B43_NPHY_BBCFG_RSTCCA);
	b43_phy_write(dev, B43_NPHY_BBCFG, tmp & ~B43_NPHY_BBCFG_RSTCCA);
	b43_nphy_bmac_clock_fgc(dev, 0);

	/* TODO N PHY MAC PHY Clock Set with argument 1 */

	b43_nphy_pa_override(dev, false);
	b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RX2TX);
	b43_nphy_force_rf_sequence(dev, B43_RFSEQ_RESET2RX);
	b43_nphy_pa_override(dev, true);

	b43_nphy_classifier(dev, 0, 0);
	b43_nphy_read_clip_detection(dev, clip);
	tx_pwr_state = nphy->txpwrctrl;
	/* TODO N PHY TX power control with argument 0
		(turning off power control) */
	/* TODO Fix the TX Power Settings */
	/* TODO N PHY TX Power Control Idle TSSI */
	/* TODO N PHY TX Power Control Setup */

	if (phy->rev >= 3) {
		/* TODO */
	} else {
		b43_ntab_write_bulk(dev, B43_NTAB32(26, 192), 128,
					b43_ntab_tx_gain_rev0_1_2);
		b43_ntab_write_bulk(dev, B43_NTAB32(27, 192), 128,
					b43_ntab_tx_gain_rev0_1_2);
	}

	if (nphy->phyrxchain != 3)
		;/* TODO N PHY RX Core Set State with phyrxchain as argument */
	if (nphy->mphase_cal_phase_id > 0)
		;/* TODO PHY Periodic Calibration Multi-Phase Restart */

	do_rssi_cal = false;
	if (phy->rev >= 3) {
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
			do_rssi_cal =
				b43_empty_chanspec(&nphy->rssical_chanspec_2G);
		else
			do_rssi_cal =
				b43_empty_chanspec(&nphy->rssical_chanspec_5G);

		if (do_rssi_cal)
			b43_nphy_rssi_cal(dev);
		else
			b43_nphy_restore_rssi_cal(dev);
	} else {
		b43_nphy_rssi_cal(dev);
	}

	if (!((nphy->measure_hold & 0x6) != 0)) {
		if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
			do_cal = b43_empty_chanspec(&nphy->iqcal_chanspec_2G);
		else
			do_cal = b43_empty_chanspec(&nphy->iqcal_chanspec_5G);

		if (nphy->mute)
			do_cal = false;

		if (do_cal) {
			target = b43_nphy_get_tx_gains(dev);

			if (nphy->antsel_type == 2)
				b43_nphy_superswitch_init(dev, true);
			if (nphy->perical != 2) {
				b43_nphy_rssi_cal(dev);
				if (phy->rev >= 3) {
					nphy->cal_orig_pwr_idx[0] =
					    nphy->txpwrindex[0].index_internal;
					nphy->cal_orig_pwr_idx[1] =
					    nphy->txpwrindex[1].index_internal;
					/* TODO N PHY Pre Calibrate TX Gain */
					target = b43_nphy_get_tx_gains(dev);
				}
			}
		}
	}

	if (!b43_nphy_cal_tx_iq_lo(dev, target, true, false)) {
		if (b43_nphy_cal_rx_iq(dev, target, 2, 0) == 0)
			b43_nphy_save_cal(dev);
		else if (nphy->mphase_cal_phase_id == 0)
			;/* N PHY Periodic Calibration with argument 3 */
	} else {
		b43_nphy_restore_cal(dev);
	}

	b43_nphy_tx_pwr_ctrl_coef_setup(dev);
	/* TODO N PHY TX Power Control Enable with argument tx_pwr_state */
	b43_phy_write(dev, B43_NPHY_TXMACIF_HOLDOFF, 0x0015);
	b43_phy_write(dev, B43_NPHY_TXMACDELAY, 0x0320);
	if (phy->rev >= 3 && phy->rev <= 6)
		b43_phy_write(dev, B43_NPHY_PLOAD_CSENSE_EXTLEN, 0x0014);
	b43_nphy_tx_lp_fbw(dev);
	if (phy->rev >= 3)
		b43_nphy_spur_workaround(dev);

	b43err(dev->wl, "IEEE 802.11n devices are not supported, yet.\n");
	return 0;
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/ChanspecSetup */
static void b43_nphy_chanspec_setup(struct b43_wldev *dev,
				const struct b43_phy_n_sfo_cfg *e,
				struct b43_chanspec chanspec)
{
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_n *nphy = dev->phy.n;

	u16 tmp;
	u32 tmp32;

	tmp = b43_phy_read(dev, B43_NPHY_BANDCTL) & B43_NPHY_BANDCTL_5GHZ;
	if (chanspec.b_freq == 1 && tmp == 0) {
		tmp32 = b43_read32(dev, B43_MMIO_PSM_PHY_HDR);
		b43_write32(dev, B43_MMIO_PSM_PHY_HDR, tmp32 | 4);
		b43_phy_set(dev, B43_PHY_B_BBCFG, 0xC000);
		b43_write32(dev, B43_MMIO_PSM_PHY_HDR, tmp32);
		b43_phy_set(dev, B43_NPHY_BANDCTL, B43_NPHY_BANDCTL_5GHZ);
	} else if (chanspec.b_freq == 1) {
		b43_phy_mask(dev, B43_NPHY_BANDCTL, ~B43_NPHY_BANDCTL_5GHZ);
		tmp32 = b43_read32(dev, B43_MMIO_PSM_PHY_HDR);
		b43_write32(dev, B43_MMIO_PSM_PHY_HDR, tmp32 | 4);
		b43_phy_mask(dev, B43_PHY_B_BBCFG, 0x3FFF);
		b43_write32(dev, B43_MMIO_PSM_PHY_HDR, tmp32);
	}

	b43_chantab_phy_upload(dev, e);

	tmp = chanspec.channel;
	if (chanspec.b_freq == 1)
		tmp |= 0x0100;
	if (chanspec.b_width == 3)
		tmp |= 0x0200;
	b43_shm_write16(dev, B43_SHM_SHARED, 0xA0, tmp);

	if (nphy->radio_chanspec.channel == 14) {
		b43_nphy_classifier(dev, 2, 0);
		b43_phy_set(dev, B43_PHY_B_TEST, 0x0800);
	} else {
		b43_nphy_classifier(dev, 2, 2);
		if (chanspec.b_freq == 2)
			b43_phy_mask(dev, B43_PHY_B_TEST, ~0x840);
	}

	if (nphy->txpwrctrl)
		b43_nphy_tx_power_fix(dev);

	if (dev->phy.rev < 3)
		b43_nphy_adjust_lna_gain_table(dev);

	b43_nphy_tx_lp_fbw(dev);

	if (dev->phy.rev >= 3 && 0) {
		/* TODO */
	}

	b43_phy_write(dev, B43_NPHY_NDATAT_DUP40, 0x3830);

	if (phy->rev >= 3)
		b43_nphy_spur_workaround(dev);
}

/* http://bcm-v4.sipsolutions.net/802.11/PHY/N/SetChanspec */
static int b43_nphy_set_chanspec(struct b43_wldev *dev,
					struct b43_chanspec chanspec)
{
	struct b43_phy_n *nphy = dev->phy.n;

	const struct b43_nphy_channeltab_entry_rev2 *tabent_r2;
	const struct b43_nphy_channeltab_entry_rev3 *tabent_r3;

	u8 tmp;
	u8 channel = chanspec.channel;

	if (dev->phy.rev >= 3) {
		/* TODO */
		tabent_r3 = NULL;
		if (!tabent_r3)
			return -ESRCH;
	} else {
		tabent_r2 = b43_nphy_get_chantabent_rev2(dev, channel);
		if (!tabent_r2)
			return -ESRCH;
	}

	nphy->radio_chanspec = chanspec;

	if (chanspec.b_width != nphy->b_width)
		; /* TODO: BMAC BW Set (chanspec.b_width) */

	/* TODO: use defines */
	if (chanspec.b_width == 3) {
		if (chanspec.sideband == 2)
			b43_phy_set(dev, B43_NPHY_RXCTL,
					B43_NPHY_RXCTL_BSELU20);
		else
			b43_phy_mask(dev, B43_NPHY_RXCTL,
					~B43_NPHY_RXCTL_BSELU20);
	}

	if (dev->phy.rev >= 3) {
		tmp = (chanspec.b_freq == 1) ? 4 : 0;
		b43_radio_maskset(dev, 0x08, 0xFFFB, tmp);
		/* TODO: PHY Radio2056 Setup (dev, tabent_r3); */
		b43_nphy_chanspec_setup(dev, &(tabent_r3->phy_regs), chanspec);
	} else {
		tmp = (chanspec.b_freq == 1) ? 0x0020 : 0x0050;
		b43_radio_maskset(dev, B2055_MASTER1, 0xFF8F, tmp);
		b43_radio_2055_setup(dev, tabent_r2);
		b43_nphy_chanspec_setup(dev, &(tabent_r2->phy_regs), chanspec);
	}

	return 0;
}

/* Tune the hardware to a new channel */
static int nphy_channel_switch(struct b43_wldev *dev, unsigned int channel)
{
	struct b43_phy_n *nphy = dev->phy.n;

	struct b43_chanspec chanspec;
	chanspec = nphy->radio_chanspec;
	chanspec.channel = channel;

	return b43_nphy_set_chanspec(dev, chanspec);
}

static int b43_nphy_op_allocate(struct b43_wldev *dev)
{
	struct b43_phy_n *nphy;

	nphy = kzalloc(sizeof(*nphy), GFP_KERNEL);
	if (!nphy)
		return -ENOMEM;
	dev->phy.n = nphy;

	return 0;
}

static void b43_nphy_op_prepare_structs(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_n *nphy = phy->n;

	memset(nphy, 0, sizeof(*nphy));

	//TODO init struct b43_phy_n
}

static void b43_nphy_op_free(struct b43_wldev *dev)
{
	struct b43_phy *phy = &dev->phy;
	struct b43_phy_n *nphy = phy->n;

	kfree(nphy);
	phy->n = NULL;
}

static int b43_nphy_op_init(struct b43_wldev *dev)
{
	return b43_phy_initn(dev);
}

static inline void check_phyreg(struct b43_wldev *dev, u16 offset)
{
#if B43_DEBUG
	if ((offset & B43_PHYROUTE) == B43_PHYROUTE_OFDM_GPHY) {
		/* OFDM registers are onnly available on A/G-PHYs */
		b43err(dev->wl, "Invalid OFDM PHY access at "
		       "0x%04X on N-PHY\n", offset);
		dump_stack();
	}
	if ((offset & B43_PHYROUTE) == B43_PHYROUTE_EXT_GPHY) {
		/* Ext-G registers are only available on G-PHYs */
		b43err(dev->wl, "Invalid EXT-G PHY access at "
		       "0x%04X on N-PHY\n", offset);
		dump_stack();
	}
#endif /* B43_DEBUG */
}

static u16 b43_nphy_op_read(struct b43_wldev *dev, u16 reg)
{
	check_phyreg(dev, reg);
	b43_write16(dev, B43_MMIO_PHY_CONTROL, reg);
	return b43_read16(dev, B43_MMIO_PHY_DATA);
}

static void b43_nphy_op_write(struct b43_wldev *dev, u16 reg, u16 value)
{
	check_phyreg(dev, reg);
	b43_write16(dev, B43_MMIO_PHY_CONTROL, reg);
	b43_write16(dev, B43_MMIO_PHY_DATA, value);
}

static u16 b43_nphy_op_radio_read(struct b43_wldev *dev, u16 reg)
{
	/* Register 1 is a 32-bit register. */
	B43_WARN_ON(reg == 1);
	/* N-PHY needs 0x100 for read access */
	reg |= 0x100;

	b43_write16(dev, B43_MMIO_RADIO_CONTROL, reg);
	return b43_read16(dev, B43_MMIO_RADIO_DATA_LOW);
}

static void b43_nphy_op_radio_write(struct b43_wldev *dev, u16 reg, u16 value)
{
	/* Register 1 is a 32-bit register. */
	B43_WARN_ON(reg == 1);

	b43_write16(dev, B43_MMIO_RADIO_CONTROL, reg);
	b43_write16(dev, B43_MMIO_RADIO_DATA_LOW, value);
}

/* http://bcm-v4.sipsolutions.net/802.11/Radio/Switch%20Radio */
static void b43_nphy_op_software_rfkill(struct b43_wldev *dev,
					bool blocked)
{
	struct b43_phy_n *nphy = dev->phy.n;

	if (b43_read32(dev, B43_MMIO_MACCTL) & B43_MACCTL_ENABLED)
		b43err(dev->wl, "MAC not suspended\n");

	if (blocked) {
		b43_phy_mask(dev, B43_NPHY_RFCTL_CMD,
				~B43_NPHY_RFCTL_CMD_CHIP0PU);
		if (dev->phy.rev >= 3) {
			b43_radio_mask(dev, 0x09, ~0x2);

			b43_radio_write(dev, 0x204D, 0);
			b43_radio_write(dev, 0x2053, 0);
			b43_radio_write(dev, 0x2058, 0);
			b43_radio_write(dev, 0x205E, 0);
			b43_radio_mask(dev, 0x2062, ~0xF0);
			b43_radio_write(dev, 0x2064, 0);

			b43_radio_write(dev, 0x304D, 0);
			b43_radio_write(dev, 0x3053, 0);
			b43_radio_write(dev, 0x3058, 0);
			b43_radio_write(dev, 0x305E, 0);
			b43_radio_mask(dev, 0x3062, ~0xF0);
			b43_radio_write(dev, 0x3064, 0);
		}
	} else {
		if (dev->phy.rev >= 3) {
			b43_radio_init2056(dev);
			b43_nphy_set_chanspec(dev, nphy->radio_chanspec);
		} else {
			b43_radio_init2055(dev);
		}
	}
}

static void b43_nphy_op_switch_analog(struct b43_wldev *dev, bool on)
{
	b43_phy_write(dev, B43_NPHY_AFECTL_OVER,
		      on ? 0 : 0x7FFF);
}

static int b43_nphy_op_switch_channel(struct b43_wldev *dev,
				      unsigned int new_channel)
{
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ) {
		if ((new_channel < 1) || (new_channel > 14))
			return -EINVAL;
	} else {
		if (new_channel > 200)
			return -EINVAL;
	}

	return nphy_channel_switch(dev, new_channel);
}

static unsigned int b43_nphy_op_get_default_chan(struct b43_wldev *dev)
{
	if (b43_current_band(dev->wl) == IEEE80211_BAND_2GHZ)
		return 1;
	return 36;
}

const struct b43_phy_operations b43_phyops_n = {
	.allocate		= b43_nphy_op_allocate,
	.free			= b43_nphy_op_free,
	.prepare_structs	= b43_nphy_op_prepare_structs,
	.init			= b43_nphy_op_init,
	.phy_read		= b43_nphy_op_read,
	.phy_write		= b43_nphy_op_write,
	.radio_read		= b43_nphy_op_radio_read,
	.radio_write		= b43_nphy_op_radio_write,
	.software_rfkill	= b43_nphy_op_software_rfkill,
	.switch_analog		= b43_nphy_op_switch_analog,
	.switch_channel		= b43_nphy_op_switch_channel,
	.get_default_chan	= b43_nphy_op_get_default_chan,
	.recalc_txpower		= b43_nphy_op_recalc_txpower,
	.adjust_txpower		= b43_nphy_op_adjust_txpower,
};
