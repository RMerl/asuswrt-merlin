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

#ifndef _LIBQMI_GLIB_QMI_ENUMS_NAS_H_
#define _LIBQMI_GLIB_QMI_ENUMS_NAS_H_

/**
 * SECTION: qmi-enums-nas
 * @title: NAS enumerations and flags
 *
 * This section defines enumerations and flags used in the NAS service
 * interface.
 */

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Event Report' indication */

/**
 * QmiNasRadioInterface:
 * @QMI_NAS_RADIO_INTERFACE_UNKNOWN: Not known or not needed.
 * @QMI_NAS_RADIO_INTERFACE_NONE: None, no service.
 * @QMI_NAS_RADIO_INTERFACE_CDMA_1X: CDMA2000 1X.
 * @QMI_NAS_RADIO_INTERFACE_CDMA_1XEVDO: CDMA2000 HRPD (1xEV-DO).
 * @QMI_NAS_RADIO_INTERFACE_AMPS: AMPS.
 * @QMI_NAS_RADIO_INTERFACE_GSM: GSM.
 * @QMI_NAS_RADIO_INTERFACE_UMTS: UMTS.
 * @QMI_NAS_RADIO_INTERFACE_LTE: LTE.
 * @QMI_NAS_RADIO_INTERFACE_TD_SCDMA: TD-SCDMA.
 *
 * Radio interface technology.
 */
typedef enum {
    QMI_NAS_RADIO_INTERFACE_UNKNOWN     = -1,
    QMI_NAS_RADIO_INTERFACE_NONE        = 0x00,
    QMI_NAS_RADIO_INTERFACE_CDMA_1X     = 0x01,
    QMI_NAS_RADIO_INTERFACE_CDMA_1XEVDO = 0x02,
    QMI_NAS_RADIO_INTERFACE_AMPS        = 0x03,
    QMI_NAS_RADIO_INTERFACE_GSM         = 0x04,
    QMI_NAS_RADIO_INTERFACE_UMTS        = 0x05,
    QMI_NAS_RADIO_INTERFACE_LTE         = 0x08,
    QMI_NAS_RADIO_INTERFACE_TD_SCDMA    = 0x09
} QmiNasRadioInterface;

/**
 * QmiNasActiveBand:
 * @QMI_NAS_ACTIVE_BAND_BC_0: Band class 0.
 * @QMI_NAS_ACTIVE_BAND_BC_1: Band class 1.
 * @QMI_NAS_ACTIVE_BAND_BC_2: Band class 2.
 * @QMI_NAS_ACTIVE_BAND_BC_3: Band class 3.
 * @QMI_NAS_ACTIVE_BAND_BC_4: Band class 4.
 * @QMI_NAS_ACTIVE_BAND_BC_5: Band class 5.
 * @QMI_NAS_ACTIVE_BAND_BC_6: Band class 6.
 * @QMI_NAS_ACTIVE_BAND_BC_7: Band class 7.
 * @QMI_NAS_ACTIVE_BAND_BC_8: Band class 8.
 * @QMI_NAS_ACTIVE_BAND_BC_9: Band class 9.
 * @QMI_NAS_ACTIVE_BAND_BC_10: Band class 10.
 * @QMI_NAS_ACTIVE_BAND_BC_11: Band class 11.
 * @QMI_NAS_ACTIVE_BAND_BC_12: Band class 12.
 * @QMI_NAS_ACTIVE_BAND_BC_13: Band class 13.
 * @QMI_NAS_ACTIVE_BAND_BC_14: Band class 14.
 * @QMI_NAS_ACTIVE_BAND_BC_15: Band class 15.
 * @QMI_NAS_ACTIVE_BAND_BC_16: Band class 16.
 * @QMI_NAS_ACTIVE_BAND_BC_17: Band class 17.
 * @QMI_NAS_ACTIVE_BAND_BC_18: Band class 18.
 * @QMI_NAS_ACTIVE_BAND_BC_19: Band class 19.
 * @QMI_NAS_ACTIVE_BAND_GSM_450: GSM 450.
 * @QMI_NAS_ACTIVE_BAND_GSM_480: GSM 480.
 * @QMI_NAS_ACTIVE_BAND_GSM_750: GSM 750.
 * @QMI_NAS_ACTIVE_BAND_GSM_850: GSM 850.
 * @QMI_NAS_ACTIVE_BAND_GSM_900_EXTENDED: GSM 900 (Extended).
 * @QMI_NAS_ACTIVE_BAND_GSM_900_PRIMARY: GSM 900 (Primary).
 * @QMI_NAS_ACTIVE_BAND_GSM_900_RAILWAYS: GSM 900 (Railways).
 * @QMI_NAS_ACTIVE_BAND_GSM_DCS_1800: GSM 1800.
 * @QMI_NAS_ACTIVE_BAND_GSM_PCS_1900: GSM 1900.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_2100: WCDMA 2100.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_PCS_1900: WCDMA PCS 1900.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_DCS_1800: WCDMA DCS 1800.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_1700_US: WCDMA 1700 (U.S.).
 * @QMI_NAS_ACTIVE_BAND_WCDMA_850: WCDMA 850.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_800: WCDMA 800.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_2600: WCDMA 2600.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_900: WCDMA 900.
 * @QMI_NAS_ACTIVE_BAND_WCDMA_1700_JAPAN: WCDMA 1700 (Japan).
 * @QMI_NAS_ACTIVE_BAND_WCDMA_1500_JAPAN: WCDMA 1500 (Japan).
 * @QMI_NAS_ACTIVE_BAND_WCDMA_850_JAPAN: WCDMA 850 (Japan).
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_1: EUTRAN band 1.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_2: EUTRAN band 2.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_3: EUTRAN band 3.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_4: EUTRAN band 4.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_5: EUTRAN band 5.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_6: EUTRAN band 6.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_7: EUTRAN band 7.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_8: EUTRAN band 8.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_9: EUTRAN band 9.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_10: EUTRAN band 10.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_11: EUTRAN band 11.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_12: EUTRAN band 12.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_13: EUTRAN band 13.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_14: EUTRAN band 14.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_17: EUTRAN band 17.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_18: EUTRAN band 18.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_19: EUTRAN band 19.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_20: EUTRAN band 20.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_21: EUTRAN band 21.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_24: EUTRAN band 24.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_25: EUTRAN band 25.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_33: EUTRAN band 33.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_34: EUTRAN band 34.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_35: EUTRAN band 35.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_36: EUTRAN band 36.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_37: EUTRAN band 37.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_38: EUTRAN band 38.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_39: EUTRAN band 39.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_40: EUTRAN band 40.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_41: EUTRAN band 41.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_42: EUTRAN band 42.
 * @QMI_NAS_ACTIVE_BAND_EUTRAN_43: EUTRAN band 43.
 * @QMI_NAS_ACTIVE_BAND_TDSCDMA_A: TD-SCDMA Band A.
 * @QMI_NAS_ACTIVE_BAND_TDSCDMA_B: TD-SCDMA Band B.
 * @QMI_NAS_ACTIVE_BAND_TDSCDMA_C: TD-SCDMA Band C.
 * @QMI_NAS_ACTIVE_BAND_TDSCDMA_D: TD-SCDMA Band D.
 * @QMI_NAS_ACTIVE_BAND_TDSCDMA_E: TD-SCDMA Band E.
 * @QMI_NAS_ACTIVE_BAND_TDSCDMA_F: TD-SCDMA Band F.
 *
 * Band classes.
 */
