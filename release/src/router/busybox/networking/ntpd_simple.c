/*
 * NTP client/server, based on OpenNTPD 3.9p1
 *
 * Author: Adam Tkac <vonsch@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <netinet/ip.h> /* For IPTOS_LOWDELAY definition */
#ifndef IPTOS_LOWDELAY
# define IPTOS_LOWDELAY 0x10
#endif
#ifndef IP_PKTINFO
# error "Sorry, your kernel has to support IP_PKTINFO"
#endif


/* Sync to peers every N secs */
#define INTERVAL_QUERY_NORMAL    30
#define INTERVAL_QUERY_PATHETIC  60
#define INTERVAL_QUERY_AGRESSIVE  5

/* Bad if *less than* TRUSTLEVEL_BADPEER */
#define TRUSTLEVEL_BADPEER        6
#define TRUSTLEVEL_PATHETIC       2
#define TRUSTLEVEL_AGRESSIVE      8
#define TRUSTLEVEL_MAX           10

#define QSCALE_OFF_MIN         0.05
#define QSCALE_OFF_MAX         0.50

/* Single query might take N secs max */
#define QUERYTIME_MAX            15
/* Min offset for settime at start. "man ntpd" says it's 128 ms */
#define STEPTIME_MIN_OFFSET   0.128

typedef struct {
	uint32_t int_partl;
	uint32_t fractionl;
} l_fixedpt_t;

typedef struct {
	uint16_t int_parts;
	uint16_t fractions;
} s_fixedpt_t;

enum {
	NTP_DIGESTSIZE     = 16,
	NTP_MSGSIZE_NOAUTH = 48,
	NTP_MSGSIZE        = (NTP_MSGSIZE_NOAUTH + 4 + NTP_DIGESTSIZE),
};

typedef struct {
	uint8_t     m_status;     /* status of local clock and leap info */
	uint8_t     m_stratum;    /* stratum level */
	uint8_t     m_ppoll;      /* poll value */
	int8_t      m_precision_exp;
	s_fixedpt_t m_rootdelay;
	s_fixedpt_t m_dispersion;
	uint32_t    m_refid;
	l_fixedpt_t m_reftime;
	l_fixedpt_t m_orgtime;
	l_fixedpt_t m_rectime;
	l_fixedpt_t m_xmttime;
	uint32_t    m_keyid;
	uint8_t     m_digest[NTP_DIGESTSIZE];
} msg_t;

enum {
	NTP_VERSION     = 4,
	NTP_MAXSTRATUM  = 15,

	/* Status Masks */
	MODE_MASK       = (7 << 0),
	VERSION_MASK    = (7 << 3),
	VERSION_SHIFT   = 3,
	LI_MASK         = (3 << 6),

	/* Leap Second Codes (high order two bits of m_status) */
	LI_NOWARNING    = (0 << 6),    /* no warning */
	LI_PLUSSEC      = (1 << 6),    /* add a second (61 seconds) */
	LI_MINUSSEC     = (2 << 6),    /* minus a second (59 seconds) */
	LI_ALARM        = (3 << 6),    /* alarm condition */

	/* Mode values */
	MODE_RES0       = 0,    /* reserved */
	MODE_SYM_ACT    = 1,    /* symmetric active */
	MODE_SYM_PAS    = 2,    /* symmetric passive */
	MODE_CLIENT     = 3,    /* client */
	MODE_SERVER     = 4,    /* server */
	MODE_BROADCAST  = 5,    /* broadcast */
	MODE_RES1       = 6,    /* reserved for NTP control message */
	MODE_RES2       = 7,    /* reserved for private use */
};

#define OFFSET_1900_1970 2208988800UL  /* 1970 - 1900 in seconds */

typedef struct {
	double   d_offset;
	double   d_delay;
	//UNUSED: double d_error;
	time_t   d_rcv_time;
	uint32_t d_refid4;
	uint8_t  d_leap;
	uint8_t  d_stratum;
	uint8_t  d_good;
} datapoint_t;

#define NUM_DATAPOINTS  8
typedef struct {
	len_and_sockaddr *p_lsa;
	char             *p_dotted;
	/* When to send new query (if p_fd == -1)
	 * or when receive times out (if p_fd >= 0): */
	time_t           next_action_time;
	int              p_fd;
	uint8_t          p_datapoint_idx;
	uint8_t          p_trustlevel;
	double           p_xmttime;
	datapoint_t      update;
	datapoint_t      p_datapoint[NUM_DATAPOINTS];
	msg_t            p_xmt_msg;
} peer_t;

