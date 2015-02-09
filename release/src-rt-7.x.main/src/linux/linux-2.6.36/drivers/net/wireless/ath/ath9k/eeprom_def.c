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
#include "ar9002_phy.h"

static void ath9k_get_txgain_index(struct ath_hw *ah,
		struct ath9k_channel *chan,
		struct calDataPerFreqOpLoop *rawDatasetOpLoop,
		u8 *calChans,  u16 availPiers, u8 *pwr, u8 *pcdacIdx)
{
	u8 pcdac, i = 0;
	u16 idxL = 0, idxR = 0, numPiers;
	bool match;
	struct chan_centers centers;

	ath9k_hw_get_channel_centers(ah, chan, &centers);

	for (numPiers = 0; numPiers < availPiers; numPiers++)
		if (calChans[numPiers] == AR5416_BCHAN_UNUSED)
			break;

	match = ath9k_hw_get_lower_upper_index(
			(u8)FREQ2FBIN(centers.synth_center, IS_CHAN_2GHZ(chan)),
			calChans, numPiers, &idxL, &idxR);
	if (match) {
		pcdac = rawDatasetOpLoop[idxL].pcdac[0][0];
		*pwr = rawDatasetOpLoop[idxL].pwrPdg[0][0];
	} else {
		pcdac = rawDatasetOpLoop[idxR].pcdac[0][0];
		*pwr = (rawDatasetOpLoop[idxL].pwrPdg[0][0] +
				rawDatasetOpLoop[idxR].pwrPdg[0][0])/2;
	}

	while (pcdac > ah->originalGain[i] &&
			i < (AR9280_TX_GAIN_TABLE_SIZE - 1))
		i++;

	*pcdacIdx = i;
}

static void ath9k_olc_get_pdadcs(struct ath_hw *ah,
				u32 initTxGain,
				int txPower,
				u8 *pPDADCValues)
{
	u32 i;
	u32 offset;

	REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL6_0,
			AR_PHY_TX_PWRCTRL_ERR_EST_MODE, 3);
	REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL6_1,
			AR_PHY_TX_PWRCTRL_ERR_EST_MODE, 3);

	REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL7,
			AR_PHY_TX_PWRCTRL_INIT_TX_GAIN, initTxGain);

	offset = txPower;
	for (i = 0; i < AR5416_NUM_PDADC_VALUES; i++)
		if (i < offset)
			pPDADCValues[i] = 0x0;
		else
			pPDADCValues[i] = 0xFF;
}

static int ath9k_hw_def_get_eeprom_ver(struct ath_hw *ah)
{
	return ((ah->eeprom.def.baseEepHeader.version >> 12) & 0xF);
}

static int ath9k_hw_def_get_eeprom_rev(struct ath_hw *ah)
{
	return ((ah->eeprom.def.baseEepHeader.version) & 0xFFF);
}

static bool ath9k_hw_def_fill_eeprom(struct ath_hw *ah)
{
#define SIZE_EEPROM_DEF (sizeof(struct ar5416_eeprom_def) / sizeof(u16))
	struct ath_common *common = ath9k_hw_common(ah);
	u16 *eep_data = (u16 *)&ah->eeprom.def;
	int addr, ar5416_eep_start_loc = 0x100;

	for (addr = 0; addr < SIZE_EEPROM_DEF; addr++) {
		if (!ath9k_hw_nvram_read(common, addr + ar5416_eep_start_loc,
					 eep_data)) {
			ath_print(ath9k_hw_common(ah), ATH_DBG_FATAL,
				  "Unable to read eeprom region\n");
			return false;
		}
		eep_data++;
	}
	return true;
#undef SIZE_EEPROM_DEF
}

static int ath9k_hw_def_check_eeprom(struct ath_hw *ah)
{
	struct ar5416_eeprom_def *eep =
		(struct ar5416_eeprom_def *) &ah->eeprom.def;
	struct ath_common *common = ath9k_hw_common(ah);
	u16 *eepdata, temp, magic, magic2;
	u32 sum = 0, el;
	bool need_swap = false;
	int i, addr, size;

	if (!ath9k_hw_nvram_read(common, AR5416_EEPROM_MAGIC_OFFSET, &magic)) {
		ath_print(common, ATH_DBG_FATAL, "Reading Magic # failed\n");
		return false;
	}

	if (!ath9k_hw_use_flash(ah)) {
		ath_print(common, ATH_DBG_EEPROM,
			  "Read Magic = 0x%04X\n", magic);

		if (magic != AR5416_EEPROM_MAGIC) {
			magic2 = swab16(magic);

			if (magic2 == AR5416_EEPROM_MAGIC) {
				size = sizeof(struct ar5416_eeprom_def);
				need_swap = true;
				eepdata = (u16 *) (&ah->eeprom);

				for (addr = 0; addr < size / sizeof(u16); addr++) {
					temp = swab16(*eepdata);
					*eepdata = temp;
					eepdata++;
				}
			} else {
				ath_print(common, ATH_DBG_FATAL,
					  "Invalid EEPROM Magic. "
					  "Endianness mismatch.\n");
				return -EINVAL;
			}
		}
	}

	ath_print(common, ATH_DBG_EEPROM, "need_swap = %s.\n",
		  need_swap ? "True" : "False");

	if (need_swap)
		el = swab16(ah->eeprom.def.baseEepHeader.length);
	else
		el = ah->eeprom.def.baseEepHeader.length;

	if (el > sizeof(struct ar5416_eeprom_def))
		el = sizeof(struct ar5416_eeprom_def) / sizeof(u16);
	else
		el = el / sizeof(u16);

	eepdata = (u16 *)(&ah->eeprom);

	for (i = 0; i < el; i++)
		sum ^= *eepdata++;

	if (need_swap) {
		u32 integer, j;
		u16 word;

		ath_print(common, ATH_DBG_EEPROM,
			  "EEPROM Endianness is not native.. Changing.\n");

		word = swab16(eep->baseEepHeader.length);
		eep->baseEepHeader.length = word;

		word = swab16(eep->baseEepHeader.checksum);
		eep->baseEepHeader.checksum = word;

		word = swab16(eep->baseEepHeader.version);
		eep->baseEepHeader.version = word;

		word = swab16(eep->baseEepHeader.regDmn[0]);
		eep->baseEepHeader.regDmn[0] = word;

		word = swab16(eep->baseEepHeader.regDmn[1]);
		eep->baseEepHeader.regDmn[1] = word;

		word = swab16(eep->baseEepHeader.rfSilent);
		eep->baseEepHeader.rfSilent = word;

		word = swab16(eep->baseEepHeader.blueToothOptions);
		eep->baseEepHeader.blueToothOptions = word;

		word = swab16(eep->baseEepHeader.deviceCap);
		eep->baseEepHeader.deviceCap = word;

		for (j = 0; j < ARRAY_SIZE(eep->modalHeader); j++) {
			struct modal_eep_header *pModal =
				&eep->modalHeader[j];
			integer = swab32(pModal->antCtrlCommon);
			pModal->antCtrlCommon = integer;

			for (i = 0; i < AR5416_MAX_CHAINS; i++) {
				integer = swab32(pModal->antCtrlChain[i]);
				pModal->antCtrlChain[i] = integer;
			}

			for (i = 0; i < AR5416_EEPROM_MODAL_SPURS; i++) {
				word = swab16(pModal->spurChans[i].spurChan);
				pModal->spurChans[i].spurChan = word;
			}
		}
	}

	if (sum != 0xffff || ah->eep_ops->get_eeprom_ver(ah) != AR5416_EEP_VER ||
	    ah->eep_ops->get_eeprom_rev(ah) < AR5416_EEP_NO_BACK_VER) {
		ath_print(common, ATH_DBG_FATAL,
			  "Bad EEPROM checksum 0x%x or revision 0x%04x\n",
			sum, ah->eep_ops->get_eeprom_ver(ah));
		return -EINVAL;
	}

	/* Enable fixup for AR_AN_TOP2 if necessary */
	if (AR_SREV_9280_10_OR_LATER(ah) &&
	    (eep->baseEepHeader.version & 0xff) > 0x0a &&
	    eep->baseEepHeader.pwdclkind == 0)
		ah->need_an_top2_fixup = 1;

	return 0;
}