typedef enum {
    QMI_NAS_ACTIVE_BAND_BC_0 = 0,
    QMI_NAS_ACTIVE_BAND_BC_1 = 1,
    QMI_NAS_ACTIVE_BAND_BC_2 = 2,
    QMI_NAS_ACTIVE_BAND_BC_3 = 3,
    QMI_NAS_ACTIVE_BAND_BC_4 = 4,
    QMI_NAS_ACTIVE_BAND_BC_5 = 5,
    QMI_NAS_ACTIVE_BAND_BC_6 = 6,
    QMI_NAS_ACTIVE_BAND_BC_7 = 7,
    QMI_NAS_ACTIVE_BAND_BC_8 = 8,
    QMI_NAS_ACTIVE_BAND_BC_9 = 9,
    QMI_NAS_ACTIVE_BAND_BC_10 = 10,
    QMI_NAS_ACTIVE_BAND_BC_11 = 11,
    QMI_NAS_ACTIVE_BAND_BC_12 = 12,
    QMI_NAS_ACTIVE_BAND_BC_13 = 13,
    QMI_NAS_ACTIVE_BAND_BC_14 = 14,
    QMI_NAS_ACTIVE_BAND_BC_15 = 15,
    QMI_NAS_ACTIVE_BAND_BC_16 = 16,
    QMI_NAS_ACTIVE_BAND_BC_17 = 17,
    QMI_NAS_ACTIVE_BAND_BC_18 = 18,
    QMI_NAS_ACTIVE_BAND_BC_19 = 19,
    QMI_NAS_ACTIVE_BAND_GSM_450 = 40,
    QMI_NAS_ACTIVE_BAND_GSM_480 = 41,
    QMI_NAS_ACTIVE_BAND_GSM_750 = 42,
    QMI_NAS_ACTIVE_BAND_GSM_850 = 43,
    QMI_NAS_ACTIVE_BAND_GSM_900_EXTENDED = 44,
    QMI_NAS_ACTIVE_BAND_GSM_900_PRIMARY = 45,
    QMI_NAS_ACTIVE_BAND_GSM_900_RAILWAYS = 46,
    QMI_NAS_ACTIVE_BAND_GSM_DCS_1800 = 47,
    QMI_NAS_ACTIVE_BAND_GSM_PCS_1900 = 48,
    QMI_NAS_ACTIVE_BAND_WCDMA_2100 = 80,
    QMI_NAS_ACTIVE_BAND_WCDMA_PCS_1900 = 81,
    QMI_NAS_ACTIVE_BAND_WCDMA_DCS_1800 = 82,
    QMI_NAS_ACTIVE_BAND_WCDMA_1700_US = 83,
    QMI_NAS_ACTIVE_BAND_WCDMA_850 = 84,
    QMI_NAS_ACTIVE_BAND_WCDMA_800 = 85,
    QMI_NAS_ACTIVE_BAND_WCDMA_2600 = 86,
    QMI_NAS_ACTIVE_BAND_WCDMA_900 = 87,
    QMI_NAS_ACTIVE_BAND_WCDMA_1700_JAPAN = 88,
    QMI_NAS_ACTIVE_BAND_WCDMA_1500_JAPAN = 90,
    QMI_NAS_ACTIVE_BAND_WCDMA_850_JAPAN = 91,
    QMI_NAS_ACTIVE_BAND_EUTRAN_1 = 120,
    QMI_NAS_ACTIVE_BAND_EUTRAN_2 = 121,
    QMI_NAS_ACTIVE_BAND_EUTRAN_3 = 122,
    QMI_NAS_ACTIVE_BAND_EUTRAN_4 = 123,
    QMI_NAS_ACTIVE_BAND_EUTRAN_5 = 124,
    QMI_NAS_ACTIVE_BAND_EUTRAN_6 = 125,
    QMI_NAS_ACTIVE_BAND_EUTRAN_7 = 126,
    QMI_NAS_ACTIVE_BAND_EUTRAN_8 = 127,
    QMI_NAS_ACTIVE_BAND_EUTRAN_9 = 128,
    QMI_NAS_ACTIVE_BAND_EUTRAN_10 = 129,
    QMI_NAS_ACTIVE_BAND_EUTRAN_11 = 130,
    QMI_NAS_ACTIVE_BAND_EUTRAN_12 = 131,
    QMI_NAS_ACTIVE_BAND_EUTRAN_13 = 132,
    QMI_NAS_ACTIVE_BAND_EUTRAN_14 = 133,
    QMI_NAS_ACTIVE_BAND_EUTRAN_17 = 134,
    QMI_NAS_ACTIVE_BAND_EUTRAN_18 = 143,
    QMI_NAS_ACTIVE_BAND_EUTRAN_19 = 144,
    QMI_NAS_ACTIVE_BAND_EUTRAN_20 = 145,
    QMI_NAS_ACTIVE_BAND_EUTRAN_21 = 146,
    QMI_NAS_ACTIVE_BAND_EUTRAN_24 = 147,
    QMI_NAS_ACTIVE_BAND_EUTRAN_25 = 148,
    QMI_NAS_ACTIVE_BAND_EUTRAN_33 = 135,
    QMI_NAS_ACTIVE_BAND_EUTRAN_34 = 136,
    QMI_NAS_ACTIVE_BAND_EUTRAN_35 = 137,
    QMI_NAS_ACTIVE_BAND_EUTRAN_36 = 138,
    QMI_NAS_ACTIVE_BAND_EUTRAN_37 = 139,
    QMI_NAS_ACTIVE_BAND_EUTRAN_38 = 140,
    QMI_NAS_ACTIVE_BAND_EUTRAN_39 = 141,
    QMI_NAS_ACTIVE_BAND_EUTRAN_40 = 142,
    QMI_NAS_ACTIVE_BAND_EUTRAN_41 = 149,
    QMI_NAS_ACTIVE_BAND_EUTRAN_42 = 150,
    QMI_NAS_ACTIVE_BAND_EUTRAN_43 = 151,
    QMI_NAS_ACTIVE_BAND_TDSCDMA_A = 200,
    QMI_NAS_ACTIVE_BAND_TDSCDMA_B = 201,
    QMI_NAS_ACTIVE_BAND_TDSCDMA_C = 202,
    QMI_NAS_ACTIVE_BAND_TDSCDMA_D = 203,
    QMI_NAS_ACTIVE_BAND_TDSCDMA_E = 204,
    QMI_NAS_ACTIVE_BAND_TDSCDMA_F = 205
} QmiNasActiveBand;

