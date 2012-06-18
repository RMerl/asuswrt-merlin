// HtmlPrep.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#ifdef WIN32
#include "HtmlPrep.h"
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
#include "scanner.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object
#ifdef WIN32
CWinApp theApp;
#endif

using namespace std;


char m_OutputMsgFile[1024];


void mywprintf(const WCHAR* fmt, ...)
{
	WCHAR buf[1024];
	FILE* fp;
	if (m_OutputMsgFile[0] == 0)
	{
		va_list arg;
		va_start(arg, fmt);
		// show to console
		vwprintf(fmt, arg);
		va_end(arg);	
		return;
	}
	
	fp = fopen(m_OutputMsgFile,"a+");
	va_list arg;
	va_start(arg, fmt);
	// save to file
#ifdef WIN32	
	vswprintf(buf, fmt, arg);
#else
	vswprintf(buf, 1024, fmt, arg);	
#endif		
	fwprintf(fp, buf);
	// show to console
	vwprintf(fmt, arg);
	va_end(arg);
	fclose(fp);		
}

void tok_assert(token tok, token expected_tok)
{
    if (tok != expected_tok)
    {
        mywprintf(L"tok is not expected value\r\n");        
        mywprintf(L"%d",tok);
        mywprintf(L"\r\n");        
    }
}

void tok_assert_two(token tok, token expected_tok0, token expected_tok1)
{
    if (tok == expected_tok0 || tok == expected_tok1)
    {
    }
    else
    {
        mywprintf(L"tok is not expected value0 or value1\r\n");        
        mywprintf(L"%d",tok);
        mywprintf(L"\r\n");
    }    
}


void tok_assert_buf(WCHAR* tok_buf, WCHAR* tok_buf_expected)
{
    if (wcscmp(tok_buf,tok_buf_expected) != 0)
    {
        mywprintf(L"tok buf is not expected\r\n");        
        mywprintf(tok_buf);
        mywprintf(L"\r\n");
    }
}


BOOL CompareModel(WCHAR* CompareBuf,WCHAR* ModelName)
{
    scanner_set(CompareBuf, wcslen(CompareBuf));
	token tok;
	WCHAR* pRespStr;
	while (1)
	{
		tok = scanner();
        if (tok == TOKEN_EOF)
        {
            break;
        }
        if (tok == TOKEN_LB)
        {
			token tok2;
            tok2 = scanner_get_str_bracket();
			pRespStr = scanner_get_str();
			//wcscpy(m_FnList[NumHtml].SrcFn, pRespStr);
			//NumHtml++;
			//mywprintf(pRespStr);
			//mywprintf(L"\r\n");
			if (wcscmp(pRespStr, ModelName)==0) return TRUE;
        }
	}

	return FALSE;
}

#if 0

static int m_NumDefaultModel = 0;
static WCHAR m_DefaultModel[256][256];

#ifdef WIN32
void LoadDefaultModels(WCHAR* SrcFile)
#else
void LoadDefaultModels(char* SrcFile)
#endif
{
#ifndef WIN32
	CTextFileRead myfile(SrcFile);
	if (!myfile.IsOpen()) 
	{
		mywprintf(L"default models txt open failed");
	}
	if (myfile.IsOpen())
	{
		while(!myfile.Eof())
		{
			CString line;
			myfile.ReadLine(line);
			line = line.Trim();
			if (line != L"")
			{
				wcscpy_x(m_DefaultModel[m_NumDefaultModel], line);
				m_NumDefaultModel++;
			}
		}
	}
#endif
}

BOOL DefaultModels(WCHAR* ModelName)
{
	if (!wcscmp(ModelName,L"RT-N66U")) return TRUE;
#ifndef WIN32
	for (int i=0; i<m_NumDefaultModel; i++)
	{
		if (!wcscmp(m_DefaultModel[i],ModelName))
		{
			return TRUE;
		}
	}
	return FALSE;
#endif
}

#endif

#define CODE_PREP_START2 L"#ifdef RTCONFIG_DUALWAN // RTCONFIG_DUALWAN"
#define CODE_PREP_END2 L"#endif // RTCONFIG_DUALWAN"
#define CODE_PREP_ELSE2 L"#else // RTCONFIG_DUALWAN"


