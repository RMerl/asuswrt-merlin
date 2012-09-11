/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on GCT Semiconductor GDM7213 & GDM7205 chip.
 * Copyright (С) 2010 Yaroslav Levandovsky <leyarx@gmail.com>
 *
 * Based on  madWiMAX driver writed by Alexander Gordeev
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <string.h>
#include <ctype.h>
#include "logging.h"
#include "protocol.h"
#include "eap_auth.h"

static int process_normal_config_response(struct wimax_dev_status *dev, const unsigned char *buf, int len)
{
	if (len > 4){
		int i, j;
		short param_len;
		for (i = 0x04;i < len; )
		{
			param_len = buf[i+1];
			switch (buf[i]){
				case 0x00: {	//	-- Device MAC
						if (param_len == 0x6) {
							memcpy(dev->mac, buf + i + 2, param_len);
							dev->info_updated |= WDS_MAC;					
						} else  wmlog_msg(1, "bad param_len");
						break;
					}
				case 0x01: {	//	-- BSID
						if (param_len == 0x6) {
							memcpy(dev->bsid, buf + i + 2, param_len);
							dev->info_updated |= WDS_BSID;					
						} else  wmlog_msg(1, "bad param_len");
						break;
					}
				case 0x1b: {	//	-- Device SW Version  xxxx: (FW FirmWare)
							for (j = i+2; j < param_len + i + 2; j++)
							{
								dev->firmware[j - (i + 2)] = buf[j] + 0x30; /* rewrite this part to use itoa() */
							}
							dev->firmware[0x04] = 0x3a;
							//dev->info_updated |= WDS_FIRMWARE;  // -test
							break;

					}
				case 0x1d: {	//	-- Device SW Version :xxxx
							for (j = i+2; j < param_len + i + 2; j++)
							{
								dev->firmware[j - (i + 2) + 5] = buf[j] + 0x30; /* rewrite this part to use itoa() */
							}
							//memcpy(dev->firmware + 0x05, buf + i + 2, param_len);
							dev->info_updated |= WDS_FIRMWARE;
							break;
					}
				case 0x1f: {	//	-- Device HW Version (RF Version)
							for (j = i+2; j < param_len + i + 2; j++)
							{
								dev->chip[j - (i + 2)] = buf[j] + 0x30; /* rewrite this part to use itoa() */
							}
							//memcpy(dev->chip, buf + i + 2, param_len);
							dev->info_updated |= WDS_CHIP;
							break;
					}
				case 0x60: {	//	-- RSSI-1
							dev->rssi1 =  buf[i+0x2]-0x100;
							dev->info_updated |= WDS_RSSI1;
							break;
					}
				case 0x71: {	//	-- RSSI-2
							dev->rssi2 =  buf[i+0x2]-0x100;
							dev->info_updated |= WDS_RSSI2;
							break;
					}
				case 0x61: {	//	-- CINR-1
							dev->cinr1 =  buf[i+0x2];
							dev->info_updated |= WDS_CINR1;
							break;
					}
				case 0x72: {	//	-- CINR-2
							dev->cinr2 =  buf[i+0x2];
							dev->info_updated |= WDS_CINR2;
							break;
					}
				case 0x6a: {	//	-- Tx - Power
							dev->txpwr =  buf[i+0x2] / 2;
							dev->info_updated |= WDS_TXPWR;
							break;
					}
				case 0x7f: {	//	-- Frequency
							dev->freq = (buf[i+0x2] << 24) + (buf[i+0x3] << 16) + (buf[i+0x4] << 8) + buf[i+0x5];
							dev->info_updated |= WDS_FREQ;
							break;
					}
		        case 0xd1: {	//	-- Home Network data from device
						break;
					}
		        case 0xd2: {	//	--  H-NSP ID
						if (param_len == 0x3) {
							dev->nspid=(buf[i + 2]<<16) + (buf[i + 3]<<8) + buf[i + 4];
							dev->info_updated |= WDS_NSPID;
						} else  
							wmlog_msg(1, "bad param_len H-NSPID");
						break;
					}
/*				case 0xd4:	//	-- Network name
						memcpy(dev->netname, buf + i + 2, param_len);
						dev->info_updated |= WDS_NETNAME;				
*/						
				default: {
						wmlog_msg(3, "bad config format %02x",buf[i]);
						break;
						//return -1;
					}
			}
			if (buf[i] == 0xd1)
				i += 4;
			else
				i += buf[i+1] + 2;
		}  // error if all param are unknown!!!
		return 0;
	}
	
	dev->info_updated |= WDS_OTHER;

	return 0;
	
	/*
	short type_a = (buf[0x14] << 8) + buf[0x15];
	short type_b = (buf[0x16] << 8) + buf[0x17];
	short param_len = (buf[0x18] << 8) + buf[0x19];

	if (type_a == 0x8 && type_b == 0x2) {
		if (param_len != 0x80) {
			wmlog_msg(1, "bad param_len");
			return -1;
		}
		memcpy(dev->chip, buf + 0x1a, 0x40);
		memcpy(dev->firmware, buf + 0x5a, 0x40);
		dev->info_updated |= WDS_CHIP | WDS_FIRMWARE;
		return 0;
	}
	if (type_a == 0x3 && type_b == 0x2) {
		if (param_len != 0x6) {
			wmlog_msg(1, "bad param_len");
			return -1;
		}
		memcpy(dev->mac, buf + 0x1a, 0x6);
		dev->info_updated |= WDS_MAC;
		return 0;
	}
	if (type_a == 0x1 && type_b == 0x2) {
		if (param_len != 0x2) {
			wmlog_msg(1, "bad param_len");
			return -1;
		}
		set_link_status((buf[0x1a] << 8) + buf[0x1b]);
		return 0;
	}
	if (type_a == 0x1 && type_b == 0x3) {
		if (param_len != 0x4) {
			wmlog_msg(1, "bad param_len");
			return -1;
		}
		set_link_status(0);
		return 0;
	}
	if (type_a == 0x1 && type_b == 0xa) {
		if (param_len != 0x16) {
			wmlog_msg(1, "bad param_len");
			return -1;
		}
		dev->rssi = (buf[0x1a] << 8) + buf[0x1b];
		dev->cinr = (float)((short)((buf[0x1c] << 8) + buf[0x1d])) / 8;
		memcpy(dev->bsid, buf + 0x1e, 0x6);
		dev->txpwr = (buf[0x26] << 8) + buf[0x27];
		dev->freq = (buf[0x28] << 24) + (buf[0x29] << 16) + (buf[0x2a] << 8) + buf[0x2b];
		dev->info_updated |= WDS_RSSI | WDS_CINR | WDS_BSID | WDS_TXPWR | WDS_FREQ;
		return 0;
	}
	if (type_a == 0x1 && type_b == 0xc) {
		if (param_len != 0x2) {
			wmlog_msg(1, "bad param_len");
			return -1;
		}
		set_state((buf[0x1a] << 8) + buf[0x1b]);
		return 0;
	}

	dev->info_updated |= WDS_OTHER;

	return 0;
	*/
}
/*
static int process_debug_config_response(struct wimax_dev_status *dev, const unsigned char *buf, int len)
{
	dev->info_updated |= WDS_OTHER;
	return 0;
}
*/
static int process_config_response(struct wimax_dev_status *dev, const unsigned char *buf, int len)
{
	/*
	if (buf[0x12] != 0x15) {
		wmlog_msg(1, "bad format");
		return -1;
	}
*/
	switch (buf[1]) {
		case 0x03:
			return process_normal_config_response(dev, buf, len);
	/*	case 0x02:
			return process_debug_config_response(dev, buf, len);
		case 0x04:
			break;*/
		default: {
				wmlog_msg(1, "bad format");
				return -1;
			}
	}

	dev->info_updated |= WDS_OTHER;

	return 0;
}

