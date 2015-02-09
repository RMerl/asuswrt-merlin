/*
 * Copyright (c) 2008-2009 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "hw.h"

static inline u16 ath9k_hw_fbin2freq(u8 fbin, bool is2GHz)
{
	if (fbin == AR5416_BCHAN_UNUSED)
		return fbin;

	return (u16) ((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

void ath9k_hw_analog_shift_regwrite(struct ath_hw *ah, u32 reg, u32 val)
{
        REG_WRITE(ah, reg, val);

        if (ah->config.analog_shiftreg)
		udelay(100);
}

void ath9k_hw_analog_shift_rmw(struct ath_hw *ah, u32 reg, u32 mask,
			       u32 shift, u32 val)
{
	u32 regVal;

	regVal = REG_READ(ah, reg) & ~mask;
	regVal |= (val << shift) & mask;

	REG_WRITE(ah, reg, regVal);

	if (ah->config.analog_shiftreg)
		udelay(100);
}

int16_t ath9k_hw_interpolate(u16 target, u16 srcLeft, u16 srcRight,
			     int16_t targetLeft, int16_t targetRight)
{
	int16_t rv;

	if (srcRight == srcLeft) {
		rv = targetLeft;
	} else {
		rv = (int16_t) (((target - srcLeft) * targetRight +
				 (srcRight - target) * targetLeft) /
				(srcRight - srcLeft));
	}
	return rv;
}

bool ath9k_hw_get_lower_upper_index(u8 target, u8 *pList, u16 listSize,
				    u16 *indexL, u16 *indexR)
{
	u16 i;

	if (target <= pList[0]) {
		*indexL = *indexR = 0;
		return true;
	}
	if (target >= pList[listSize - 1]) {
		*indexL = *indexR = (u16) (listSize - 1);
		return true;
	}

	for (i = 0; i < listSize - 1; i++) {
		if (pList[i] == target) {
			*indexL = *indexR = i;
			return true;
		}
		if (target < pList[i + 1]) {
			*indexL = i;
			*indexR = (u16) (i + 1);
			return false;
		}
	}
	return false;
}

bool ath9k_hw_nvram_read(struct ath_common *common, u32 off, u16 *data)
{
	return common->bus_ops->eeprom_read(common, off, data);
}

void ath9k_hw_fill_vpd_table(u8 pwrMin, u8 pwrMax, u8 *pPwrList,
			     u8 *pVpdList, u16 numIntercepts,
			     u8 *pRetVpdList)
{
	u16 i, k;
	u8 currPwr = pwrMin;
	u16 idxL = 0, idxR = 0;

	for (i = 0; i <= (pwrMax - pwrMin) / 2; i++) {
		ath9k_hw_get_lower_upper_index(currPwr, pPwrList,
					       numIntercepts, &(idxL),
					       &(idxR));
		if (idxR < 1)
			idxR = 1;
		if (idxL == numIntercepts - 1)
			idxL = (u16) (numIntercepts - 2);
		if (pPwrList[idxL] == pPwrList[idxR])
			k = pVpdList[idxL];
		else
			k = (u16)(((currPwr - pPwrList[idxL]) * pVpdList[idxR] +
				   (pPwrList[idxR] - currPwr) * pVpdList[idxL]) /
				  (pPwrList[idxR] - pPwrList[idxL]));
		pRetVpdList[i] = (u8) k;
		currPwr += 2;
	}
}

void ath9k_hw_get_legacy_target_powers(struct ath_hw *ah,
				       struct ath9k_channel *chan,
				       struct cal_target_power_leg *powInfo,
				       u16 numChannels,
				       struct cal_target_power_leg *pNewPower,
				       u16 numRates, bool isExtTarget)
{
	struct chan_centers centers;
	u16 clo, chi;
	int i;
	int matchIndex = -1, lowIndex = -1;
	u16 freq;

	ath9k_hw_get_channel_centers(ah, chan, &centers);
	freq = (isExtTarget) ? centers.ext_center : centers.ctl_center;

	if (freq <= ath9k_hw_fbin2freq(powInfo[0].bChannel,
				       IS_CHAN_2GHZ(chan))) {
		matchIndex = 0;
	} else {
		for (i = 0; (i < numChannels) &&
			     (powInfo[i].bChannel != AR5416_BCHAN_UNUSED); i++) {
			if (freq == ath9k_hw_fbin2freq(powInfo[i].bChannel,
						       IS_CHAN_2GHZ(chan))) {
				matchIndex = i;
				break;
			} else if (freq < ath9k_hw_fbin2freq(powInfo[i].bChannel,
						IS_CHAN_2GHZ(chan)) && i > 0 &&
				   freq > ath9k_hw_fbin2freq(powInfo[i - 1].bChannel,
						IS_CHAN_2GHZ(chan))) {
				lowIndex = i - 1;
				break;
			}
		}
		if ((matchIndex == -1) && (lowIndex == -1))
			matchIndex = i - 1;
	}

	if (matchIndex != -1) {
		*pNewPower = powInfo[matchIndex];
	} else {
		clo = ath9k_hw_fbin2freq(powInfo[lowIndex].bChannel,
					 IS_CHAN_2GHZ(chan));
		chi = ath9k_hw_fbin2freq(powInfo[lowIndex + 1].bChannel,
					 IS_CHAN_2GHZ(chan));

		for (i = 0; i < numRates; i++) {
			pNewPower->tPow2x[i] =
				(u8)ath9k_hw_interpolate(freq, clo, chi,
						powInfo[lowIndex].tPow2x[i],
						powInfo[lowIndex + 1].tPow2x[i]);
		}
	}
}

void ath9k_hw_get_target_powers(struct ath_hw *ah,
				struct ath9k_channel *chan,
				struct cal_target_power_ht *powInfo,
				u16 numChannels,
				struct cal_target_power_ht *pNewPower,
				u16 numRates, bool isHt40Target)
{
	struct chan_centers centers;
	u16 clo, chi;
	int i;
	int matchIndex = -1, lowIndex = -1;
	u16 freq;

	ath9k_hw_get_channel_centers(ah, chan, &centers);
	freq = isHt40Target ? centers.synth_center : centers.ctl_center;

	if (freq <= ath9k_hw_fbin2freq(powInfo[0].bChannel, IS_CHAN_2GHZ(chan))) {
		matchIndex = 0;
	} else {
		for (i = 0; (i < numChannels) &&
			     (powInfo[i].bChannel != AR5416_BCHAN_UNUSED); i++) {
			if (freq == ath9k_hw_fbin2freq(powInfo[i].bChannel,
						       IS_CHAN_2GHZ(chan))) {
				matchIndex = i;
				break;
			} else
				if (freq < ath9k_hw_fbin2freq(powInfo[i].bChannel,
						IS_CHAN_2GHZ(chan)) && i > 0 &&
				    freq > ath9k_hw_fbin2freq(powInfo[i - 1].bChannel,
						IS_CHAN_2GHZ(chan))) {
					lowIndex = i - 1;
					break;
				}
		}
		if ((matchIndex == -1) && (lowIndex == -1))
			matchIndex = i - 1;
	}

	if (matchIndex != -1) {
		*pNewPower = powInfo[matchIndex];
	} else {
		clo = ath9k_hw_fbin2freq(powInfo[lowIndex].bChannel,
					 IS_CHAN_2GHZ(chan));
		chi = ath9k_hw_fbin2freq(powInfo[lowIndex + 1].bChannel,
					 IS_CHAN_2GHZ(chan));

		for (i = 0; i < numRates; i++) {
			pNewPower->tPow2x[i] = (u8)ath9k_hw_interpolate(freq,
						clo, chi,
						powInfo[lowIndex].tPow2x[i],
						powInfo[lowIndex + 1].tPow2x[i]);
		}
	}
}

u16 ath9k_hw_get_max_edge_power(u16 freq, struct cal_ctl_edges *pRdEdgesPower,
				bool is2GHz, int num_band_edges)
{
	u16 twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
	int i;

	for (i = 0; (i < num_band_edges) &&
		     (pRdEdgesPower[i].bChannel != AR5416_BCHAN_UNUSED); i++) {
		if (freq == ath9k_hw_fbin2freq(pRdEdgesPower[i].bChannel, is2GHz)) {
			twiceMaxEdgePower = CTL_EDGE_TPOWER(pRdEdgesPower[i].ctl);
			break;
		} else if ((i > 0) &&
			   (freq < ath9k_hw_fbin2freq(pRdEdgesPower[i].bChannel,
						      is2GHz))) {
			if (ath9k_hw_fbin2freq(pRdEdgesPower[i - 1].bChannel,
					       is2GHz) < freq &&
			    CTL_EDGE_FLAGS(pRdEdgesPower[i - 1].ctl)) {
				twiceMaxEdgePower =
					CTL_EDGE_TPOWER(pRdEdgesPower[i - 1].ctl);
			}
			break;
		}
	}

	return twiceMaxEdgePower;
}

void ath9k_hw_update_regulatory_maxpower(struct ath_hw *ah)
{
	struct ath_common *common = ath9k_hw_common(ah);
	struct ath_regulatory *regulatory = ath9k_hw_regulatory(ah);

	switch (ar5416_get_ntxchains(ah->txchainmask)) {
	case 1:
		break;
	case 2:
		regulatory->max_power_level += INCREASE_MAXPOW_BY_TWO_CHAIN;
		break;
	case 3:
		regulatory->max_power_level += INCREASE_MAXPOW_BY_THREE_CHAIN;
		break;
	default:
		ath_print(common, ATH_DBG_EEPROM,
			  "Invalid chainmask configuration\n");
		break;
	}
}

int ath9k_hw_eeprom_init(struct ath_hw *ah)
{
	int status;

	if (AR_SREV_9300_20_OR_LATER(ah))
		ah->eep_ops = &eep_ar9300_ops;
	else if (AR_SREV_9287(ah)) {
		ah->eep_ops = &eep_ar9287_ops;
	} else if (AR_SREV_9285(ah) || AR_SREV_9271(ah)) {
		ah->eep_ops = &eep_4k_ops;
	} else {
		ah->eep_ops = &eep_def_ops;
	}

	if (!ah->eep_ops->fill_eeprom(ah))
		return -EIO;

	status = ah->eep_ops->check_eeprom(ah);

	return status;
}