static u32 ath9k_hw_def_get_eeprom(struct ath_hw *ah,
				   enum eeprom_param param)
{
	struct ar5416_eeprom_def *eep = &ah->eeprom.def;
	struct modal_eep_header *pModal = eep->modalHeader;
	struct base_eep_header *pBase = &eep->baseEepHeader;

	switch (param) {
	case EEP_NFTHRESH_5:
		return pModal[0].noiseFloorThreshCh[0];
	case EEP_NFTHRESH_2:
		return pModal[1].noiseFloorThreshCh[0];
	case EEP_MAC_LSW:
		return pBase->macAddr[0] << 8 | pBase->macAddr[1];
	case EEP_MAC_MID:
		return pBase->macAddr[2] << 8 | pBase->macAddr[3];
	case EEP_MAC_MSW:
		return pBase->macAddr[4] << 8 | pBase->macAddr[5];
	case EEP_REG_0:
		return pBase->regDmn[0];
	case EEP_REG_1:
		return pBase->regDmn[1];
	case EEP_OP_CAP:
		return pBase->deviceCap;
	case EEP_OP_MODE:
		return pBase->opCapFlags;
	case EEP_RF_SILENT:
		return pBase->rfSilent;
	case EEP_OB_5:
		return pModal[0].ob;
	case EEP_DB_5:
		return pModal[0].db;
	case EEP_OB_2:
		return pModal[1].ob;
	case EEP_DB_2:
		return pModal[1].db;
	case EEP_MINOR_REV:
		return AR5416_VER_MASK;
	case EEP_TX_MASK:
		return pBase->txMask;
	case EEP_RX_MASK:
		return pBase->rxMask;
	case EEP_FSTCLK_5G:
		return pBase->fastClk5g;
	case EEP_RXGAIN_TYPE:
		return pBase->rxGainType;
	case EEP_TXGAIN_TYPE:
		return pBase->txGainType;
	case EEP_OL_PWRCTRL:
		if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_19)
			return pBase->openLoopPwrCntl ? true : false;
		else
			return false;
	case EEP_RC_CHAIN_MASK:
		if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_19)
			return pBase->rcChainMask;
		else
			return 0;
	case EEP_DAC_HPWR_5G:
		if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_20)
			return pBase->dacHiPwrMode_5G;
		else
			return 0;
	case EEP_FRAC_N_5G:
		if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_22)
			return pBase->frac_n_5g;
		else
			return 0;
	case EEP_PWR_TABLE_OFFSET:
		if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_21)
			return pBase->pwr_table_offset;
		else
			return AR5416_PWR_TABLE_OFFSET_DB;
	default:
		return 0;
	}
}

static void ath9k_hw_def_set_gain(struct ath_hw *ah,
				  struct modal_eep_header *pModal,
				  struct ar5416_eeprom_def *eep,
				  u8 txRxAttenLocal, int regChainOffset, int i)
{
	if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_3) {
		txRxAttenLocal = pModal->txRxAttenCh[i];

		if (AR_SREV_9280_10_OR_LATER(ah)) {
			REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
			      AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN,
			      pModal->bswMargin[i]);
			REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
			      AR_PHY_GAIN_2GHZ_XATTEN1_DB,
			      pModal->bswAtten[i]);
			REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
			      AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN,
			      pModal->xatten2Margin[i]);
			REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
			      AR_PHY_GAIN_2GHZ_XATTEN2_DB,
			      pModal->xatten2Db[i]);
		} else {
			REG_WRITE(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
			  (REG_READ(ah, AR_PHY_GAIN_2GHZ + regChainOffset) &
			   ~AR_PHY_GAIN_2GHZ_BSW_MARGIN)
			  | SM(pModal-> bswMargin[i],
			       AR_PHY_GAIN_2GHZ_BSW_MARGIN));
			REG_WRITE(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
			  (REG_READ(ah, AR_PHY_GAIN_2GHZ + regChainOffset) &
			   ~AR_PHY_GAIN_2GHZ_BSW_ATTEN)
			  | SM(pModal->bswAtten[i],
			       AR_PHY_GAIN_2GHZ_BSW_ATTEN));
		}
	}

	if (AR_SREV_9280_10_OR_LATER(ah)) {
		REG_RMW_FIELD(ah,
		      AR_PHY_RXGAIN + regChainOffset,
		      AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
		REG_RMW_FIELD(ah,
		      AR_PHY_RXGAIN + regChainOffset,
		      AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[i]);
	} else {
		REG_WRITE(ah,
			  AR_PHY_RXGAIN + regChainOffset,
			  (REG_READ(ah, AR_PHY_RXGAIN + regChainOffset) &
			   ~AR_PHY_RXGAIN_TXRX_ATTEN)
			  | SM(txRxAttenLocal, AR_PHY_RXGAIN_TXRX_ATTEN));
		REG_WRITE(ah,
			  AR_PHY_GAIN_2GHZ + regChainOffset,
			  (REG_READ(ah, AR_PHY_GAIN_2GHZ + regChainOffset) &
			   ~AR_PHY_GAIN_2GHZ_RXTX_MARGIN) |
			  SM(pModal->rxTxMarginCh[i], AR_PHY_GAIN_2GHZ_RXTX_MARGIN));
	}
}

static void ath9k_hw_def_set_board_values(struct ath_hw *ah,
					  struct ath9k_channel *chan)
{
	struct modal_eep_header *pModal;
	struct ar5416_eeprom_def *eep = &ah->eeprom.def;
	int i, regChainOffset;
	u8 txRxAttenLocal;

	pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);
	txRxAttenLocal = IS_CHAN_2GHZ(chan) ? 23 : 44;

	REG_WRITE(ah, AR_PHY_SWITCH_COM,
		  ah->eep_ops->get_eeprom_antenna_cfg(ah, chan));

	for (i = 0; i < AR5416_MAX_CHAINS; i++) {
		if (AR_SREV_9280(ah)) {
			if (i >= 2)
				break;
		}

		if (AR_SREV_5416_20_OR_LATER(ah) &&
		    (ah->rxchainmask == 5 || ah->txchainmask == 5) && (i != 0))
			regChainOffset = (i == 1) ? 0x2000 : 0x1000;
		else
			regChainOffset = i * 0x1000;

		REG_WRITE(ah, AR_PHY_SWITCH_CHAIN_0 + regChainOffset,
			  pModal->antCtrlChain[i]);

		REG_WRITE(ah, AR_PHY_TIMING_CTRL4(0) + regChainOffset,
			  (REG_READ(ah, AR_PHY_TIMING_CTRL4(0) + regChainOffset) &
			   ~(AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF |
			     AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF)) |
			  SM(pModal->iqCalICh[i],
			     AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF) |
			  SM(pModal->iqCalQCh[i],
			     AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF));

		if ((i == 0) || AR_SREV_5416_20_OR_LATER(ah))
			ath9k_hw_def_set_gain(ah, pModal, eep, txRxAttenLocal,
					      regChainOffset, i);
	}

	if (AR_SREV_9280_10_OR_LATER(ah)) {
		if (IS_CHAN_2GHZ(chan)) {
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF2G1_CH0,
						  AR_AN_RF2G1_CH0_OB,
						  AR_AN_RF2G1_CH0_OB_S,
						  pModal->ob);
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF2G1_CH0,
						  AR_AN_RF2G1_CH0_DB,
						  AR_AN_RF2G1_CH0_DB_S,
						  pModal->db);
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF2G1_CH1,
						  AR_AN_RF2G1_CH1_OB,
						  AR_AN_RF2G1_CH1_OB_S,
						  pModal->ob_ch1);
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF2G1_CH1,
						  AR_AN_RF2G1_CH1_DB,
						  AR_AN_RF2G1_CH1_DB_S,
						  pModal->db_ch1);
		} else {
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF5G1_CH0,
						  AR_AN_RF5G1_CH0_OB5,
						  AR_AN_RF5G1_CH0_OB5_S,
						  pModal->ob);
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF5G1_CH0,
						  AR_AN_RF5G1_CH0_DB5,
						  AR_AN_RF5G1_CH0_DB5_S,
						  pModal->db);
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF5G1_CH1,
						  AR_AN_RF5G1_CH1_OB5,
						  AR_AN_RF5G1_CH1_OB5_S,
						  pModal->ob_ch1);
			ath9k_hw_analog_shift_rmw(ah, AR_AN_RF5G1_CH1,
						  AR_AN_RF5G1_CH1_DB5,
						  AR_AN_RF5G1_CH1_DB5_S,
						  pModal->db_ch1);
		}
		ath9k_hw_analog_shift_rmw(ah, AR_AN_TOP2,
					  AR_AN_TOP2_XPABIAS_LVL,
					  AR_AN_TOP2_XPABIAS_LVL_S,
					  pModal->xpaBiasLvl);
		ath9k_hw_analog_shift_rmw(ah, AR_AN_TOP2,
					  AR_AN_TOP2_LOCALBIAS,
					  AR_AN_TOP2_LOCALBIAS_S,
					  pModal->local_bias);
		REG_RMW_FIELD(ah, AR_PHY_XPA_CFG, AR_PHY_FORCE_XPA_CFG,
			      pModal->force_xpaon);
	}

	REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH,
		      pModal->switchSettling);
	REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ, AR_PHY_DESIRED_SZ_ADC,
		      pModal->adcDesiredSize);

	if (!AR_SREV_9280_10_OR_LATER(ah))
		REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ,
			      AR_PHY_DESIRED_SZ_PGA,
			      pModal->pgaDesiredSize);

	REG_WRITE(ah, AR_PHY_RF_CTL4,
		  SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAA_OFF)
		  | SM(pModal->txEndToXpaOff,
		       AR_PHY_RF_CTL4_TX_END_XPAB_OFF)
		  | SM(pModal->txFrameToXpaOn,
		       AR_PHY_RF_CTL4_FRAME_XPAA_ON)
		  | SM(pModal->txFrameToXpaOn,
		       AR_PHY_RF_CTL4_FRAME_XPAB_ON));

	REG_RMW_FIELD(ah, AR_PHY_RF_CTL3, AR_PHY_TX_END_TO_A2_RX_ON,
		      pModal->txEndToRxOn);

	if (AR_SREV_9280_10_OR_LATER(ah)) {
		REG_RMW_FIELD(ah, AR_PHY_CCA, AR9280_PHY_CCA_THRESH62,
			      pModal->thresh62);
		REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0,
			      AR_PHY_EXT_CCA0_THRESH62,
			      pModal->thresh62);
	} else {
		REG_RMW_FIELD(ah, AR_PHY_CCA, AR_PHY_CCA_THRESH62,
			      pModal->thresh62);
		REG_RMW_FIELD(ah, AR_PHY_EXT_CCA,
			      AR_PHY_EXT_CCA_THRESH62,
			      pModal->thresh62);
	}

	if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_2) {
		REG_RMW_FIELD(ah, AR_PHY_RF_CTL2,
			      AR_PHY_TX_END_DATA_START,
			      pModal->txFrameToDataStart);
		REG_RMW_FIELD(ah, AR_PHY_RF_CTL2, AR_PHY_TX_END_PA_ON,
			      pModal->txFrameToPaOn);
	}

	if (AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_3) {
		if (IS_CHAN_HT40(chan))
			REG_RMW_FIELD(ah, AR_PHY_SETTLING,
				      AR_PHY_SETTLING_SWITCH,
				      pModal->swSettleHt40);
	}

	if (AR_SREV_9280_20_OR_LATER(ah) &&
	    AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_19)
		REG_RMW_FIELD(ah, AR_PHY_CCK_TX_CTRL,
			      AR_PHY_CCK_TX_CTRL_TX_DAC_SCALE_CCK,
			      pModal->miscBits);


	if (AR_SREV_9280_20(ah) && AR5416_VER_MASK >= AR5416_EEP_MINOR_VER_20) {
		if (IS_CHAN_2GHZ(chan))
			REG_RMW_FIELD(ah, AR_AN_TOP1, AR_AN_TOP1_DACIPMODE,
					eep->baseEepHeader.dacLpMode);
		else if (eep->baseEepHeader.dacHiPwrMode_5G)
			REG_RMW_FIELD(ah, AR_AN_TOP1, AR_AN_TOP1_DACIPMODE, 0);
		else
			REG_RMW_FIELD(ah, AR_AN_TOP1, AR_AN_TOP1_DACIPMODE,
				      eep->baseEepHeader.dacLpMode);

		udelay(100);

		REG_RMW_FIELD(ah, AR_PHY_FRAME_CTL, AR_PHY_FRAME_CTL_TX_CLIP,
			      pModal->miscBits >> 2);

		REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL9,
			      AR_PHY_TX_DESIRED_SCALE_CCK,
			      eep->baseEepHeader.desiredScaleCCK);
	}
}

static void ath9k_hw_def_set_addac(struct ath_hw *ah,
				   struct ath9k_channel *chan)
{
#define XPA_LVL_FREQ(cnt) (pModal->xpaBiasLvlFreq[cnt])
	struct modal_eep_header *pModal;
	struct ar5416_eeprom_def *eep = &ah->eeprom.def;
	u8 biaslevel;

	if (ah->hw_version.macVersion != AR_SREV_VERSION_9160)
		return;

	if (ah->eep_ops->get_eeprom_rev(ah) < AR5416_EEP_MINOR_VER_7)
		return;

	pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);

	if (pModal->xpaBiasLvl != 0xff) {
		biaslevel = pModal->xpaBiasLvl;
	} else {
		u16 resetFreqBin, freqBin, freqCount = 0;
		struct chan_centers centers;

		ath9k_hw_get_channel_centers(ah, chan, &centers);

		resetFreqBin = FREQ2FBIN(centers.synth_center,
					 IS_CHAN_2GHZ(chan));
		freqBin = XPA_LVL_FREQ(0) & 0xff;
		biaslevel = (u8) (XPA_LVL_FREQ(0) >> 14);

		freqCount++;

		while (freqCount < 3) {
			if (XPA_LVL_FREQ(freqCount) == 0x0)
				break;

			freqBin = XPA_LVL_FREQ(freqCount) & 0xff;
			if (resetFreqBin >= freqBin)
				biaslevel = (u8)(XPA_LVL_FREQ(freqCount) >> 14);
			else
				break;
			freqCount++;
		}
	}

	if (IS_CHAN_2GHZ(chan)) {
		INI_RA(&ah->iniAddac, 7, 1) = (INI_RA(&ah->iniAddac,
					7, 1) & (~0x18)) | biaslevel << 3;
	} else {
		INI_RA(&ah->iniAddac, 6, 1) = (INI_RA(&ah->iniAddac,
					6, 1) & (~0xc0)) | biaslevel << 6;
	}
#undef XPA_LVL_FREQ
}