/**
 * QmiNasNetworkServiceDomain:
 * @QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE: No service.
 * @QMI_NAS_NETWORK_SERVICE_DOMAIN_CS: Circuit switched.
 * @QMI_NAS_NETWORK_SERVICE_DOMAIN_PS: Packet switched.
 * @QMI_NAS_NETWORK_SERVICE_DOMAIN_CS_PS: Circuit and packet switched.
 * @QMI_NAS_NETWORK_SERVICE_DOMAIN_UNKNOWN: Unknown service.
 *
 * Network Service Domain.
 */
typedef enum {
    QMI_NAS_NETWORK_SERVICE_DOMAIN_NONE    = 0x00,
    QMI_NAS_NETWORK_SERVICE_DOMAIN_CS      = 0x01,
    QMI_NAS_NETWORK_SERVICE_DOMAIN_PS      = 0x02,
    QMI_NAS_NETWORK_SERVICE_DOMAIN_CS_PS   = 0x03,
    QMI_NAS_NETWORK_SERVICE_DOMAIN_UNKNOWN = 0x04,
} QmiNasNetworkServiceDomain;

/**
 * QmiNasEvdoSinrLevel:
 * @QMI_NAS_EVDO_SINR_LEVEL_0: -9 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_1: -6 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_2: -4.5 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_3: -3 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_4: -2 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_5: +1 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_6: +3 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_7: +6 dB.
 * @QMI_NAS_EVDO_SINR_LEVEL_8: +9 dB.
 *
 * EV-DO SINR level.
 */
typedef enum {
    QMI_NAS_EVDO_SINR_LEVEL_0 = 0,
    QMI_NAS_EVDO_SINR_LEVEL_1 = 1,
    QMI_NAS_EVDO_SINR_LEVEL_2 = 2,
    QMI_NAS_EVDO_SINR_LEVEL_3 = 3,
    QMI_NAS_EVDO_SINR_LEVEL_4 = 4,
    QMI_NAS_EVDO_SINR_LEVEL_5 = 5,
    QMI_NAS_EVDO_SINR_LEVEL_6 = 6,
    QMI_NAS_EVDO_SINR_LEVEL_7 = 7,
    QMI_NAS_EVDO_SINR_LEVEL_8 = 8
} QmiNasEvdoSinrLevel;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Get Signal Strength' request/response */

/**
 * QmiNasSignalStrengthRequest:
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_NONE: None.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_RSSI: Request RSSI information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_ECIO: Request ECIO information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_IO: Request IO information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_SINR: Request SINR information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_ERROR_RATE: Request error rate information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_RSRQ: Request RSRQ information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_LTE_SNR: Request LTE SNR information.
 * @QMI_NAS_SIGNAL_STRENGTH_REQUEST_LTE_RSRP: Request LTE RSRP information.
 *
 * Extra information to request when gathering Signal Strength.
 */
typedef enum {
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_NONE       = 0,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_RSSI       = 1 << 0,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_ECIO       = 1 << 1,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_IO         = 1 << 2,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_SINR       = 1 << 3,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_ERROR_RATE = 1 << 4,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_RSRQ       = 1 << 5,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_LTE_SNR    = 1 << 6,
    QMI_NAS_SIGNAL_STRENGTH_REQUEST_LTE_RSRP   = 1 << 7
} QmiNasSignalStrengthRequest;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Network Scan' request/response */

/**
 * QmiNasNetworkScanType:
 * @QMI_NAS_NETWORK_SCAN_TYPE_GSM: GSM network.
 * @QMI_NAS_NETWORK_SCAN_TYPE_UMTS: UMTS network.
 * @QMI_NAS_NETWORK_SCAN_TYPE_LTE: LTE network.
 * @QMI_NAS_NETWORK_SCAN_TYPE_TD_SCDMA: TD-SCDMA network.
 *
 * Flags to use when specifying which networks to scan.
 */
typedef enum {
    QMI_NAS_NETWORK_SCAN_TYPE_GSM      = 1 << 0,
    QMI_NAS_NETWORK_SCAN_TYPE_UMTS     = 1 << 1,
    QMI_NAS_NETWORK_SCAN_TYPE_LTE      = 1 << 2,
    QMI_NAS_NETWORK_SCAN_TYPE_TD_SCDMA = 1 << 3
} QmiNasNetworkScanType;