#define CODE_PREP_START L"#ifdef RTCONFIG_DUALWAN"
#define CODE_PREP_END L"#endif"
#define CODE_PREP_ELSE L"#else"

#ifdef WIN32
bool ConvHtml(WCHAR* SrcFile)
#else
bool ConvHtml(char* SrcFile)
#endif
{
	bool RetVal = true;
#ifdef WIN32
	CString DstFile = SrcFile;
	DstFile += L".tmp";
#else
	char DstFile[512];
	strcpy(DstFile,SrcFile);
	strcat(DstFile,".tmp");
#endif
	WCHAR CompareBuf[512];

	CTextFileRead myfile(SrcFile);
	CTextFileWrite myfile2(DstFile, CTextFileWrite::UTF_8);

	if (!myfile.IsOpen()) 
	{
		mywprintf(L"Src File Fail - ");
		//mywprintf(SrcFile);
	}
	if (!myfile2.IsOpen())
	{
		mywprintf(L"dst File Fail - ");
		/*
		WCHAR tmp[256];
		mywprintf(DstFile);
		wcscpy_x(tmp, DstFile);
		mywprintf(tmp);
		*/
	}

	if (myfile.IsOpen() && myfile2.IsOpen())
	{
		while(!myfile.Eof())
		{
			CString line;
			CString lineTest;
			myfile.ReadLine(line);
			lineTest = line;
			lineTest.Trim();
			//BOOL SectionHit = FALSE;
			if (lineTest == CODE_PREP_START)
			{
				BOOL ExitToMainLoop = FALSE;
				while(!myfile.Eof())
				{
					if (ExitToMainLoop) break;

					BOOL SectionElseHit = FALSE;
					BOOL SectionEndHit = FALSE;
					//BOOL SectionHit = FALSE;
					while(!myfile.Eof())
					{
						CString line2;
						CString lineTest;
						myfile.ReadLine(line2);
						lineTest = line2;
						lineTest.Trim();
						if (lineTest == CODE_PREP_ELSE)
						{
							SectionElseHit = TRUE;
							break;
						}
						else if (lineTest == CODE_PREP_END)
						{
							SectionEndHit = TRUE;
							break;
						}
					}
					if (SectionElseHit == FALSE)
					{
						mywprintf(L"CODE_PREP_ELSE no found\n");
						// exit to main loop
						ExitToMainLoop = TRUE;
						break;
					}
					if (SectionEndHit) 
					{
						mywprintf(L"CODE_PREP_END hits before CODE_PREP_ELSE\n");
						// exit to main loop
						ExitToMainLoop = TRUE;
						break;
					}

					while(!myfile.Eof())
					{
						CString line2;
						CString lineTest;
						if (SectionElseHit == FALSE)
						{
							myfile.ReadLine(line2);
							lineTest = line2;
							lineTest.Trim();
						}
						else
						{
							lineTest = CODE_PREP_ELSE;
						}

						if (lineTest == CODE_PREP_END)
						{
Section_End:
							// exit to main loop
							ExitToMainLoop = TRUE;
							break;
						}

						if (lineTest == CODE_PREP_ELSE)
						{
							while(!myfile.Eof())
							{
								CString line5;
								myfile.ReadLine(line5);
								//line5 = line5.Trim();
								CString line6;
								line6 = line5;
								//line6 = line6.Trim();
								if (line6 == CODE_PREP_END)
								{
									goto Section_End;
								}
								//myfile2 << (LPTSTR)(LPCTSTR)line5;
								//line5 = line5.Trim();
								TextFileWrite(myfile2, line5);
								myfile2 << '\n';
							}
							break;
						}

						while(!myfile.Eof())
						{
							CString line3;
							myfile.ReadLine(line3);
							CString line4;
							line4 = line3;
							//line4 = line4.Trim();
							if (line4 == CODE_PREP_END)
							{
								goto Section_End;
							}
							//myfile2 << (LPTSTR)(LPCTSTR)line3;
							//line3 = line3.Trim();
							TextFileWrite(myfile2, line3);
							myfile2 << '\n';
						}
					}
				}
			}
//////////////////////////
			else if (lineTest == CODE_PREP_START2)
			{
				BOOL ExitToMainLoop = FALSE;
				while(!myfile.Eof())
				{
					if (ExitToMainLoop) break;

					BOOL SectionElseHit = FALSE;
					BOOL SectionEndHit = FALSE;
					//BOOL SectionHit = FALSE;
					while(!myfile.Eof())
					{
						CString line2;
						CString lineTest;
						myfile.ReadLine(line2);
						lineTest = line2;
						lineTest.Trim();
						if (lineTest == CODE_PREP_ELSE2)
						{
							SectionElseHit = TRUE;
							break;
						}
						else if (lineTest == CODE_PREP_END2)
						{
							SectionEndHit = TRUE;
							break;
						}
					}
					if (SectionElseHit == FALSE)
					{
						mywprintf(L"CODE_PREP_ELSE2 no found\n");
						// exit to main loop
						ExitToMainLoop = TRUE;
						break;
					}
					if (SectionEndHit) 
					{
						mywprintf(L"CODE_PREP_END2 hits before CODE_PREP_ELSE2\n");
						// exit to main loop
						ExitToMainLoop = TRUE;
						break;
					}

					while(!myfile.Eof())
					{
						CString line2;
						CString lineTest;
						if (SectionElseHit == FALSE)
						{
							myfile.ReadLine(line2);
							lineTest = line2;
							lineTest.Trim();
						}
						else
						{
							lineTest = CODE_PREP_ELSE2;
						}

						if (lineTest == CODE_PREP_END2)
						{
Section_End2:
							// exit to main loop
							ExitToMainLoop = TRUE;
							break;
						}

						if (lineTest == CODE_PREP_ELSE2)
						{
							while(!myfile.Eof())
							{
								CString line5;
								myfile.ReadLine(line5);
								//line5 = line5.Trim();
								CString line6;
								line6 = line5;
								//line6 = line6.Trim();
								if (line6 == CODE_PREP_END2)
								{
									goto Section_End2;
								}
								//myfile2 << (LPTSTR)(LPCTSTR)line5;
								//line5 = line5.Trim();
								TextFileWrite(myfile2, line5);
								myfile2 << '\n';
							}
							break;
						}

						while(!myfile.Eof())
						{
							CString line3;
							myfile.ReadLine(line3);
							CString line4;
							line4 = line3;
							//line4 = line4.Trim();
							if (line4 == CODE_PREP_END2)
							{
								goto Section_End2;
							}
							//myfile2 << (LPTSTR)(LPCTSTR)line3;
							//line3 = line3.Trim();
							TextFileWrite(myfile2, line3);
							myfile2 << '\n';
						}
					}
				}
			}
			else
			{
				TextFileWrite(myfile2, line);
				myfile2 << '\n';
			}


		}
	}
	else
	{
		RetVal = false;
	}

	myfile.Close();
	myfile2.Close();
	
#ifndef WIN32
	remove (SrcFile);
	rename (DstFile,SrcFile);
#endif	
	
	
	
	return RetVal;
}


#ifdef WIN32
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
		return nRetCode;
	}
#endif	

	if (argc != 3)
	{
		mywprintf(L"wrong parameter. for ex, %s test.c output.txt\r\n",argv[0]);
		return 0;
	}


#ifdef WIN32
	strcpy(m_OutputMsgFile, "C:\\a.txt");
#else
	strcpy(m_OutputMsgFile, argv[2]);
#endif

	mywprintf(L"HTML file : %s\r\n",argv[1]);
	//mywprintf(L"Model : %s\r\n",argv[2]);
	//mywprintf(L"Output : %s\r\n",argv[3]);
	//mywprintf(L"Default : %s\r\n",argv[4]);

//	LoadDefaultModels(argv[4]);

	WCHAR ModelName[512];
	//int Ret = ConvHtml(argv[1], L"RT-N66U");
	//int Ret = ConvHtml(argv[1], L"DSL-OTHER");
	int Ret = ConvHtml(argv[1]);
	if (Ret == FALSE)
	{
		mywprintf(L"error conv, open not exist? %s\r\n", argv[1]);
	}

	mywprintf(L"Done!\n");
	

	//int dummy;
	//scanf("%d",&dummy);

	return nRetCode;
}
