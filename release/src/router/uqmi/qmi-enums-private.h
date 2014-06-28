/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * uqmi -- tiny QMI support implementation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2012 Google Inc.
 */

#ifndef _LIBQMI_GLIB_QMI_ENUMS_PRIVATE_H_
#define _LIBQMI_GLIB_QMI_ENUMS_PRIVATE_H_

/*****************************************************************************/
/* QMI Control */

/**
 * QmiCtlDataFormat:
 * @QMI_CTL_DATA_FORMAT_QOS_FLOW_HEADER_ABSENT: QoS header absent
 * @QMI_CTL_DATA_FORMAT_QOS_FLOW_HEADER_PRESENT: QoS header present
 *
 * Controls whether the network port data format includes a QoS header or not.
 * Should normally be set to ABSENT.
 */
typedef enum {
    QMI_CTL_DATA_FORMAT_QOS_FLOW_HEADER_ABSENT  = 0,
    QMI_CTL_DATA_FORMAT_QOS_FLOW_HEADER_PRESENT = 1,
} QmiCtlDataFormat;

/**
 * QmiCtlDataLinkProtocol:
 * @QMI_CTL_DATA_LINK_PROTOCOL_802_3: data frames formatted as 802.3 Ethernet
 * @QMI_CTL_DATA_LINK_PROTOCOL_RAW_IP: data frames are raw IP packets
 *
 * Determines the network port data format.
 */
typedef enum {
    QMI_CTL_DATA_LINK_PROTOCOL_UNKNOWN = 0,
    QMI_CTL_DATA_LINK_PROTOCOL_802_3   = 1,
    QMI_CTL_DATA_LINK_PROTOCOL_RAW_IP  = 2,
} QmiCtlDataLinkProtocol;

/**
 * QmiCtlFlag:
 * @QMI_CTL_FLAG_NONE: None.
 * @QMI_CTL_FLAG_RESPONSE: Message is a response.
 * @QMI_CTL_FLAG_INDICATION: Message is an indication.
 *
 * QMI flags in messages of the %QMI_SERVICE_CTL service.
 */
typedef enum {
    QMI_CTL_FLAG_NONE       = 0,
    QMI_CTL_FLAG_RESPONSE   = 1 << 0,
    QMI_CTL_FLAG_INDICATION = 1 << 1
} QmiCtlFlag;

/**
 * QmiServiceFlag:
 * @QMI_SERVICE_FLAG_NONE: None.
 * @QMI_SERVICE_FLAG_COMPOUND: Message is compound.
 * @QMI_SERVICE_FLAG_RESPONSE: Message is a response.
 * @QMI_SERVICE_FLAG_INDICATION: Message is an indication.
 *
 * QMI flags in messages which are not of the %QMI_SERVICE_CTL service.
 */
typedef enum {
    QMI_SERVICE_FLAG_NONE       = 0,
    QMI_SERVICE_FLAG_COMPOUND   = 1 << 0,
    QMI_SERVICE_FLAG_RESPONSE   = 1 << 1,
    QMI_SERVICE_FLAG_INDICATION = 1 << 2
} QmiServiceFlag;

#endif /* _LIBQMI_GLIB_QMI_ENUMS_PRIVATE_H_ */
