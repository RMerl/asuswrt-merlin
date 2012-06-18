

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/if.h>			/* for IFNAMSIZ and co... */
#include <linux/wireless.h>

#include "rtdot1x.h"
#include "sta_info.h"
#include "eloop.h"
#include "ieee802_1x.h"
#include "radius.h"

struct sta_info* Ap_get_sta(rtapd *apd, u8 *sa, u8 *apidx, u16 ethertype, int sock)
{
	struct sta_info *s;

	s = apd->sta_hash[STA_HASH(sa)];
	while (s != NULL && memcmp(s->addr, sa, 6) != 0)
		s = s->hnext;
	
	if (s == NULL)
	{
		if (apd->num_sta >= MAX_STA_COUNT)
		{
			/* FIX: might try to remove some old STAs first? */
			DBGPRINT(RT_DEBUG_ERROR,"No more room for new STAs (%d/%d)\n", apd->num_sta, MAX_STA_COUNT);
			return NULL;
		}

		s = (struct sta_info *) malloc(sizeof(struct sta_info));
		if (s == NULL)
		{
			DBGPRINT(RT_DEBUG_ERROR,"Malloc failed\n");
			return NULL;
		}
		
		memset(s, 0, sizeof(struct sta_info));
		s->radius_identifier = -1;

		s->ethertype = ethertype;
		if (apd->conf->SsidNum > 1)
			s->ApIdx = *apidx;
		else
			s->ApIdx = 0;

		DBGPRINT(RT_DEBUG_TRACE,"Create a new STA(in %s%d)\n", apd->prefix_wlan_name, s->ApIdx);

		s->SockNum = sock;
		memcpy(s->addr, sa, ETH_ALEN);
		s->next = apd->sta_list;
		apd->sta_list = s;
		apd->num_sta++;
		Ap_sta_hash_add(apd, s);
		ieee802_1x_new_station(apd, s);
		return s;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE,"A STA has existed(in %s%d)\n", apd->prefix_wlan_name, s->ApIdx);
	}	
	
	return s;
}

struct sta_info* Ap_get_sta_radius_identifier(rtapd *apd, u8 radius_identifier)
{
	struct sta_info *s;

	s = apd->sta_list;

	while (s)
	{
		if (s->radius_identifier >= 0 && s->radius_identifier == radius_identifier)
			return s;
		s = s->next;
	}

	return NULL;
}

static void Ap_sta_list_del(rtapd *apd, struct sta_info *sta)
{
	struct sta_info *tmp;

	if (apd->sta_list == sta)
	{
		apd->sta_list = sta->next;
		return;
	}

	tmp = apd->sta_list;
	while (tmp != NULL && tmp->next != sta)
		tmp = tmp->next;
	if (tmp == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Could not remove STA " MACSTR " from list.\n", MAC2STR(sta->addr));
	} else
		tmp->next = sta->next;
}

void Ap_sta_hash_add(rtapd *apd, struct sta_info *sta)
{
	sta->hnext = apd->sta_hash[STA_HASH(sta->addr)];
	apd->sta_hash[STA_HASH(sta->addr)] = sta;
}

static void Ap_sta_hash_del(rtapd *apd, struct sta_info *sta)
{
	struct sta_info *s;

	s = apd->sta_hash[STA_HASH(sta->addr)];
	if (s == NULL) return;
	if (memcmp(s->addr, sta->addr, 6) == 0)
	{
		apd->sta_hash[STA_HASH(sta->addr)] = s->hnext;
		return;
	}

	while (s->hnext != NULL && memcmp(s->hnext->addr, sta->addr, 6) != 0)
		s = s->hnext;
	if (s->hnext != NULL)
		s->hnext = s->hnext->hnext;
	else
		DBGPRINT(RT_DEBUG_ERROR,"AP: could not remove STA " MACSTR " from hash table\n", MAC2STR(sta->addr));
}

/*
	========================================================================
	Routine Description:
	   remove the specified input-argumented sta from linked list.. 
	Arguments:
		*sta	to-be-removed station.
	Return Value:
	========================================================================
*/
void Ap_free_sta(rtapd *apd, struct sta_info *sta)
{
	DBGPRINT(RT_DEBUG_TRACE," AP_free_sta \n")

	Ap_sta_hash_del(apd, sta);
	Ap_sta_list_del(apd, sta);

	if (sta->aid > 0)
		apd->sta_aid[sta->aid - 1] = NULL;

	apd->num_sta--;

	ieee802_1x_free_station(sta);

	if (sta->last_assoc_req)
		free(sta->last_assoc_req);

	free(sta);
}

/*
	========================================================================
	Description:
		remove all stations.
	========================================================================
*/
void Apd_free_stas(rtapd *apd)
{
	struct sta_info *sta, *prev;

	sta = apd->sta_list;
	DBGPRINT(RT_DEBUG_TRACE,"Apd_free_stas\n");
	while (sta)
	{
		prev = sta;
		sta = sta->next;
		DBGPRINT(RT_DEBUG_ERROR,"Removing station " MACSTR "\n", MAC2STR(prev->addr));
		Ap_free_sta(apd, prev);
	}
}

void Ap_handle_session_timer(void *eloop_ctx, void *timeout_ctx)
{
	char *buf;
	rtapd *apd = eloop_ctx;
	size_t len;
	struct ieee8023_hdr *hdr3;
	struct sta_info *sta = timeout_ctx;
	
	DBGPRINT(RT_DEBUG_TRACE,"AP_HANDLE_SESSION_TIMER \n");	  
	len = sizeof(*hdr3) + 2;
	buf = (char *) malloc(len);
	if (buf == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR,"malloc() failed for ieee802_1x_send(len=%d)\n", len);
		return;
	}

	memset(buf, 0, len);
	hdr3 = (struct ieee8023_hdr *) buf;
	memcpy(hdr3->dAddr, sta->addr, ETH_ALEN);
	memcpy(hdr3->sAddr, apd->own_addr[sta->ApIdx], ETH_ALEN);
	// send deauth
	DBGPRINT(RT_DEBUG_TRACE,"AP_HANDLE_SESSION_TIMER : Send Deauth \n");	  
	if (RT_ioctl(apd->ioctl_sock, 
				 RT_PRIV_IOCTL, buf, len, 
				 apd->prefix_wlan_name, sta->ApIdx, 
				 RT_OID_802_DOT1X_RADIUS_DATA))
	{
		DBGPRINT(RT_DEBUG_ERROR," ioctl \n");
		return;
	}
	free(buf);

//	Ap_free_sta(apd, sta);
}

void Ap_sta_session_timeout(rtapd *apd, struct sta_info *sta, u32 session_timeout)
{
	DBGPRINT(RT_DEBUG_TRACE,"AP_STA_SESSION_TIMEOUT %d seconds \n",session_timeout);
	eloop_cancel_timeout(Ap_handle_session_timer, apd, sta);
	eloop_register_timeout(session_timeout, 0, Ap_handle_session_timer, apd, sta);
}

void Ap_sta_no_session_timeout(rtapd *apd, struct sta_info *sta)
{
	eloop_cancel_timeout(Ap_handle_session_timer, apd, sta);
}