static void ath9k_hw_get_def_gain_boundaries_pdadcs(struct ath_hw *ah,
				struct ath9k_channel *chan,
				struct cal_data_per_freq *pRawDataSet,
				u8 *bChans, u16 availPiers,
				u16 tPdGainOverlap,
				u16 *pPdGainBoundaries, u8 *pPDADCValues,
				u16 numXpdGains)
{
	int i, j, k;
	int16_t ss;
	u16 idxL = 0, idxR = 0, numPiers;
	static u8 vpdTableL[AR5416_NUM_PD_GAINS]
		[AR5416_MAX_PWR_RANGE_IN_HALF_DB];
	static u8 vpdTableR[AR5416_NUM_PD_GAINS]
		[AR5416_MAX_PWR_RANGE_IN_HALF_DB];
	static u8 vpdTableI[AR5416_NUM_PD_GAINS]
		[AR5416_MAX_PWR_RANGE_IN_HALF_DB];

	u8 *pVpdL, *pVpdR, *pPwrL, *pPwrR;
	u8 minPwrT4[AR5416_NUM_PD_GAINS];
	u8 maxPwrT4[AR5416_NUM_PD_GAINS];
	int16_t vpdStep;
	int16_t tmpVal;
	u16 sizeCurrVpdTable, maxIndex, tgtIndex;
	bool match;
	int16_t minDelta = 0;
	struct chan_centers centers;

	memset(&minPwrT4, 0, AR9287_NUM_PD_GAINS);
	ath9k_hw_get_channel_centers(ah, chan, &centers);

	for (numPiers = 0; numPiers < availPiers; numPiers++) {
		if (bChans[numPiers] == AR5416_BCHAN_UNUSED)
			break;
	}

	match = ath9k_hw_get_lower_upper_index((u8)FREQ2FBIN(centers.synth_center,
							     IS_CHAN_2GHZ(chan)),
					       bChans, numPiers, &idxL, &idxR);

	if (match) {
		for (i = 0; i < numXpdGains; i++) {
			minPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][0];
			maxPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][4];
			ath9k_hw_fill_vpd_table(minPwrT4[i], maxPwrT4[i],
					pRawDataSet[idxL].pwrPdg[i],
					pRawDataSet[idxL].vpdPdg[i],
					AR5416_PD_GAIN_ICEPTS,
					vpdTableI[i]);
		}
	} else {
		for (i = 0; i < numXpdGains; i++) {
			pVpdL = pRawDataSet[idxL].vpdPdg[i];
			pPwrL = pRawDataSet[idxL].pwrPdg[i];
			pVpdR = pRawDataSet[idxR].vpdPdg[i];
			pPwrR = pRawDataSet[idxR].pwrPdg[i];

			minPwrT4[i] = max(pPwrL[0], pPwrR[0]);

			maxPwrT4[i] =
				min(pPwrL[AR5416_PD_GAIN_ICEPTS - 1],
				    pPwrR[AR5416_PD_GAIN_ICEPTS - 1]);


			ath9k_hw_fill_vpd_table(minPwrT4[i], maxPwrT4[i],
						pPwrL, pVpdL,
						AR5416_PD_GAIN_ICEPTS,
						vpdTableL[i]);
			ath9k_hw_fill_vpd_table(minPwrT4[i], maxPwrT4[i],
						pPwrR, pVpdR,
						AR5416_PD_GAIN_ICEPTS,
						vpdTableR[i]);

			for (j = 0; j <= (maxPwrT4[i] - minPwrT4[i]) / 2; j++) {
				vpdTableI[i][j] =
					(u8)(ath9k_hw_interpolate((u16)
					     FREQ2FBIN(centers.
						       synth_center,
						       IS_CHAN_2GHZ
						       (chan)),
					     bChans[idxL], bChans[idxR],
					     vpdTableL[i][j], vpdTableR[i][j]));
			}
		}
	}

	k = 0;

	for (i = 0; i < numXpdGains; i++) {
		if (i == (numXpdGains - 1))
			pPdGainBoundaries[i] =
				(u16)(maxPwrT4[i] / 2);
		else
			pPdGainBoundaries[i] =
				(u16)((maxPwrT4[i] + minPwrT4[i + 1]) / 4);

		pPdGainBoundaries[i] =
			min((u16)AR5416_MAX_RATE_POWER, pPdGainBoundaries[i]);

		if ((i == 0) && !AR_SREV_5416_20_OR_LATER(ah)) {
			minDelta = pPdGainBoundaries[0] - 23;
			pPdGainBoundaries[0] = 23;
		} else {
			minDelta = 0;
		}

		if (i == 0) {
			if (AR_SREV_9280_10_OR_LATER(ah))
				ss = (int16_t)(0 - (minPwrT4[i] / 2));
			else
				ss = 0;
		} else {
			ss = (int16_t)((pPdGainBoundaries[i - 1] -
					(minPwrT4[i] / 2)) -
				       tPdGainOverlap + 1 + minDelta);
		}
		vpdStep = (int16_t)(vpdTableI[i][1] - vpdTableI[i][0]);
		vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);

		while ((ss < 0) && (k < (AR5416_NUM_PDADC_VALUES - 1))) {
			tmpVal = (int16_t)(vpdTableI[i][0] + ss * vpdStep);
			pPDADCValues[k++] = (u8)((tmpVal < 0) ? 0 : tmpVal);
			ss++;
		}

		sizeCurrVpdTable = (u8) ((maxPwrT4[i] - minPwrT4[i]) / 2 + 1);
		tgtIndex = (u8)(pPdGainBoundaries[i] + tPdGainOverlap -
				(minPwrT4[i] / 2));
		maxIndex = (tgtIndex < sizeCurrVpdTable) ?
			tgtIndex : sizeCurrVpdTable;

		while ((ss < maxIndex) && (k < (AR5416_NUM_PDADC_VALUES - 1))) {
			pPDADCValues[k++] = vpdTableI[i][ss++];
		}

		vpdStep = (int16_t)(vpdTableI[i][sizeCurrVpdTable - 1] -
				    vpdTableI[i][sizeCurrVpdTable - 2]);
		vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);

		if (tgtIndex >= maxIndex) {
			while ((ss <= tgtIndex) &&
			       (k < (AR5416_NUM_PDADC_VALUES - 1))) {
				tmpVal = (int16_t)((vpdTableI[i][sizeCurrVpdTable - 1] +
						    (ss - maxIndex + 1) * vpdStep));
				pPDADCValues[k++] = (u8)((tmpVal > 255) ?
							 255 : tmpVal);
				ss++;
			}
		}
	}

	while (i < AR5416_PD_GAINS_IN_MASK) {
		pPdGainBoundaries[i] = pPdGainBoundaries[i - 1];
		i++;
	}

	while (k < AR5416_NUM_PDADC_VALUES) {
		pPDADCValues[k] = pPDADCValues[k - 1];
		k++;
	}
}

static int16_t ath9k_change_gain_boundary_setting(struct ath_hw *ah,
				u16 *gb,
				u16 numXpdGain,
				u16 pdGainOverlap_t2,
				int8_t pwr_table_offset,
				int16_t *diff)

{
	u16 k;

	/* Prior to writing the boundaries or the pdadc vs. power table
	 * into the chip registers the default starting point on the pdadc
	 * vs. power table needs to be checked and the curve boundaries
	 * adjusted accordingly
	 */
	if (AR_SREV_9280_20_OR_LATER(ah)) {
		u16 gb_limit;

		if (AR5416_PWR_TABLE_OFFSET_DB != pwr_table_offset) {
			/* get the difference in dB */
			*diff = (u16)(pwr_table_offset - AR5416_PWR_TABLE_OFFSET_DB);
			/* get the number of half dB steps */
			*diff *= 2;
			/* change the original gain boundary settings
			 * by the number of half dB steps
			 */
			for (k = 0; k < numXpdGain; k++)
				gb[k] = (u16)(gb[k] - *diff);
		}
		/* Because of a hardware limitation, ensure the gain boundary
		 * is not larger than (63 - overlap)
		 */
		gb_limit = (u16)(AR5416_MAX_RATE_POWER - pdGainOverlap_t2);

		for (k = 0; k < numXpdGain; k++)
			gb[k] = (u16)min(gb_limit, gb[k]);
	}

	return *diff;
}