static int process_E_response(struct wimax_dev_status *dev, const unsigned char *buf, int len)
{
	int i;
	short param_len;
	
	switch (buf[0x01]){
		case 0x0f: {	//	-- find network
				if (len == 0x04) 
					dev->link_status = 0;
				else {
					//int i;
					//short param_len;				
					for (i = 0x04;i < len; i += buf[i+1] + 2)
					{
						//param_len = buf[i+1];
						//if (buf[i] == 0xd6) memcpy(dev->napid, buf + i + 2, param_len);
						if (buf[i] == 0xd6) dev->napid=(buf[i + 2]<<16) + (buf[i + 3]<<8) + buf[i + 4];
						/*
						if (buf[i] == 0x61 && i+14 < len){
							if (buf[i+2] == buf[len-4] && buf[i+5] == buf[len-1])
								memcpy(dev->bsid, buf + i + 8, 0x06);
								
							i += 14;
						}
						*/
					}
					dev->link_status = 1;
				}
				dev->info_updated |= WDS_LINK_STATUS;
				return 0;
				//break;
			}
	
		case 0x12: {	//	--  Connection error
				
				dev->auth_info = buf[0x04];
								
				if (buf[0x04] == 0x00){
					
					return 0;
				}
				//if (buf[len-1] == 0xff) 
				else {
					dev->link_status = 0;
					dev->info_updated |= WDS_LINK_STATUS;
					return 0;
				}
				
				//break;
			}
		case 0x13: {	//	--  Connecting to BS;
				for (i = 0x04;i < len; i += buf[i+1] + 2)
				{
					param_len = buf[i+1];
					if (buf[i] == 0x01) {
						memcpy(dev->bsid, buf + i + 2, 0x06);
						wmlog_msg(0, "Connecting to BSID: %02x:%02x:%02x:%02x:%02x:%02x", dev->bsid[0], dev->bsid[1], dev->bsid[2], dev->bsid[3], dev->bsid[4], dev->bsid[5]);
					}
				}
				dev->link_status = 1;
				dev->info_updated |= WDS_LINK_STATUS;
				return 0;
				//break;
			}
		case 0x14: {	//	--  Connecting to state;
				if(buf[0x04] == 0x00) {
					wmlog_msg(0, "Connected to BSID: %02x:%02x:%02x:%02x:%02x:%02x", dev->bsid[0], dev->bsid[1], dev->bsid[2], dev->bsid[3], dev->bsid[4], dev->bsid[5]);
					dev->link_status = 1;
				} else {
					wmlog_msg(0, "Error connecting to BSID: %02x:%02x:%02x:%02x:%02x:%02x", dev->bsid[0], dev->bsid[1], dev->bsid[2], dev->bsid[3], dev->bsid[4], dev->bsid[5]);
					dev->link_status = 0;
				}
				dev->info_updated |= WDS_LINK_STATUS;
				return 0;
				//break;
			}
		case 0x15: {
			if (buf[0x03] > 0){
				switch (buf[0x04])
				{
					case 0x00: {
						wmlog_msg(1, "Disconnected by BS");
						break;
					}
					case 0x01: {
						wmlog_msg(1, "Disconnected by User");
						break;
					}
					case 0x03: {
						wmlog_msg(1, "Disconnected no BS");
						break;
					}
					default:
						wmlog_msg(1, "Unknown: %02x %02x ... %02x",buf[0x00], buf[0x01], buf[0x04]);
				}
			}
			else {
				wmlog_msg(1, "Unknown: %02x %02x %02x %02x",buf[0x00], buf[0x01], buf[0x02], buf[0x03]);
			}
			return 0;
			}
		case 0x16: {	//	--  Auth state
				if (len == 0x07) {
					//memcpy(dev->auth_state,buf+4,0x03);
					dev->auth_state = (buf[4]<<16) + (buf[5]<<8) + buf[6];
							//dev->freq = (buf[i+0x2] << 24) + (buf[i+0x3] << 16) + (buf[i+0x4] << 8) + buf[i+0x5];
					dev->info_updated |= WDS_AUTH_STATE;
					if ( dev->auth_state == 0x0201ff)
						dev->info_updated |= WDS_EAP;
					
					//wmlog_msg(0, "Auth State: %02x %02x %02x",dev->auth_state[0],dev->auth_state[1],dev->auth_state[2]); 
				} else  wmlog_msg(1, "bad param_len");		
				return 0;
			}

		case 0x17: {	//	--    Changing BS
			if (len == 5 && buf[4] == 0x01) {
				wmlog_msg(0, "Changing BS");
			} else  wmlog_msg(1, "bad format: %02x %02x -> %02x",buf[0], buf[1], buf[4]);		
			return 0;
			}

		case 0x18: {	//	--    BS Changed, new BSID
			if (buf[4] == 0x00) {
				for (i = 5;i < len; i += buf[i+1] + 2)
				{
					param_len = buf[i+1];
					if (buf[i] == 0x01) {
						memcpy(dev->bsid, buf + i + 2, 0x06);
						wmlog_msg(0, "BSID Changed to: %02x:%02x:%02x:%02x:%02x:%02x", dev->bsid[0], dev->bsid[1], dev->bsid[2], dev->bsid[3], dev->bsid[4], dev->bsid[5]);
					}
				}
			} else  wmlog_msg(1, "bad format: %02x %02x -> %02x",buf[0], buf[1], buf[4]);		
			return 0;
			}
			
		case 0x19: {	//	--  RF state
				if (len == 0x05) {
					dev->rf_state =  buf[len-1];
					dev->info_updated |= WDS_RF_STATE;	
				} else  wmlog_msg(1, "bad param_len");
				return 0;
				//break;
			}
			
		default: {
			wmlog_msg(3, "bad format: %02x %02x",buf[0], buf[1]);
			return -1;
			}
	}
/*

	if (buf[0x5] == 0x3) {
		dev->proto_flags = buf[0x7];
		dev->info_updated |= WDS_PROTO_FLAGS;
		return 0;
	}
*/
	dev->info_updated |= WDS_OTHER;

	return 0;
}

