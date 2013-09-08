
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "rtdot1x.h"
#include "radius.h"
#include "radius_client.h"
#include "eloop.h"

/* Defaults for RADIUS retransmit values (exponential backoff) */
#define RADIUS_CLIENT_FIRST_WAIT 1 /* seconds */
#define RADIUS_CLIENT_MAX_WAIT 120 /* seconds */
#define RADIUS_CLIENT_MAX_RETRIES 10 /* maximum number of retransmit attempts
					  * before entry is removed from retransmit list */
#define RADIUS_CLIENT_MAX_ENTRIES 30 /* maximum number of entries in retransmit
					  * list (oldest will be removed, if this limit is exceeded) */
#define RADIUS_CLIENT_NUM_FAILOVER 4 /* try to change RADIUS server after this
					  * many failed retry attempts */

static int
Radius_change_server(rtapd *rtapd, struct hostapd_radius_server *nserv,
			 struct hostapd_radius_server *oserv, int sock, int auth);

static void Radius_client_msg_free(struct radius_msg_list *req)
{
	Radius_msg_free(req->msg);
	free(req->msg);
	free(req);
}

int Radius_client_register(rtapd *apd, RadiusType msg_type,
			   RadiusRxResult (*handler)(rtapd *apd, struct radius_msg *msg, struct radius_msg *req,
							 u8 *shared_secret, size_t shared_secret_len, void *data), void *data)
{
	struct radius_rx_handler **handlers, *newh;
	size_t *num;

	handlers = &apd->radius->auth_handlers;
	num = &apd->radius->num_auth_handlers;

	newh = (struct radius_rx_handler *)
		realloc(*handlers, (*num + 1) * sizeof(struct radius_rx_handler));
	if (newh == NULL)
		return -1;

	newh[*num].handler = handler;
	newh[*num].data = data;
	(*num)++;
	*handlers = newh;

	return 0;
}

static int Radius_client_retransmit(rtapd *rtapd, struct radius_msg_list *entry, time_t now)
{
	int s;

#if MULTIPLE_RADIUS
	s = rtapd->radius->mbss_auth_serv_sock[entry->ApIdx];
#else
	s = rtapd->radius->auth_serv_sock;
#endif
	/* retransmit; remove entry if too many attempts */
	entry->attempts++;

	if (send(s, entry->msg->buf, entry->msg->buf_used, 0) < 0)
		perror("send[RADIUS]");

	entry->next_try = now + entry->next_wait;
	entry->next_wait *= 2;
	if (entry->next_wait > RADIUS_CLIENT_MAX_WAIT)
		entry->next_wait = RADIUS_CLIENT_MAX_WAIT;
	if (entry->attempts >= RADIUS_CLIENT_MAX_RETRIES)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Removing un-ACKed RADIUS message due to too many failed retransmit attempts\n");
		return 1;
	}

	return 0;
}