static void ath9k_adjust_pdadc_values(struct ath_hw *ah,
				      int8_t pwr_table_offset,
				      int16_t diff,
				      u8 *pdadcValues)
{
#define NUM_PDADC(diff) (AR5416_NUM_PDADC_VALUES - diff)
	u16 k;

	/* If this is a board that has a pwrTableOffset that differs from
	 * the default AR5416_PWR_TABLE_OFFSET_DB then the start of the
	 * pdadc vs pwr table needs to be adjusted prior to writing to the
	 * chip.
	 */
	if (AR_SREV_9280_20_OR_LATER(ah)) {
		if (AR5416_PWR_TABLE_OFFSET_DB != pwr_table_offset) {
			/* shift the table to start at the new offset */
			for (k = 0; k < (u16)NUM_PDADC(diff); k++ ) {
				pdadcValues[k] = pdadcValues[k + diff];
			}

			/* fill the back of the table */
			for (k = (u16)NUM_PDADC(diff); k < NUM_PDADC(0); k++) {
				pdadcValues[k] = pdadcValues[NUM_PDADC(diff)];
			}
		}
	}
#undef NUM_PDADC
}

static void ath9k_hw_set_def_power_cal_table(struct ath_hw *ah,
				  struct ath9k_channel *chan,
				  int16_t *pTxPowerIndexOffset)
{
#define SM_PD_GAIN(x) SM(0x38, AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_##x)
#define SM_PDGAIN_B(x, y) \
		SM((gainBoundaries[x]), AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_##y)
	struct ath_common *common = ath9k_hw_common(ah);
	struct ar5416_eeprom_def *pEepData = &ah->eeprom.def;
	struct cal_data_per_freq *pRawDataset;
	u8 *pCalBChans = NULL;
	u16 pdGainOverlap_t2;
	static u8 pdadcValues[AR5416_NUM_PDADC_VALUES];
	u16 gainBoundaries[AR5416_PD_GAINS_IN_MASK];
	u16 numPiers, i, j;
	int16_t diff = 0;
	u16 numXpdGain, xpdMask;
	u16 xpdGainValues[AR5416_NUM_PD_GAINS] = { 0, 0, 0, 0 };
	u32 reg32, regOffset, regChainOffset;
	int16_t modalIdx;
	int8_t pwr_table_offset;

	modalIdx = IS_CHAN_2GHZ(chan) ? 1 : 0;
	xpdMask = pEepData->modalHeader[modalIdx].xpdGain;

	pwr_table_offset = ah->eep_ops->get_eeprom(ah, EEP_PWR_TABLE_OFFSET);

	if ((pEepData->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
	    AR5416_EEP_MINOR_VER_2) {
		pdGainOverlap_t2 =
			pEepData->modalHeader[modalIdx].pdGainOverlap;
	} else {
		pdGainOverlap_t2 = (u16)(MS(REG_READ(ah, AR_PHY_TPCRG5),
					    AR_PHY_TPCRG5_PD_GAIN_OVERLAP));
	}

	if (IS_CHAN_2GHZ(chan)) {
		pCalBChans = pEepData->calFreqPier2G;
		numPiers = AR5416_NUM_2G_CAL_PIERS;
	} else {
		pCalBChans = pEepData->calFreqPier5G;
		numPiers = AR5416_NUM_5G_CAL_PIERS;
	}

	if (OLC_FOR_AR9280_20_LATER && IS_CHAN_2GHZ(chan)) {
		pRawDataset = pEepData->calPierData2G[0];
		ah->initPDADC = ((struct calDataPerFreqOpLoop *)
				 pRawDataset)->vpdPdg[0][0];
	}

	numXpdGain = 0;

	for (i = 1; i <= AR5416_PD_GAINS_IN_MASK; i++) {
		if ((xpdMask >> (AR5416_PD_GAINS_IN_MASK - i)) & 1) {
			if (numXpdGain >= AR5416_NUM_PD_GAINS)
				break;
			xpdGainValues[numXpdGain] =
				(u16)(AR5416_PD_GAINS_IN_MASK - i);
			numXpdGain++;
		}
	}

	REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_NUM_PD_GAIN,
		      (numXpdGain - 1) & 0x3);
	REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_1,
		      xpdGainValues[0]);
	REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_2,
		      xpdGainValues[1]);
	REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_3,
		      xpdGainValues[2]);

	for (i = 0; i < AR5416_MAX_CHAINS; i++) {
		if (AR_SREV_5416_20_OR_LATER(ah) &&
		    (ah->rxchainmask == 5 || ah->txchainmask == 5) &&
		    (i != 0)) {
			regChainOffset = (i == 1) ? 0x2000 : 0x1000;
		} else
			regChainOffset = i * 0x1000;

		if (pEepData->baseEepHeader.txMask & (1 << i)) {
			if (IS_CHAN_2GHZ(chan))
				pRawDataset = pEepData->calPierData2G[i];
			else
				pRawDataset = pEepData->calPierData5G[i];


			if (OLC_FOR_AR9280_20_LATER) {
				u8 pcdacIdx;
				u8 txPower;

				ath9k_get_txgain_index(ah, chan,
				(struct calDataPerFreqOpLoop *)pRawDataset,
				pCalBChans, numPiers, &txPower, &pcdacIdx);
				ath9k_olc_get_pdadcs(ah, pcdacIdx,
						     txPower/2, pdadcValues);
			} else {
				ath9k_hw_get_def_gain_boundaries_pdadcs(ah,
							chan, pRawDataset,
							pCalBChans, numPiers,
							pdGainOverlap_t2,
							gainBoundaries,
							pdadcValues,
							numXpdGain);
			}

			diff = ath9k_change_gain_boundary_setting(ah,
							   gainBoundaries,
							   numXpdGain,
							   pdGainOverlap_t2,
							   pwr_table_offset,
							   &diff);

			if ((i == 0) || AR_SREV_5416_20_OR_LATER(ah)) {
				if (OLC_FOR_AR9280_20_LATER) {
					REG_WRITE(ah,
						AR_PHY_TPCRG5 + regChainOffset,
						SM(0x6,
						AR_PHY_TPCRG5_PD_GAIN_OVERLAP) |
						SM_PD_GAIN(1) | SM_PD_GAIN(2) |
						SM_PD_GAIN(3) | SM_PD_GAIN(4));
				} else {
					REG_WRITE(ah,
						AR_PHY_TPCRG5 + regChainOffset,
						SM(pdGainOverlap_t2,
						AR_PHY_TPCRG5_PD_GAIN_OVERLAP)|
						SM_PDGAIN_B(0, 1) |
						SM_PDGAIN_B(1, 2) |
						SM_PDGAIN_B(2, 3) |
						SM_PDGAIN_B(3, 4));
				}
			}


			ath9k_adjust_pdadc_values(ah, pwr_table_offset,
						  diff, pdadcValues);

			regOffset = AR_PHY_BASE + (672 << 2) + regChainOffset;
			for (j = 0; j < 32; j++) {
				reg32 = ((pdadcValues[4 * j + 0] & 0xFF) << 0) |
					((pdadcValues[4 * j + 1] & 0xFF) << 8) |
					((pdadcValues[4 * j + 2] & 0xFF) << 16)|
					((pdadcValues[4 * j + 3] & 0xFF) << 24);
				REG_WRITE(ah, regOffset, reg32);

				ath_print(common, ATH_DBG_EEPROM,
					  "PDADC (%d,%4x): %4.4x %8.8x\n",
					  i, regChainOffset, regOffset,
					  reg32);
				ath_print(common, ATH_DBG_EEPROM,
					  "PDADC: Chain %d | PDADC %3d "
					  "Value %3d | PDADC %3d Value %3d | "
					  "PDADC %3d Value %3d | PDADC %3d "
					  "Value %3d |\n",
					  i, 4 * j, pdadcValues[4 * j],
					  4 * j + 1, pdadcValues[4 * j + 1],
					  4 * j + 2, pdadcValues[4 * j + 2],
					  4 * j + 3,
					  pdadcValues[4 * j + 3]);

				regOffset += 4;
			}
		}
	}

	*pTxPowerIndexOffset = 0;