static int process_data_response(struct wimax_dev_status *dev, const unsigned char *buf, int len)
{
	switch (buf[0x01]){
		case 0x01: {	//	-- EAP Authentification
				eap_peer_rx(buf + 0x04, len - 0x04);
				dev->info_updated |= WDS_EAP;
			}
		case 0x03:{ 	//	-- Ethernet frame
				write_netif(buf + 0x04, len - 0x04);
				return 0;
			}
		default: {
				wmlog_msg(1, "bad format: %02x %02x",buf[0], buf[1]);
				return -1;
			}
	}
	return 0;
}

static int process_P_response(struct wimax_dev_status *dev, const unsigned char *buf, int len) //83
{
	short param_len;
	
	switch (buf[0x01]){
		case 0x0d: {	//	-- Text out
				int i;
				//unsigned char data[buf[0x02]<<8+buf[0x03]+1];
				unsigned char data[0x4000];
				for (i = 4; i < len; i++)
				{
					if (isprint(buf[i])) {
						data[i-4] = buf[i];
					}
					else {
						if (buf[i] == 0x0a){
							data[i-4] = 0x0a;
						}
						else {
							data[i-4] = 0x20;
						}
					}		
				}
				data[len-4] = 0x00; //последний байт
				wmlog_msg(2, "%s", data);
				//memset(data,0,sizeof(data));
				//dev->info_updated |= WDS_OTHER;
				break;
			}
		case 0x11: {
				short code;
				int offset;
				param_len = (buf[2] << 8) + buf[3] - 6;
				code = (buf[4] << 8) + buf[5];
				offset = (buf[6] << 24) + (buf[7] << 16) + (buf[8] << 8) + buf[9];
				wmlog_msg(0, "Image DL code: %04x offset: %08x len: %04x", code, offset, param_len);  
				dev->info_updated |= WDS_CERT;
				break;
			}		
		case 0x13: {
				param_len = (buf[2] << 8) + buf[3];
				if (param_len == 6)
				{
					memcpy(dev->cert,buf+4,param_len);
					if (buf[6] == 0xff && ((dev->cert_buf[0]<<8)+dev->cert_buf[1]) == 0)
						wmlog_msg(1, "Cert %d is empty.",buf[5]);
					if (buf[6] == 0xff)
						wmlog_msg(2, "Reading cert %d finished.",buf[5]);
					dev->info_updated |= WDS_CERT;
					break;
				}
				if (param_len > 6)
				{
					if (buf[8] == 0x00)
						wmlog_msg(2, "Start reading cert %d.",buf[5]);
					memcpy(dev->cert,buf+4,6);
					dev->cert_buf[0] = ((param_len-6) >> 8) & 0xff;
					dev->cert_buf[1] = (param_len-6) & 0xff;
					memcpy(dev->cert_buf+2,buf+10,param_len-6);
					wmlog_msg(2, "Read %d bytes of cert %d.",param_len-6+(buf[8]<<8),buf[5]);
					dev->info_updated |= WDS_CERT;
					break;
				}
				wmlog_msg(2, "bad cert size %d",param_len);
				dev->info_updated |= WDS_CERT;
				break;
			}
		case 0x23: {
				if(buf[0x04] == 0x07) {
					dev->link_status = 0;
					wmlog_msg(0, "Signal is Lost");
					//dev->info_updated |= WDS_LINK_STATUS;
					return 0;
				} else {
					wmlog_msg(1, "bad format: %02x %02x",buf[0], buf[1]);
					return 0;
				}
			}
		default: {
			wmlog_msg(1, "bad format: %02x %02x",buf[0], buf[1]);
			break;
			}
	}
	dev->info_updated |= WDS_OTHER;
	return 0;
}