static void Radius_client_timer(void *eloop_ctx, void *timeout_ctx)
{
	rtapd *rtapd = eloop_ctx;
	time_t now, first;
	struct radius_msg_list *entry, *prev, *tmp;
#if MULTIPLE_RADIUS
	int	i;
	int mbss_auth_failover[MAX_MBSSID_NUM];
#else
	int auth_failover = 0;
#endif

#if MULTIPLE_RADIUS
	for (i = 0; i < MAX_MBSSID_NUM; i++)
		mbss_auth_failover[i] = 0;
#endif

	entry = rtapd->radius->msgs;
	if (!entry)
		return;

	time(&now);
	first = 0;

	prev = NULL;
	while (entry)
	{
		if (now >= entry->next_try && Radius_client_retransmit(rtapd, entry, now))
		{
			if (prev)
				prev->next = entry->next;
			else
				rtapd->radius->msgs = entry->next;

			tmp = entry;
			entry = entry->next;
			Radius_client_msg_free(tmp);
			rtapd->radius->num_msgs--;
			continue;
		}

		if (entry->attempts > RADIUS_CLIENT_NUM_FAILOVER)
		{
			if (entry->msg_type == RADIUS_AUTH)
			{
#if MULTIPLE_RADIUS
				mbss_auth_failover[entry->ApIdx]++;
#else
				auth_failover++;
#endif
				DBGPRINT(RT_DEBUG_WARN, "Radius_client_timer : Failed retry attempts(%d) \n", RADIUS_CLIENT_NUM_FAILOVER);
			}	
		}

		if (first == 0 || entry->next_try < first)
			first = entry->next_try;

		prev = entry;
		entry = entry->next;
	}

	if (rtapd->radius->msgs)
	{
		if (first < now)
			first = now;
		eloop_register_timeout(first - now, 0, Radius_client_timer, rtapd, NULL);
	}
#if MULTIPLE_RADIUS
	for (i = 0; i < rtapd->conf->SsidNum; i++)
	{	
		if (mbss_auth_failover[i] && rtapd->conf->mbss_num_auth_servers[i] > 1)
		{
			struct hostapd_radius_server *next, *old;
			old = rtapd->conf->mbss_auth_server[i];

			next = old + 1;
			if (next > &(rtapd->conf->mbss_auth_servers[i][rtapd->conf->mbss_num_auth_servers[i]- 1]))
				next = rtapd->conf->mbss_auth_servers[i];
			rtapd->conf->mbss_auth_server[i] = next;
			Radius_change_server(rtapd, next, old, rtapd->radius->mbss_auth_serv_sock[i], 1);
			DBGPRINT(RT_DEBUG_WARN, "Radius_client_timer : ready to change RADIUS server for %s%d\n", rtapd->prefix_wlan_name, i);
		}	
	}
#else
	if (auth_failover && rtapd->conf->num_auth_servers > 1)
	{
		struct hostapd_radius_server *next, *old;
		old = rtapd->conf->auth_server;

		next = old + 1;
		if (next > &(rtapd->conf->auth_servers[rtapd->conf->num_auth_servers - 1]))
			next = rtapd->conf->auth_servers;
		rtapd->conf->auth_server = next;
		Radius_change_server(rtapd, next, old, rtapd->radius->auth_serv_sock, 1);
		DBGPRINT(RT_DEBUG_WARN, "==> Radius_client_timer : ready to change RADIUS server \n");	
	}
#endif			
}

static void Radius_client_list_add(rtapd *rtapd, struct radius_msg *msg,
				   RadiusType msg_type, u8 *shared_secret, size_t shared_secret_len, u8 ApIdx)
{
	struct radius_msg_list *entry, *prev;

	if (eloop_terminated())
	{
		/* No point in adding entries to retransmit queue since event
		 * loop has already been terminated. */
		DBGPRINT(RT_DEBUG_TRACE,"eloop_terminate \n");
		Radius_msg_free(msg);
		free(msg);
		return;
	}

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
	{
		DBGPRINT(RT_DEBUG_TRACE,"Failed to add RADIUS packet into retransmit list\n");
		Radius_msg_free(msg);
		free(msg);
		return;
	}

	memset(entry, 0, sizeof(*entry));
	entry->msg = msg;
	entry->msg_type = msg_type;
	entry->shared_secret = shared_secret;
	entry->shared_secret_len = shared_secret_len;
	entry->ApIdx = ApIdx;
	time(&entry->first_try);
	entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
	entry->attempts = 1;
	entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;

	if (!rtapd->radius->msgs)
	{		
		eloop_register_timeout(RADIUS_CLIENT_FIRST_WAIT, 0, Radius_client_timer, rtapd, NULL);
	}

	entry->next = rtapd->radius->msgs;
	rtapd->radius->msgs = entry;

	if (rtapd->radius->num_msgs >= RADIUS_CLIENT_MAX_ENTRIES)
	{
		DBGPRINT(RT_DEBUG_TRACE,"Removing the oldest un-ACKed RADIUS packet due to retransmit list limits.\n");
		prev = NULL;
		while (entry->next)
		{
			prev = entry;
			entry = entry->next;
		}
		if (prev)
		{
			prev->next = NULL;
			Radius_client_msg_free(entry);
		}
	} else
		rtapd->radius->num_msgs++;
}