enum {
	OPT_n = (1 << 0),
	OPT_q = (1 << 1),
	OPT_N = (1 << 2),
	OPT_x = (1 << 3),
	/* Insert new options above this line. */
	/* Non-compat options: */
	OPT_p = (1 << 4),
	OPT_l = (1 << 5) * ENABLE_FEATURE_NTPD_SERVER,
};


struct globals {
	/* total round trip delay to currently selected reference clock */
	double   rootdelay;
	/* reference timestamp: time when the system clock was last set or corrected */
	double   reftime;
	llist_t  *ntp_peers;
#if ENABLE_FEATURE_NTPD_SERVER
	int      listen_fd;
#endif
	unsigned verbose;
	unsigned peer_cnt;
	unsigned scale;
	uint32_t refid;
	uint32_t refid4;
	uint8_t  synced;
	uint8_t  leap;
#define G_precision_exp -6
//	int8_t   precision_exp;
	uint8_t  stratum;
	uint8_t  time_was_stepped;
	uint8_t  first_adj_done;
};
#define G (*ptr_to_globals)

static const int const_IPTOS_LOWDELAY = IPTOS_LOWDELAY;


static void
set_next(peer_t *p, unsigned t)
{
	p->next_action_time = time(NULL) + t;
}

static void
add_peers(char *s)
{
	peer_t *p;

	p = xzalloc(sizeof(*p));
	p->p_lsa = xhost2sockaddr(s, 123);
	p->p_dotted = xmalloc_sockaddr2dotted_noport(&p->p_lsa->u.sa);
	p->p_fd = -1;
	p->p_xmt_msg.m_status = MODE_CLIENT | (NTP_VERSION << 3);
	p->p_trustlevel = TRUSTLEVEL_PATHETIC;
	p->next_action_time = time(NULL); /* = set_next(p, 0); */

	llist_add_to(&G.ntp_peers, p);
	G.peer_cnt++;
}

static double
gettime1900d(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL); /* never fails */
	return (tv.tv_sec + 1.0e-6 * tv.tv_usec + OFFSET_1900_1970);
}

static void
d_to_tv(double d, struct timeval *tv)
{
	tv->tv_sec = (long)d;
	tv->tv_usec = (d - tv->tv_sec) * 1000000;
}

static double
lfp_to_d(l_fixedpt_t lfp)
{
	double ret;
	lfp.int_partl = ntohl(lfp.int_partl);
	lfp.fractionl = ntohl(lfp.fractionl);
	ret = (double)lfp.int_partl + ((double)lfp.fractionl / UINT_MAX);
	return ret;
}

#if 0 //UNUSED
static double
sfp_to_d(s_fixedpt_t sfp)
{
	double ret;
	sfp.int_parts = ntohs(sfp.int_parts);
	sfp.fractions = ntohs(sfp.fractions);
	ret = (double)sfp.int_parts + ((double)sfp.fractions / USHRT_MAX);
	return ret;
}
#endif

#if ENABLE_FEATURE_NTPD_SERVER
static l_fixedpt_t
d_to_lfp(double d)
{
	l_fixedpt_t lfp;
	lfp.int_partl = (uint32_t)d;
	lfp.fractionl = (uint32_t)((d - lfp.int_partl) * UINT_MAX);
	lfp.int_partl = htonl(lfp.int_partl);
	lfp.fractionl = htonl(lfp.fractionl);
	return lfp;
}

static s_fixedpt_t
d_to_sfp(double d)
{
	s_fixedpt_t sfp;
	sfp.int_parts = (uint16_t)d;
	sfp.fractions = (uint16_t)((d - sfp.int_parts) * USHRT_MAX);
	sfp.int_parts = htons(sfp.int_parts);
	sfp.fractions = htons(sfp.fractions);
	return sfp;
}
#endif

static unsigned
error_interval(void)
{
	unsigned interval, r;
	interval = INTERVAL_QUERY_PATHETIC * QSCALE_OFF_MAX / QSCALE_OFF_MIN;
	r = (unsigned)random() % (unsigned)(interval / 10);
	return (interval + r);
}

static int
do_sendto(int fd,
		const struct sockaddr *from, const struct sockaddr *to, socklen_t addrlen,
		msg_t *msg, ssize_t len)
{
	ssize_t ret;

	errno = 0;
	if (!from) {
		ret = sendto(fd, msg, len, MSG_DONTWAIT, to, addrlen);
	} else {
		ret = send_to_from(fd, msg, len, MSG_DONTWAIT, to, from, addrlen);
	}
	if (ret != len) {
		bb_perror_msg("send failed");
		return -1;
	}
	return 0;
}

