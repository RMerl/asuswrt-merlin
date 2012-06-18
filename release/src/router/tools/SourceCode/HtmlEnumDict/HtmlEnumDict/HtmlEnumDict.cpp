// HtmlEnumDict.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#ifdef WIN32
#define TextFileWrite(a,b) a << (LPTSTR)(LPCTSTR)b
#define wcscpy_x(a, b) wcscpy(a,(LPCTSTR)b)
#else
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
void wcscpy_x(WCHAR* Dst, CString& Src)
{
	WCHAR* Ptr;
	Ptr = Src.GetBuffer(Src.GetLength());
	wcscpy(Dst,Ptr);
	Src.ReleaseBuffer();
}

int wcscmp_x(char* str1, WCHAR* str2)
{
	char tmp[256];
	wcstombs(tmp,str2,256);
	return strcmp(str1,tmp);
}
#define TextFileWrite(a,b) \
{ \
WCHAR* Ptr; \
Ptr = b.GetBuffer(b.GetLength()); \
a << Ptr; \
b.ReleaseBuffer(); \
}
#endif
#include "textfile.h"
#include "HtmlEnumDict.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


typedef struct {
	WCHAR Name[128];
	WCHAR Val[1024];
} DICT_VAR;


DICT_VAR m_DictNameTbl[10240];
int m_NumDictName = 0;

#ifdef WIN32
void LoadDict(WCHAR* fnDict)
#else
void LoadDict(char* fnDict)
#endif
{
	// dict
	CTextFileRead myfile3(fnDict);
	while(!myfile3.Eof())
	{
		CString line3;
		myfile3.ReadLine(line3);
		line3 = line3.Trim();
		if (line3.Left(1) == L"[" && line3.Right(1) == L"]")
		{
			// skip
		}
		else
		{
			WCHAR NameBuf[2048];
			wcscpy_x(NameBuf,line3);
			int BufIdx;
			for (BufIdx=0; BufIdx<sizeof(NameBuf); BufIdx++)
			{
                if (NameBuf[BufIdx]	== 0) break;
				if (NameBuf[BufIdx] == '=')
				{
					NameBuf[BufIdx] = 0;
					wcscpy(m_DictNameTbl[m_NumDictName].Name,NameBuf);
					wcscpy(m_DictNameTbl[m_NumDictName].Val,&NameBuf[BufIdx+1]);
					m_NumDictName++;
					break;
				}
			}
		}
	}
	myfile3.Close();
}

static WCHAR m_NotFoundBufDict[256];

#ifdef WIN32     
static WCHAR m_OutputMsgFile[512];
static WCHAR m_InplaceFn[512];
#else
static char m_OutputMsgFile[512];
static char m_InplaceFn[512];
#endif

WCHAR* DefaultStrDict(WCHAR* name)
{
    FILE* fp;
    char buf[256];
#ifdef WIN32     
    fp = _wfopen(m_OutputMsgFile,L"a+");    
#else
	fp = fopen(m_OutputMsgFile,"a+");    
#endif	
    fwprintf(fp,L"dict no found : %s , %ls\n",m_InplaceFn,name);
    fclose(fp);
    
	wcscpy(m_NotFoundBufDict,L"*** not_found_dict : ");
	wcscat(m_NotFoundBufDict,name);
	wcscat(m_NotFoundBufDict,L"***");
	return(m_NotFoundBufDict);
}

void GetRepName(WCHAR *name, WCHAR *RepName)
{
    int i;
    WCHAR NameBuf[1024];
    WCHAR* pName;
    pName=name;
    for (i=0; i<sizeof(NameBuf) && *pName!='>';) {
        NameBuf[i] = *pName++;
        i++;
    }
    NameBuf[i++] = '>';
    NameBuf[i] = 0;
    wcscpy(RepName,NameBuf);
}

WCHAR* GetDictContent(WCHAR *name)
{
    int i;
    int j;  
    WCHAR* pName;
    WCHAR* pVar;  
    WCHAR NameBuf[128];
	
    pName=name;

    for (i=0; i<sizeof(NameBuf) && *pName!='#';) {
        if (*pName == ' ' || *pName == ';')
        {
            // skip
            pName++;
        }
        else
        {
            NameBuf[i] = *pName++;
            i++;
        }
    }
    NameBuf[i] = 0;

    for (j = 0 ; j < m_NumDictName ; j++) {
        if (wcscmp(m_DictNameTbl[j].Val,NameBuf)==0)
        {
            return m_DictNameTbl[j].Name;
        }

    }
	


    return DefaultStrDict(NameBuf);
}

static WCHAR m_FileBuf[10240];


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
	
	if (argc != 4)
	{
		wprintf(L"wrong parameter. for ex, %s inplace_src.asp enumdict.txt output_dict_no_found.txt\r\n",argv[0]);
		return 0;
	}
	
    LoadDict(argv[2]);
#ifdef WIN32     
    wcscpy(m_InplaceFn,argv[1]);
    wcscpy(m_OutputMsgFile,argv[3]);
#else
    strcpy(m_InplaceFn,argv[1]);
    strcpy(m_OutputMsgFile,argv[3]);
#endif    

	FILE* fp;
#ifdef WIN32     
    fp = _wfopen(m_OutputMsgFile,L"a+");    
#else
	fp = fopen(m_OutputMsgFile,"a+");    
#endif	
    fwprintf(fp,L"Dict no found file...\n");
    fclose(fp);

	
	CTextFileRead myfileDup(argv[1]);
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
        int LineLen = StrTest.GetLength();
		WCHAR* TmpPtr;
		TmpPtr = StrTest.GetBuffer(LineLen);				
		wcscpy(m_FileBuf,TmpPtr);
		StrTest.ReleaseBuffer();
        
        for (int i = 0; i < LineLen; i++)
        {
            if (m_FileBuf[i] == '<') {
                if (m_FileBuf[i+1] == '#') {
                    // send unsend
                    WCHAR RepName[1024];
                    GetRepName(&m_FileBuf[i],RepName);
                    WCHAR* Content=GetDictContent(&m_FileBuf[i+2]);
                    WCHAR FullDictNameBuf[1024];
                    wcscpy(FullDictNameBuf,L"<#");
                    wcscat(FullDictNameBuf,Content);
                    wcscat(FullDictNameBuf,L"#>");
                    StrTest.Replace(RepName,FullDictNameBuf);
                }
            }
        }
		
        //myfileOut << StrTest;
		TextFileWrite(myfileOut,StrTest);
        //myfileOut.WriteEndl();
        myfileOut << '\n';
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
