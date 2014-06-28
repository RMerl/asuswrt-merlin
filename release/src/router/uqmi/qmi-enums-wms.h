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

#ifndef _LIBQMI_GLIB_QMI_ENUMS_WMS_H_
#define _LIBQMI_GLIB_QMI_ENUMS_WMS_H_

/**
 * SECTION: qmi-enums-wms
 * @title: WMS enumerations and flags
 *
 * This section defines enumerations and flags used in the WMS service
 * interface.
 */

/*****************************************************************************/
/* Helper enums for the 'QMI WMS Event Report' indication */

/**
 * QmiWmsStorageType:
 * @QMI_WMS_STORAGE_TYPE_UIM: Message stored in UIM.
 * @QMI_WMS_STORAGE_TYPE_NV: Message stored in non-volatile memory.
 * @QMI_WMS_STORAGE_TYPE_NONE: None.
 *
 * Type of messaging storage
 */
typedef enum {
    QMI_WMS_STORAGE_TYPE_UIM  = 0x00,
    QMI_WMS_STORAGE_TYPE_NV   = 0x01,
    QMI_WMS_STORAGE_TYPE_NONE = 0xFF
} QmiWmsStorageType;

/**
 * QmiWmsAckIndicator:
 * @QMI_WMS_ACK_INDICATOR_SEND: ACK needs to be sent.
 * @QMI_WMS_ACK_INDICATOR_DO_NOT_SEND: ACK doesn't need to be sent.
 *
 * Indication of whether ACK needs to be sent or not.
 */
typedef enum {
    QMI_WMS_ACK_INDICATOR_SEND        = 0x00,
    QMI_WMS_ACK_INDICATOR_DO_NOT_SEND = 0x01
} QmiWmsAckIndicator;

/**
 * QmiWmsMessageFormat:
 * @QMI_WMS_MESSAGE_FORMAT_CDMA: CDMA message.
 * @QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_POINT_TO_POINT: Point-to-point 3GPP message.
 * @QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_BROADCAST: Broadcast 3GPP message.
 * @QMI_WMS_MESSAGE_FORMAT_MWI: Message Waiting Indicator.
 *
 * Type of message.
 */
typedef enum {
    QMI_WMS_MESSAGE_FORMAT_CDMA                     = 0x00,
    QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_POINT_TO_POINT = 0x06,
    QMI_WMS_MESSAGE_FORMAT_GSM_WCDMA_BROADCAST      = 0x07,
    QMI_WMS_MESSAGE_FORMAT_MWI                      = 0x08
} QmiWmsMessageFormat;

/**
 * QmiWmsMessageMode:
 * @QMI_WMS_MESSAGE_MODE_CDMA: Message sent using 3GPP2 technologies.
 * @QMI_WMS_MESSAGE_MODE_GSM_WCDMA: Message sent using 3GPP technologies.
 *
 * Message mode.
 */
typedef enum {
    QMI_WMS_MESSAGE_MODE_CDMA      = 0x00,
    QMI_WMS_MESSAGE_MODE_GSM_WCDMA = 0x01
} QmiWmsMessageMode;

/**
 * QmiWmsNotificationType:
 * @QMI_WMS_NOTIFICATION_TYPE_PRIMARY: Primary.
 * @QMI_WMS_NOTIFICATION_TYPE_SECONDARY_GSM: Secondary GSM.
 * @QMI_WMS_NOTIFICATION_TYPE_SECONDARY_UMTS: Secondary UMTS.
 *
 * Type of notification.
 */
typedef enum {
    QMI_WMS_NOTIFICATION_TYPE_PRIMARY        = 0x00,
    QMI_WMS_NOTIFICATION_TYPE_SECONDARY_GSM  = 0x01,
    QMI_WMS_NOTIFICATION_TYPE_SECONDARY_UMTS = 0x02
} QmiWmsNotificationType;

/*****************************************************************************/
/* Helper enums for the 'QMI WMS Raw Send' request/response */

/**
 * QmiWmsCdmaServiceOption:
 * @QMI_WMS_CDMA_SERVICE_OPTION_AUTO: Automatic selection of service option.
 * @QMI_WMS_CDMA_SERVICE_OPTION_6: Use service option 6.
 * @QMI_WMS_CDMA_SERVICE_OPTION_14: Use service option 14.
 *
 * CDMA service option selection.
 */