static int
send_query_to_peer(peer_t *p)
{
	// Why do we need to bind()?
	// See what happens when we don't bind:
	//
	// socket(PF_INET, SOCK_DGRAM, IPPROTO_IP) = 3
	// setsockopt(3, SOL_IP, IP_TOS, [16], 4) = 0
	// gettimeofday({1259071266, 327885}, NULL) = 0
	// sendto(3, "xxx", 48, MSG_DONTWAIT, {sa_family=AF_INET, sin_port=htons(123), sin_addr=inet_addr("10.34.32.125")}, 16) = 48
	// ^^^ we sent it from some source port picked by kernel.
	// time(NULL)              = 1259071266
	// write(2, "ntpd: entering poll 15 secs\n", 28) = 28
	// poll([{fd=3, events=POLLIN}], 1, 15000) = 1 ([{fd=3, revents=POLLIN}])
	// recv(3, "yyy", 68, MSG_DONTWAIT) = 48
	// ^^^ this recv will receive packets to any local port!
	//
	// Uncomment this and use strace to see it in action:
#define PROBE_LOCAL_ADDR // { len_and_sockaddr lsa; lsa.len = LSA_SIZEOF_SA; getsockname(p->query.fd, &lsa.u.sa, &lsa.len); }

	if (p->p_fd == -1) {
		int fd, family;
		len_and_sockaddr *local_lsa;

		family = p->p_lsa->u.sa.sa_family;
		p->p_fd = fd = xsocket_type(&local_lsa, family, SOCK_DGRAM);
		/* local_lsa has "null" address and port 0 now.
		 * bind() ensures we have a *particular port* selected by kernel
		 * and remembered in p->p_fd, thus later recv(p->p_fd)
		 * receives only packets sent to this port.
		 */
		PROBE_LOCAL_ADDR
		xbind(fd, &local_lsa->u.sa, local_lsa->len);
		PROBE_LOCAL_ADDR
#if ENABLE_FEATURE_IPV6
		if (family == AF_INET)
#endif
			setsockopt(fd, IPPROTO_IP, IP_TOS, &const_IPTOS_LOWDELAY, sizeof(const_IPTOS_LOWDELAY));
		free(local_lsa);
	}

	/*
	 * Send out a random 64-bit number as our transmit time.  The NTP
	 * server will copy said number into the originate field on the
	 * response that it sends us.  This is totally legal per the SNTP spec.
	 *
	 * The impact of this is two fold: we no longer send out the current
	 * system time for the world to see (which may aid an attacker), and
	 * it gives us a (not very secure) way of knowing that we're not
	 * getting spoofed by an attacker that can't capture our traffic
	 * but can spoof packets from the NTP server we're communicating with.
	 *
	 * Save the real transmit timestamp locally.
	 */
	p->p_xmt_msg.m_xmttime.int_partl = random();
	p->p_xmt_msg.m_xmttime.fractionl = random();
	p->p_xmttime = gettime1900d();

	if (do_sendto(p->p_fd, /*from:*/ NULL, /*to:*/ &p->p_lsa->u.sa, /*addrlen:*/ p->p_lsa->len,
			&p->p_xmt_msg, NTP_MSGSIZE_NOAUTH) == -1
	) {
		close(p->p_fd);
		p->p_fd = -1;
		set_next(p, INTERVAL_QUERY_PATHETIC);
		return -1;
	}

	if (G.verbose)
		bb_error_msg("sent query to %s", p->p_dotted);
	set_next(p, QUERYTIME_MAX);

	return 0;
}


/* Time is stepped only once, when the first packet from a peer is received.
 */
static void
step_time_once(double offset)
{
	double dtime;
	llist_t *item;
	struct timeval tv;
	char buf[80];
	time_t tval;

	if (G.time_was_stepped)
		goto bail;
	G.time_was_stepped = 1;

	/* if the offset is small, don't step, slew (later) */
	if (offset < STEPTIME_MIN_OFFSET && offset > -STEPTIME_MIN_OFFSET)
		goto bail;

	gettimeofday(&tv, NULL); /* never fails */
	dtime = offset + tv.tv_sec;
	dtime += 1.0e-6 * tv.tv_usec;
	d_to_tv(dtime, &tv);

	if (settimeofday(&tv, NULL) == -1)
		bb_perror_msg_and_die("settimeofday");

	tval = tv.tv_sec;
	strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Z %Y", localtime(&tval));

	bb_error_msg("setting clock to %s (offset %fs)", buf, offset);

	for (item = G.ntp_peers; item != NULL; item = item->link) {
		peer_t *p = (peer_t *) item->data;
		p->next_action_time -= (time_t)offset;
	}

 bail:
	if (option_mask32 & OPT_q)
		exit(0);
}


/* Time is periodically slewed when we collect enough
 * good data points.
 */