int process_response(struct wimax_dev_status *dev, const unsigned char *buf, int len) //Ok
{
	int check_len;

	if(len < 4) {
		wmlog_msg(1, "short read");
		return -1;
	}
/*
	if(buf[0] != 0x57) {
		wmlog_msg(1, "bad header");
		return -1;
	}
*/
	check_len = 4 + (buf[2] << 8) + buf[3];
/*	
	
	if(buf[0] == 0x82 && buf[1] == 0x03) {
		check_len += 3;
	}


	if(check_len != len) {
		wmlog_msg(1, "bad length: %02x instead of %02x", check_len, len);
		return -1;
	}	
*/	

	if(check_len != len && check_len+1 != len && check_len+2 != len && check_len+3 != len) {
		wmlog_msg(1, "bad length: %02x instead of %02x", check_len, len);
		return -1;
	}

	switch (buf[0x00]) {
		case 0x80:
			return process_config_response(dev, buf, len);
		case 0x81:
			return process_E_response(dev, buf, len);
		case 0x82:
			return process_data_response(dev, buf, len);
		case 0x83:
			return process_P_response(dev, buf, len);
		default: {
			wmlog_msg(1, "bad response type: %02x", buf[0]);
			return -1;
			}
	}
}

// static inline void fill_outgoing_packet_header(unsigned char *buf, unsigned char type, int body_len)
static inline void fill_outgoing_packet_header(unsigned char *buf, unsigned short type_a, unsigned char type_b, int body_len) //Ok
{
	buf[0x00] = type_a;
	buf[0x01] = type_b;
	buf[0x02] = (body_len >> 8) & 0xff;
	buf[0x03] = body_len & 0xff;
}
/*
int fill_protocol_info_req(unsigned char *buf, unsigned char flags)
{
	fill_outgoing_packet_header(buf, 0x45, 4);
	buf += HEADER_LENGTH_LOWLEVEL;
	buf[0x00] = 0x00;
	buf[0x01] = 0x02;
	buf[0x02] = 0x00;
	buf[0x03] = flags;
	return HEADER_LENGTH_LOWLEVEL + 4;
}

int fill_mac_lowlevel_req(unsigned char *buf)
{
	fill_outgoing_packet_header(buf, 0x50, 0x14);
	buf += HEADER_LENGTH_LOWLEVEL;
	memset(buf, 0, 0x14);
	buf[0x0c] = 0x15;
	buf[0x0d] = 0x0a;
	return HEADER_LENGTH_LOWLEVEL + 0x14;
}
*/
/*
static inline void fill_config_req(unsigned char *buf, int body_len)
{
	fill_outgoing_packet_header(buf, 0x00, 0x02, body_len);
	//memset(buf + HEADER_LENGTH, 0, 12);
}
*/
static int fill_normal_config_req(unsigned char *buf, unsigned short type_a, unsigned short type_b, unsigned short param_len, unsigned char *param) //Ok
{
	int body_len = param_len;
	fill_outgoing_packet_header(buf, type_a, type_b, body_len);
	//fill_config_req(buf, body_len);
	/*
	buf[0x04] = 0x15;
	buf[0x05] = 0x00;
	buf += HEADER_LENGTH;
	buf[0x0c] = 0x15;
	buf[0x0d] = 0x00;
	buf[0x0e] = type_a >> 8;
	buf[0x0f] = type_a & 0xff;
	buf[0x10] = type_b >> 8;
	buf[0x11] = type_b & 0xff;
	buf[0x12] = param_len >> 8;
	buf[0x13] = param_len & 0xff;
	*/
	memcpy(buf + 0x04, param, param_len);
	return HEADER_LENGTH + body_len;
}
/*
static int fill_normal_config_req(unsigned char *buf, unsigned short type_a, unsigned short type_b, unsigned short param_len, unsigned char *param)
{
	int body_len = 0x14 + param_len;
	fill_config_req(buf, body_len);
	buf[0x04] = 0x15;
	buf[0x05] = 0x00;
	buf += HEADER_LENGTH;
	buf[0x0c] = 0x15;
	buf[0x0d] = 0x00;
	buf[0x0e] = type_a >> 8;
	buf[0x0f] = type_a & 0xff;
	buf[0x10] = type_b >> 8;
	buf[0x11] = type_b & 0xff;
	buf[0x12] = param_len >> 8;
	buf[0x13] = param_len & 0xff;
	memcpy(buf + 0x14, param, param_len);
	return HEADER_LENGTH + body_len;
}
*/
//>> -- Change Open Mode  00 = Normal 01 = Test (last byte)
//00 01 00 03 b1 01 00
int fill_init_cmd(unsigned char *buf) // Ok
{
	unsigned char param[] = {0xb1, 0x01, 0x00};
	return fill_normal_config_req(buf, 0x00, 0x01, sizeof(param), param); 
	/*
	fill_config_req(buf, 0x12);
	buf[0x04] = 0x15;
	buf[0x05] = 0x04;
	buf += HEADER_LENGTH;
	buf[0x0c] = 0x15;
	buf[0x0d] = 0x04;
	buf[0x0e] = 0x50;
	buf[0x0f] = 0x04;
	return HEADER_LENGTH + 0x12; */
}