#undef SM_PD_GAIN
#undef SM_PDGAIN_B
}

static void ath9k_hw_set_def_power_per_rate_table(struct ath_hw *ah,
						  struct ath9k_channel *chan,
						  int16_t *ratesArray,
						  u16 cfgCtl,
						  u16 AntennaReduction,
						  u16 twiceMaxRegulatoryPower,
						  u16 powerLimit)
{
#define REDUCE_SCALED_POWER_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define REDUCE_SCALED_POWER_BY_THREE_CHAIN   9 /* 10*log10(3)*2 */

	struct ath_regulatory *regulatory = ath9k_hw_regulatory(ah);
	struct ar5416_eeprom_def *pEepData = &ah->eeprom.def;
	u16 twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
	static const u16 tpScaleReductionTable[5] =
		{ 0, 3, 6, 9, AR5416_MAX_RATE_POWER };

	int i;
	int16_t twiceLargestAntenna;
	struct cal_ctl_data *rep;
	struct cal_target_power_leg targetPowerOfdm, targetPowerCck = {
		0, { 0, 0, 0, 0}
	};
	struct cal_target_power_leg targetPowerOfdmExt = {
		0, { 0, 0, 0, 0} }, targetPowerCckExt = {
		0, { 0, 0, 0, 0 }
	};
	struct cal_target_power_ht targetPowerHt20, targetPowerHt40 = {
		0, {0, 0, 0, 0}
	};
	u16 scaledPower = 0, minCtlPower, maxRegAllowedPower;
	u16 ctlModesFor11a[] =
		{ CTL_11A, CTL_5GHT20, CTL_11A_EXT, CTL_5GHT40 };
	u16 ctlModesFor11g[] =
		{ CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT,
		  CTL_2GHT40
		};
	u16 numCtlModes, *pCtlMode, ctlMode, freq;
	struct chan_centers centers;
	int tx_chainmask;
	u16 twiceMinEdgePower;

	tx_chainmask = ah->txchainmask;

	ath9k_hw_get_channel_centers(ah, chan, &centers);

	twiceLargestAntenna = max(
		pEepData->modalHeader
			[IS_CHAN_2GHZ(chan)].antennaGainCh[0],
		pEepData->modalHeader
			[IS_CHAN_2GHZ(chan)].antennaGainCh[1]);

	twiceLargestAntenna = max((u8)twiceLargestAntenna,
				  pEepData->modalHeader
				  [IS_CHAN_2GHZ(chan)].antennaGainCh[2]);

	twiceLargestAntenna = (int16_t)min(AntennaReduction -
					   twiceLargestAntenna, 0);

	maxRegAllowedPower = twiceMaxRegulatoryPower + twiceLargestAntenna;

	if (regulatory->tp_scale != ATH9K_TP_SCALE_MAX) {
		maxRegAllowedPower -=
			(tpScaleReductionTable[(regulatory->tp_scale)] * 2);
	}

	scaledPower = min(powerLimit, maxRegAllowedPower);

	switch (ar5416_get_ntxchains(tx_chainmask)) {
	case 1:
		break;
	case 2:
		if (scaledPower > REDUCE_SCALED_POWER_BY_TWO_CHAIN)
			scaledPower -= REDUCE_SCALED_POWER_BY_TWO_CHAIN;
		else
			scaledPower = 0;
		break;
	case 3:
		if (scaledPower > REDUCE_SCALED_POWER_BY_THREE_CHAIN)
			scaledPower -= REDUCE_SCALED_POWER_BY_THREE_CHAIN;
		else
			scaledPower = 0;
		break;
	}

	if (IS_CHAN_2GHZ(chan)) {
		numCtlModes = ARRAY_SIZE(ctlModesFor11g) -
			SUB_NUM_CTL_MODES_AT_2G_40;
		pCtlMode = ctlModesFor11g;

		ath9k_hw_get_legacy_target_powers(ah, chan,
			pEepData->calTargetPowerCck,
			AR5416_NUM_2G_CCK_TARGET_POWERS,
			&targetPowerCck, 4, false);
		ath9k_hw_get_legacy_target_powers(ah, chan,
			pEepData->calTargetPower2G,
			AR5416_NUM_2G_20_TARGET_POWERS,
			&targetPowerOfdm, 4, false);
		ath9k_hw_get_target_powers(ah, chan,
			pEepData->calTargetPower2GHT20,
			AR5416_NUM_2G_20_TARGET_POWERS,
			&targetPowerHt20, 8, false);

		if (IS_CHAN_HT40(chan)) {
			numCtlModes = ARRAY_SIZE(ctlModesFor11g);
			ath9k_hw_get_target_powers(ah, chan,
				pEepData->calTargetPower2GHT40,
				AR5416_NUM_2G_40_TARGET_POWERS,
				&targetPowerHt40, 8, true);
			ath9k_hw_get_legacy_target_powers(ah, chan,
				pEepData->calTargetPowerCck,
				AR5416_NUM_2G_CCK_TARGET_POWERS,
				&targetPowerCckExt, 4, true);
			ath9k_hw_get_legacy_target_powers(ah, chan,
				pEepData->calTargetPower2G,
				AR5416_NUM_2G_20_TARGET_POWERS,
				&targetPowerOfdmExt, 4, true);
		}
	} else {
		numCtlModes = ARRAY_SIZE(ctlModesFor11a) -
			SUB_NUM_CTL_MODES_AT_5G_40;
		pCtlMode = ctlModesFor11a;

		ath9k_hw_get_legacy_target_powers(ah, chan,
			pEepData->calTargetPower5G,
			AR5416_NUM_5G_20_TARGET_POWERS,
			&targetPowerOfdm, 4, false);
		ath9k_hw_get_target_powers(ah, chan,
			pEepData->calTargetPower5GHT20,
			AR5416_NUM_5G_20_TARGET_POWERS,
			&targetPowerHt20, 8, false);

		if (IS_CHAN_HT40(chan)) {
			numCtlModes = ARRAY_SIZE(ctlModesFor11a);
			ath9k_hw_get_target_powers(ah, chan,
				pEepData->calTargetPower5GHT40,
				AR5416_NUM_5G_40_TARGET_POWERS,
				&targetPowerHt40, 8, true);
			ath9k_hw_get_legacy_target_powers(ah, chan,
				pEepData->calTargetPower5G,
				AR5416_NUM_5G_20_TARGET_POWERS,
				&targetPowerOfdmExt, 4, true);
		}
	}

	for (ctlMode = 0; ctlMode < numCtlModes; ctlMode++) {
		bool isHt40CtlMode = (pCtlMode[ctlMode] == CTL_5GHT40) ||
			(pCtlMode[ctlMode] == CTL_2GHT40);
		if (isHt40CtlMode)
			freq = centers.synth_center;
		else if (pCtlMode[ctlMode] & EXT_ADDITIVE)
			freq = centers.ext_center;
		else
			freq = centers.ctl_center;

		if (ah->eep_ops->get_eeprom_ver(ah) == 14 &&
		    ah->eep_ops->get_eeprom_rev(ah) <= 2)
			twiceMaxEdgePower = AR5416_MAX_RATE_POWER;

		for (i = 0; (i < AR5416_NUM_CTLS) && pEepData->ctlIndex[i]; i++) {
			if ((((cfgCtl & ~CTL_MODE_M) |
			      (pCtlMode[ctlMode] & CTL_MODE_M)) ==
			     pEepData->ctlIndex[i]) ||
			    (((cfgCtl & ~CTL_MODE_M) |
			      (pCtlMode[ctlMode] & CTL_MODE_M)) ==
			     ((pEepData->ctlIndex[i] & CTL_MODE_M) | SD_NO_CTL))) {
				rep = &(pEepData->ctlData[i]);

				twiceMinEdgePower = ath9k_hw_get_max_edge_power(freq,
				rep->ctlEdges[ar5416_get_ntxchains(tx_chainmask) - 1],
				IS_CHAN_2GHZ(chan), AR5416_NUM_BAND_EDGES);

				if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL) {
					twiceMaxEdgePower = min(twiceMaxEdgePower,
								twiceMinEdgePower);
				} else {
					twiceMaxEdgePower = twiceMinEdgePower;
					break;
				}
			}
		}

		minCtlPower = min(twiceMaxEdgePower, scaledPower);

		switch (pCtlMode[ctlMode]) {
		case CTL_11B:
			for (i = 0; i < ARRAY_SIZE(targetPowerCck.tPow2x); i++) {
				targetPowerCck.tPow2x[i] =
					min((u16)targetPowerCck.tPow2x[i],
					    minCtlPower);
			}
			break;
		case CTL_11A:
		case CTL_11G:
			for (i = 0; i < ARRAY_SIZE(targetPowerOfdm.tPow2x); i++) {
				targetPowerOfdm.tPow2x[i] =
					min((u16)targetPowerOfdm.tPow2x[i],
					    minCtlPower);
			}
			break;
		case CTL_5GHT20:
		case CTL_2GHT20:
			for (i = 0; i < ARRAY_SIZE(targetPowerHt20.tPow2x); i++) {
				targetPowerHt20.tPow2x[i] =
					min((u16)targetPowerHt20.tPow2x[i],
					    minCtlPower);
			}
			break;
		case CTL_11B_EXT:
			targetPowerCckExt.tPow2x[0] = min((u16)
					targetPowerCckExt.tPow2x[0],
					minCtlPower);
			break;
		case CTL_11A_EXT:
		case CTL_11G_EXT:
			targetPowerOfdmExt.tPow2x[0] = min((u16)
					targetPowerOfdmExt.tPow2x[0],
					minCtlPower);
			break;
		case CTL_5GHT40:
		case CTL_2GHT40:
			for (i = 0; i < ARRAY_SIZE(targetPowerHt40.tPow2x); i++) {
				targetPowerHt40.tPow2x[i] =
					min((u16)targetPowerHt40.tPow2x[i],
					    minCtlPower);
			}
			break;
		default:
			break;
		}
	}

	ratesArray[rate6mb] = ratesArray[rate9mb] = ratesArray[rate12mb] =
		ratesArray[rate18mb] = ratesArray[rate24mb] =
		targetPowerOfdm.tPow2x[0];
	ratesArray[rate36mb] = targetPowerOfdm.tPow2x[1];
	ratesArray[rate48mb] = targetPowerOfdm.tPow2x[2];
	ratesArray[rate54mb] = targetPowerOfdm.tPow2x[3];
	ratesArray[rateXr] = targetPowerOfdm.tPow2x[0];

	for (i = 0; i < ARRAY_SIZE(targetPowerHt20.tPow2x); i++)
		ratesArray[rateHt20_0 + i] = targetPowerHt20.tPow2x[i];

	if (IS_CHAN_2GHZ(chan)) {
		ratesArray[rate1l] = targetPowerCck.tPow2x[0];
		ratesArray[rate2s] = ratesArray[rate2l] =
			targetPowerCck.tPow2x[1];
		ratesArray[rate5_5s] = ratesArray[rate5_5l] =
			targetPowerCck.tPow2x[2];
		ratesArray[rate11s] = ratesArray[rate11l] =
			targetPowerCck.tPow2x[3];
	}
	if (IS_CHAN_HT40(chan)) {
		for (i = 0; i < ARRAY_SIZE(targetPowerHt40.tPow2x); i++) {
			ratesArray[rateHt40_0 + i] =
				targetPowerHt40.tPow2x[i];
		}
		ratesArray[rateDupOfdm] = targetPowerHt40.tPow2x[0];
		ratesArray[rateDupCck] = targetPowerHt40.tPow2x[0];
		ratesArray[rateExtOfdm] = targetPowerOfdmExt.tPow2x[0];
		if (IS_CHAN_2GHZ(chan)) {
			ratesArray[rateExtCck] =
				targetPowerCckExt.tPow2x[0];
		}
	}
}