/**
 * QmiNasNetworkStatus:
 * @QMI_NAS_NETWORK_STATUS_CURRENT_SERVING: Network is in use, current serving.
 * @QMI_NAS_NETWORK_STATUS_AVAILABLE: Network is vailable.
 * @QMI_NAS_NETWORK_STATUS_HOME: Network is home network.
 * @QMI_NAS_NETWORK_STATUS_ROAMING: Network is a roaming network.
 * @QMI_NAS_NETWORK_STATUS_FORBIDDEN: Network is forbidden.
 * @QMI_NAS_NETWORK_STATUS_NOT_FORBIDDEN: Network is not forbidden.
 * @QMI_NAS_NETWORK_STATUS_PREFERRED: Network is preferred.
 * @QMI_NAS_NETWORK_STATUS_NOT_PREFERRED: Network is not preferred.
 *
 * Flags to specify the status of a given network.
 */
typedef enum {
    QMI_NAS_NETWORK_STATUS_CURRENT_SERVING = 1 << 0,
    QMI_NAS_NETWORK_STATUS_AVAILABLE       = 1 << 1,
    QMI_NAS_NETWORK_STATUS_HOME            = 1 << 2,
    QMI_NAS_NETWORK_STATUS_ROAMING         = 1 << 3,
    QMI_NAS_NETWORK_STATUS_FORBIDDEN       = 1 << 4,
    QMI_NAS_NETWORK_STATUS_NOT_FORBIDDEN   = 1 << 5,
    QMI_NAS_NETWORK_STATUS_PREFERRED       = 1 << 6,
    QMI_NAS_NETWORK_STATUS_NOT_PREFERRED   = 1 << 7
} QmiNasNetworkStatus;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Initiate Network Register' request/response */

/**
 * QmiNasNetworkRegisterType:
 * @QMI_NAS_NETWORK_REGISTER_TYPE_AUTOMATIC: Automatic network registration.
 * @QMI_NAS_NETWORK_REGISTER_TYPE_MANUAL: Manual network registration.
 *
 * Type of network registration.
 */
typedef enum {
    QMI_NAS_NETWORK_REGISTER_TYPE_AUTOMATIC = 0x01,
    QMI_NAS_NETWORK_REGISTER_TYPE_MANUAL    = 0x02
} QmiNasNetworkRegisterType;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Get Serving System' request/response */

/**
 * QmiNasRegistrationState:
 * @QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED: Not registered.
 * @QMI_NAS_REGISTRATION_STATE_REGISTERED: Registered.
 * @QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING: Searching.
 * @QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED: Registration denied.
 * @QMI_NAS_REGISTRATION_STATE_UNKNOWN: Unknown.
 *
 * Status of the network registration.
 */
typedef enum {
    QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED           = 0x00,
    QMI_NAS_REGISTRATION_STATE_REGISTERED               = 0x01,
    QMI_NAS_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING = 0x02,
    QMI_NAS_REGISTRATION_STATE_REGISTRATION_DENIED      = 0x03,
    QMI_NAS_REGISTRATION_STATE_UNKNOWN                  = 0x04
} QmiNasRegistrationState;

/**
 * QmiNasAttachState:
 * @QMI_NAS_ATTACH_STATE_UNKNOWN: Unknown attach state.
 * @QMI_NAS_ATTACH_STATE_ATTACHED: Attached.
 * @QMI_NAS_ATTACH_STATE_DETACHED: Detached.
 *
 * Domain attach state.
 */
typedef enum {
    QMI_NAS_ATTACH_STATE_UNKNOWN  = 0x00,
    QMI_NAS_ATTACH_STATE_ATTACHED = 0x01,
    QMI_NAS_ATTACH_STATE_DETACHED = 0x02,
} QmiNasAttachState;

/**
 * QmiNasNetworkType:
 * @QMI_NAS_NETWORK_TYPE_UNKNOWN: Unknown.
 * @QMI_NAS_NETWORK_TYPE_3GPP2: 3GPP2 network.
 * @QMI_NAS_NETWORK_TYPE_3GPP: 3GPP network.
 *
 * Type of network.
 */
typedef enum {
    QMI_NAS_NETWORK_TYPE_UNKNOWN = 0x00,
    QMI_NAS_NETWORK_TYPE_3GPP2   = 0x01,
    QMI_NAS_NETWORK_TYPE_3GPP    = 0x02,
} QmiNasNetworkType;

/**
 * QmiNasRoamingIndicatorStatus:
 * @QMI_NAS_ROAMING_INDICATOR_STATUS_ON: Roaming.
 * @QMI_NAS_ROAMING_INDICATOR_STATUS_OFF: Home.
 *
 * Status of the roaming indication.
 */
typedef enum {
    QMI_NAS_ROAMING_INDICATOR_STATUS_ON  = 0x00,
    QMI_NAS_ROAMING_INDICATOR_STATUS_OFF = 0x01,
    /* next values only for 3GPP2 */
} QmiNasRoamingIndicatorStatus;

/**
 * QmiNasDataCapability:
 * @QMI_NAS_DATA_CAPABILITY_NONE: None or unknown.
 * @QMI_NAS_DATA_CAPABILITY_GPRS: GPRS.
 * @QMI_NAS_DATA_CAPABILITY_EDGE: EDGE.
 * @QMI_NAS_DATA_CAPABILITY_HSDPA: HSDPA.
 * @QMI_NAS_DATA_CAPABILITY_HSUPA: HSUPA.
 * @QMI_NAS_DATA_CAPABILITY_WCDMA: WCDMA.
 * @QMI_NAS_DATA_CAPABILITY_CDMA: CDMA.
 * @QMI_NAS_DATA_CAPABILITY_EVDO_REV_0: EV-DO revision 0.
 * @QMI_NAS_DATA_CAPABILITY_EVDO_REV_A: EV-DO revision A.
 * @QMI_NAS_DATA_CAPABILITY_GSM: GSM.
 * @QMI_NAS_DATA_CAPABILITY_EVDO_REV_B: EV-DO revision B.
 * @QMI_NAS_DATA_CAPABILITY_LTE: LTE.
 * @QMI_NAS_DATA_CAPABILITY_HSDPA_PLUS: HSDPA+.
 * @QMI_NAS_DATA_CAPABILITY_DC_HSDPA_PLUS: DC-HSDPA+.
 *
 * Data capability of the network.
 */