int fill_auth_on_req(unsigned char *buf) // OK
{
	unsigned char param[] = {0xaf, 0x01, 0x01};
	return fill_normal_config_req(buf, 0x00, 0x01, sizeof(param), param);
}

int fill_string_info_req(unsigned char *buf) // Ok
{
	unsigned char param[] = {0x1b, 0x1d, 0x1f};
	return fill_normal_config_req(buf, 0x00, 0x02, sizeof(param), param); 
}

int fill_get_nspid_req(unsigned char *buf) // Ok
{
	unsigned char param[] = {0xd1};
	return fill_normal_config_req(buf, 0x00, 0x02, sizeof(param), param); 
}
/*
int fill_diode_control_cmd(unsigned char *buf, int turn_on)
{
	unsigned char param[0x2] = {0x0, turn_on ? 0x1 : 0x0};
	return fill_normal_config_req(buf, 0x30, 0x1, sizeof(param), param);
}
*/

int fill_mac_req(unsigned char *buf) // Ok
{
	unsigned char param[] = {0x00};
	return fill_normal_config_req(buf, 0x00, 0x02, sizeof(param), param); 
}

int fill_get_data_req_start(unsigned char *buf, short code)
{
	unsigned char param[6];
	memset(param, 0, sizeof(param));
	param[0] = (code >> 8) & 0xff;
	param[1] = code & 0xff;
	return fill_normal_config_req(buf, 0x03, 0x12, sizeof(param), param);
}

