/* $Id: Symbian_ua_guiSettingItemListSettings.h 3550 2011-05-05 05:33:27Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef SYMBIAN_UA_GUISETTINGITEMLISTSETTINGS_H
#define SYMBIAN_UA_GUISETTINGITEMLISTSETTINGS_H
			
// [[[ begin generated region: do not modify [Generated Includes]
#include <e32std.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Constants]
const int KEd_registrarMaxLength = 255;
const int KEd_userMaxLength = 255;
const int KEd_passwordMaxLength = 32;
const int KEd_stun_serverMaxLength = 255;
// ]]] end generated region [Generated Constants]

/**
 * @class	TSymbian_ua_guiSettingItemListSettings Symbian_ua_guiSettingItemListSettings.h
 */
class TSymbian_ua_guiSettingItemListSettings
	{
public:
	// construct and destroy
	static TSymbian_ua_guiSettingItemListSettings* NewL();
	void ConstructL();
		
private:
	// constructor
	TSymbian_ua_guiSettingItemListSettings();
	// [[[ begin generated region: do not modify [Generated Accessors]
public:
	TDes& Ed_registrar();
	void SetEd_registrar(const TDesC& aValue);
	TDes& Ed_user();
	void SetEd_user(const TDesC& aValue);
	TDes& Ed_password();
	void SetEd_password(const TDesC& aValue);
	TBool& B_srtp();
	void SetB_srtp(const TBool& aValue);
	TBool& B_ice();
	void SetB_ice(const TBool& aValue);
	TDes& Ed_stun_server();
	void SetEd_stun_server(const TDesC& aValue);
	// ]]] end generated region [Generated Accessors]
	
	// [[[ begin generated region: do not modify [Generated Members]
protected:
	TBuf<KEd_registrarMaxLength> iEd_registrar;
	TBuf<KEd_userMaxLength> iEd_user;
	TBuf<KEd_passwordMaxLength> iEd_password;
	TBool iB_srtp;
	TBool iB_ice;
	TBuf<KEd_stun_serverMaxLength> iEd_stun_server;
	// ]]] end generated region [Generated Members]
	
	};
#endif // SYMBIAN_UA_GUISETTINGITEMLISTSETTINGS_H