typedef enum {
    QMI_WMS_CDMA_SERVICE_OPTION_AUTO = 0x00,
    QMI_WMS_CDMA_SERVICE_OPTION_6    = 0x06,
    QMI_WMS_CDMA_SERVICE_OPTION_14   = 0x0E
} QmiWmsCdmaServiceOption;

/**
 * QmiWmsCdmaCauseCode:
 * @QMI_WDS_CDMA_CAUSE_CODE_NETWORK_ADDRESS_VACANT: Address is valid but not yet allocated.
 * @QMI_WDS_CDMA_CAUSE_CODE_NETWORK_ADDRESS_TRANSLATION_FAILURE: Address is invalid.
 * @QMI_WDS_CDMA_CAUSE_CODE_NETWORK_RESOURCE_SHORTAGE: Network resource shortage.
 * @QMI_WDS_CDMA_CAUSE_CODE_NETWORK_FAILURE: Network failed.
 * @QMI_WDS_CDMA_CAUSE_CODE_NETWORK_INVALID_TELESERVICE_ID: SMS teleservice ID is invalid.
 * @QMI_WDS_CDMA_CAUSE_CODE_NETWORK_OTHER: Other network error.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_NO_PAGE_RESPONSE: No page response from destination.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_BUSY: Destination is busy.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_NO_ACK: No acknowledge from destination.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_RESOURCE_SHORTAGE: Destination resource shortage.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_SMS_DELIVERY_POSTPONED: SMS deliver postponed.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_OUT_OF_SERVICE: Destination out of service.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_NOT_AT_ADDRESS: Destination not at address.
 * @QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_OTHER: Other destination error.
 * @QMI_WDS_CDMA_CAUSE_CODE_RADIO_INTERFACE_RESOURCE_SHORTAGE: Radio interface resource shortage.
 * @QMI_WDS_CDMA_CAUSE_CODE_RADIO_INTERFACE_INCOMPATIBILITY: Radio interface incompatibility.
 * @QMI_WDS_CDMA_CAUSE_CODE_RADIO_INTERFACE_OTHER: Other radio interface error.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_ENCODING: Encoding error.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SMS_ORIGIN_DENIED: SMS origin denied.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SMS_DESTINATION_DENIED: SMS destination denied.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED: Supplementary service not supported.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SMS_NOT_SUPPORTED: SMS not supported.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_MISSING_EXPECTED_PARAMETER: Missing optional expected parameter.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_MISSING_MANDATORY_PARAMETER: Missing mandatory parameter.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_UNRECOGNIZED_PARAMETER_VALUE: Unrecognized parameter value.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_UNEXPECTED_PARAMETER_VALUE: Unexpected parameter value.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_USER_DATA_SIZE_ERROR: user data size error.
 * @QMI_WDS_CDMA_CAUSE_CODE_GENERAL_OTHER: Other general error.
 *
 * Cause codes when failed to send an SMS in CDMA.
 */
typedef enum {
    /* Network errors */
    QMI_WDS_CDMA_CAUSE_CODE_NETWORK_ADDRESS_VACANT              = 0x00,
    QMI_WDS_CDMA_CAUSE_CODE_NETWORK_ADDRESS_TRANSLATION_FAILURE = 0x01,
    QMI_WDS_CDMA_CAUSE_CODE_NETWORK_RESOURCE_SHORTAGE           = 0x02,
    QMI_WDS_CDMA_CAUSE_CODE_NETWORK_FAILURE                     = 0x03,
    QMI_WDS_CDMA_CAUSE_CODE_NETWORK_INVALID_TELESERVICE_ID      = 0x04,
    QMI_WDS_CDMA_CAUSE_CODE_NETWORK_OTHER                       = 0x05,

    /* Destination errors */
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_NO_PAGE_RESPONSE       = 0x20,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_BUSY                   = 0x21,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_NO_ACK                 = 0x22,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_RESOURCE_SHORTAGE      = 0x23,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_SMS_DELIVERY_POSTPONED = 0x24,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_OUT_OF_SERVICE         = 0x25,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_NOT_AT_ADDRESS         = 0x26,
    QMI_WDS_CDMA_CAUSE_CODE_DESTINATION_OTHER                  = 0x27,

    /* Radio Interface errors */
    QMI_WDS_CDMA_CAUSE_CODE_RADIO_INTERFACE_RESOURCE_SHORTAGE = 0x40,
    QMI_WDS_CDMA_CAUSE_CODE_RADIO_INTERFACE_INCOMPATIBILITY   = 0x41,
    QMI_WDS_CDMA_CAUSE_CODE_RADIO_INTERFACE_OTHER             = 0x42,

    /* General errors */
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_ENCODING                            = 0x60,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SMS_ORIGIN_DENIED                   = 0x61,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SMS_DESTINATION_DENIED              = 0x62,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SUPPLEMENTARY_SERVICE_NOT_SUPPORTED = 0x63,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_SMS_NOT_SUPPORTED                   = 0x64,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_MISSING_EXPECTED_PARAMETER          = 0x65,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_MISSING_MANDATORY_PARAMETER         = 0x66,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_UNRECOGNIZED_PARAMETER_VALUE        = 0x67,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_UNEXPECTED_PARAMETER_VALUE          = 0x68,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_USER_DATA_SIZE_ERROR                = 0x69,
    QMI_WDS_CDMA_CAUSE_CODE_GENERAL_OTHER                               = 0x6A
} QmiWmsCdmaCauseCode;