int fill_get_data_req_step(unsigned char *buf, short code, int offset)
{
	unsigned char param[10];
	memset(param, 0, sizeof(param));
	param[0] = (code >> 8) & 0xff;
	param[1] = code & 0xff;
	param[2] = (offset >> 24) & 0xff;
	param[3] = (offset >> 16) & 0xff;
	param[4] = (offset >> 8) & 0xff;
	param[5] = offset & 0xff;	
	return fill_normal_config_req(buf, 0x03, 0x14, sizeof(param), param);
}

int fill_rf_on_req(unsigned char *buf) // * Ok
{
	return fill_normal_config_req(buf, 0x00, 0x06, 0, NULL); 
}

int fill_rf_off_req(unsigned char *buf) // * Ok
{
	return fill_normal_config_req(buf, 0x00, 0x04, 0, NULL);
}

int fill_eap_key_req(unsigned char *buf, unsigned char *key, int key_len)  //	EAP	key
{
	unsigned char param[key_len+2];
	param[0] = 0x02; // key_len / 32
	param[1] = key_len;
	memcpy(param + 0x02, key, key_len);	
	return fill_normal_config_req(buf, 0x00, 0x01, sizeof(param), param);
}

int fill_image_dl_req(unsigned char *buf, int code, int offset, unsigned char *data, int len)
{
	unsigned char param[len+6];
	param[0] = (code >> 8) & 0xff;
	param[1] = code & 0xff;
	param[2] = (offset >> 24) & 0xff;
	param[3] = (offset >> 16) & 0xff;
	param[4] = (offset >> 8) & 0xff;
	param[5] = offset & 0xff;
	memcpy(param + 6, data, len);	
	return fill_normal_config_req(buf, 0x03, 0x10, sizeof(param), param);	
}
/*
int fill_some_req(unsigned char *buf) // * Ok  X3  O_O
{
	unsigned char param[] = {   0x02, 0x40, 0xa2, 0x89, 0x17, 0x0a, 0x37, 0xfe, 0xf9, 0x06, 0x0b, 0xaa, \
		0x24, 0x8b, 0x8b, 0xcd, 0xf2, 0xd4, 0xd3, 0xea, 0xdb, 0xcb, 0xf7, 0xf9, 0xce, 0x16, 0x3e, 0xc9, \
		0x60, 0x74, 0x53, 0x69, 0x55, 0xe5, 0x5f, 0x19, 0x42, 0x56, 0x4e, 0xf9, 0x65, 0x5d, 0x32, 0x77, \
        0xac, 0xb1, 0x9b, 0xa3, 0xa0, 0xd3, 0xef, 0xdf, 0x81, 0x0b, 0x69, 0x27, 0x76, 0xeb, 0xac, 0xd0, \
		0x2c, 0x5c, 0xaf, 0x3f, 0xfb, 0x4e};
	return fill_normal_config_req(buf, 0x00, 0x01, sizeof(param), param);
}
*/
int fill_debug_req(unsigned char *buf) // OK
{
	unsigned char param[] = {0xb1, 0x01, 0x00}; // last bit 00 = Normal 01 = Test
	return fill_normal_config_req(buf, 0x00, 0x01, sizeof(param), param);
}
/*
int fill_auth_policy_req(unsigned char *buf)
{
	return fill_normal_config_req(buf, 0x20, 0x8, 0x0, NULL);
}

int fill_auth_method_req(unsigned char *buf)
{
	return fill_normal_config_req(buf, 0x20, 0xc, 0x0, NULL);
}

int fill_auth_set_cmd(unsigned char *buf, char *netid)
{
	short netid_len = strlen(netid) + 1;
	unsigned char param[netid_len + 4];
	param[0] = 0x0;
	param[1] = 0x10;
	param[2] = netid_len >> 8;
	param[3] = netid_len & 0xff;
	memcpy(param + 4, netid, netid_len);
	return fill_normal_config_req(buf, 0x20, 0x20, sizeof(param), param);
}
*/
int fill_dm_cmd_req(unsigned char *buf, char *cmd)
{
	unsigned char param[strlen(cmd)+1];
	memcpy(param, cmd, strlen(cmd));
	param[strlen(cmd)] = '\n';
	
//	printf("DM command: %s", param);
	
	return fill_normal_config_req(buf, 0x03, 0x0c, sizeof(param), param);
}