static void ath9k_hw_def_set_txpower(struct ath_hw *ah,
				    struct ath9k_channel *chan,
				    u16 cfgCtl,
				    u8 twiceAntennaReduction,
				    u8 twiceMaxRegulatoryPower,
				    u8 powerLimit)
{
#define RT_AR_DELTA(x) (ratesArray[x] - cck_ofdm_delta)
	struct ath_regulatory *regulatory = ath9k_hw_regulatory(ah);
	struct ar5416_eeprom_def *pEepData = &ah->eeprom.def;
	struct modal_eep_header *pModal =
		&(pEepData->modalHeader[IS_CHAN_2GHZ(chan)]);
	int16_t ratesArray[Ar5416RateSize];
	int16_t txPowerIndexOffset = 0;
	u8 ht40PowerIncForPdadc = 2;
	int i, cck_ofdm_delta = 0;

	memset(ratesArray, 0, sizeof(ratesArray));

	if ((pEepData->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
	    AR5416_EEP_MINOR_VER_2) {
		ht40PowerIncForPdadc = pModal->ht40PowerIncForPdadc;
	}

	ath9k_hw_set_def_power_per_rate_table(ah, chan,
					       &ratesArray[0], cfgCtl,
					       twiceAntennaReduction,
					       twiceMaxRegulatoryPower,
					       powerLimit);

	ath9k_hw_set_def_power_cal_table(ah, chan, &txPowerIndexOffset);

	for (i = 0; i < ARRAY_SIZE(ratesArray); i++) {
		ratesArray[i] =	(int16_t)(txPowerIndexOffset + ratesArray[i]);
		if (ratesArray[i] > AR5416_MAX_RATE_POWER)
			ratesArray[i] = AR5416_MAX_RATE_POWER;
	}

	if (AR_SREV_9280_10_OR_LATER(ah)) {
		for (i = 0; i < Ar5416RateSize; i++) {
			int8_t pwr_table_offset;

			pwr_table_offset = ah->eep_ops->get_eeprom(ah,
							EEP_PWR_TABLE_OFFSET);
			ratesArray[i] -= pwr_table_offset * 2;
		}
	}

	REG_WRITE(ah, AR_PHY_POWER_TX_RATE1,
		  ATH9K_POW_SM(ratesArray[rate18mb], 24)
		  | ATH9K_POW_SM(ratesArray[rate12mb], 16)
		  | ATH9K_POW_SM(ratesArray[rate9mb], 8)
		  | ATH9K_POW_SM(ratesArray[rate6mb], 0));
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE2,
		  ATH9K_POW_SM(ratesArray[rate54mb], 24)
		  | ATH9K_POW_SM(ratesArray[rate48mb], 16)
		  | ATH9K_POW_SM(ratesArray[rate36mb], 8)
		  | ATH9K_POW_SM(ratesArray[rate24mb], 0));

	if (IS_CHAN_2GHZ(chan)) {
		if (OLC_FOR_AR9280_20_LATER) {
			cck_ofdm_delta = 2;
			REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
				ATH9K_POW_SM(RT_AR_DELTA(rate2s), 24)
				| ATH9K_POW_SM(RT_AR_DELTA(rate2l), 16)
				| ATH9K_POW_SM(ratesArray[rateXr], 8)
				| ATH9K_POW_SM(RT_AR_DELTA(rate1l), 0));
			REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
				ATH9K_POW_SM(RT_AR_DELTA(rate11s), 24)
				| ATH9K_POW_SM(RT_AR_DELTA(rate11l), 16)
				| ATH9K_POW_SM(RT_AR_DELTA(rate5_5s), 8)
				| ATH9K_POW_SM(RT_AR_DELTA(rate5_5l), 0));
		} else {
			REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
				ATH9K_POW_SM(ratesArray[rate2s], 24)
				| ATH9K_POW_SM(ratesArray[rate2l], 16)
				| ATH9K_POW_SM(ratesArray[rateXr], 8)
				| ATH9K_POW_SM(ratesArray[rate1l], 0));
			REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
				ATH9K_POW_SM(ratesArray[rate11s], 24)
				| ATH9K_POW_SM(ratesArray[rate11l], 16)
				| ATH9K_POW_SM(ratesArray[rate5_5s], 8)
				| ATH9K_POW_SM(ratesArray[rate5_5l], 0));
		}
	}

	REG_WRITE(ah, AR_PHY_POWER_TX_RATE5,
		  ATH9K_POW_SM(ratesArray[rateHt20_3], 24)
		  | ATH9K_POW_SM(ratesArray[rateHt20_2], 16)
		  | ATH9K_POW_SM(ratesArray[rateHt20_1], 8)
		  | ATH9K_POW_SM(ratesArray[rateHt20_0], 0));
	REG_WRITE(ah, AR_PHY_POWER_TX_RATE6,
		  ATH9K_POW_SM(ratesArray[rateHt20_7], 24)
		  | ATH9K_POW_SM(ratesArray[rateHt20_6], 16)
		  | ATH9K_POW_SM(ratesArray[rateHt20_5], 8)
		  | ATH9K_POW_SM(ratesArray[rateHt20_4], 0));

	if (IS_CHAN_HT40(chan)) {
		REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
			  ATH9K_POW_SM(ratesArray[rateHt40_3] +
				       ht40PowerIncForPdadc, 24)
			  | ATH9K_POW_SM(ratesArray[rateHt40_2] +
					 ht40PowerIncForPdadc, 16)
			  | ATH9K_POW_SM(ratesArray[rateHt40_1] +
					 ht40PowerIncForPdadc, 8)
			  | ATH9K_POW_SM(ratesArray[rateHt40_0] +
					 ht40PowerIncForPdadc, 0));
		REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
			  ATH9K_POW_SM(ratesArray[rateHt40_7] +
				       ht40PowerIncForPdadc, 24)
			  | ATH9K_POW_SM(ratesArray[rateHt40_6] +
					 ht40PowerIncForPdadc, 16)
			  | ATH9K_POW_SM(ratesArray[rateHt40_5] +
					 ht40PowerIncForPdadc, 8)
			  | ATH9K_POW_SM(ratesArray[rateHt40_4] +
					 ht40PowerIncForPdadc, 0));
		if (OLC_FOR_AR9280_20_LATER) {
			REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
				ATH9K_POW_SM(ratesArray[rateExtOfdm], 24)
				| ATH9K_POW_SM(RT_AR_DELTA(rateExtCck), 16)
				| ATH9K_POW_SM(ratesArray[rateDupOfdm], 8)
				| ATH9K_POW_SM(RT_AR_DELTA(rateDupCck), 0));
		} else {
			REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
				ATH9K_POW_SM(ratesArray[rateExtOfdm], 24)
				| ATH9K_POW_SM(ratesArray[rateExtCck], 16)
				| ATH9K_POW_SM(ratesArray[rateDupOfdm], 8)
				| ATH9K_POW_SM(ratesArray[rateDupCck], 0));
		}
	}

	REG_WRITE(ah, AR_PHY_POWER_TX_SUB,
		  ATH9K_POW_SM(pModal->pwrDecreaseFor3Chain, 6)
		  | ATH9K_POW_SM(pModal->pwrDecreaseFor2Chain, 0));

	i = rate6mb;

	if (IS_CHAN_HT40(chan))
		i = rateHt40_0;
	else if (IS_CHAN_HT20(chan))
		i = rateHt20_0;

	if (AR_SREV_9280_10_OR_LATER(ah))
		regulatory->max_power_level =
			ratesArray[i] + AR5416_PWR_TABLE_OFFSET_DB * 2;
	else
		regulatory->max_power_level = ratesArray[i];

	switch(ar5416_get_ntxchains(ah->txchainmask)) {
	case 1:
		break;
	case 2:
		regulatory->max_power_level += INCREASE_MAXPOW_BY_TWO_CHAIN;
		break;
	case 3:
		regulatory->max_power_level += INCREASE_MAXPOW_BY_THREE_CHAIN;
		break;
	default:
		ath_print(ath9k_hw_common(ah), ATH_DBG_EEPROM,
			  "Invalid chainmask configuration\n");
		break;
	}
}