/**
 * QmiWmsCdmaErrorClass:
 * @QMI_WMS_CDMA_ERROR_CLASS_TEMPORARY: Temporary error.
 * @QMI_WMS_CDMA_ERROR_CLASS_PERMANENT: Permanent error.
 *
 * Error class when failed to send an SMS in CDMA.
 */
typedef enum {
    QMI_WMS_CDMA_ERROR_CLASS_TEMPORARY = 0x00,
    QMI_WMS_CDMA_ERROR_CLASS_PERMANENT = 0x01
} QmiWmsCdmaErrorClass;

/**
 * QmiWmsGsmUmtsRpCause:
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_UNASSIGNED_NUMBER: Unassigned number.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_OPERATOR_DETERMINED_BARRING: Operator determined barring.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_CALL_BARRED: Call barred.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_RESERVED: Reserved.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_SMS_TRANSFER_REJECTED: SMS transfer rejected.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_MEMORY_CAPACITY_EXCEEDED: Memory capacity exceeded.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_DESTINATION_OUT_OF_ORDER: Destination out of order.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_UNIDENTIFIED_SUBSCRIBER: Unidentified subscriber.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_FACILITY_REJECTED: Facility rejected.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_UNKNOWN_SUBSCRIBER: Unknown subscriber.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_NETWORK_OUF_OF_ORDER: Network out of order.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_TEMPORARY_FAILURE: Temporary failure.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_CONGESTION: Congestion.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_RESOURCES_UNAVAILABLE: Resources unavailable.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_FACILITY_NOT_SUBSCRIBED: Facility not subscribed.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_FACILITY_NOT_IMPLEMENTED: Facility not implemented.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_INVALID_SMS_TRANSFER_REFERENCE_VALUE: Invalid SMS transfer reference value.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE: Semantically incorrect message.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_INVALID_MANDATORY_INFO: Invalid mandatory info.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_MESSAGE_TYPE_NOT_IMPLEMENTED: Message type not implemented.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_MESSAGE_NOT_COMPATIBLE_WITH_SMS: Message not compatible with SMS.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_INFORMATION_ELEMENT_NOT_IMPLEMENTED: Information element not implemented.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_PROTOCOL_ERROR: Protocol error.
 * @QMI_WMS_GSM_UMTS_RP_CAUSE_INTERWORKING: Interworking error.
 *
 * RP cause codes when failed to send an SMS in GSM/WCDMA.
 */