typedef enum {
    QMI_NAS_DATA_CAPABILITY_NONE          = 0x00,
    QMI_NAS_DATA_CAPABILITY_GPRS          = 0x01,
    QMI_NAS_DATA_CAPABILITY_EDGE          = 0x02,
    QMI_NAS_DATA_CAPABILITY_HSDPA         = 0x03,
    QMI_NAS_DATA_CAPABILITY_HSUPA         = 0x04,
    QMI_NAS_DATA_CAPABILITY_WCDMA         = 0x05,
    QMI_NAS_DATA_CAPABILITY_CDMA          = 0x06,
    QMI_NAS_DATA_CAPABILITY_EVDO_REV_0    = 0x07,
    QMI_NAS_DATA_CAPABILITY_EVDO_REV_A    = 0x08,
    QMI_NAS_DATA_CAPABILITY_GSM           = 0x09,
    QMI_NAS_DATA_CAPABILITY_EVDO_REV_B    = 0x0A,
    QMI_NAS_DATA_CAPABILITY_LTE           = 0x0B,
    QMI_NAS_DATA_CAPABILITY_HSDPA_PLUS    = 0x0C,
    QMI_NAS_DATA_CAPABILITY_DC_HSDPA_PLUS = 0x0D
} QmiNasDataCapability;

/**
 * QmiNasServiceStatus:
 * @QMI_NAS_SERVICE_STATUS_NONE: No service.
 * @QMI_NAS_SERVICE_STATUS_LIMITED: Limited service.
 * @QMI_NAS_SERVICE_STATUS_AVAILABLE: Service available.
 * @QMI_NAS_SERVICE_STATUS_LIMITED_REGIONAL: Limited regional service.
 * @QMI_NAS_SERVICE_STATUS_POWER_SAVE: Device in power save mode.
 *
 * Status of the service.
 */
typedef enum {
    QMI_NAS_SERVICE_STATUS_NONE             = 0x00,
    QMI_NAS_SERVICE_STATUS_LIMITED          = 0x01,
    QMI_NAS_SERVICE_STATUS_AVAILABLE        = 0x02,
    QMI_NAS_SERVICE_STATUS_LIMITED_REGIONAL = 0x03,
    QMI_NAS_SERVICE_STATUS_POWER_SAVE       = 0x04
} QmiNasServiceStatus;

/**
 * QmiNasHdrPersonality:
 * @QMI_NAS_HDR_PERSONALITY_UNKNOWN: Unknown.
 * @QMI_NAS_HDR_PERSONALITY_HRPD: HRPD.
 * @QMI_NAS_HDR_PERSONALITY_EHRPD: eHRPD.
 *
 * HDR personality type.
 */
typedef enum {
    QMI_NAS_HDR_PERSONALITY_UNKNOWN = 0x00,
    QMI_NAS_HDR_PERSONALITY_HRPD    = 0x01,
    QMI_NAS_HDR_PERSONALITY_EHRPD   = 0x02,
} QmiNasHdrPersonality;

/**
 * QmiNasCallBarringStatus:
 * @QMI_NAS_CALL_BARRING_STATUS_NORMAL_ONLY: Normal calls only.
 * @QMI_NAS_CALL_BARRING_STATUS_EMERGENCY_ONLY: Emergency calls only.
 * @QMI_NAS_CALL_BARRING_STATUS_NO_CALLS: No calls allowed.
 * @QMI_NAS_CALL_BARRING_STATUS_ALL_CALLS: All calls allowed.
 * @QMI_NAS_CALL_BARRING_STATUS_UNKNOWN: Unknown.
 *
 * Status of the call barring functionality.
 */
typedef enum {
    QMI_NAS_CALL_BARRING_STATUS_NORMAL_ONLY    = 0x00,
    QMI_NAS_CALL_BARRING_STATUS_EMERGENCY_ONLY = 0x01,
    QMI_NAS_CALL_BARRING_STATUS_NO_CALLS       = 0x02,
    QMI_NAS_CALL_BARRING_STATUS_ALL_CALLS      = 0x03,
    QMI_NAS_CALL_BARRING_STATUS_UNKNOWN        = -1
} QmiNasCallBarringStatus;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Get Home Network' request/response */

/**
 * QmiNasNetworkDescriptionDisplay:
 * @QMI_NAS_NETWORK_DESCRIPTION_DISPLAY_NO: Don't display.
 * @QMI_NAS_NETWORK_DESCRIPTION_DISPLAY_YES: Display.
 * @QMI_NAS_NETWORK_DESCRIPTION_DISPLAY_UNKNOWN: Unknown.
 *
 * Setup to define whether the network description should be displayed.
 */
typedef enum {
    QMI_NAS_NETWORK_DESCRIPTION_DISPLAY_NO      = 0x00,
    QMI_NAS_NETWORK_DESCRIPTION_DISPLAY_YES     = 0x01,
    QMI_NAS_NETWORK_DESCRIPTION_DISPLAY_UNKNOWN = 0xFF
} QmiNasNetworkDescriptionDisplay;

/**
 * QmiNasNetworkDescriptionEncoding:
 * @QMI_NAS_NETWORK_DESCRIPTION_ENCODING_UNSPECIFIED: Unspecified.
 * @QMI_NAS_NETWORK_DESCRIPTION_ENCODING_ASCII7: ASCII-7.
 * @QMI_NAS_NETWORK_DESCRIPTION_ENCODING_UNICODE: Unicode.
 * @QMI_NAS_NETWORK_DESCRIPTION_ENCODING_GSM: GSM 7-bit.
 *
 * Type of encoding used in the network description.
 */
