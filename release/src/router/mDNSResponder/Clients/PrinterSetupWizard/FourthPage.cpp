/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 1997-2004 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):
    
$Log: FourthPage.cpp,v $
Revision 1.8  2006/08/14 23:24:09  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.7  2005/07/07 17:53:20  shersche
Fix problems associated with the CUPS printer workaround fix.

Revision 1.6  2005/02/08 21:45:06  shersche
<rdar://problem/3947490> Default to Generic PostScript or PCL if unable to match driver

Revision 1.5  2005/01/06 08:17:08  shersche
Display the selected protocol ("Raw", "LPR", "IPP") rather than the port name

Revision 1.4  2004/07/13 20:15:04  shersche
<rdar://problem/3726363> Load large font name from resource
Bug #: 3726363

Revision 1.3  2004/07/12 06:59:03  shersche
<rdar://problem/3723695> Use resource strings for Yes/No
Bug #: 3723695

Revision 1.2  2004/06/26 23:27:12  shersche
support for installing multiple printers of the same name

Revision 1.1  2004/06/18 04:36:57  rpantos
First checked in


*/

#include "stdafx.h"
#include "PrinterSetupWizardApp.h"
#include "PrinterSetupWizardSheet.h"
#include "FourthPage.h"


// CFourthPage dialog

IMPLEMENT_DYNAMIC(CFourthPage, CPropertyPage)
CFourthPage::CFourthPage()
	: CPropertyPage(CFourthPage::IDD),
		m_initialized(false)
{
	CString fontName;

	m_psp.dwFlags &= ~(PSP_HASHELP);
	m_psp.dwFlags |= PSP_DEFAULT|PSP_HIDEHEADER;

	fontName.LoadString(IDS_LARGE_FONT);

	// create the large font
	m_largeFont.CreateFont(-16, 0, 0, 0, 
		FW_BOLD, FALSE, FALSE, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, 
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, fontName);
}

CFourthPage::~CFourthPage()
{
}

void CFourthPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GOODBYE, m_goodbye);
	DDX_Control(pDX, IDC_PRINTER_NAME, m_printerNameCtrl);
	DDX_Control(pDX, IDC_PRINTER_MANUFACTURER, m_printerManufacturerCtrl);
	DDX_Control(pDX, IDC_PRINTER_MODEL, m_printerModelCtrl);
	DDX_Control(pDX, IDC_PRINTER_PROTOCOL, m_printerProtocolCtrl);
	DDX_Control(pDX, IDC_PRINTER_DEFAULT, m_printerDefault);
}


BEGIN_MESSAGE_MAP(CFourthPage, CPropertyPage)
END_MESSAGE_MAP()


// CFourthPage message handlers
OSStatus 
CFourthPage::OnInitPage()
{
	return kNoErr;
}


BOOL
CFourthPage::OnSetActive()
{
	CPrinterSetupWizardSheet	*	psheet;
	CString							goodbyeText;
	Printer						*	printer;
	CString							defaultText;

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );
   
	printer = psheet->GetSelectedPrinter();
	require_quiet( psheet, exit );
	
	psheet->SetWizardButtons(PSWIZB_BACK|PSWIZB_FINISH);

	if (m_initialized == false)
	{
		m_initialized = true;
		OnInitPage();
	}

	m_goodbye.SetFont(&m_largeFont);

	goodbyeText.LoadString(IDS_GOODBYE);
	m_goodbye.SetWindowText(goodbyeText);

	m_printerNameCtrl.SetWindowText( printer->actualName );
	m_printerManufacturerCtrl.SetWindowText ( printer->manufacturer );
	m_printerModelCtrl.SetWindowText ( printer->displayModelName );

	Service * service = printer->services.front();
	require_quiet( service, exit );
	m_printerProtocolCtrl.SetWindowText ( service->protocol );

	if (printer->deflt)
	{
		defaultText.LoadString(IDS_YES);
	}
	else
	{
		defaultText.LoadString(IDS_NO);
	}

	m_printerDefault.SetWindowText ( defaultText );

exit:

	return CPropertyPage::OnSetActive();
}


BOOL
CFourthPage::OnKillActive()
{
	CPrinterSetupWizardSheet * psheet;

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );   
   
	psheet->SetLastPage(this);

exit:

	return CPropertyPage::OnKillActive();
}