static int
compare_offsets(const void *aa, const void *bb)
{
	const peer_t *const *a = aa;
	const peer_t *const *b = bb;
	if ((*a)->update.d_offset < (*b)->update.d_offset)
		return -1;
	return ((*a)->update.d_offset > (*b)->update.d_offset);
}
static unsigned
updated_scale(double offset)
{
	if (offset < 0)
		offset = -offset;
	if (offset > QSCALE_OFF_MAX)
		return 1;
	if (offset < QSCALE_OFF_MIN)
		return QSCALE_OFF_MAX / QSCALE_OFF_MIN;
	return QSCALE_OFF_MAX / offset;
}
static void
slew_time(void)
{
	llist_t *item;
	double offset_median;
	struct timeval tv;

	{
		peer_t **peers = xzalloc(sizeof(peers[0]) * G.peer_cnt);
		unsigned goodpeer_cnt = 0;
		unsigned middle;

		for (item = G.ntp_peers; item != NULL; item = item->link) {
			peer_t *p = (peer_t *) item->data;
			if (p->p_trustlevel < TRUSTLEVEL_BADPEER)
				continue;
			if (!p->update.d_good) {
				free(peers);
				return;
			}
			peers[goodpeer_cnt++] = p;
		}

		if (goodpeer_cnt == 0) {
			free(peers);
			goto clear_good;
		}

		qsort(peers, goodpeer_cnt, sizeof(peers[0]), compare_offsets);

		middle = goodpeer_cnt / 2;
		if (middle != 0 && (goodpeer_cnt & 1) == 0) {
			offset_median = (peers[middle-1]->update.d_offset + peers[middle]->update.d_offset) / 2;
			G.rootdelay = (peers[middle-1]->update.d_delay + peers[middle]->update.d_delay) / 2;
			G.stratum = 1 + MAX(peers[middle-1]->update.d_stratum, peers[middle]->update.d_stratum);
		} else {
			offset_median = peers[middle]->update.d_offset;
			G.rootdelay = peers[middle]->update.d_delay;
			G.stratum = 1 + peers[middle]->update.d_stratum;
		}
		G.leap = peers[middle]->update.d_leap;
		G.refid4 = peers[middle]->update.d_refid4;
		G.refid =
#if ENABLE_FEATURE_IPV6
			peers[middle]->p_lsa->u.sa.sa_family != AF_INET ?
				G.refid4 :
#endif
				peers[middle]->p_lsa->u.sin.sin_addr.s_addr;
		free(peers);
	}
//TODO: if (offset_median > BIG) step_time(offset_median)?

	G.scale = updated_scale(offset_median);

	bb_error_msg("adjusting clock by %fs, our stratum is %u, time scale %u",
			offset_median, G.stratum, G.scale);

	errno = 0;
	d_to_tv(offset_median, &tv);
	if (adjtime(&tv, &tv) == -1)
		bb_perror_msg_and_die("adjtime failed");
	if (G.verbose >= 2)
		bb_error_msg("old adjust: %d.%06u", (int)tv.tv_sec, (unsigned)tv.tv_usec);

	if (G.first_adj_done) {
		uint8_t synced = (tv.tv_sec == 0 && tv.tv_usec == 0);
		if (synced != G.synced) {
			G.synced = synced;
			bb_error_msg("clock is %ssynced", synced ? "" : "un");
		}
	}
	G.first_adj_done = 1;

	G.reftime = gettime1900d();

 clear_good:
	for (item = G.ntp_peers; item != NULL; item = item->link) {
		peer_t *p = (peer_t *) item->data;
		p->update.d_good = 0;
	}
}

static void
update_peer_data(peer_t *p)
{
	/* Clock filter.
	 * Find the datapoint with the lowest delay.
	 * Use that as the peer update.
	 * Invalidate it and all older ones.
	 */
	int i;
	int best = -1;
	int good = 0;

	for (i = 0; i < NUM_DATAPOINTS; i++) {
		if (p->p_datapoint[i].d_good) {
			good++;
			if (best < 0 || p->p_datapoint[i].d_delay < p->p_datapoint[best].d_delay)
				best = i;
		}
	}

	if (good < 8) //FIXME: was it meant to be NUM_DATAPOINTS, not 8?
		return;

	p->update = p->p_datapoint[best]; /* struct copy */
	slew_time();

	for (i = 0; i < NUM_DATAPOINTS; i++)
		if (p->p_datapoint[i].d_rcv_time <= p->p_datapoint[best].d_rcv_time)
			p->p_datapoint[i].d_good = 0;
}