typedef enum {
    QMI_NAS_NETWORK_DESCRIPTION_ENCODING_UNSPECIFIED = 0x00,
    QMI_NAS_NETWORK_DESCRIPTION_ENCODING_ASCII7      = 0x01,
    QMI_NAS_NETWORK_DESCRIPTION_ENCODING_UNICODE     = 0x04,
    QMI_NAS_NETWORK_DESCRIPTION_ENCODING_GSM         = 0x09
} QmiNasNetworkDescriptionEncoding;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Get Technology Preference' request/response */

/**
 * QmiNasRadioTechnologyPreference:
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AUTO: Automatic selection.
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP2: 3GPP2 technology.
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP: 3GPP technology.
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AMPS_OR_GSM: AMPS if 3GPP2, GSM if 3GPP.
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA: CDMA if 3GPP2, WCDMA if 3GPP.
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_HDR: CDMA EV-DO.
 * @QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_LTE: LTE.
 *
 * Flags to specify the radio technology preference.
 */
typedef enum {
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AUTO          = 0,
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP2         = 1 << 0,
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_3GPP          = 1 << 1,
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_AMPS_OR_GSM   = 1 << 2,
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_CDMA_OR_WCDMA = 1 << 3,
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_HDR           = 1 << 4,
    QMI_NAS_RADIO_TECHNOLOGY_PREFERENCE_LTE           = 1 << 5
} QmiNasRadioTechnologyPreference;

/**
 * QmiNasPreferenceDuration:
 * @QMI_NAS_PREFERENCE_DURATION_PERMANENT: Permanent.
 * @QMI_NAS_PREFERENCE_DURATION_POWER_CYCLE: Until the next power cycle.
 * @QMI_NAS_PREFERENCE_DURATION_ONE_CALL: Until end of call.
 * @QMI_NAS_PREFERENCE_DURATION_ONE_CALL_OR_TIME: Until end of call or a specified time.
 * @QMI_NAS_PREFERENCE_DURATION_INTERNAL_ONE_CALL_1: Internal reason 1, one call.
 * @QMI_NAS_PREFERENCE_DURATION_INTERNAL_ONE_CALL_2: Internal reason 2, one call.
 * @QMI_NAS_PREFERENCE_DURATION_INTERNAL_ONE_CALL_3: Internal reason 3, one call.
 *
 * Duration of the preference setting.
 */
typedef enum {
    QMI_NAS_PREFERENCE_DURATION_PERMANENT           = 0x00,
    QMI_NAS_PREFERENCE_DURATION_POWER_CYCLE         = 0x01,
    QMI_NAS_PREFERENCE_DURATION_ONE_CALL            = 0x02,
    QMI_NAS_PREFERENCE_DURATION_ONE_CALL_OR_TIME    = 0x03,
    QMI_NAS_PREFERENCE_DURATION_INTERNAL_ONE_CALL_1 = 0x04,
    QMI_NAS_PREFERENCE_DURATION_INTERNAL_ONE_CALL_2 = 0x05,
    QMI_NAS_PREFERENCE_DURATION_INTERNAL_ONE_CALL_3 = 0x06
} QmiNasPreferenceDuration;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Get/Set System Selection Preference'
 * requests/responses */

/**
 * QmiNasRatModePreference:
 * @QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X: CDMA2000 1X.
 * @QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO: CDMA2000 HRPD (1xEV-DO).
 * @QMI_NAS_RAT_MODE_PREFERENCE_GSM: GSM.
 * @QMI_NAS_RAT_MODE_PREFERENCE_UMTS: UMTS.
 * @QMI_NAS_RAT_MODE_PREFERENCE_LTE: LTE.
 * @QMI_NAS_RAT_MODE_PREFERENCE_TD_SCDMA: TD-SCDMA.
 *
 * Flags specifying radio access technology mode preference.
 */
typedef enum {
    QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1X     = 1 << 0,
    QMI_NAS_RAT_MODE_PREFERENCE_CDMA_1XEVDO = 1 << 1,
    QMI_NAS_RAT_MODE_PREFERENCE_GSM         = 1 << 2,
    QMI_NAS_RAT_MODE_PREFERENCE_UMTS        = 1 << 3,
    QMI_NAS_RAT_MODE_PREFERENCE_LTE         = 1 << 4,
    QMI_NAS_RAT_MODE_PREFERENCE_TD_SCDMA    = 1 << 5
} QmiNasRatModePreference;

/**
 * QmiNasCdmaPrlPreference:
 * @QMI_NAS_CDMA_PRL_PREFERENCE_A_SIDE_ONLY: System A only.
 * @QMI_NAS_CDMA_PRL_PREFERENCE_B_SIDE_ONLY: System B only.
 * @QMI_NAS_CDMA_PRL_PREFERENCE_ANY: Any system.
 *
 * Flags specifying the preference when using CDMA Band Class 0.
 */
typedef enum {
    QMI_NAS_CDMA_PRL_PREFERENCE_A_SIDE_ONLY = 0x0001,
    QMI_NAS_CDMA_PRL_PREFERENCE_B_SIDE_ONLY = 0x0002,
    QMI_NAS_CDMA_PRL_PREFERENCE_ANY         = 0x3FFF
} QmiNasCdmaPrlPreference;

/**
 * QmiNasRoamingPreference:
 * @QMI_NAS_ROAMING_PREFERENCE_OFF: Only non-roaming networks.
 * @QMI_NAS_ROAMING_PREFERENCE_NOT_OFF: Only roaming networks.
 * @QMI_NAS_ROAMING_PREFERENCE_NOT_FLASHING: Only non-roaming networks or not flashing.
 * @QMI_NAS_ROAMING_PREFERENCE_ANY: Don't filter by roaming when acquiring networks.
 *
 * Roaming preference.
 */
typedef enum {
    QMI_NAS_ROAMING_PREFERENCE_OFF          = 0x01,
    QMI_NAS_ROAMING_PREFERENCE_NOT_OFF      = 0x02,
    QMI_NAS_ROAMING_PREFERENCE_NOT_FLASHING = 0x03,
    QMI_NAS_ROAMING_PREFERENCE_ANY          = 0xFF
} QmiNasRoamingPreference;

