/*
 * Copyright 1998-2008 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2008 S3 Graphics, Inc. All Rights Reserved.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTIES OR REPRESENTATIONS; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef __LCD_H__
#define __LCD_H__

/*Definition TMDS Device ID register*/
#define     VT1631_DEVICE_ID_REG        0x02
#define     VT1631_DEVICE_ID            0x92

#define     VT3271_DEVICE_ID_REG        0x02
#define     VT3271_DEVICE_ID            0x71

/* Definition DVI Panel ID*/
/* Resolution: 640x480,   Channel: single, Dithering: Enable */
#define     LCD_PANEL_ID0_640X480       0x00
/* Resolution: 800x600,   Channel: single, Dithering: Enable */
#define     LCD_PANEL_ID1_800X600       0x01
/* Resolution: 1024x768,  Channel: single, Dithering: Enable */
#define     LCD_PANEL_ID2_1024X768      0x02
/* Resolution: 1280x768,  Channel: single, Dithering: Enable */
#define     LCD_PANEL_ID3_1280X768      0x03
/* Resolution: 1280x1024, Channel: dual,   Dithering: Enable */
#define     LCD_PANEL_ID4_1280X1024     0x04
/* Resolution: 1400x1050, Channel: dual,   Dithering: Enable */
#define     LCD_PANEL_ID5_1400X1050     0x05
/* Resolution: 1600x1200, Channel: dual,   Dithering: Enable */
#define     LCD_PANEL_ID6_1600X1200     0x06
/* Resolution: 1366x768,  Channel: single, Dithering: Disable */
#define     LCD_PANEL_ID7_1366X768      0x07
/* Resolution: 1024x600,  Channel: single, Dithering: Enable*/
#define     LCD_PANEL_ID8_1024X600      0x08
/* Resolution: 1280x800,  Channel: single, Dithering: Enable*/
#define     LCD_PANEL_ID9_1280X800      0x09
/* Resolution: 800x480,   Channel: single, Dithering: Enable*/
#define     LCD_PANEL_IDA_800X480       0x0A
/* Resolution: 1360x768,   Channel: single, Dithering: Disable*/
#define     LCD_PANEL_IDB_1360X768     0x0B
/* Resolution: 480x640,  Channel: single, Dithering: Enable */
#define     LCD_PANEL_IDC_480X640      0x0C
/* Resolution: 1200x900,  Channel: single, Dithering: Disable */
#define     LCD_PANEL_IDD_1200X900      0x0D


extern int viafb_LCD2_ON;
extern int viafb_LCD_ON;
extern int viafb_DVI_ON;

void viafb_disable_lvds_vt1636(struct lvds_setting_information
			 *plvds_setting_info,
			 struct lvds_chip_information *plvds_chip_info);
void viafb_enable_lvds_vt1636(struct lvds_setting_information
			*plvds_setting_info,
			struct lvds_chip_information *plvds_chip_info);
void viafb_lcd_disable(void);
void viafb_lcd_enable(void);
void viafb_init_lcd_size(void);
void viafb_init_lvds_output_interface(struct lvds_chip_information
				*plvds_chip_info,
				struct lvds_setting_information
				*plvds_setting_info);
void viafb_lcd_set_mode(struct crt_mode_table *mode_crt_table,
		  struct lvds_setting_information *plvds_setting_info,
		  struct lvds_chip_information *plvds_chip_info);
int viafb_lvds_trasmitter_identify(void);
void viafb_init_lvds_output_interface(struct lvds_chip_information
				*plvds_chip_info,
				struct lvds_setting_information
				*plvds_setting_info);
bool viafb_lcd_get_mobile_state(bool *mobile);
void viafb_load_crtc_timing(struct display_timing device_timing,
	int set_iga);

#endif /* __LCD_H__ */