static unsigned
scale_interval(unsigned requested)
{
	unsigned interval, r;
	interval = requested * G.scale;
	r = (unsigned)random() % (unsigned)(MAX(5, interval / 10));
	return (interval + r);
}
static void
recv_and_process_peer_pkt(peer_t *p)
{
	ssize_t     size;
	msg_t       msg;
	double      T1, T2, T3, T4;
	unsigned    interval;
	datapoint_t *datapoint;

	/* We can recvfrom here and check from.IP, but some multihomed
	 * ntp servers reply from their *other IP*.
	 * TODO: maybe we should check at least what we can: from.port == 123?
	 */
	size = recv(p->p_fd, &msg, sizeof(msg), MSG_DONTWAIT);
	if (size == -1) {
		bb_perror_msg("recv(%s) error", p->p_dotted);
		if (errno == EHOSTUNREACH || errno == EHOSTDOWN
		 || errno == ENETUNREACH || errno == ENETDOWN
		 || errno == ECONNREFUSED || errno == EADDRNOTAVAIL
		 || errno == EAGAIN
		) {
//TODO: always do this?
			set_next(p, error_interval());
			goto close_sock;
		}
		xfunc_die();
	}

	if (size != NTP_MSGSIZE_NOAUTH && size != NTP_MSGSIZE) {
		bb_error_msg("malformed packet received from %s", p->p_dotted);
		goto bail;
	}

	if (msg.m_orgtime.int_partl != p->p_xmt_msg.m_xmttime.int_partl
	 || msg.m_orgtime.fractionl != p->p_xmt_msg.m_xmttime.fractionl
	) {
		goto bail;
	}

	if ((msg.m_status & LI_ALARM) == LI_ALARM
	 || msg.m_stratum == 0
	 || msg.m_stratum > NTP_MAXSTRATUM
	) {
// TODO: stratum 0 responses may have commands in 32-bit m_refid field:
// "DENY", "RSTR" - peer does not like us at all
// "RATE" - peer is overloaded, reduce polling freq
		interval = error_interval();
		bb_error_msg("reply from %s: not synced, next query in %us", p->p_dotted, interval);
		goto close_sock;
	}

	/*
	 * From RFC 2030 (with a correction to the delay math):
	 *
	 * Timestamp Name          ID   When Generated
	 * ------------------------------------------------------------
	 * Originate Timestamp     T1   time request sent by client
	 * Receive Timestamp       T2   time request received by server
	 * Transmit Timestamp      T3   time reply sent by server
	 * Destination Timestamp   T4   time reply received by client
	 *
	 * The roundtrip delay and local clock offset are defined as
	 *
	 * delay = (T4 - T1) - (T3 - T2); offset = ((T2 - T1) + (T3 - T4)) / 2
	 */
	T1 = p->p_xmttime;
	T2 = lfp_to_d(msg.m_rectime);
	T3 = lfp_to_d(msg.m_xmttime);
	T4 = gettime1900d();

	datapoint = &p->p_datapoint[p->p_datapoint_idx];

	datapoint->d_offset = ((T2 - T1) + (T3 - T4)) / 2;
	datapoint->d_delay = (T4 - T1) - (T3 - T2);
	if (datapoint->d_delay < 0) {
		bb_error_msg("reply from %s: negative delay %f", p->p_dotted, datapoint->d_delay);
		interval = error_interval();
		set_next(p, interval);
		goto close_sock;
	}
	//UNUSED: datapoint->d_error = (T2 - T1) - (T3 - T4);
	datapoint->d_rcv_time = (time_t)(T4 - OFFSET_1900_1970); /* = time(NULL); */
	datapoint->d_good = 1;

	datapoint->d_leap = (msg.m_status & LI_MASK);
	//UNUSED: datapoint->o_precision = msg.m_precision_exp;
	//UNUSED: datapoint->o_rootdelay = sfp_to_d(msg.m_rootdelay);
	//UNUSED: datapoint->o_rootdispersion = sfp_to_d(msg.m_dispersion);
	//UNUSED: datapoint->d_refid = ntohl(msg.m_refid);
	datapoint->d_refid4 = msg.m_xmttime.fractionl;
	//UNUSED: datapoint->o_reftime = lfp_to_d(msg.m_reftime);
	//UNUSED: datapoint->o_poll = msg.m_ppoll;
	datapoint->d_stratum = msg.m_stratum;

	if (p->p_trustlevel < TRUSTLEVEL_PATHETIC)
		interval = scale_interval(INTERVAL_QUERY_PATHETIC);
	else if (p->p_trustlevel < TRUSTLEVEL_AGRESSIVE)
		interval = scale_interval(INTERVAL_QUERY_AGRESSIVE);
	else
		interval = scale_interval(INTERVAL_QUERY_NORMAL);

	set_next(p, interval);

	/* Every received reply which we do not discard increases trust */
	if (p->p_trustlevel < TRUSTLEVEL_MAX) {
		p->p_trustlevel++;
		if (p->p_trustlevel == TRUSTLEVEL_BADPEER)
			bb_error_msg("peer %s now valid", p->p_dotted);
	}

	if (G.verbose)
		bb_error_msg("reply from %s: offset %f delay %f, next query in %us", p->p_dotted,
			datapoint->d_offset, datapoint->d_delay, interval);

	update_peer_data(p);
//TODO: do it after all peers had a chance to return at least one reply?
	step_time_once(datapoint->d_offset);

	p->p_datapoint_idx++;
	if (p->p_datapoint_idx >= NUM_DATAPOINTS)
		p->p_datapoint_idx = 0;

 close_sock:
	/* We do not expect any more packets from this peer for now.
	 * Closing the socket informs kernel about it.
	 * We open a new socket when we send a new query.
	 */
	close(p->p_fd);
	p->p_fd = -1;
 bail:
	return;
}