/**
 * QmiNasNetworkSelectionPreference:
 * @QMI_NAS_NETWORK_SELECTION_PREFERENCE_AUTOMATIC: Automatic.
 * @QMI_NAS_NETWORK_SELECTION_PREFERENCE_MANUAL: Manual.
 *
 * Network selection preference.
 */
typedef enum {
    QMI_NAS_NETWORK_SELECTION_PREFERENCE_AUTOMATIC = 0x00,
    QMI_NAS_NETWORK_SELECTION_PREFERENCE_MANUAL    = 0x01
} QmiNasNetworkSelectionPreference;

/**
 * QmiNasChangeDuration:
 * @QMI_NAS_CHANGE_DURATION_PERMANENT: Permanent.
 * @QMI_NAS_CHANGE_DURATION_POWER_CYCLE: Until the next power cycle.
 *
 * Duration of the change setting.
 */
typedef enum {
    QMI_NAS_CHANGE_DURATION_POWER_CYCLE = 0x00,
    QMI_NAS_CHANGE_DURATION_PERMANENT   = 0x01
} QmiNasChangeDuration;

/**
 * QmiNasServiceDomainPreference:
 * @QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_ONLY: Circuit-switched only.
 * @QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ONLY: Packet-switched only.
 * @QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_PS: Circuit-switched and packet-switched.
 * @QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH: Packet-switched attach.
 * @QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_DETACH:Packet-switched dettach.
 *
 * Service domain preference.
 */
typedef enum {
    QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_ONLY   = 0x00,
    QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ONLY   = 0x01,
    QMI_NAS_SERVICE_DOMAIN_PREFERENCE_CS_PS     = 0x02,
    QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_ATTACH = 0x03,
    QMI_NAS_SERVICE_DOMAIN_PREFERENCE_PS_DETACH = 0x04,
} QmiNasServiceDomainPreference;

/**
 * QmiNasGsmWcdmaAcquisitionOrderPreference:
 * @QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_AUTOMATIC: Automatic.
 * @QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_GSM: GSM first, then WCDMA.
 * @QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_WCDMA: WCDMA first, then GSM.
 *
 * GSM/WCDMA acquisition order preference.
 */
typedef enum {
    QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_AUTOMATIC = 0x00,
    QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_GSM       = 0x01,
    QMI_NAS_GSM_WCDMA_ACQUISITION_ORDER_PREFERENCE_WCDMA     = 0x02
} QmiNasGsmWcdmaAcquisitionOrderPreference;

/**
 * QmiNasTdScdmaBandPreference:
 * @QMI_NAS_TD_SCDMA_BAND_PREFERENCE_A: Band A.
 * @QMI_NAS_TD_SCDMA_BAND_PREFERENCE_B: Band B.
 * @QMI_NAS_TD_SCDMA_BAND_PREFERENCE_C: Band C.
 * @QMI_NAS_TD_SCDMA_BAND_PREFERENCE_D: Band D.
 * @QMI_NAS_TD_SCDMA_BAND_PREFERENCE_E: Band E.
 * @QMI_NAS_TD_SCDMA_BAND_PREFERENCE_F: Band F.
 *
 * Flags to specify TD-SCDMA-specific frequency band preferences.
 */
typedef enum {
    QMI_NAS_TD_SCDMA_BAND_PREFERENCE_A = 1 << 0,
    QMI_NAS_TD_SCDMA_BAND_PREFERENCE_B = 1 << 1,
    QMI_NAS_TD_SCDMA_BAND_PREFERENCE_C = 1 << 2,
    QMI_NAS_TD_SCDMA_BAND_PREFERENCE_D = 1 << 3,
    QMI_NAS_TD_SCDMA_BAND_PREFERENCE_E = 1 << 4,
    QMI_NAS_TD_SCDMA_BAND_PREFERENCE_F = 1 << 5
} QmiNasTdScdmaBandPreference;

/*****************************************************************************/
/* Helper enums for the 'QMI NAS Get System Info' request/response */

/**
 * QmiNasRoamingStatus:
 * @QMI_NAS_ROAMING_STATUS_OFF: Off.
 * @QMI_NAS_ROAMING_STATUS_ON: On.
 * @QMI_NAS_ROAMING_STATUS_BLINK: Blinking.
 * @QMI_NAS_ROAMING_STATUS_OUT_OF_NEIGHBORHOOD: Out of neighborhood.
 * @QMI_NAS_ROAMING_STATUS_OUT_OF_BUILDING: Out of building.
 * @QMI_NAS_ROAMING_STATUS_PREFERRED_SYSTEM: Preferred system.
 * @QMI_NAS_ROAMING_STATUS_AVAILABLE_SYSTEM: Available system.
 * @QMI_NAS_ROAMING_STATUS_ALLIANCE_PARTNER: Alliance partner.
 * @QMI_NAS_ROAMING_STATUS_PREMIUM_PARTNER: Premium partner.
 * @QMI_NAS_ROAMING_STATUS_FULL_SERVICE: Full service.
 * @QMI_NAS_ROAMING_STATUS_PARTIAL_SERVICE: Partial service.
 * @QMI_NAS_ROAMING_STATUS_BANNER_ON: Banner on.
 * @QMI_NAS_ROAMING_STATUS_BANNER_OFF: Banner off.
*/
typedef enum {
    QMI_NAS_ROAMING_STATUS_OFF                 = 0x00,
    QMI_NAS_ROAMING_STATUS_ON                  = 0x01,
    /* Next ones only for 3GPP2 */
    QMI_NAS_ROAMING_STATUS_BLINK               = 0x02,
    QMI_NAS_ROAMING_STATUS_OUT_OF_NEIGHBORHOOD = 0x03,
    QMI_NAS_ROAMING_STATUS_OUT_OF_BUILDING     = 0x04,
    QMI_NAS_ROAMING_STATUS_PREFERRED_SYSTEM    = 0x05,
    QMI_NAS_ROAMING_STATUS_AVAILABLE_SYSTEM    = 0x06,
    QMI_NAS_ROAMING_STATUS_ALLIANCE_PARTNER    = 0x07,
    QMI_NAS_ROAMING_STATUS_PREMIUM_PARTNER     = 0x08,
    QMI_NAS_ROAMING_STATUS_FULL_SERVICE        = 0x09,
    QMI_NAS_ROAMING_STATUS_PARTIAL_SERVICE     = 0x0A,
    QMI_NAS_ROAMING_STATUS_BANNER_ON           = 0x0B,
    QMI_NAS_ROAMING_STATUS_BANNER_OFF          = 0x0C
} QmiNasRoamingStatus;

