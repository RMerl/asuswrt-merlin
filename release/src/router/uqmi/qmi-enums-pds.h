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

#ifndef _LIBQMI_GLIB_QMI_ENUMS_PDS_H_
#define _LIBQMI_GLIB_QMI_ENUMS_PDS_H_

/**
 * SECTION: qmi-enums-pds
 * @title: PDS enumerations and flags
 *
 * This section defines enumerations and flags used in the PDS service
 * interface.
 */

/*****************************************************************************/
/* Helper enums for the 'QMI PDS Event Report' indication */

/**
 * QmiPdsOperationMode:
 * @QMI_PDS_OPERATION_MODE_UNKNOWN: Unknown (position not fixed yet).
 * @QMI_PDS_OPERATION_MODE_STANDALONE: Standalone.
 * @QMI_PDS_OPERATION_MODE_MS_BASED: MS based.
 * @QMI_PDS_OPERATION_MODE_MS_ASSISTED: MS assisted.
 *
 * Operation mode used to compute the position.
 */
typedef enum {
    QMI_PDS_OPERATION_MODE_UNKNOWN     = -1,
    QMI_PDS_OPERATION_MODE_STANDALONE  =  0,
    QMI_PDS_OPERATION_MODE_MS_BASED    =  1,
    QMI_PDS_OPERATION_MODE_MS_ASSISTED =  2
} QmiPdsOperationMode;

/**
 * QmiPdsPositionSessionStatus:
 * @QMI_PDS_POSITION_SESSION_STATUS_SUCCESS: Success.
 * @QMI_PDS_POSITION_SESSION_STATUS_IN_PROGRESS: In progress.
 * @QMI_PDS_POSITION_SESSION_STATUS_GENERAL_FAILURE: General failure.
 * @QMI_PDS_POSITION_SESSION_STATUS_TIMEOUT: Timeout.
 * @QMI_PDS_POSITION_SESSION_STATUS_USER_ENDED_SESSION: User ended session.
 * @QMI_PDS_POSITION_SESSION_STATUS_BAD_PARAMETER: Bad parameter.
 * @QMI_PDS_POSITION_SESSION_STATUS_PHONE_OFFLINE: Phone is offline.
 * @QMI_PDS_POSITION_SESSION_STATUS_ENGINE_LOCKED: Engine locked.
 * @QMI_PDS_POSITION_SESSION_STATUS_E911_SESSION_IN_PROGRESS: Emergency call in progress.
 *
 * Status of the positioning session.
 */
typedef enum {
    QMI_PDS_POSITION_SESSION_STATUS_SUCCESS                  = 0x00,
    QMI_PDS_POSITION_SESSION_STATUS_IN_PROGRESS              = 0x01,
    QMI_PDS_POSITION_SESSION_STATUS_GENERAL_FAILURE          = 0x02,
    QMI_PDS_POSITION_SESSION_STATUS_TIMEOUT                  = 0x03,
    QMI_PDS_POSITION_SESSION_STATUS_USER_ENDED_SESSION       = 0x04,
    QMI_PDS_POSITION_SESSION_STATUS_BAD_PARAMETER            = 0x05,
    QMI_PDS_POSITION_SESSION_STATUS_PHONE_OFFLINE            = 0x06,
    QMI_PDS_POSITION_SESSION_STATUS_ENGINE_LOCKED            = 0x07,
    QMI_PDS_POSITION_SESSION_STATUS_E911_SESSION_IN_PROGRESS = 0x08
} QmiPdsPositionSessionStatus;