int Radius_client_send(rtapd *rtapd, struct radius_msg *msg, RadiusType msg_type, u8 ApIdx)
{
	u8 *shared_secret;
	size_t shared_secret_len;
	char *name;
	int s, res = 0;

#if MULTIPLE_RADIUS
	shared_secret = rtapd->conf->mbss_auth_server[ApIdx]->shared_secret;
	shared_secret_len = rtapd->conf->mbss_auth_server[ApIdx]->shared_secret_len;
	s = rtapd->radius->mbss_auth_serv_sock[ApIdx];
	DBGPRINT(RT_DEBUG_TRACE, "Send packet to server (%s)\n",
						inet_ntoa(rtapd->conf->mbss_auth_server[ApIdx]->addr));
#else
	shared_secret = rtapd->conf->auth_server->shared_secret;
	shared_secret_len = rtapd->conf->auth_server->shared_secret_len;
	s = rtapd->radius->auth_serv_sock;
#endif	
	Radius_msg_finish(msg, shared_secret, shared_secret_len);
	name = "authentication";

	res = send(s, msg->buf, msg->buf_used, 0);
	if (res < 0)
		perror("send[RADIUS]");

	Radius_client_list_add(rtapd, msg, msg_type, shared_secret, shared_secret_len, ApIdx);

	return res;
}

static void Radius_client_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	rtapd *rtapd = eloop_ctx;
	RadiusType msg_type = (RadiusType) sock_ctx;
	int len, i,len_80211hdr=24;
	unsigned char buf[3000];
	struct radius_msg *msg;
	struct radius_rx_handler *handlers;
	size_t num_handlers;
	struct radius_msg_list *req, *prev_req;

	DBGPRINT(RT_DEBUG_TRACE, "RADIUS_CLIENT_RECEIVE : msg_type= %d \n", msg_type);
	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0)
	{
		perror("recv[RADIUS]");
		return;
	}
	if (len == sizeof(buf))
	{
		DBGPRINT(RT_DEBUG_ERROR,"Possibly too long UDP frame for our buffer - dropping it\n");
		return;
	}
	
	if(buf[0]!=0xff)
	{
		int i;
		DBGPRINT(RT_DEBUG_INFO,"  r_dump %d bytes:",len);
		for (i = 0; i < len_80211hdr; i++)
			DBGPRINT(RT_DEBUG_INFO," %02x", buf[i]);
		DBGPRINT(RT_DEBUG_INFO,"\n");
	}

	msg = Radius_msg_parse(buf, len);
	if (msg == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Parsing incoming RADIUS frame failed\n");
		return;
	}

	handlers = rtapd->radius->auth_handlers;
	num_handlers = rtapd->radius->num_auth_handlers;

	prev_req = NULL;
	req = rtapd->radius->msgs;
	while (req)
	{
		/* TODO: also match by src addr:port of the packet when using
		 * alternative RADIUS servers (?) */
		if (req->msg_type == msg_type && req->msg->hdr->identifier == msg->hdr->identifier)
			break;

		prev_req = req;
		req = req->next;
	}

	if (req == NULL)
	{
		goto fail;
	}

	/* Remove ACKed RADIUS packet from retransmit list */
	if (prev_req)
		prev_req->next = req->next;
	else
		rtapd->radius->msgs = req->next;
	rtapd->radius->num_msgs--;

	for (i = 0; i < num_handlers; i++)
	{
		RadiusRxResult res;
		res = handlers[i].handler(rtapd, msg, req->msg, req->shared_secret, req->shared_secret_len, handlers[i].data);
		switch (res) 
		{
			case RADIUS_RX_PROCESSED:
			Radius_msg_free(msg);
			free(msg);
			/* continue */
			case RADIUS_RX_QUEUED:
			Radius_client_msg_free(req);
			return;
			case RADIUS_RX_UNKNOWN:
			/* continue with next handler */
			break;
		}
	}

	DBGPRINT(RT_DEBUG_ERROR,"No RADIUS RX handler found (type=%d code=%d id=%d) - dropping "
		   "packet\n", msg_type, msg->hdr->code, msg->hdr->identifier);
	Radius_client_msg_free(req);

 fail:
	Radius_msg_free(msg);
	free(msg);
}

u8 Radius_client_get_id(rtapd *rtapd)
{
	struct radius_msg_list *entry, *prev, *remove;
	u8 id = rtapd->radius->next_radius_identifier++;

	/* remove entries with matching id from retransmit list to avoid
	 * using new reply from the RADIUS server with an old request */
	entry = rtapd->radius->msgs;
	prev = NULL;
	while (entry)
	{
		if (entry->msg->hdr->identifier == id)
		{
			if (prev)
				prev->next = entry->next;
			else
				rtapd->radius->msgs = entry->next;
			remove = entry;
		} else
			remove = NULL;
		prev = entry;
		entry = entry->next;

		if (remove)
			Radius_client_msg_free(remove);
	}

	return id;
}

