// PocketPJ.h : main header file for the POCKETPJ application
//

#if !defined(AFX_POCKETPJ_H__D90320F8_01F9_4F5C_8655_13CF2FFDDF48__INCLUDED_)
#define AFX_POCKETPJ_H__D90320F8_01F9_4F5C_8655_13CF2FFDDF48__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CPocketPJApp:
// See PocketPJ.cpp for the implementation of this class
//

class CPocketPJApp : public CWinApp
{
public:
	CPocketPJApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPocketPJApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CPocketPJApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_POCKETPJ_H__D90320F8_01F9_4F5C_8655_13CF2FFDDF48__INCLUDED_)