typedef enum {
    QMI_WMS_GSM_UMTS_RP_CAUSE_UNASSIGNED_NUMBER                    = 0x01,
    QMI_WMS_GSM_UMTS_RP_CAUSE_OPERATOR_DETERMINED_BARRING          = 0x08,
    QMI_WMS_GSM_UMTS_RP_CAUSE_CALL_BARRED                          = 0x0A,
    QMI_WMS_GSM_UMTS_RP_CAUSE_RESERVED                             = 0x0B,
    QMI_WMS_GSM_UMTS_RP_CAUSE_SMS_TRANSFER_REJECTED                = 0x15,
    QMI_WMS_GSM_UMTS_RP_CAUSE_MEMORY_CAPACITY_EXCEEDED             = 0x16,
    QMI_WMS_GSM_UMTS_RP_CAUSE_DESTINATION_OUT_OF_ORDER             = 0x1B,
    QMI_WMS_GSM_UMTS_RP_CAUSE_UNIDENTIFIED_SUBSCRIBER              = 0x1C,
    QMI_WMS_GSM_UMTS_RP_CAUSE_FACILITY_REJECTED                    = 0x1D,
    QMI_WMS_GSM_UMTS_RP_CAUSE_UNKNOWN_SUBSCRIBER                   = 0x1E,
    QMI_WMS_GSM_UMTS_RP_CAUSE_NETWORK_OUF_OF_ORDER                 = 0x20,
    QMI_WMS_GSM_UMTS_RP_CAUSE_TEMPORARY_FAILURE                    = 0x21,
    QMI_WMS_GSM_UMTS_RP_CAUSE_CONGESTION                           = 0x2A,
    QMI_WMS_GSM_UMTS_RP_CAUSE_RESOURCES_UNAVAILABLE                = 0x2F,
    QMI_WMS_GSM_UMTS_RP_CAUSE_FACILITY_NOT_SUBSCRIBED              = 0x32,
    QMI_WMS_GSM_UMTS_RP_CAUSE_FACILITY_NOT_IMPLEMENTED             = 0x45,
    QMI_WMS_GSM_UMTS_RP_CAUSE_INVALID_SMS_TRANSFER_REFERENCE_VALUE = 0x51,
    QMI_WMS_GSM_UMTS_RP_CAUSE_SEMANTICALLY_INCORRECT_MESSAGE       = 0x5F,
    QMI_WMS_GSM_UMTS_RP_CAUSE_INVALID_MANDATORY_INFO               = 0x60,
    QMI_WMS_GSM_UMTS_RP_CAUSE_MESSAGE_TYPE_NOT_IMPLEMENTED         = 0x61,
    QMI_WMS_GSM_UMTS_RP_CAUSE_MESSAGE_NOT_COMPATIBLE_WITH_SMS      = 0x62,
    QMI_WMS_GSM_UMTS_RP_CAUSE_INFORMATION_ELEMENT_NOT_IMPLEMENTED  = 0x63,
    QMI_WMS_GSM_UMTS_RP_CAUSE_PROTOCOL_ERROR                       = 0x6F,
    QMI_WMS_GSM_UMTS_RP_CAUSE_INTERWORKING                         = 0x7F
} QmiWmsGsmUmtsRpCause;

/**
 * QmiWmsGsmUmtsTpCause:
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_TELE_INTERWORKING_NOT_SUPPORTED: Tele interworking not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SHORT_MESSAGE_TYPE_0_NOT_SUPPORTED: Short message type 0 not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SHORT_MESSAGE_CANNOT_BE_REPLACED: Short message cannot be replaced.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_PID_ERROR: Unspecified TP-PID error.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_DCS_NOT_SUPPORTED: Data coding scheme not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_MESSAGE_CLASS_NOT_SUPPORTED: Message class not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_DCS_ERROR: Unspecified data coding scheme error.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_COMMAND_CANNOT_BE_ACTIONED: Command cannot be actioned.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_COMMAND_UNSUPPORTED: Command unsupported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_COMMAND_ERROR: Unspecified command error.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_TPDU_NOT_SUPPORTED: TPDU not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SC_BUSY: SC busy.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_NO_SC_SUBSCRIPTION: No SC subscription.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SC_SYSTEM_FAILURE: SC system failure.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_INVALID_SME_ADDRESS: Invalid SME address.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_DESTINATION_SME_BARRED: Destination SME barred.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SM_REJECTED_OR_DUPLICATE: SM rejected or duplicate.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_VPF_NOT_SUPPORTED: TP-VPF not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_VP_NOT_SUPPORTED: TP-VP not supported.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SIM_SMS_STORAGE_FULL: SIM SMS storage full.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_NO_SMS_STORAGE_CAPABILITY_IN_SIM: No SMS storage capability in SIM.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_MS_ERROR: MS error.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_MEMORY_CAPACITY_EXCEEDED: Memory capacity exceeded.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SIM_APPLICATION_TOOLKIT_BUSY: SIM application toolkit busy.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_SIM_DATA_DOWNLOAD_ERROR: SIM data download error.
 * @QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_ERROR: Unspecified error.
 *
 * RT cause codes when failed to send an SMS in GSM/WCDMA.
 */