void Radius_client_flush(rtapd *rtapd)
{
	struct radius_msg_list *entry, *prev;

	if (!rtapd->radius)
		return;

	eloop_cancel_timeout(Radius_client_timer, rtapd, NULL);

	entry = rtapd->radius->msgs;
	rtapd->radius->msgs = NULL;
	rtapd->radius->num_msgs = 0;
	while (entry)
	{
		prev = entry;
		entry = entry->next;
		Radius_client_msg_free(prev);
	}
}

static int
Radius_change_server(rtapd *rtapd, struct hostapd_radius_server *nserv,
			 struct hostapd_radius_server *oserv, int sock, int auth)
{
	struct sockaddr_in serv;
	if (!oserv || nserv->shared_secret_len != oserv->shared_secret_len ||
		memcmp(nserv->shared_secret, oserv->shared_secret, nserv->shared_secret_len) != 0)
	{
		/* Pending RADIUS packets used different shared
		 * secret, so they would need to be modified. Could
		 * update all message authenticators and
		 * User-Passwords, etc. and retry with new server. For
		 * now, just drop all pending packets. */
		Radius_client_flush(rtapd);
	} 
	else
	{
		/* Reset retry counters for the new server */
		struct radius_msg_list *entry;
		entry = rtapd->radius->msgs;
		while (entry)
		{
			entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
			entry->attempts = 0;
			entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;
			entry = entry->next;
		}
		if (rtapd->radius->msgs)
		{
			eloop_cancel_timeout(Radius_client_timer, rtapd, NULL);
			eloop_register_timeout(RADIUS_CLIENT_FIRST_WAIT, 0, Radius_client_timer, rtapd, NULL);
		}
	}
	// bind before connect to assign local port
/*Comment by rory
	memset(&serv, 0, sizeof(serv));
	port = 2048;
	serv.sin_family = AF_INET;
	//serv.sin_addr.s_addr = inet_addr("192.168.1.138");
	serv.sin_addr.s_addr = rtapd->conf->own_ip_addr.s_addr;
	serv.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) &serv, sizeof(serv)) < 0)
	{
		perror("bind");
		return -1;
	}*/
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = nserv->addr.s_addr;
	serv.sin_port = htons(nserv->port);
	if (connect(sock, (struct sockaddr *) &serv, sizeof(serv)) < 0)
	{
		perror("connect[radius]");
		return -1;
	}
	DBGPRINT(RT_DEBUG_TRACE, "Radius_change_server :: Connect to Radius Server(%s)\n", inet_ntoa(nserv->addr));
	return 0;
}

static void Radius_retry_primary_timer(void *eloop_ctx, void *timeout_ctx)
{
	rtapd *rtapd = eloop_ctx;
	struct hostapd_radius_server *oserv;

	DBGPRINT(RT_DEBUG_TRACE, "RUN Radius_retry_primary_timer.....\n");
#if MULTIPLE_RADIUS
	int	i;

	for (i = 0; i < rtapd->conf->SsidNum; i++)
	{
		if (rtapd->radius->mbss_auth_serv_sock[i] >= 0 && rtapd->conf->mbss_auth_servers[i] &&
			rtapd->conf->mbss_auth_server[i] != rtapd->conf->mbss_auth_servers[i])
		{
			oserv = rtapd->conf->mbss_auth_server[i];
			rtapd->conf->mbss_auth_server[i] = rtapd->conf->mbss_auth_servers[i];
			Radius_change_server(rtapd, rtapd->conf->mbss_auth_server[i], oserv, rtapd->radius->mbss_auth_serv_sock[i], 1);
		}
	}
#else
	if (rtapd->radius->auth_serv_sock >= 0 && rtapd->conf->auth_servers &&
		rtapd->conf->auth_server != rtapd->conf->auth_servers)
	{
		oserv = rtapd->conf->auth_server;
		rtapd->conf->auth_server = rtapd->conf->auth_servers;
		Radius_change_server(rtapd, rtapd->conf->auth_server, oserv, rtapd->radius->auth_serv_sock, 1);
	}
#endif

	if (rtapd->conf->radius_retry_primary_interval)
		eloop_register_timeout(rtapd->conf->radius_retry_primary_interval, 0, Radius_retry_primary_timer, rtapd, NULL);
}

