/*
 * Radius code which used to be in nas.c
 * Radius support for Network Access Server
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: nas_radius.c 462003 2014-03-14 11:35:32Z $
 */

#include <typedefs.h>
#include <stdlib.h>

#include <proto/eap.h>
#include <bcmutils.h>
#ifdef __ECOS
#include <sys/socket.h>
#include <net/if.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#include <nas.h>
#include <nas_wksp.h>
#include <nas_radius.h>
#include <nas_wksp_radius.h>
#include <mppe.h>

static void radius_add(radius_header_t *radius, unsigned char type,
                       unsigned char *buf, unsigned char length);

/* Proxy EAP packet from RADIUS server to PAE */
void
radius_dispatch(nas_t *nas, radius_header_t *response)
{
	nas_sta_t *sta = &nas->sta[response->id % MAX_SUPPLICANTS];
	radius_header_t *request;

	int left, type, length = 0, index, authenticated = 0;
	int nRadiusFlag = 0;
	unsigned char buf[16], *cur;
	eapol_header_t eapol;
	eap_header_t *eap = NULL;
	unsigned int vendor, ssnto;
	unsigned char *mppe_send = NULL, *mppe_recv = NULL, *mppe_key;
	struct iovec frags[RADIUS_MAX_ATTRIBUTES];
	int nfrags = 0;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	memset(&nas->m_deauth_params, 0, sizeof(deauth_params_t));
	memset(&nas->m_subrem_params, 0, sizeof(subrem_params_t));
	memset(&nas->m_bsstran_params, 0, sizeof(bsstran_params_t));

	/* The STA could have been toss during the wait. */
	if (!sta->used)
		return;

	request =  sta->pae.radius.request;
	if (!request || request->id != response->id) {
		dbg(nas, "bogus RADIUS packet response->id=%d request->id=%d", response->id,
			request ? request->id : 0);
		return;
	}

	/* Parse attributes */
	left = ntohs(response->length) - RADIUS_HEADER_LEN;
	cur = response->attributes;
	while (left >= 2) {
		int attribute_error = 0;

		type = *cur++;
		length = *cur++ - 2;
		left -= 2;

		/* Bad attribute length */
		if (length > left) {
			dbg(nas, "bad attribute length %d", length);
			break;
		}

		switch (type) {

		case RD_TP_MESSAGE_AUTHENTICATOR:
			if (length < 16) {
				dbg(nas, "bad signature length %d", length);
				attribute_error = 1;
				break;
			}

			/* Validate HMAC-MD5 checksum */
			memcpy(buf, cur, 16);
			memset(cur, 0, 16);
			memcpy(response->vector, request->vector, 16);

			/* Calculate HMAC-MD5 checksum with request vector and null signature */
			hmac_md5((unsigned char *) response, ntohs(response->length),
			         nas->secret.data, nas->secret.length, cur);
			if ((authenticated = !memcmp(buf, cur, 16)) == 0) {
				dbg(nas, "Invalid signature");
				attribute_error = 1;
			}
			break;

		case RD_TP_STATE:
			/* Preserve server state unmodified */
			sta->pae.radius.state.length = length;
			if (sta->pae.radius.state.data)
				free(sta->pae.radius.state.data);
			sta->pae.radius.state.length = length;
			if (!(sta->pae.radius.state.data = malloc(sta->pae.radius.state.length))) {
				perror("malloc");
				attribute_error = 1;
			} else
				memcpy(sta->pae.radius.state.data, cur,
				       sta->pae.radius.state.length);
			break;

		case RD_TP_EAP_MESSAGE:
			/* Initialize EAPOL header */
			if (!nfrags) {
				memcpy(&eapol.eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
				memcpy(&eapol.eth.ether_shost, &nas->ea, ETHER_ADDR_LEN);
				if (sta->flags & STA_FLAG_PRE_AUTH)
					eapol.eth.ether_type = htons(ETHER_TYPE_802_1X_PREAUTH);
				else
					eapol.eth.ether_type = htons(ETHER_TYPE_802_1X);
				eapol.version = sta->eapol_version;
				eapol.type = EAP_PACKET;
				eapol.length = htons(0);
				eap = (eap_header_t *) cur;
				frags[nfrags].iov_base = (caddr_t) &eapol;
				frags[nfrags].iov_len = EAPOL_HEADER_LEN;
				nfrags++;
				/* Set up internal flags */
				if (eap->code == EAP_SUCCESS)
					sta->pae.flags |= PAE_FLAG_EAP_SUCCESS;
			}
			/* Gather fragmented EAP messages */
			if (nfrags < ARRAYSIZE(frags)) {
				eapol.length = htons(ntohs(eapol.length) + length);
				frags[nfrags].iov_base = (caddr_t) cur;
				frags[nfrags].iov_len = length;
				nfrags++;
			}
			break;

		case RD_TP_VENDOR_SPECIFIC:
			if (length < 6) {
				dbg(nas, "bad vendor attribute length %d", length);
				attribute_error = 1;
				break;
			}
			memcpy(&vendor, cur, 4);
			vendor = ntohl(vendor);
			cur += 4;
			type = *cur++;
			length = *cur++ - 2;
			left -= 6;

			/* Bad attribute length */
			if (length > left) {
				dbg(nas, "bad vendor attribute length %d", length);
				attribute_error = 1;
				break;
			}

			/* Parse vendor-specific attributes */
			switch (vendor) {

			case RD_VENDOR_MICROSOFT:
				switch (type) {

				case RD_MS_MPPE_SEND:
				case RD_MS_MPPE_RECV:
					if (response->code != RADIUS_ACCESS_ACCEPT) {
						dbg(nas, "ignore MS-MPPE-Key in non"
						    " RADIUS_ACCESS_ACCEPT packet");
						break;
					}

					/* Key length (minus salt) must be a multiple of 16 and
					 * greater than 32
					 */
					if ((length - 2) % 16 || (length - 2) <= 32) {
						dbg(nas, "bad MS-MPPE-Key length %d", length);
						attribute_error = 1;
						break;
					}
					/* Allocate key */
					if (!(mppe_key = malloc(length - 2))) {
						perror("malloc");
						attribute_error = 1;
						break;
					}
					/* Decrypt key */
					memcpy(mppe_key, &cur[2], length - 2);
					mppe_crypt(cur, mppe_key, length - 2,
					           nas->secret.data, nas->secret.length,
					           request->vector, 0);
					/* Set key pointers */
					if (type == RD_MS_MPPE_SEND)
						mppe_send = mppe_key;
					else
						mppe_recv = mppe_key;
					break;
				}
				break;


			case RD_VENDOR_WFA:
				switch (type) {
				/*  1 - Passpoint 2.0 subscription remediation needed */
				/* url="https:// remediation-server.R2-testbed.wi-fi.org" */
				case  RD_VENDOR_WFA_SR_VALUE:
					if (response->code == RADIUS_ACCESS_ACCEPT) {

						dbg(nas, "Access Accept and "
						    "Subscription Remediation");

						nas->m_subrem_params.serverMethod = *cur++;
						dbg(nas, "serverMethod attribute for "
							"Subscription Remediation = %d",
							nas->m_subrem_params.serverMethod);

						/* already we reduced 2 octect earlier */
						length -= 1;
						dbg(nas, "length = %d", length);
						/* already we reduced 6 octect earlier */
						left -= 1;
						dbg(nas, "left = %d", left);

						/* Bad attribute length */
						if (length > left) {
							dbg(nas, "bad vendor attribute"
								" length %d", length);
							attribute_error = 1;
							break;
						}

						if (length > 0) {
							/* Parse the URL from the STA */
							if (!(nas->m_subrem_params.subrem_url
									= malloc(length + 1))) {
								printf("URL "
									"allocation failed***\n");
								perror("malloc");
								break;
							}
							memcpy(nas->m_subrem_params.subrem_url,
								&cur[0], length);
							*(nas->m_subrem_params.subrem_url
								+ length) = '\0';

							dbg(nas, "Sub Rem URL = %s",
								nas->m_subrem_params.subrem_url);
						}
						else {
							nas->m_subrem_params.subrem_url = NULL;
						}

						nas->m_subrem_params.subrem_req = 1;


						break;
					}
					break;

				/*	2 - Passpoint 2.0 AP version */
				case  RD_VENDOR_WFA_AP_VALUE:
					if (response->code == RADIUS_ACCESS_REQUEST) {
						/* Fill the attributes with the AP version */
						break;
					}
					break;

				/*	3 - Passpoint 2.0 STA version */
				case  RD_VENDOR_WFA_STA_VALUE:
					if (response->code == RADIUS_ACCESS_REQUEST) {
						/* Fill the attributes with the STA version */
						break;

					}
					break;

				/*  4 - Passpoint 2.0 Deauthentication Request  */
				case  RD_VENDOR_WFA_DAR_VALUE:
					if (response->code == RADIUS_ACCESS_ACCEPT) {
						dbg(nas, "Access Accept and "
							"Deauthentication Request");

						nas->m_deauth_params.reason_code = *cur++;
						dbg(nas, "code attribute for "
							"Deauthentication Request = %d",
							nas->m_deauth_params.reason_code);

						nas->m_deauth_params.reauth_delay =
							cur[0] | cur[1] << 8;
						dbg(nas, "reauth_delay attribute for "
							"Deauthentication Request = %d",
							nas->m_deauth_params.reauth_delay);
						cur += 2;

						/* already we reduced 2 octect earlier */
						length -= 3;
						dbg(nas, "length = %d", length);
						/* already we reduced 2 octect earlier */
						left -= 3;
						dbg(nas, "left = %d", left);

						/* Bad attribute length */
						if (length > left) {
							dbg(nas, "bad vendor attribute"
								" length %d", length);
							attribute_error = 1;
							break;
						}

						if (length > 0) {
							/* Parse the URL from RADIUS */
							if (!(nas->m_deauth_params.reason_url
									= malloc(length + 1))) {
								printf("URL "
									"allocation failed***\n");
								perror("malloc");
								break;
							}
							memcpy(nas->m_deauth_params.reason_url,
								&cur[0], length);
							*(nas->m_deauth_params.reason_url
								+ length) = '\0';

							dbg(nas, "Deauth Request URL = %s",
								nas->m_deauth_params.reason_url);
						}
						else {
							nas->m_deauth_params.reason_url = NULL;
						}

						nas->m_deauth_params.deauth_req = 1;

						break;
					}
					break;

				/* 5 - Session Information URL - Send BSS Trans */
				case  RD_VENDOR_WFA_SIU_VALUE:
					if (response->code == RADIUS_ACCESS_ACCEPT) {

						dbg(nas, "Access Accept and "
								"BSS Transition Request");

						nas->m_bsstran_params.bsstran_swt = *cur++;
						dbg(nas, "Session Warning Time attribute"
							"for Session Information URL = %d",
							nas->m_bsstran_params.bsstran_swt);

						/* already we reduced 2 octect earlier */
						length -= 1;
						dbg(nas, "length = %d", length);
						/* already we reduced 2 octect earlier */
						left -= 1;
						dbg(nas, "left = %d", left);

						/* Bad attribute length */
						if (length > left) {
							dbg(nas, "bad vendor attribute"
								" length %d", length);
							attribute_error = 1;
							break;
						}

						if (length > 0) {
							/* Parse the URL from RADIUS */
							if (!(nas->m_bsstran_params.bsstran_url
									= malloc(length + 1))) {
								printf("URL "
									"allocation failed***\n");
								perror("malloc");
								break;
							}
							memcpy(nas->m_bsstran_params.bsstran_url,
								&cur[0], length);
							*(nas->m_bsstran_params.bsstran_url
								+ length) = '\0';
						}
						else {
							nas->m_bsstran_params.bsstran_url = NULL;
						}

						nas->m_bsstran_params.bsstran_reqmode =
					/* DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL | */
							DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT |
							DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT;

						nas->m_bsstran_params.bsstran_dur = 0;
						nas->m_bsstran_params.bsstran_req = 1;

						break;
					}
					break;
				}
				break;

			default:
				dbg(nas, "unknown vendor attribute = %d", vendor);
				dbg(nas, "    vendor type = %d", type);
				dbg(nas, "    attribute string = %s", cur);
				break;
			}
			break;

		case RD_TP_SESSION_TIMEOUT:
			if (response->code != RADIUS_ACCESS_ACCEPT)
				break;
			if (length < 4) {
				dbg(nas, "bad session timeout attribute length %d", length);
				attribute_error = 1;
				break;
			}
			memcpy(&ssnto, cur, 4);
			sta->pae.ssnto = ntohl(ssnto);
			dbg(nas, "session timeout in %d seconds", sta->pae.ssnto);
			break;

		default:
			/* Ignore all other attributes */
			break;
		}
		/* Don't go on looking if something already went wrong. */
		if (attribute_error)
			goto done;

		left -= length;
		cur += length;
	}

	if (!authenticated && response->code != RADIUS_ACCESS_REJECT) {
		dbg(nas, "missing signature");
		goto done;
	}

	if (eap)
		sta->pae.id = eap->id;

	if (eap &&
	    (eap->code != EAP_SUCCESS || response->code != RADIUS_ACCESS_ACCEPT) &&
	    (eap->code != EAP_FAILURE || response->code != RADIUS_ACCESS_REJECT) &&
	    nfrags) {
		if (sta->flags & STA_FLAG_PRE_AUTH)
			nas_preauth_send_packet(nas, frags, nfrags);
		else
			nas_eapol_send_packet(nas, frags, nfrags);
	}

	/* RADIUS event */
	switch (response->code) {

	case RADIUS_ACCESS_ACCEPT:
		if (nRadiusFlag > 0) {
			break;
		}
		/* Check for EAP-Success before allowing complete access */
		if (!(sta->pae.flags & PAE_FLAG_EAP_SUCCESS)) {
			dbg(nas, "Radius success without EAP success?!");
			pae_state(nas, sta, HELD);
			dbg(nas, "deauthenticating %s", ether_etoa((uint8 *)&sta->ea, eabuf));
			nas_deauthorize(nas, &sta->ea);
			goto done;
		}

		dbg(nas, "Access Accept");
		pae_state(nas, sta, AUTHENTICATED);

		/* overwrite session timeout with global setting */
		if (!sta->pae.ssnto || sta->pae.ssnto > nas->ssn_to)
			sta->pae.ssnto = nas->ssn_to;

		/* WPA-mode needs to do the 4-way handshake here instead. */
		if (CHECK_WPA(sta->mode) && mppe_recv) {
			fix_wpa(nas, sta, (char *)&mppe_recv[1], (int)mppe_recv[0]);
			break;
		}

		/* Plump the keys to driver and send them to peer as well */
		if (mppe_recv) {
			/* Cobble a multicast key if there isn't one yet. */
			if (!(nas->flags & NAS_FLAG_GTK_PLUMBED)) {
				nas->wpa->gtk_index = GTK_INDEX_1;
				if (nas->wpa->gtk_len == 0)
					nas->wpa->gtk_len = WEP128_KEY_SIZE;
				nas_rand128(nas->wpa->gtk);
				if (nas_set_key(nas, NULL, nas->wpa->gtk,
				                nas->wpa->gtk_len, nas->wpa->gtk_index,
				                1, 0, 0) < 0) {
					err(nas, "invalid multicast key");
					nas_handle_error(nas, 1);
				}
				nas->flags |= NAS_FLAG_GTK_PLUMBED;
			}
			sta->rc4keysec = -1;
			sta->rc4keyusec = -1;
			/* Send multicast key */
			index = nas->wpa->gtk_index;
			length = nas->wpa->gtk_len;
			if (mppe_send)
				eapol_key(nas, sta, &mppe_send[1], mppe_send[0],
					&mppe_recv[1], mppe_recv[0],
					nas->wpa->gtk, length, index, 0);
			else
				eapol_key(nas, sta, NULL, 0,
					&mppe_recv[1], mppe_recv[0],
					nas->wpa->gtk, length, index, 0);

			/* MS-MPPE-Recv-Key is MS-MPPE-Send-Key on the Suppl */
			index = DOT11_MAX_DEFAULT_KEYS - 1;
			length = WEP128_KEY_SIZE;
			if (nas_set_key(nas, &sta->ea, &mppe_recv[1], length, index, 1, 0, 0) < 0) {
				dbg(nas, "unicast key rejected by driver, assuming too many"
				    " associated STAs");
				cleanup_sta(nas, sta, DOT11_RC_BUSY, 0);
			}
			/* Set unicast key index */
			if (mppe_send)
				eapol_key(nas, sta, &mppe_send[1], mppe_send[0],
					NULL, 0,
					NULL, length, index, 1);
			else
				eapol_key(nas, sta, NULL, 0,
					NULL, 0,
					NULL, length, index, 1);
			dbg(nas, "authorize %s (802.1x)", ether_etoa((uint8 *)&sta->ea, eabuf));
			nas_authorize(nas, &sta->ea);
		}
		break;

	case RADIUS_ACCESS_REJECT:
		dbg(nas, "Access Reject");
		pae_state(nas, sta, HELD);
		dbg(nas, "deauthenticating %s", ether_etoa((uint8 *)&sta->ea, eabuf));
		nas_deauthorize(nas, &sta->ea);
		sta->pae.ssnto = 0;
		break;

	case RADIUS_ACCESS_CHALLENGE:
		dbg(nas, "Access Challenge");
		break;

	default:
		dbg(nas, "unknown RADIUS code %d", response->code);
		break;
	}

done:
	if (mppe_send)
		free(mppe_send);
	if (mppe_recv)
		free(mppe_recv);
	free(request);
	sta->pae.radius.request = NULL;
}

/* Add an attribute value pair */
static void
radius_add(radius_header_t *radius, unsigned char type, unsigned char *buf, unsigned char length)
{
	unsigned char *cur;

	if ((ntohs(radius->length) + length) <= RADIUS_MAX_LEN) {
		cur = (unsigned char *) radius + ntohs(radius->length);
		*cur++ = type;
		*cur++ = 2 + length;
		memcpy(cur, buf, length);
		radius->length = htons(ntohs(radius->length) + 2 + length);
	}
}

/* Proxy EAP packet from PAE to RADIUS server */
void
radius_forward(nas_t *nas, nas_sta_t *sta, eap_header_t *eap)
{
	radius_header_t *request;

	unsigned char buf[16], *ptr;
	long val;
	int left;

	/* Allocate packet */
	if (!(request = malloc(RADIUS_MAX_LEN))) {
		perror("malloc");
		return;
	}

	/* Fill header */
	request->code = RADIUS_ACCESS_REQUEST;
	request->id = sta - nas->sta;
	request->length = htons(RADIUS_HEADER_LEN);

	/* Fill Request Authenticator */
	nas_rand128(request->vector);

	/* Fill attributes */

	/* User Name */
	if (sta->pae.radius.username.data && sta->pae.radius.username.length)
		radius_add(request, RD_TP_USER_NAME, sta->pae.radius.username.data,
		           sta->pae.radius.username.length);
	/* NAS IP Address */
	radius_add(request, RD_TP_NAS_IP_ADDRESS, (unsigned char *) &nas->client.sin_addr,
	           sizeof(nas->client.sin_addr));
	/* Called Station Id */

	snprintf((char *)buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
	         ((unsigned char *) &nas->ea)[0], ((unsigned char *) &nas->ea)[1],
	         ((unsigned char *) &nas->ea)[2],
	         ((unsigned char *) &nas->ea)[3], ((unsigned char *) &nas->ea)[4],
	         ((unsigned char *) &nas->ea)[5]);

	radius_add(request, RD_TP_CALLED_STATION_ID, buf, strlen((char *)buf));
	/* Calling Station Id */
	snprintf((char *)buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
	         ((unsigned char *) &sta->ea)[0], ((unsigned char *) &sta->ea)[1],
	         ((unsigned char *) &sta->ea)[2],
	         ((unsigned char *) &sta->ea)[3], ((unsigned char *) &sta->ea)[4],
	         ((unsigned char *) &sta->ea)[5]);
	radius_add(request, RD_TP_CALLING_STATION_ID, buf, strlen((char *)buf));
	/* NAS identifier */
	if (strlen(nas->nas_id))
		radius_add(request, RD_TP_NAS_IDENTIFIER, (unsigned char *)nas->nas_id,
		           strlen(nas->nas_id));
	else {
		snprintf((char *)buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
		         ((unsigned char *) &nas->ea)[0], ((unsigned char *) &nas->ea)[1],
		         ((unsigned char *) &nas->ea)[2],
		         ((unsigned char *) &nas->ea)[3], ((unsigned char *) &nas->ea)[4],
		         ((unsigned char *) &nas->ea)[5]);
		radius_add(request, RD_TP_NAS_IDENTIFIER, buf, strlen((char *)buf));
	}
	/* NAS Port */
	val = htonl((long) pae_hash(&sta->ea));
	radius_add(request, RD_TP_NAS_PORT, (unsigned char *) &val, sizeof(val));
	val = htonl(1400);
	radius_add(request, RD_TP_FRAMED_MTU, (unsigned char *) &val,
	           sizeof(val));
	/* State */
	if (sta->pae.radius.state.data && sta->pae.radius.state.length) {
		radius_add(request, RD_TP_STATE, sta->pae.radius.state.data,
		           sta->pae.radius.state.length);
		free(sta->pae.radius.state.data);
		sta->pae.radius.state.data = NULL;
		sta->pae.radius.state.length = 0;
	}
	/* NAS Port Type */
	val = htonl((long) nas->type);
	radius_add(request, RD_TP_NAS_PORT_TYPE, (unsigned char *) &val, 4);
	/* EAP Message(s) */
	if (eap) {
		for (left = ntohs(eap->length); left > 0; left -= 253) {
			radius_add(request, RD_TP_EAP_MESSAGE,
			           (unsigned char *) eap + ntohs(eap->length) - left,
			           left <= 253 ? left : 253);
		}
	}
	/* Message Authenticator */
	memset(buf, 0, 16);
	radius_add(request, RD_TP_MESSAGE_AUTHENTICATOR, buf, 16);
	ptr = (unsigned char *) request + ntohs(request->length) - 16;
	/* Calculate HMAC-MD5 checksum with null signature */
	hmac_md5((unsigned char *) request, ntohs(request->length),
	         nas->secret.data, nas->secret.length, ptr);

	/* Send packet */
	if (NAS_RADIUS_SEND_PACKET(nas, request, ntohs(request->length)) < 0) {
		perror(inet_ntoa(nas->server.sin_addr));
		free(request);
		request = NULL;
	}

	/* Save original request packet */
	if (sta->pae.radius.request)
		free(sta->pae.radius.request);
	sta->pae.radius.request = request;
}
