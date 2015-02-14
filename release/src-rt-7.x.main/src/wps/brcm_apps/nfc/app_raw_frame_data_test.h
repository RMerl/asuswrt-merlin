/*****************************************************************************
 **
 **  Name:           app_raw_frame_data_test.h
 **
 **  Description:    NSA generic application: RAW FRAME test data
 **
 **  Copyright (c) 2012, Broadcom Corp., All Rights Reserved.
 **  Broadcom Bluetooth Core. Proprietary and confidential.
 **
 *****************************************************************************/

#ifndef APP_RAW_FRAME_DATA_TEST_H
#define APP_RAW_FRAME_DATA_TEST_H

#define T4T_NDEF_APP_LENGTH		13
#define T4T_SELECT_CC_LENGTH	7
#define T4T_READ_LENGTH			5

#define SEND_DUMMY_MAX			6
#define T4T_RSP_CMD_COMPLETED	0x0090

/*RAW data for command to select the NDEF tag application v2.0 and v1.0*/
UINT8 	t4t_select_ndef_app_v2[T4T_NDEF_APP_LENGTH] = { 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00 };
UINT8 	t4t_select_ndef_app_v1[T4T_NDEF_APP_LENGTH-1] = { 0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00 };

/*RAW data for command to select the Capability Container*/
UINT8 	t4t_select_cc[T4T_SELECT_CC_LENGTH] = { 0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03 };

/*RAW data for command to read binary from the Capability Container*/
UINT8 	t4t_read_cc[T4T_READ_LENGTH] = { 0x00, 0xB0, 0x00, 0x00, 0x0F };

/*RAW data for command to read dummy, just to test CMD - RSP back timing (even if no data are back but just answer not allowed)*/
UINT8 	t4t_read_dummy[T4T_READ_LENGTH] = { 0x00, 0xB0, 0x00, 0x00, 0x01 };

#endif
