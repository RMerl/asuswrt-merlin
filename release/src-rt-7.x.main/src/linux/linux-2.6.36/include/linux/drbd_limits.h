/*
  drbd_limits.h
  This file is part of DRBD by Philipp Reisner and Lars Ellenberg.
*/

/*
 * Our current limitations.
 * Some of them are hard limits,
 * some of them are arbitrary range limits, that make it easier to provide
 * feedback about nonsense settings for certain configurable values.
 */

#ifndef DRBD_LIMITS_H
#define DRBD_LIMITS_H 1

#define DEBUG_RANGE_CHECK 0

#define DRBD_MINOR_COUNT_MIN 1
#define DRBD_MINOR_COUNT_MAX 255

#define DRBD_DIALOG_REFRESH_MIN 0
#define DRBD_DIALOG_REFRESH_MAX 600

/* valid port number */
#define DRBD_PORT_MIN 1
#define DRBD_PORT_MAX 0xffff

/* startup { */
  /* if you want more than 3.4 days, disable */
#define DRBD_WFC_TIMEOUT_MIN 0
#define DRBD_WFC_TIMEOUT_MAX 300000
#define DRBD_WFC_TIMEOUT_DEF 0

#define DRBD_DEGR_WFC_TIMEOUT_MIN 0
#define DRBD_DEGR_WFC_TIMEOUT_MAX 300000
#define DRBD_DEGR_WFC_TIMEOUT_DEF 0

#define DRBD_OUTDATED_WFC_TIMEOUT_MIN 0
#define DRBD_OUTDATED_WFC_TIMEOUT_MAX 300000
#define DRBD_OUTDATED_WFC_TIMEOUT_DEF 0
/* }*/

/* net { */
  /* timeout, unit centi seconds
   * more than one minute timeout is not usefull */
#define DRBD_TIMEOUT_MIN 1
#define DRBD_TIMEOUT_MAX 600
#define DRBD_TIMEOUT_DEF 60       /* 6 seconds */

  /* active connection retries when C_WF_CONNECTION */
#define DRBD_CONNECT_INT_MIN 1
#define DRBD_CONNECT_INT_MAX 120
#define DRBD_CONNECT_INT_DEF 10   /* seconds */

  /* keep-alive probes when idle */
#define DRBD_PING_INT_MIN 1
#define DRBD_PING_INT_MAX 120
#define DRBD_PING_INT_DEF 10

 /* timeout for the ping packets.*/
#define DRBD_PING_TIMEO_MIN  1
#define DRBD_PING_TIMEO_MAX  100
#define DRBD_PING_TIMEO_DEF  5

  /* max number of write requests between write barriers */
#define DRBD_MAX_EPOCH_SIZE_MIN 1
#define DRBD_MAX_EPOCH_SIZE_MAX 20000
#define DRBD_MAX_EPOCH_SIZE_DEF 2048

  /* I don't think that a tcp send buffer of more than 10M is usefull */
#define DRBD_SNDBUF_SIZE_MIN  0
#define DRBD_SNDBUF_SIZE_MAX  (10<<20)
#define DRBD_SNDBUF_SIZE_DEF  0

#define DRBD_RCVBUF_SIZE_MIN  0
#define DRBD_RCVBUF_SIZE_MAX  (10<<20)
#define DRBD_RCVBUF_SIZE_DEF  0

  /* @4k PageSize -> 128kB - 512MB */
#define DRBD_MAX_BUFFERS_MIN  32
#define DRBD_MAX_BUFFERS_MAX  131072
#define DRBD_MAX_BUFFERS_DEF  2048

  /* @4k PageSize -> 4kB - 512MB */
#define DRBD_UNPLUG_WATERMARK_MIN  1
#define DRBD_UNPLUG_WATERMARK_MAX  131072
#define DRBD_UNPLUG_WATERMARK_DEF (DRBD_MAX_BUFFERS_DEF/16)

  /* 0 is disabled.
   * 200 should be more than enough even for very short timeouts */
#define DRBD_KO_COUNT_MIN  0
#define DRBD_KO_COUNT_MAX  200
#define DRBD_KO_COUNT_DEF  0
/* } */

/* syncer { */
#define DRBD_RATE_MIN 1
/* channel bonding 10 GbE, or other hardware */
#define DRBD_RATE_MAX (4 << 20)
#define DRBD_RATE_DEF 250  /* kb/second */

  /* less than 7 would hit performance unneccessarily.
   * 3833 is the largest prime that still does fit
   * into 64 sectors of activity log */
#define DRBD_AL_EXTENTS_MIN  7
#define DRBD_AL_EXTENTS_MAX  3833
#define DRBD_AL_EXTENTS_DEF  127

#define DRBD_AFTER_MIN  -1
#define DRBD_AFTER_MAX  255
#define DRBD_AFTER_DEF  -1

/* } */

/* drbdsetup XY resize -d Z
 * you are free to reduce the device size to nothing, if you want to.
 * the upper limit with 64bit kernel, enough ram and flexible meta data
 * is 16 TB, currently. */
/* DRBD_MAX_SECTORS */
#define DRBD_DISK_SIZE_SECT_MIN  0
#define DRBD_DISK_SIZE_SECT_MAX  (16 * (2LLU << 30))
#define DRBD_DISK_SIZE_SECT_DEF  0 /* = disabled = no user size... */

#define DRBD_ON_IO_ERROR_DEF EP_PASS_ON
#define DRBD_FENCING_DEF FP_DONT_CARE
#define DRBD_AFTER_SB_0P_DEF ASB_DISCONNECT
#define DRBD_AFTER_SB_1P_DEF ASB_DISCONNECT
#define DRBD_AFTER_SB_2P_DEF ASB_DISCONNECT
#define DRBD_RR_CONFLICT_DEF ASB_DISCONNECT

#define DRBD_MAX_BIO_BVECS_MIN 0
#define DRBD_MAX_BIO_BVECS_MAX 128
#define DRBD_MAX_BIO_BVECS_DEF 0

#define DRBD_DP_VOLUME_MIN 4
#define DRBD_DP_VOLUME_MAX 1048576
#define DRBD_DP_VOLUME_DEF 16384

#define DRBD_DP_INTERVAL_MIN 1
#define DRBD_DP_INTERVAL_MAX 600
#define DRBD_DP_INTERVAL_DEF 5

#define DRBD_RS_THROTTLE_TH_MIN 1
#define DRBD_RS_THROTTLE_TH_MAX 600
#define DRBD_RS_THROTTLE_TH_DEF 20

#define DRBD_RS_HOLD_OFF_TH_MIN 1
#define DRBD_RS_HOLD_OFF_TH_MAX 6000
#define DRBD_RS_HOLD_OFF_TH_DEF 100

#undef RANGE
#endif
