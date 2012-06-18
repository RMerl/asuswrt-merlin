#ifndef _IPT_PSD_H
#define _IPT_PSD_H

#include <linux/param.h>
#include <linux/types.h>

/*
 * High port numbers have a lower weight to reduce the frequency of false
 * positives, such as from passive mode FTP transfers.
 */
#define PORT_WEIGHT_PRIV		3
#define PORT_WEIGHT_HIGH		1

/*
 * Port scan detection thresholds: at least COUNT ports need to be scanned
 * from the same source, with no longer than DELAY ticks between ports.
 */
#define SCAN_MIN_COUNT			7
#define SCAN_MAX_COUNT			(SCAN_MIN_COUNT * PORT_WEIGHT_PRIV)
#define SCAN_WEIGHT_THRESHOLD		SCAN_MAX_COUNT
#define SCAN_DELAY_THRESHOLD		(HZ * 3)

/*
 * Keep track of up to LIST_SIZE source addresses, using a hash table of
 * HASH_SIZE entries for faster lookups, but limiting hash collisions to
 * HASH_MAX source addresses per the same hash value.
 */
#define LIST_SIZE			0x100
#define HASH_LOG			9
#define HASH_SIZE			(1 << HASH_LOG)
#define HASH_MAX			0x10

struct ipt_psd_info {
	unsigned int weight_threshold;
	unsigned int delay_threshold;
	unsigned short lo_ports_weight;
	unsigned short hi_ports_weight;
};

#endif /*_IPT_PSD_H*/
