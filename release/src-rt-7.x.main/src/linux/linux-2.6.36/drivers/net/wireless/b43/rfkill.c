/*

  Broadcom B43 wireless driver
  RFKILL support

  Copyright (c) 2007 Michael Buesch <mb@bu3sch.de>

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

#include "b43.h"


/* Returns TRUE, if the radio is enabled in hardware. */
bool b43_is_hw_radio_enabled(struct b43_wldev *dev)
{
	if (dev->phy.rev >= 3 || dev->phy.type == B43_PHYTYPE_LP) {
		if (!(b43_read32(dev, B43_MMIO_RADIO_HWENABLED_HI)
		      & B43_MMIO_RADIO_HWENABLED_HI_MASK))
			return 1;
	} else {
		/* To prevent CPU fault on PPC, do not read a register
		 * unless the interface is started; however, on resume
		 * for hibernation, this routine is entered early. When
		 * that happens, unconditionally return TRUE.
		 */
		if (b43_status(dev) < B43_STAT_STARTED)
			return 1;
		if (b43_read16(dev, B43_MMIO_RADIO_HWENABLED_LO)
		    & B43_MMIO_RADIO_HWENABLED_LO_MASK)
			return 1;
	}
	return 0;
}

/* The poll callback for the hardware button. */
void b43_rfkill_poll(struct ieee80211_hw *hw)
{
	struct b43_wl *wl = hw_to_b43_wl(hw);
	struct b43_wldev *dev = wl->current_dev;
	struct ssb_bus *bus = dev->dev->bus;
	bool enabled;
	bool brought_up = false;

	mutex_lock(&wl->mutex);
	if (unlikely(b43_status(dev) < B43_STAT_INITIALIZED)) {
		if (ssb_bus_powerup(bus, 0)) {
			mutex_unlock(&wl->mutex);
			return;
		}
		ssb_device_enable(dev->dev, 0);
		brought_up = true;
	}

	enabled = b43_is_hw_radio_enabled(dev);

	if (unlikely(enabled != dev->radio_hw_enable)) {
		dev->radio_hw_enable = enabled;
		b43info(wl, "Radio hardware status changed to %s\n",
			enabled ? "ENABLED" : "DISABLED");
		wiphy_rfkill_set_hw_state(hw->wiphy, !enabled);
		if (enabled != dev->phy.radio_on)
			b43_software_rfkill(dev, !enabled);
	}

	if (brought_up) {
		ssb_device_disable(dev->dev, 0);
		ssb_bus_may_powerdown(bus);
	}

	mutex_unlock(&wl->mutex);
}