/**
 * QmiPdsDataValid:
 * @QMI_PDS_DATA_VALID_TIMESTAMP_CALENDAR: Timestamp calendar (GPS time).
 * @QMI_PDS_DATA_VALID_TIMESTAMP_UTC: Timestamp (UTC).
 * @QMI_PDS_DATA_VALID_LEAP_SECONDS: Leap seconds.
 * @QMI_PDS_DATA_VALID_TIME_UNCERTAINTY: Time uncertainty.
 * @QMI_PDS_DATA_VALID_LATITUDE: Latitude.
 * @QMI_PDS_DATA_VALID_LONGITUDE: Longitude.
 * @QMI_PDS_DATA_VALID_ELLIPSOID_ALTITUDE: Ellipsoid altitude.
 * @QMI_PDS_DATA_VALID_MEAN_SEA_LEVEL_ALTITUDE: Mean sea level altitude.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_SPEED: Horizontal speed.
 * @QMI_PDS_DATA_VALID_VERTICAL_SPEED: Vertical speed.
 * @QMI_PDS_DATA_VALID_HEADING: Heading.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_CIRCULAR: Horizontal uncertainty circular.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_ELLIPSE_SEMI_MAJOR: Horizontal uncertainty ellipse semi-major.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_ELLIPSE_SEMI_MINOR: Horizontal uncertainty ellipse semi-minor.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_ELLIPSE_ORIENT_AZIMUTH: Horizontal uncertainty ellipse orient azimuth.
 * @QMI_PDS_DATA_VALID_VERTICAL_UNCERTAINTY: Vertical uncertainty.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_VELOCITY_UNCERTAINTY: Horizontal velocity uncertainty.
 * @QMI_PDS_DATA_VALID_VERTICAL_VELOCITY_UNCERTAINTY: Vertical velocity uncertainty.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_CONFIDENCE: Horizontal confidence.
 * @QMI_PDS_DATA_VALID_POSITION_DOP: Position dillution of precision.
 * @QMI_PDS_DATA_VALID_HORIZONTAL_DOP: Horizontal dillution of precision.
 * @QMI_PDS_DATA_VALID_VERTICAL_DOP: Vertical dillution of precision.
 * @QMI_PDS_DATA_VALID_OPERATING_MODE: Operating mode.
 *
 * Flags to indicate which position data parameters are valid.
 */
typedef enum {
    QMI_PDS_DATA_VALID_TIMESTAMP_CALENDAR      = 1 << 0,
    QMI_PDS_DATA_VALID_TIMESTAMP_UTC           = 1 << 1,
    QMI_PDS_DATA_VALID_LEAP_SECONDS            = 1 << 2,
    QMI_PDS_DATA_VALID_TIME_UNCERTAINTY        = 1 << 3,
    QMI_PDS_DATA_VALID_LATITUDE                = 1 << 4,
    QMI_PDS_DATA_VALID_LONGITUDE               = 1 << 5,
    QMI_PDS_DATA_VALID_ELLIPSOID_ALTITUDE      = 1 << 6,
    QMI_PDS_DATA_VALID_MEAN_SEA_LEVEL_ALTITUDE = 1 << 7,
    QMI_PDS_DATA_VALID_HORIZONTAL_SPEED        = 1 << 8,
    QMI_PDS_DATA_VALID_VERTICAL_SPEED          = 1 << 9,
    QMI_PDS_DATA_VALID_HEADING                 = 1 << 10,
    QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_CIRCULAR               = 1 << 11,
    QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_ELLIPSE_SEMI_MAJOR     = 1 << 12,
    QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_ELLIPSE_SEMI_MINOR     = 1 << 13,
    QMI_PDS_DATA_VALID_HORIZONTAL_UNCERTAINTY_ELLIPSE_ORIENT_AZIMUTH = 1 << 14,
    QMI_PDS_DATA_VALID_VERTICAL_UNCERTAINTY                          = 1 << 15,
    QMI_PDS_DATA_VALID_HORIZONTAL_VELOCITY_UNCERTAINTY               = 1 << 16,
    QMI_PDS_DATA_VALID_VERTICAL_VELOCITY_UNCERTAINTY                 = 1 << 17,
    QMI_PDS_DATA_VALID_HORIZONTAL_CONFIDENCE   = 1 << 18,
    QMI_PDS_DATA_VALID_POSITION_DOP            = 1 << 19,
    QMI_PDS_DATA_VALID_HORIZONTAL_DOP          = 1 << 20,
    QMI_PDS_DATA_VALID_VERTICAL_DOP            = 1 << 21,
    QMI_PDS_DATA_VALID_OPERATING_MODE          = 1 << 22
} QmiPdsDataValid;

/*****************************************************************************/
/* Helper enums for the 'QMI PDS Get GPS Service State' request/response */

/**
 * QmiPdsTrackingSessionState:
 * @QMI_PDS_TRACKING_SESSION_STATE_UNKNOWN: Unknown state.
 * @QMI_PDS_TRACKING_SESSION_STATE_INACTIVE: Session inactive.
 * @QMI_PDS_TRACKING_SESSION_STATE_ACTIVE: Session active.
 *
 * State of the tracking session.
 */
typedef enum {
    QMI_PDS_TRACKING_SESSION_STATE_UNKNOWN  = 0,
    QMI_PDS_TRACKING_SESSION_STATE_INACTIVE = 1,
    QMI_PDS_TRACKING_SESSION_STATE_ACTIVE   = 2
} QmiPdsTrackingSessionState;

#endif /* _LIBQMI_GLIB_QMI_ENUMS_PDS_H_ */
