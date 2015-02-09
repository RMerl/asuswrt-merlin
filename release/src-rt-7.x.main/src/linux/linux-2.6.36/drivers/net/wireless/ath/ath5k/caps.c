/*
 * Copyright (c) 2004-2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006-2008 Nick Kossifidis <mickflemm@gmail.com>
 * Copyright (c) 2007-2008 Jiri Slaby <jirislaby@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
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
 *
 */

/**************\
* Capabilities *
\**************/

#include "ath5k.h"
#include "reg.h"
#include "debug.h"
#include "base.h"

/*
 * Fill the capabilities struct
 * TODO: Merge this with EEPROM code when we are done with it
 */
int ath5k_hw_set_capabilities(struct ath5k_hw *ah)
{
	u16 ee_header;

	/* Capabilities stored in the EEPROM */
	ee_header = ah->ah_capabilities.cap_eeprom.ee_header;

	if (ah->ah_version == AR5K_AR5210) {
		/*
		 * Set radio capabilities
		 * (The AR5110 only supports the middle 5GHz band)
		 */
		ah->ah_capabilities.cap_range.range_5ghz_min = 5120;
		ah->ah_capabilities.cap_range.range_5ghz_max = 5430;
		ah->ah_capabilities.cap_range.range_2ghz_min = 0;
		ah->ah_capabilities.cap_range.range_2ghz_max = 0;

		/* Set supported modes */
		__set_bit(AR5K_MODE_11A, ah->ah_capabilities.cap_mode);
		__set_bit(AR5K_MODE_11A_TURBO, ah->ah_capabilities.cap_mode);
	} else {

		/*
		 * Set radio capabilities
		 */

		if (AR5K_EEPROM_HDR_11A(ee_header)) {
			/* 4920 */
			ah->ah_capabilities.cap_range.range_5ghz_min = 5005;
			ah->ah_capabilities.cap_range.range_5ghz_max = 6100;

			/* Set supported modes */
			__set_bit(AR5K_MODE_11A,
					ah->ah_capabilities.cap_mode);
			__set_bit(AR5K_MODE_11A_TURBO,
					ah->ah_capabilities.cap_mode);
			if (ah->ah_version == AR5K_AR5212)
				__set_bit(AR5K_MODE_11G_TURBO,
						ah->ah_capabilities.cap_mode);
		}

		/* Enable  802.11b if a 2GHz capable radio (2111/5112) is
		 * connected */
		if (AR5K_EEPROM_HDR_11B(ee_header) ||
		    (AR5K_EEPROM_HDR_11G(ee_header) &&
		     ah->ah_version != AR5K_AR5211)) {
			/* 2312 */
			ah->ah_capabilities.cap_range.range_2ghz_min = 2412;
			ah->ah_capabilities.cap_range.range_2ghz_max = 2732;

			if (AR5K_EEPROM_HDR_11B(ee_header))
				__set_bit(AR5K_MODE_11B,
						ah->ah_capabilities.cap_mode);

			if (AR5K_EEPROM_HDR_11G(ee_header) &&
			    ah->ah_version != AR5K_AR5211)
				__set_bit(AR5K_MODE_11G,
						ah->ah_capabilities.cap_mode);
		}
	}

	/* Set number of supported TX queues */
	if (ah->ah_version == AR5K_AR5210)
		ah->ah_capabilities.cap_queues.q_tx_num =
			AR5K_NUM_TX_QUEUES_NOQCU;
	else
		ah->ah_capabilities.cap_queues.q_tx_num = AR5K_NUM_TX_QUEUES;

	/* newer hardware has PHY error counters */
	if (ah->ah_mac_srev >= AR5K_SREV_AR5213A)
		ah->ah_capabilities.cap_has_phyerr_counters = true;
	else
		ah->ah_capabilities.cap_has_phyerr_counters = false;

	return 0;
}

/* Main function used by the driver part to check caps */
int ath5k_hw_get_capability(struct ath5k_hw *ah,
		enum ath5k_capability_type cap_type,
		u32 capability, u32 *result)
{
	switch (cap_type) {
	case AR5K_CAP_NUM_TXQUEUES:
		if (result) {
			if (ah->ah_version == AR5K_AR5210)
				*result = AR5K_NUM_TX_QUEUES_NOQCU;
			else
				*result = AR5K_NUM_TX_QUEUES;
			goto yes;
		}
	case AR5K_CAP_VEOL:
		goto yes;
	case AR5K_CAP_COMPRESSION:
		if (ah->ah_version == AR5K_AR5212)
			goto yes;
		else
			goto no;
	case AR5K_CAP_BURST:
		goto yes;
	case AR5K_CAP_TPC:
		goto yes;
	case AR5K_CAP_BSSIDMASK:
		if (ah->ah_version == AR5K_AR5212)
			goto yes;
		else
			goto no;
	case AR5K_CAP_XR:
		if (ah->ah_version == AR5K_AR5212)
			goto yes;
		else
			goto no;
	default:
		goto no;
	}

no:
	return -EINVAL;
yes:
	return 0;
}

/*
 * TODO: Following functions should be part of a new function
 * set_capability
 */

int ath5k_hw_enable_pspoll(struct ath5k_hw *ah, u8 *bssid,
		u16 assoc_id)
{
	if (ah->ah_version == AR5K_AR5210) {
		AR5K_REG_DISABLE_BITS(ah, AR5K_STA_ID1,
			AR5K_STA_ID1_NO_PSPOLL | AR5K_STA_ID1_DEFAULT_ANTENNA);
		return 0;
	}

	return -EIO;
}

int ath5k_hw_disable_pspoll(struct ath5k_hw *ah)
{
	if (ah->ah_version == AR5K_AR5210) {
		AR5K_REG_ENABLE_BITS(ah, AR5K_STA_ID1,
			AR5K_STA_ID1_NO_PSPOLL | AR5K_STA_ID1_DEFAULT_ANTENNA);
		return 0;
	}

	return -EIO;
}
