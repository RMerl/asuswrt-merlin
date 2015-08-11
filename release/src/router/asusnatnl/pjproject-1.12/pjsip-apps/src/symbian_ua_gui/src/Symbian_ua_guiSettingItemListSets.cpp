/* $Id: Symbian_ua_guiSettingItemListSets.cpp 3550 2011-05-05 05:33:27Z nanang $ */
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
/**
 *	Generated helper class which manages the settings contained 
 *	in 'symbian_ua_guiSettingItemList'.  Each CAknSettingItem maintains
 *	a reference to data in this class so that changes in the setting
 *	item list can be synchronized with this storage.
 */
 
// [[[ begin generated region: do not modify [Generated Includes]
#include <e32base.h>
#include <stringloader.h>
#include <barsread.h>
#include <symbian_ua_gui.rsg>
#include "Symbian_ua_guiSettingItemListSettings.h"
// ]]] end generated region [Generated Includes]

/**
 * C/C++ constructor for settings data, cannot throw
 */
TSymbian_ua_guiSettingItemListSettings::TSymbian_ua_guiSettingItemListSettings()
	{
	}

/**
 * Two-phase constructor for settings data
 */
TSymbian_ua_guiSettingItemListSettings* TSymbian_ua_guiSettingItemListSettings::NewL()
	{
	TSymbian_ua_guiSettingItemListSettings* data = new( ELeave ) TSymbian_ua_guiSettingItemListSettings;
	CleanupStack::PushL( data );
	data->ConstructL();
	CleanupStack::Pop( data );
	return data;
	}
	
/**
 *	Second phase for initializing settings data
 */
void TSymbian_ua_guiSettingItemListSettings::ConstructL()
	{
	// [[[ begin generated region: do not modify [Generated Initializers]
		{
		HBufC* text = StringLoader::LoadLC( R_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_ED_REGISTRAR );
		SetEd_registrar( text->Des() );
		CleanupStack::PopAndDestroy( text );
		}
		{
		HBufC* text = StringLoader::LoadLC( R_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_ED_USER );
		SetEd_user( text->Des() );
		CleanupStack::PopAndDestroy( text );
		}
	SetB_srtp( 0 );
	SetB_ice( 0 );
		{
		HBufC* text = StringLoader::LoadLC( R_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_ED_STUN_SERVER );
		SetEd_stun_server( text->Des() );
		CleanupStack::PopAndDestroy( text );
		}
	// ]]] end generated region [Generated Initializers]
	
	}
	
// [[[ begin generated region: do not modify [Generated Contents]
TDes& TSymbian_ua_guiSettingItemListSettings::Ed_registrar()
	{
	return iEd_registrar;
	}

void TSymbian_ua_guiSettingItemListSettings::SetEd_registrar(const TDesC& aValue)
	{
	if ( aValue.Length() < KEd_registrarMaxLength)
		iEd_registrar.Copy( aValue );
	else
		iEd_registrar.Copy( aValue.Ptr(), KEd_registrarMaxLength);
	}

TDes& TSymbian_ua_guiSettingItemListSettings::Ed_user()
	{
	return iEd_user;
	}

void TSymbian_ua_guiSettingItemListSettings::SetEd_user(const TDesC& aValue)
	{
	if ( aValue.Length() < KEd_userMaxLength)
		iEd_user.Copy( aValue );
	else
		iEd_user.Copy( aValue.Ptr(), KEd_userMaxLength);
	}

TDes& TSymbian_ua_guiSettingItemListSettings::Ed_password()
	{
	return iEd_password;
	}

void TSymbian_ua_guiSettingItemListSettings::SetEd_password(const TDesC& aValue)
	{
	if ( aValue.Length() < KEd_passwordMaxLength)
		iEd_password.Copy( aValue );
	else
		iEd_password.Copy( aValue.Ptr(), KEd_passwordMaxLength);
	}

TBool& TSymbian_ua_guiSettingItemListSettings::B_srtp()
	{
	return iB_srtp;
	}

void TSymbian_ua_guiSettingItemListSettings::SetB_srtp(const TBool& aValue)
	{
	iB_srtp = aValue;
	}

TBool& TSymbian_ua_guiSettingItemListSettings::B_ice()
	{
	return iB_ice;
	}

void TSymbian_ua_guiSettingItemListSettings::SetB_ice(const TBool& aValue)
	{
	iB_ice = aValue;
	}

TDes& TSymbian_ua_guiSettingItemListSettings::Ed_stun_server()
	{
	return iEd_stun_server;
	}

void TSymbian_ua_guiSettingItemListSettings::SetEd_stun_server(const TDesC& aValue)
	{
	if ( aValue.Length() < KEd_stun_serverMaxLength)
		iEd_stun_server.Copy( aValue );
	else
		iEd_stun_server.Copy( aValue.Ptr(), KEd_stun_serverMaxLength);
	}

// ]]] end generated region [Generated Contents]