#if ENABLE_FEATURE_NTPD_SERVER
static void
recv_and_process_client_pkt(void /*int fd*/)
{
	ssize_t          size;
	uint8_t          version;
	double           rectime;
	len_and_sockaddr *to;
	struct sockaddr  *from;
	msg_t            msg;
	uint8_t          query_status;
	uint8_t          query_ppoll;
	l_fixedpt_t      query_xmttime;

	to = get_sock_lsa(G.listen_fd);
	from = xzalloc(to->len);

	size = recv_from_to(G.listen_fd, &msg, sizeof(msg), MSG_DONTWAIT, from, &to->u.sa, to->len);
	if (size != NTP_MSGSIZE_NOAUTH && size != NTP_MSGSIZE) {
		char *addr;
		if (size < 0) {
			if (errno == EAGAIN)
				goto bail;
			bb_perror_msg_and_die("recv");
		}
		addr = xmalloc_sockaddr2dotted_noport(from);
		bb_error_msg("malformed packet received from %s: size %u", addr, (int)size);
		free(addr);
		goto bail;
	}

	query_status = msg.m_status;
	query_ppoll = msg.m_ppoll;
	query_xmttime = msg.m_xmttime;

	/* Build a reply packet */
	memset(&msg, 0, sizeof(msg));
	msg.m_status = G.synced ? G.leap : LI_ALARM;
	msg.m_status |= (query_status & VERSION_MASK);
	msg.m_status |= ((query_status & MODE_MASK) == MODE_CLIENT) ?
			 MODE_SERVER : MODE_SYM_PAS;
	msg.m_stratum = G.stratum;
	msg.m_ppoll = query_ppoll;
	msg.m_precision_exp = G_precision_exp;
	rectime = gettime1900d();
	msg.m_xmttime = msg.m_rectime = d_to_lfp(rectime);
	msg.m_reftime = d_to_lfp(G.reftime);
	//msg.m_xmttime = d_to_lfp(gettime1900d()); // = msg.m_rectime
	msg.m_orgtime = query_xmttime;
	msg.m_rootdelay = d_to_sfp(G.rootdelay);
	version = (query_status & VERSION_MASK); /* ... >> VERSION_SHIFT - done below instead */
	msg.m_refid = (version > (3 << VERSION_SHIFT)) ? G.refid4 : G.refid;

	/* We reply from the local address packet was sent to,
	 * this makes to/from look swapped here: */
	do_sendto(G.listen_fd,
		/*from:*/ &to->u.sa, /*to:*/ from, /*addrlen:*/ to->len,
		&msg, size);

 bail:
	free(to);
	free(from);
}
#endif