static u8 ath9k_hw_def_get_num_ant_config(struct ath_hw *ah,
					  enum ieee80211_band freq_band)
{
	struct ar5416_eeprom_def *eep = &ah->eeprom.def;
	struct modal_eep_header *pModal =
		&(eep->modalHeader[ATH9K_HAL_FREQ_BAND_2GHZ == freq_band]);
	struct base_eep_header *pBase = &eep->baseEepHeader;
	u8 num_ant_config;

	num_ant_config = 1;

	if (pBase->version >= 0x0E0D)
		if (pModal->useAnt1)
			num_ant_config += 1;

	return num_ant_config;
}

static u32 ath9k_hw_def_get_eeprom_antenna_cfg(struct ath_hw *ah,
					       struct ath9k_channel *chan)
{
	struct ar5416_eeprom_def *eep = &ah->eeprom.def;
	struct modal_eep_header *pModal =
		&(eep->modalHeader[IS_CHAN_2GHZ(chan)]);

	return pModal->antCtrlCommon;
}

static u16 ath9k_hw_def_get_spur_channel(struct ath_hw *ah, u16 i, bool is2GHz)
{
#define EEP_DEF_SPURCHAN \
	(ah->eeprom.def.modalHeader[is2GHz].spurChans[i].spurChan)
	struct ath_common *common = ath9k_hw_common(ah);

	u16 spur_val = AR_NO_SPUR;

	ath_print(common, ATH_DBG_ANI,
		  "Getting spur idx %d is2Ghz. %d val %x\n",
		  i, is2GHz, ah->config.spurchans[i][is2GHz]);

	switch (ah->config.spurmode) {
	case SPUR_DISABLE:
		break;
	case SPUR_ENABLE_IOCTL:
		spur_val = ah->config.spurchans[i][is2GHz];
		ath_print(common, ATH_DBG_ANI,
			  "Getting spur val from new loc. %d\n", spur_val);
		break;
	case SPUR_ENABLE_EEPROM:
		spur_val = EEP_DEF_SPURCHAN;
		break;
	}

	return spur_val;

#undef EEP_DEF_SPURCHAN
}

const struct eeprom_ops eep_def_ops = {
	.check_eeprom		= ath9k_hw_def_check_eeprom,
	.get_eeprom		= ath9k_hw_def_get_eeprom,
	.fill_eeprom		= ath9k_hw_def_fill_eeprom,
	.get_eeprom_ver		= ath9k_hw_def_get_eeprom_ver,
	.get_eeprom_rev		= ath9k_hw_def_get_eeprom_rev,
	.get_num_ant_config	= ath9k_hw_def_get_num_ant_config,
	.get_eeprom_antenna_cfg	= ath9k_hw_def_get_eeprom_antenna_cfg,
	.set_board_values	= ath9k_hw_def_set_board_values,
	.set_addac		= ath9k_hw_def_set_addac,
	.set_txpower		= ath9k_hw_def_set_txpower,
	.get_spur_channel	= ath9k_hw_def_get_spur_channel
};
