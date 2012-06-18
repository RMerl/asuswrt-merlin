// RmvTabs.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#ifndef WIN32
#include <stdio.h> 
#include <wchar.h> 
#include <locale.h> 
#include <string.h> 
#define WCHAR wchar_t
#define MAX_PATH 256
#define BOOL bool
#define FALSE false
#define TRUE true
#define CString CStdString
#include "StdString.h"
#endif
#include "textfile.h"
#include "RmvTabs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object


#ifdef WIN32
CWinApp theApp;
using namespace std;
int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
#else
int main(int argc, char* argv[])
#endif
{
	int nRetCode = 0;

#ifdef WIN32
	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		// TODO: code your application's behavior here.
	}
#endif	
	
	if (argc != 2)
	{
		wprintf(L"wrong parameter. for ex, %s inplace_src.asp\r\n",argv[0]);
		return 0;
	}
	
	CTextFileRead myfileDup(argv[1]);
    argv[1];
#ifdef WIN32    
    WCHAR inplace_dst[512];
    wcscpy(inplace_dst,argv[1]);
    wcscat(inplace_dst,L".2.html");
#else
    char inplace_dst[512];
    strcpy(inplace_dst,argv[1]);
    strcat(inplace_dst,".new");
#endif        
	CTextFileWrite myfileOut(inplace_dst, CTextFileWrite::UTF_8);
	if (myfileDup.IsOpen() == FALSE || myfileOut.IsOpen() == FALSE)
	{
		wprintf(L"File not found!");
		return 0;
	}
	
	// duplicate the file first
	
	while(!myfileDup.Eof())
	{
		CString StrTest;
		myfileDup.ReadLine(StrTest);
		// remove beginning/end space and tabs
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.GetLength() > 1000) wprintf(L"Warning : file has long column %s\n",argv[1]);
		// remove tabs
		// we could not remove tabs inner one line
		// sometimes, it may have javascript error
		// ex>  else (tab) if = elseif
		StrTestTrim.Replace('\t',' ');
		while (1)
		{
		    if (StrTestTrim.Find(L"  ") == -1)
		    {
		        break;
		    }
            StrTestTrim.Replace(L"  ",L" ");		    
		}
		// remove line starting //		
		// remove empty lines
		if (StrTestTrim == L"" || StrTestTrim.Left(2) == L"//")
		{
		}
		else
		{
	        myfileOut << StrTestTrim;
	        //myfileOut.WriteEndl();
	        myfileOut << '\n';
	    }
	}
	
	myfileDup.Close();	
	myfileOut.Close();
	
#ifndef WIN32	
// inplace file handling for rename and remove
	remove(argv[1]);
	rename(inplace_dst,argv[1]);
#endif

	return nRetCode;
}