/* Upstream ntpd's options:
 *
 * -4   Force DNS resolution of host names to the IPv4 namespace.
 * -6   Force DNS resolution of host names to the IPv6 namespace.
 * -a   Require cryptographic authentication for broadcast client,
 *      multicast client and symmetric passive associations.
 *      This is the default.
 * -A   Do not require cryptographic authentication for broadcast client,
 *      multicast client and symmetric passive associations.
 *      This is almost never a good idea.
 * -b   Enable the client to synchronize to broadcast servers.
 * -c conffile
 *      Specify the name and path of the configuration file,
 *      default /etc/ntp.conf
 * -d   Specify debugging mode. This option may occur more than once,
 *      with each occurrence indicating greater detail of display.
 * -D level
 *      Specify debugging level directly.
 * -f driftfile
 *      Specify the name and path of the frequency file.
 *      This is the same operation as the "driftfile FILE"
 *      configuration command.
 * -g   Normally, ntpd exits with a message to the system log
 *      if the offset exceeds the panic threshold, which is 1000 s
 *      by default. This option allows the time to be set to any value
 *      without restriction; however, this can happen only once.
 *      If the threshold is exceeded after that, ntpd will exit
 *      with a message to the system log. This option can be used
 *      with the -q and -x options. See the tinker command for other options.
 * -i jaildir
 *      Chroot the server to the directory jaildir. This option also implies
 *      that the server attempts to drop root privileges at startup
 *      (otherwise, chroot gives very little additional security).
 *      You may need to also specify a -u option.
 * -k keyfile
 *      Specify the name and path of the symmetric key file,
 *      default /etc/ntp/keys. This is the same operation
 *      as the "keys FILE" configuration command.
 * -l logfile
 *      Specify the name and path of the log file. The default
 *      is the system log file. This is the same operation as
 *      the "logfile FILE" configuration command.
 * -L   Do not listen to virtual IPs. The default is to listen.
 * -n   Don't fork.
 * -N   To the extent permitted by the operating system,
 *      run the ntpd at the highest priority.
 * -p pidfile
 *      Specify the name and path of the file used to record the ntpd
 *      process ID. This is the same operation as the "pidfile FILE"
 *      configuration command.
 * -P priority
 *      To the extent permitted by the operating system,
 *      run the ntpd at the specified priority.
 * -q   Exit the ntpd just after the first time the clock is set.
 *      This behavior mimics that of the ntpdate program, which is
 *      to be retired. The -g and -x options can be used with this option.
 *      Note: The kernel time discipline is disabled with this option.
 * -r broadcastdelay
 *      Specify the default propagation delay from the broadcast/multicast
 *      server to this client. This is necessary only if the delay
 *      cannot be computed automatically by the protocol.
 * -s statsdir
 *      Specify the directory path for files created by the statistics
 *      facility. This is the same operation as the "statsdir DIR"
 *      configuration command.
 * -t key
 *      Add a key number to the trusted key list. This option can occur
 *      more than once.
 * -u user[:group]
 *      Specify a user, and optionally a group, to switch to.
 * -v variable
 * -V variable
 *      Add a system variable listed by default.
 * -x   Normally, the time is slewed if the offset is less than the step
 *      threshold, which is 128 ms by default, and stepped if above
 *      the threshold. This option sets the threshold to 600 s, which is
 *      well within the accuracy window to set the clock manually.
 *      Note: since the slew rate of typical Unix kernels is limited
 *      to 0.5 ms/s, each second of adjustment requires an amortization
 *      interval of 2000 s. Thus, an adjustment as much as 600 s
 *      will take almost 14 days to complete. This option can be used
 *      with the -g and -q options. See the tinker command for other options.
 *      Note: The kernel time discipline is disabled with this option.
 */

/* By doing init in a separate function we decrease stack usage
 * in main loop.
 */
static NOINLINE void ntp_init(char **argv)
{
	unsigned opts;
	llist_t *peers;

	srandom(getpid());

	if (getuid())
		bb_error_msg_and_die(bb_msg_you_must_be_root);

	peers = NULL;
	opt_complementary = "dd:p::"; /* d: counter, p: list */
	opts = getopt32(argv,
			"nqNx" /* compat */
			"p:"IF_FEATURE_NTPD_SERVER("l") /* NOT compat */
			"d" /* compat */
			"46aAbgL", /* compat, ignored */
			&peers, &G.verbose);
	if (!(opts & (OPT_p|OPT_l)))
		bb_show_usage();
	if (opts & OPT_x) /* disable stepping, only slew is allowed */
		G.time_was_stepped = 1;
	while (peers)
		add_peers(llist_pop(&peers));
	if (!(opts & OPT_n)) {
		bb_daemonize_or_rexec(DAEMON_DEVNULL_STDIO, argv);
		logmode = LOGMODE_NONE;
	}
#if ENABLE_FEATURE_NTPD_SERVER
	G.listen_fd = -1;
	if (opts & OPT_l) {
		G.listen_fd = create_and_bind_dgram_or_die(NULL, 123);
		socket_want_pktinfo(G.listen_fd);
		setsockopt(G.listen_fd, IPPROTO_IP, IP_TOS, &const_IPTOS_LOWDELAY, sizeof(const_IPTOS_LOWDELAY));
	}
#endif
	/* I hesitate to set -20 prio. -15 should be high enough for timekeeping */
	if (opts & OPT_N)
		setpriority(PRIO_PROCESS, 0, -15);

	/* Set some globals */
#if 0
	/* With constant b = 100, G.precision_exp is also constant -6.
	 * Uncomment this and you'll see */
	{
		int prec = 0;
		int b;
# if 0
		struct timespec	tp;
		/* We can use sys_clock_getres but assuming 10ms tick should be fine */
		clock_getres(CLOCK_REALTIME, &tp);
		tp.tv_sec = 0;
		tp.tv_nsec = 10000000;
		b = 1000000000 / tp.tv_nsec;  /* convert to Hz */
# else
		b = 100; /* b = 1000000000/10000000 = 100 */
# endif
		while (b > 1)
			prec--, b >>= 1;
		//G.precision_exp = prec;
		bb_error_msg("G.precision_exp:%d", prec); /* -6 */
	}
#endif
	G.scale = 1;

	bb_signals((1 << SIGTERM) | (1 << SIGINT), record_signo);
	bb_signals((1 << SIGPIPE) | (1 << SIGHUP), SIG_IGN);
}