typedef enum {
    QMI_WMS_GSM_UMTS_TP_CAUSE_TELE_INTERWORKING_NOT_SUPPORTED    = 0x80,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SHORT_MESSAGE_TYPE_0_NOT_SUPPORTED = 0x81,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SHORT_MESSAGE_CANNOT_BE_REPLACED   = 0x82,
    QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_PID_ERROR              = 0x8F,
    QMI_WMS_GSM_UMTS_TP_CAUSE_DCS_NOT_SUPPORTED                  = 0x90,
    QMI_WMS_GSM_UMTS_TP_CAUSE_MESSAGE_CLASS_NOT_SUPPORTED        = 0x91,
    QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_DCS_ERROR              = 0x9F,
    QMI_WMS_GSM_UMTS_TP_CAUSE_COMMAND_CANNOT_BE_ACTIONED         = 0xA0,
    QMI_WMS_GSM_UMTS_TP_CAUSE_COMMAND_UNSUPPORTED                = 0xA1,
    QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_COMMAND_ERROR          = 0xAF,
    QMI_WMS_GSM_UMTS_TP_CAUSE_TPDU_NOT_SUPPORTED                 = 0xB0,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SC_BUSY                            = 0xC0,
    QMI_WMS_GSM_UMTS_TP_CAUSE_NO_SC_SUBSCRIPTION                 = 0xC1,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SC_SYSTEM_FAILURE                  = 0xC2,
    QMI_WMS_GSM_UMTS_TP_CAUSE_INVALID_SME_ADDRESS                = 0xC3,
    QMI_WMS_GSM_UMTS_TP_CAUSE_DESTINATION_SME_BARRED             = 0xC4,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SM_REJECTED_OR_DUPLICATE           = 0xC5,
    QMI_WMS_GSM_UMTS_TP_CAUSE_VPF_NOT_SUPPORTED                  = 0xC6,
    QMI_WMS_GSM_UMTS_TP_CAUSE_VP_NOT_SUPPORTED                   = 0xC7,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SIM_SMS_STORAGE_FULL               = 0xD0,
    QMI_WMS_GSM_UMTS_TP_CAUSE_NO_SMS_STORAGE_CAPABILITY_IN_SIM   = 0xD1,
    QMI_WMS_GSM_UMTS_TP_CAUSE_MS_ERROR                           = 0xD2,
    QMI_WMS_GSM_UMTS_TP_CAUSE_MEMORY_CAPACITY_EXCEEDED           = 0xD3,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SIM_APPLICATION_TOOLKIT_BUSY       = 0xD4,
    QMI_WMS_GSM_UMTS_TP_CAUSE_SIM_DATA_DOWNLOAD_ERROR            = 0xD5,
    QMI_WMS_GSM_UMTS_TP_CAUSE_UNSPECIFIED_ERROR                  = 0xFF
} QmiWmsGsmUmtsTpCause;

/**
 * QmiWmsMessageDeliveryFailureType:
 * @QMI_WMS_MESSAGE_DELIVERY_FAILURE_TYPE_TEMPORARY: Temporary failure.
 * @QMI_WMS_MESSAGE_DELIVERY_FAILURE_TYPE_PERMANENT: Permanent failure.
 *
 * Type of message delivery failure.
 */
typedef enum {
    QMI_WMS_MESSAGE_DELIVERY_FAILURE_TYPE_TEMPORARY = 0x00,
    QMI_WMS_MESSAGE_DELIVERY_FAILURE_TYPE_PERMANENT = 0x01
} QmiWmsMessageDeliveryFailureType;

/*****************************************************************************/
/* Helper enums for the 'QMI WMS Read Raw' request/response */

/**
 * QmiWmsMessageTagType:
 * @QMI_WMS_MESSAGE_TAG_TYPE_MT_READ: Received SMS, already read.
 * @QMI_WMS_MESSAGE_TAG_TYPE_MT_NOT_READ: Received SMS, not read.
 * @QMI_WMS_MESSAGE_TAG_TYPE_MO_SENT: Sent SMS.
 * @QMI_WMS_MESSAGE_TAG_TYPE_MO_NOT_SENT: Not yet sent SMS.
 *
 * Type of message tag.
 */