int fill_find_network_req(unsigned char *buf, struct wimax_dev_status *dev) // OK
{
	unsigned char param[6];
	param[0] = 0x02;
	param[1] = 0xd2;
	param[2] = 0x03;
	param[0x03]	= (dev->nspid&0xff0000)>>16;
	param[0x04]	= (dev->nspid&0xff00)>>8;
	param[0x05]	= (dev->nspid&0xff);
	//memcpy(param + 0x03, dev->nspid, 0x03);
	return fill_normal_config_req(buf, 0x01, 0x0d, sizeof(param), param);
}

int fill_connect_req(unsigned char *buf, struct wimax_dev_status *dev) // OK
{
	unsigned char param[0x0a];
	param[0x00] = 0xd2;
	param[0x01] = 0x03;
	//memcpy(param + 0x02, dev->nspid, 0x03);
	param[0x02]	= (dev->nspid&0xff0000)>>16;
	param[0x03]	= (dev->nspid&0xff00)>>8;
	param[0x04]	= (dev->nspid&0xff);
	param[0x05] = 0xd6;
	param[0x06] = 0x03;
	param[0x07]	= (dev->napid&0xff0000)>>16;
	param[0x08]	= (dev->napid&0xff00)>>8;
	param[0x09]	= (dev->napid&0xff);
	//memcpy(param + 0x07, dev->napid, 0x03);
	return fill_normal_config_req(buf, 0x01, 0x10, sizeof(param), param);
}


int fill_eap_server_rx_req(unsigned char *buf,unsigned char *param, int param_len)  //	EAP	!!!
{
	return fill_normal_config_req(buf, 0x02, 0x00, param_len, param);
}


int fill_connection_params_req(unsigned char *buf) // Ok
{
	//unsigned char param[] = {0x01, 0x60, 0x61, 0x6a, 0x7f};
	unsigned char param[] = {0x60, 0x61, 0x72, 0x71, 0x6a, 0x7f};
	return fill_normal_config_req(buf, 0x00, 0x02, sizeof(param), param);
}
/*
int fill_state_req(unsigned char *buf)
{
	return fill_normal_config_req(buf, 0x1, 0xb, 0x0, NULL);
}
*//*
int fill_network_list_req(unsigned char *buf)
{
	unsigned char param[0x2] = {0x00, 0x00};
	return fill_normal_config_req(buf, 0x24, 0x1, sizeof(param), param);
}
*/

int get_header_len()
{
	return HEADER_LENGTH;
}

int fill_data_packet_header(unsigned char *buf, int body_len) // Ok
{
	fill_outgoing_packet_header(buf, 0x02, 0x02, body_len);
	return HEADER_LENGTH + body_len;
}