int ntpd_main(int argc UNUSED_PARAM, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ntpd_main(int argc UNUSED_PARAM, char **argv)
{
	struct globals g;
	struct pollfd *pfd;
	peer_t **idx2peer;

	memset(&g, 0, sizeof(g));
	SET_PTR_TO_GLOBALS(&g);

	ntp_init(argv);

	{
		/* if ENABLE_FEATURE_NTPD_SERVER, + 1 for listen_fd: */
		unsigned cnt = g.peer_cnt + ENABLE_FEATURE_NTPD_SERVER;
		idx2peer = xzalloc(sizeof(idx2peer[0]) * cnt);
		pfd = xzalloc(sizeof(pfd[0]) * cnt);
	}

	while (!bb_got_signal) {
		llist_t *item;
		unsigned i, j;
		unsigned sent_cnt, trial_cnt;
		int nfds, timeout;
		time_t cur_time, nextaction;

		/* Nothing between here and poll() blocks for any significant time */

		cur_time = time(NULL);
		nextaction = cur_time + 3600;

		i = 0;
#if ENABLE_FEATURE_NTPD_SERVER
		if (g.listen_fd != -1) {
			pfd[0].fd = g.listen_fd;
			pfd[0].events = POLLIN;
			i++;
		}
#endif
		/* Pass over peer list, send requests, time out on receives */
		sent_cnt = trial_cnt = 0;
		for (item = g.ntp_peers; item != NULL; item = item->link) {
			peer_t *p = (peer_t *) item->data;

			/* Overflow-safe "if (p->next_action_time <= cur_time) ..." */
			if ((int)(cur_time - p->next_action_time) >= 0) {
				if (p->p_fd == -1) {
					/* Time to send new req */
					trial_cnt++;
					if (send_query_to_peer(p) == 0)
						sent_cnt++;
				} else {
					/* Timed out waiting for reply */
					close(p->p_fd);
					p->p_fd = -1;
					timeout = error_interval();
					bb_error_msg("timed out waiting for %s, "
							"next query in %us", p->p_dotted, timeout);
					if (p->p_trustlevel >= TRUSTLEVEL_BADPEER) {
						p->p_trustlevel /= 2;
						if (p->p_trustlevel < TRUSTLEVEL_BADPEER)
							bb_error_msg("peer %s now invalid", p->p_dotted);
					}
					set_next(p, timeout);
				}
			}

			if (p->next_action_time < nextaction)
				nextaction = p->next_action_time;

			if (p->p_fd >= 0) {
				/* Wait for reply from this peer */
				pfd[i].fd = p->p_fd;
				pfd[i].events = POLLIN;
				idx2peer[i] = p;
				i++;
			}
		}

		if ((trial_cnt > 0 && sent_cnt == 0) || g.peer_cnt == 0)
			step_time_once(0); /* no good peers, don't wait */

		timeout = nextaction - cur_time;
		if (timeout < 1)
			timeout = 1;

		/* Here we may block */
		if (g.verbose >= 2)
			bb_error_msg("poll %us, sockets:%u", timeout, i);
		nfds = poll(pfd, i, timeout * 1000);
		if (nfds <= 0)
			continue;

		/* Process any received packets */
		j = 0;
#if ENABLE_FEATURE_NTPD_SERVER
		if (g.listen_fd != -1) {
			if (pfd[0].revents /* & (POLLIN|POLLERR)*/) {
				nfds--;
				recv_and_process_client_pkt(/*g.listen_fd*/);
			}
			j = 1;
		}
#endif
		for (; nfds != 0 && j < i; j++) {
			if (pfd[j].revents /* & (POLLIN|POLLERR)*/) {
				nfds--;
				recv_and_process_peer_pkt(idx2peer[j]);
			}
		}
	} /* while (!bb_got_signal) */

	kill_myself_with_sig(bb_got_signal);
}