#if MULTIPLE_RADIUS
static int Radius_client_init_auth(rtapd *rtapd, int apidx)
{		
	if (rtapd->conf->mbss_auth_server[apidx]->addr.s_addr == 0)
	{
		DBGPRINT(RT_DEBUG_WARN, "Radius_client_init_auth: can't create auth RADIUS socket for %s%d (it's invalid IP)\n", rtapd->prefix_wlan_name, apidx);
		return -1;
	}

	rtapd->radius->mbss_auth_serv_sock[apidx] = socket(PF_INET, SOCK_DGRAM, 0);
	if (rtapd->radius->mbss_auth_serv_sock[apidx] < 0)
	{
		perror("socket[PF_INET,SOCK_DGRAM]");
		return -1;
	}

	if (Radius_change_server(rtapd, rtapd->conf->mbss_auth_server[apidx], NULL, rtapd->radius->mbss_auth_serv_sock[apidx], 1))
		return -1;

	if (eloop_register_read_sock(rtapd->radius->mbss_auth_serv_sock[apidx], Radius_client_receive, rtapd, (void *) RADIUS_AUTH))
	{
		DBGPRINT(RT_DEBUG_ERROR,"Could not register read socket for authentication server - iface-%d\n", apidx);
		return -1;
	}
	return 0;
}
#else
static int Radius_client_init_auth(rtapd *rtapd)
{
	rtapd->radius->auth_serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (rtapd->radius->auth_serv_sock < 0)
	{
		perror("socket[PF_INET,SOCK_DGRAM]");
		return -1;
	}

	Radius_change_server(rtapd, rtapd->conf->auth_server, NULL, rtapd->radius->auth_serv_sock, 1);

	if (eloop_register_read_sock(rtapd->radius->auth_serv_sock, Radius_client_receive, rtapd, (void *) RADIUS_AUTH))
	{
		DBGPRINT(RT_DEBUG_ERROR,"Could not register read socket for authentication server\n");
		return -1;
	}
	return 0;
}
#endif


int Radius_client_init(rtapd *rtapd)
{
#if MULTIPLE_RADIUS
	int	i, ready_sock_count = 0, bReInit = 1;
#endif

	if (rtapd->radius == NULL)
	{		 
		rtapd->radius = malloc(sizeof(struct radius_client_data));
		if (rtapd->radius == NULL)
			return -1;
		
		memset(rtapd->radius, 0, sizeof(struct radius_client_data));

#if MULTIPLE_RADIUS
		for (i = 0; i < MAX_MBSSID_NUM; i++)
			rtapd->radius->mbss_auth_serv_sock[i] = -1;

		bReInit= 0;
#else
		rtapd->radius->auth_serv_sock = -1;
#endif
	}
	
#if MULTIPLE_RADIUS	
	// Create socket for auth RADIUS
	for (i = 0; i < rtapd->conf->SsidNum; i++)
	{
		if (rtapd->radius->mbss_auth_serv_sock[i] < 0)
		{
			if (rtapd->conf->mbss_auth_server[i] && (!Radius_client_init_auth(rtapd, i)))
					ready_sock_count++;		
		}
		else
			ready_sock_count++;
	}
	
	if (ready_sock_count == 0)
	{
		DBGPRINT(RT_DEBUG_ERROR, "Radius_client_init : no any auth RADIUS socket ready \n");
		return -1;
	}
	else
		DBGPRINT(RT_DEBUG_TRACE, "Radius_client_init : ready_sock_count %d \n", ready_sock_count);

	if (rtapd->conf->radius_retry_primary_interval && !bReInit && ready_sock_count > 0)
		eloop_register_timeout(rtapd->conf->radius_retry_primary_interval, 0, Radius_retry_primary_timer, rtapd, NULL);
#else
	if( rtapd->radius->auth_serv_sock < 0)
	{
		if (rtapd->conf->auth_server && Radius_client_init_auth(rtapd))
			return -1;
		if (rtapd->conf->radius_retry_primary_interval)
			eloop_register_timeout(rtapd->conf->radius_retry_primary_interval, 0, Radius_retry_primary_timer, rtapd, NULL);
	}
#endif	
	return 0;
}

void Radius_client_deinit(rtapd *rtapd)
{
	if (!rtapd->radius)
		return;

	eloop_cancel_timeout(Radius_retry_primary_timer, rtapd, NULL);

	Radius_client_flush(rtapd);
	free(rtapd->radius->auth_handlers);
	free(rtapd->radius);
	rtapd->radius = NULL;
}