typedef enum {
    QMI_WMS_MESSAGE_TAG_TYPE_MT_READ     = 0x00,
    QMI_WMS_MESSAGE_TAG_TYPE_MT_NOT_READ = 0x01,
    QMI_WMS_MESSAGE_TAG_TYPE_MO_SENT     = 0x02,
    QMI_WMS_MESSAGE_TAG_TYPE_MO_NOT_SENT = 0x03
} QmiWmsMessageTagType;

/**
 * QmiWmsMessageProtocol:
 * @QMI_WMS_MESSAGE_PROTOCOL_CDMA: CDMA.
 * @QMI_WMS_MESSAGE_PROTOCOL_WCDMA: WCDMA.
 *
 * Type of message protocol.
 */
typedef enum {
    QMI_WMS_MESSAGE_PROTOCOL_CDMA  = 0x00,
    QMI_WMS_MESSAGE_PROTOCOL_WCDMA = 0x01
} QmiWmsMessageProtocol;

/*****************************************************************************/
/* Helper enums for the 'QMI WMS Set Routes' request/response */

/**
 * QmiWmsMessageType:
 * @QMI_WMS_MESSAGE_TYPE_POINT_TO_POINT: Point to point message.
 *
 * Type of message.
 */
typedef enum {
    QMI_WMS_MESSAGE_TYPE_POINT_TO_POINT = 0x00
} QmiWmsMessageType;

/**
 * QmiWmsMessageClass:
 * @QMI_WMS_MESSAGE_CLASS_0: Class 0.
 * @QMI_WMS_MESSAGE_CLASS_1: Class 1.
 * @QMI_WMS_MESSAGE_CLASS_2: Class 2.
 * @QMI_WMS_MESSAGE_CLASS_3: Class 3.
 * @QMI_WMS_MESSAGE_CLASS_NONE: Class none.
 * @QMI_WMS_MESSAGE_CLASS_CDMA: Class CDMA.
 *
 * Message class.
 */
typedef enum {
    QMI_WMS_MESSAGE_CLASS_0    = 0x00,
    QMI_WMS_MESSAGE_CLASS_1    = 0x01,
    QMI_WMS_MESSAGE_CLASS_2    = 0x02,
    QMI_WMS_MESSAGE_CLASS_3    = 0x03,
    QMI_WMS_MESSAGE_CLASS_NONE = 0x04,
    QMI_WMS_MESSAGE_CLASS_CDMA = 0x05
} QmiWmsMessageClass;

/**
 * QmiWmsReceiptAction:
 * @QMI_WMS_RECEIPT_ACTION_DISCARD: Discard message.
 * @QMI_WMS_RECEIPT_ACTION_STORE_AND_NOTIFY: Store and notify to client.
 * @QMI_WMS_RECEIPT_ACTION_TRANSFER_ONLY: Notify to client, which should send back ACK.
 * @QMI_WMS_RECEIPT_ACTION_TRANSFER_AND_ACK: Notify to client and send back ACK.
 * @QMI_WMS_RECEIPT_ACTION_UNKNOWN: Unknown action.
 *
 * Action to perform when a message is received.
 */
typedef enum {
    QMI_WMS_RECEIPT_ACTION_DISCARD          = 0x00,
    QMI_WMS_RECEIPT_ACTION_STORE_AND_NOTIFY = 0x01,
    QMI_WMS_RECEIPT_ACTION_TRANSFER_ONLY    = 0x02,
    QMI_WMS_RECEIPT_ACTION_TRANSFER_AND_ACK = 0x03,
    QMI_WMS_RECEIPT_ACTION_UNKNOWN          = 0xFF
} QmiWmsReceiptAction;

/**
 * QmiWmsTransferIndication:
 * @QMI_WMS_TRANSFER_INDICATION_CLIENT: Status reports transferred to the client.
 *
 * Transfer indication actions.
 */
typedef enum {
    QMI_WMS_TRANSFER_INDICATION_CLIENT = 0x01
} QmiWmsTransferIndication;

#endif /* _LIBQMI_GLIB_QMI_ENUMS_WMS_H_ */
