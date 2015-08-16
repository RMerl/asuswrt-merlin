/*
 * Copyright (c) 2012 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * Common DFS re-entry definitions.
 */
#ifndef _IEEE80211_DFS_REENTRY_H
#define _IEEE80211_DFS_REENTRY_H

/*
 *  DFS-reentry
 */
#define IEEE80211_PICK_DOMIAN_MASK	0x00FF
#define IEEE80211_PICK_ALL		0x0001		/* pick channel from all available channels */
#define IEEE80211_PICK_DFS		0x0002		/* pick channel from available DFS channel */
#define IEEE80211_PICK_NONDFS		0x0004		/* pick channel from available non-DFS channel */

#define IEEE80211_PICK_ALGORITHM_MASK	0xFF00
#define IEEE80211_PICK_CLEAREST		0x0100		/* pick clearest channel */
#define IEEE80211_PICK_REENTRY		0x0200		/* pick channel again after DFS process */
#define IEEE80211_PICK_NOPICK		0x0400		/* do not pick channel */
#define IEEE80211_PICK_NOPICK_BG	0x0800		/* scan background and do not pick channel */
#define IEEE80211_PICK_DEFAULT		(IEEE80211_PICK_ALL | IEEE80211_PICK_CLEAREST)

#endif