/**
 * QmiNasHdrProtocolRevision:
 * @QMI_NAS_HDR_PROTOCOL_REVISION_NONE: None.
 * @QMI_NAS_HDR_PROTOCOL_REVISION_REL_0: HDR Rel 0.
 * @QMI_NAS_HDR_PROTOCOL_REVISION_REL_A: HDR Rel A.
 * @QMI_NAS_HDR_PROTOCOL_REVISION_REL_B: HDR Rel B.
 *
 * HDR protocol revision.
 */
typedef enum {
    QMI_NAS_HDR_PROTOCOL_REVISION_NONE  = 0x00,
    QMI_NAS_HDR_PROTOCOL_REVISION_REL_0 = 0x01,
    QMI_NAS_HDR_PROTOCOL_REVISION_REL_A = 0x02,
    QMI_NAS_HDR_PROTOCOL_REVISION_REL_B = 0x03
} QmiNasHdrProtocolRevision;

/**
 * QmiNasWcdmaHsService:
 * @QMI_NAS_WCDMA_HS_SERVICE_HSDPA_HSUPA_UNSUPPORTED: HSDPA and HSUPA not supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_HSDPA_SUPPORTED: HSDPA supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_HSUPA_SUPPORTED: HSUPA supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_HSDPA_HSUPA_SUPPORTED: HSDPA and HSUPA supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_HSDPA_PLUS_SUPPORTED: HSDPA+ supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_HSDPA_PLUS_HSUPA_SUPPORTED: HSDPA+ and HSUPA supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_DC_HSDPA_PLUS_SUPPORTED: DC-HSDPA+ supported.
 * @QMI_NAS_WCDMA_HS_SERVICE_DC_HSDPA_PLUS_HSUPA_SUPPORTED: DC-HSDPA+ and HSUPA supported.
 * Call status on high speed.
 */
typedef enum {
    QMI_NAS_WCDMA_HS_SERVICE_HSDPA_HSUPA_UNSUPPORTED       = 0x00,
    QMI_NAS_WCDMA_HS_SERVICE_HSDPA_SUPPORTED               = 0x01,
    QMI_NAS_WCDMA_HS_SERVICE_HSUPA_SUPPORTED               = 0x02,
    QMI_NAS_WCDMA_HS_SERVICE_HSDPA_HSUPA_SUPPORTED         = 0x03,
    QMI_NAS_WCDMA_HS_SERVICE_HSDPA_PLUS_SUPPORTED          = 0x04,
    QMI_NAS_WCDMA_HS_SERVICE_HSDPA_PLUS_HSUPA_SUPPORTED    = 0x05,
    QMI_NAS_WCDMA_HS_SERVICE_DC_HSDPA_PLUS_SUPPORTED       = 0x06,
    QMI_NAS_WCDMA_HS_SERVICE_DC_HSDPA_PLUS_HSUPA_SUPPORTED = 0x07
} QmiNasWcdmaHsService;

/**
 * QmiNasCellBroadcastCapability:
 * @QMI_NAS_CELL_BROADCAST_CAPABILITY_UNKNOWN: Unknown.
 * @QMI_NAS_CELL_BROADCAST_CAPABILITY_OFF: Cell broadcast not supported.
 * @QMI_NAS_CELL_BROADCAST_CAPABILITY_ON: Cell broadcast supported.
 *
 * Cell broadcast support.
 */
typedef enum {
    QMI_NAS_CELL_BROADCAST_CAPABILITY_UNKNOWN = 0x00,
    QMI_NAS_CELL_BROADCAST_CAPABILITY_OFF     = 0x01,
    QMI_NAS_CELL_BROADCAST_CAPABILITY_ON      = 0x02
} QmiNasCellBroadcastCapability;

/**
 * QmiNasSimRejectState:
 * @QMI_NAS_SIM_REJECT_STATE_SIM_UNAVAILABLE: SIM not available.
 * @QMI_NAS_SIM_REJECT_STATE_SIM_VAILABLE: SIM available.
 * @QMI_NAS_SIM_REJECT_STATE_SIM_CS_INVALID: SIM invalid for circuit-switched connections.
 * @QMI_NAS_SIM_REJECT_STATE_SIM_PS_INVALID: SIM invalid for packet-switched connections.
 * @QMI_NAS_SIM_REJECT_STATE_SIM_CS_PS_INVALID: SIM invalid for circuit-switched and packet-switched connections.
 *
 * Reject information of the SIM.
 */
typedef enum {
    QMI_NAS_SIM_REJECT_STATE_SIM_UNAVAILABLE   = 0,
    QMI_NAS_SIM_REJECT_STATE_SIM_VAILABLE      = 1,
    QMI_NAS_SIM_REJECT_STATE_SIM_CS_INVALID    = 2,
    QMI_NAS_SIM_REJECT_STATE_SIM_PS_INVALID    = 3,
    QMI_NAS_SIM_REJECT_STATE_SIM_CS_PS_INVALID = 4
} QmiNasSimRejectState;

/**
 * QmiNasCdmaPilotType:
 * @QMI_NAS_CDMA_PILOT_TYPE_ACTIVE: the pilot is part of the active set.
 * @QMI_NAS_CDMA_PILOT_TYPE_NEIGHBOR: the pilot is part of the neighbor set.
 *
 * The pilot set the pilot belongs to.
 */
typedef enum {
    QMI_NAS_CDMA_PILOT_TYPE_ACTIVE   = 0,
    QMI_NAS_CDMA_PILOT_TYPE_NEIGHBOR = 1,
} QmiNasCdmaPilotType;

#endif /* _LIBQMI_GLIB_QMI_ENUMS_NAS_H_ */
