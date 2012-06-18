// DictPrep.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#ifdef WIN32
#include "DictPrep.h"
#define TextFileWrite(a,b) a << (LPTSTR)(LPCTSTR)b
#define wcscpy_x(a, b) wcscpy(a,(LPCTSTR)b)
#else
#include <stdio.h> 
#include <stdlib.h> 
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


typedef struct {
	WCHAR SrcFn[64];
	//WCHAR Dst[32];
	//WCHAR ModelName[32];
} CONV_DICT_TYPE;

static CONV_DICT_TYPE m_FnList[200];

static WCHAR m_SuppList[200][80];
static int m_NumSupp;

static WCHAR m_SrcFolder[MAX_PATH];

#ifdef WIN32
static WCHAR m_ToolDir[MAX_PATH];
static WCHAR m_OutputMsgFile[MAX_PATH] = { 0 };
static WCHAR m_EnumFile[MAX_PATH];
#else
static char m_ToolDir[MAX_PATH];
static char m_OutputMsgFile[MAX_PATH]  = { 0 };
static char m_EnumFile[MAX_PATH];
#endif

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
	
#ifdef WIN32
	fp = _wfopen(m_OutputMsgFile,L"a+");
#else
	fp = fopen(m_OutputMsgFile,"a+");
#endif	
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


#define MAX_MODEL_DEP 10240
WCHAR StrModelDep[MAX_MODEL_DEP][128];
static int m_NumStrModelDep;
#define MAX_VALIDATE 10240
WCHAR StrValidate[MAX_VALIDATE][128];
bool bDuplicated[MAX_VALIDATE];

#define MAX_LINE_BUF 4096

#define MAX_DICT 128

CString StrDictHdrLang[MAX_DICT];
static int m_NumDictHdrLang = 0;

#ifndef WIN32
void LoadHdrMapping(char* hdrfile)
#else
void LoadHdrMapping(WCHAR* hdrfile)
#endif
{
	CTextFileRead myfile3(hdrfile);

	while(!myfile3.Eof())
	{
		CString StrTest;
		myfile3.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			// skip
		}
		else if (StrTestTrim != L"")
		{
			StrDictHdrLang[m_NumDictHdrLang] = StrTestTrim;
			m_NumDictHdrLang++;
		}
	}


	myfile3.Close();
}

#ifndef WIN32
void GenerateLangHdr(const char* dstpath, const char* hdrfile)
#else
void GenerateLangHdr(const WCHAR* dstpath, const WCHAR* hdrfile)
#endif
{
#ifdef WIN32
	WCHAR HdrFile[512];
	wcscpy(HdrFile,dstpath);
	wcscat(HdrFile,L"\\");
	wcscat(HdrFile,hdrfile);
#else
	char HdrFile[512];
	strcpy(HdrFile,dstpath);
	strcat(HdrFile,"/");
	strcat(HdrFile,hdrfile);
#endif
	CTextFileWrite myfile2(HdrFile, CTextFileWrite::UTF_8);	

	for (int IdxDictHdr = 0; IdxDictHdr < m_NumDictHdrLang; IdxDictHdr++)
	{
		// fix for linux
		// linux only could use CString to output UNICODE
		myfile2 << "LANG_";
		TextFileWrite(myfile2,StrDictHdrLang[IdxDictHdr]);
		myfile2 << "\n";
	}

	myfile2.Close();
}

// handle model dep and put lang item name to array
bool ConvDictPass1(WCHAR* SrcFile, WCHAR* DstPath, WCHAR* ModelName)
{
	bool RetVal = true;
	CString DstFile = DstPath;
#ifdef WIN32
	DstFile = DstFile + L"\\";
#else
	DstFile = DstFile + L"/";
#endif	
	DstFile = DstFile + SrcFile;
	//DstFile = DstFile + L".dict";
	DstFile = L"temp.dict";
	WCHAR CompareBuf[MAX_LINE_BUF];
	WCHAR NewSrcFile[MAX_PATH];

	wcscpy(NewSrcFile,m_SrcFolder);
	wcscat(NewSrcFile,SrcFile);
	wcscat(NewSrcFile,L".dict");
#ifdef WIN32
	CTextFileRead myfile(NewSrcFile);
	CTextFileWrite myfile2(DstFile, CTextFileWrite::UTF_8);	
#else	
	char TmpNewSrcFile[256];
	char TmpDstFile[256];
	wcstombs(TmpNewSrcFile,NewSrcFile,256);
	CTextFileRead myfile(TmpNewSrcFile);
	wcstombs(TmpDstFile,DstFile,256);	
	CTextFileWrite myfile2(TmpDstFile, CTextFileWrite::UTF_8);	
#endif	
	if (!myfile.IsOpen()) mywprintf(L"Src File Fail");
	if (!myfile2.IsOpen()) mywprintf(L"Dst File Fail");

	if (myfile.IsOpen() && myfile2.IsOpen())
	{
		while(!myfile.Eof())
		{
			CString line;
			myfile.ReadLine(line);
			line = line.Trim();
			if (line != L"")
			{
				BOOL SectionHit = FALSE;
				int ModelDepLen = wcslen(L"[###MODEL_DEP###]");
				if (line.Left(ModelDepLen) == L"[###MODEL_DEP###]")
				{
					line = line.Right(line.GetLength() - ModelDepLen);
					CString line2;
					line2 = line.Trim();
					BOOL SectionHit = FALSE;
					while (1)
					{
						if (line2.GetLength() > 2)
						{
							if (line2.Left(1) == L"[")
							{
								for (int LenIdx=1; LenIdx < line2.GetLength() - 1; LenIdx++)
								{
									if (line2.Mid(LenIdx,1) == L"]")
									{
										CString line3;
										// get modem name without mid bracket
										line3 = line2.Mid(1,LenIdx-1);
										// get reminder part
										line2 = line2.Mid(LenIdx+1,line2.GetLength()-LenIdx-1);
#ifdef WIN32
										if (line3 == ModelName)
#else	
										WCHAR TmpModelName[256];
										wcscpy_x(TmpModelName, line3);
										if (wcscmp(TmpModelName,ModelName)==0)
#endif	
										{
											SectionHit = TRUE;
										}
									}
								}
							}
							else
							{
								if (SectionHit)
								{
									line2 = line2.Trim();
									line2.Replace(L"ZVMODELVZ", ModelName);
									TextFileWrite(myfile2,line2);
									myfile2 << '\n';
									break;
								}
								break;
							}
						}
						else
						{
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		//mywprintf(L"\n!!!!\n");
		RetVal = false;
	}

	myfile.Close();
	myfile2.Close();

	// put lang item name to array

	int NumStrModelDep = 0;
	WCHAR* DstFileOutputPtr;
	DstFileOutputPtr = DstFile.GetBuffer(DstFile.GetLength());
	mywprintf(L"get modem_dep lang item name to array\n");	
	mywprintf(DstFileOutputPtr);	
	mywprintf(L"\n");
	DstFile.ReleaseBuffer();
#ifdef WIN32
	CTextFileRead myfile3(DstFile);
#else	
	char TmpDstFile3[256];
	wcstombs(TmpDstFile3,DstFile,256);	
	CTextFileRead myfile3(TmpDstFile3);
#endif	
	while(!myfile3.Eof())
	{
		CString StrTest;
		myfile3.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			// skip
		}
		else if (StrTestTrim == L"")
		{
			// skip
		}
		else
		{
			WCHAR StrContentBuf[MAX_LINE_BUF];
			wcscpy_x(StrContentBuf,StrTestTrim);
			scanner_set(StrContentBuf, wcslen(StrContentBuf));
			token tok;
			WCHAR* pRespStr;
			while (1)
			{
				tok = scanner();
				if (tok == TOKEN_EOF)
				{
					break;
				}
				if (tok == TOKEN_STRING)
				{
					if (NumStrModelDep < MAX_MODEL_DEP)
					{
						pRespStr = scanner_get_str();
						wcscpy(StrModelDep[NumStrModelDep],pRespStr);
						NumStrModelDep++;
						token tok2 = scanner();
						if (tok2 != TOKEN_EQUAL)
						{
							mywprintf(L"No equal : %ls\n",StrContentBuf);
						}
					}
					else
					{
						mywprintf(L"No free buffer\n");
					}
					break;
				}
			}
		}
	}


	myfile3.Close();

	mywprintf(L"Pass 1 - Model dep strings : %d\n",NumStrModelDep);

	m_NumStrModelDep = NumStrModelDep;

	return RetVal;
}


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
	m_NumDictName = 0;
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
		else if (line3 == L"")
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

typedef struct {
	WCHAR Content[1024];
} DICT_VAR_DEST;

DICT_VAR_DEST m_DictNameTblDest[10240];

WCHAR* GetDictContent(WCHAR *name)
{
    int j;  
    for (j = 0 ; j < m_NumDictName ; j++) {
        if (wcscmp(m_DictNameTbl[j].Val,name)==0)
        {
            return m_DictNameTbl[j].Name;
        }

    }

    return NULL;
}


bool ConvDict(WCHAR* SrcFile, WCHAR* DstPath, WCHAR* ModelName)
{
	bool RetVal = true;
	CString DstFile = DstPath;
#ifdef WIN32
	DstFile = DstFile + L"\\";
#else
	DstFile = DstFile + L"/";
#endif	
	DstFile = DstFile + SrcFile;
	DstFile = DstFile + L".dict";

	WCHAR CompareBuf[MAX_LINE_BUF];
	WCHAR NewSrcFile[MAX_PATH];

	wcscpy(NewSrcFile,m_SrcFolder);
	wcscat(NewSrcFile,SrcFile);
	wcscat(NewSrcFile,L".dict");

#ifdef WIN32	
	WCHAR TempDstFileFn[MAX_PATH];
	wcscpy(TempDstFileFn,m_ToolDir);
	wcscat(TempDstFileFn,L"\\temp_");
	wcscat(TempDstFileFn,SrcFile);
	wcscat(TempDstFileFn,L".dict");
#else	
	char TempDstFileFn[MAX_PATH];
	strcpy(TempDstFileFn,m_ToolDir);
	strcat(TempDstFileFn,"/temp_");
	char TempSrcFile[256];
	wcstombs(TempSrcFile,SrcFile,256);	
	strcat(TempDstFileFn,TempSrcFile);
	strcat(TempDstFileFn,".dict");
/*	
	{
	WCHAR tmp2[128];
	CString tmp = TempDstFileFn;
	wcscpy_x(tmp2,tmp);
	wprintf(L"\n\n%ls\n\n",tmp2);
	}
*/	
#endif	

#ifdef WIN32
	CTextFileRead myfile(NewSrcFile);
	CTextFileWrite myfile2(TempDstFileFn, CTextFileWrite::UTF_8);	
#else	
	char TmpNewSrcFile[256];
	char TmpDstFile[256];
	wcstombs(TmpNewSrcFile,NewSrcFile,256);
	CTextFileRead myfile(TmpNewSrcFile);
	CTextFileWrite myfile2(TempDstFileFn, CTextFileWrite::UTF_8);	
#endif	


	if (!myfile.IsOpen()) mywprintf(L"Src File Fail");
	if (!myfile2.IsOpen()) mywprintf(L"Dst File Fail");

	// add header
	for (int IdxDictHdr = 0; IdxDictHdr < m_NumDictHdrLang; IdxDictHdr++)
	{
		// fix for linux
		// linux only could use CString to output UNICODE
		myfile2 << "LANG_";
		TextFileWrite(myfile2,StrDictHdrLang[IdxDictHdr]);
		myfile2 << "\n";
	}

	if (myfile.IsOpen() && myfile2.IsOpen())
	{
		while(!myfile.Eof())
		{
			CString line;
			myfile.ReadLine(line);
			line = line.Trim();
			if (line != L"")
			{
				BOOL SectionHit = FALSE;
				int ModelDepLen = wcslen(L"[###MODEL_DEP###]");
				if (line.Left(ModelDepLen) == L"[###MODEL_DEP###]")
				{
					// skip
				}
				else
				{
					if (line.Left(1) == L"[" && line.Right(1) == L"]")
					{
						// skip
					}
					else
					{
						WCHAR StrContentBuf[MAX_LINE_BUF];
						wcscpy_x(StrContentBuf,line);
						scanner_set(StrContentBuf, wcslen(StrContentBuf));
						token tok;
						WCHAR* pRespStr;
						bool ModelDepStrFound = false;
						while (1)
						{
							tok = scanner();
							if (tok == TOKEN_EOF)
							{
								break;
							}
							if (tok == TOKEN_STRING)
							{
								pRespStr = scanner_get_str();

								for (int IdxMdep = 0; IdxMdep < m_NumStrModelDep; IdxMdep++)
								{
									if (!wcscmp(StrModelDep[IdxMdep],pRespStr))
									{
										// same, skip
										ModelDepStrFound = true;
										break;
									}
								}
							}
							if (ModelDepStrFound) break;
						}

						if (ModelDepStrFound == false)
						{
							line.Replace(L"[###UNTRANSLATED###]",L"");
							line.Replace(L"ZVMODELVZ", ModelName);
							//myfile2 << (LPTSTR)(LPCTSTR)line;
							TextFileWrite(myfile2,line);
							myfile2 << '\n';
						}
					}
				}
			}
		}
	}
	else
	{
		//mywprintf(L"\n!!!!\n");
		RetVal = false;
	}

	myfile.Close();

	// add model_dep strings
#ifdef WIN32	
	CTextFileRead myfile4(L"temp.dict");
#else
	CTextFileRead myfile4("temp.dict");
#endif	

	if (!myfile4.IsOpen())
	{
		mywprintf(L"temp.dict fail");
	}
	else
	{
/*
		CString StrTitle;
		StrTitle = L"[model dep strings]";
		TextFileWrite(myfile2,StrTitle);
		myfile2 << '\n';
*/
		while(!myfile4.Eof())
		{
			CString StrTest;
			myfile4.ReadLine(StrTest);
			TextFileWrite(myfile2,StrTest);
			myfile2 << '\n';
		}
	}

	myfile4.Close();

	myfile2.Close();

	int NumStrValiddate = 0;
	int GenerateEnum = 0;
	WCHAR* DstFileOutputPtr;
	DstFileOutputPtr = DstFile.GetBuffer(DstFile.GetLength());
	if (!wcscmp(SrcFile,L"EN"))
	{
		GenerateEnum = 1;
		mywprintf(L"It is EN.dict and will generate enum list\n");	
	}
	mywprintf(L"Check duplicated strang and empty line\n");	
	mywprintf(L"**** Validating ****  ");	
	mywprintf(DstFileOutputPtr);	
	mywprintf(L"\n");
	DstFile.ReleaseBuffer();
#ifdef WIN32
	CTextFileRead myfile3(TempDstFileFn);
#else	
	char TmpDstFile2[256];
	CTextFileRead myfile3(TempDstFileFn);
#endif	
	while(!myfile3.Eof())
	{
		CString StrTest;
		myfile3.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			// skip
		}
		else if (StrTestTrim == L"")
		{
			// skip
		}
		else
		{
			WCHAR StrContentBuf[MAX_LINE_BUF];
			wcscpy_x(StrContentBuf,StrTestTrim);
			scanner_set(StrContentBuf, wcslen(StrContentBuf));
			token tok;
			WCHAR* pRespStr;
			while (1)
			{
				tok = scanner();
				if (tok == TOKEN_EOF)
				{
					break;
				}
				if (tok == TOKEN_STRING)
				{
					if (NumStrValiddate < MAX_VALIDATE)
					{
						pRespStr = scanner_get_str();
						wcscpy(StrValidate[NumStrValiddate],pRespStr);
						NumStrValiddate++;
						token tok2 = scanner();
						if (tok2 != TOKEN_EQUAL)
						{
							mywprintf(L"No equal : %ls\n",StrContentBuf);
						}
					}
					else
					{
						mywprintf(L"No free buffer\n");
					}
					break;
				}
			}
		}
	}

	myfile3.Close();

	FILE* fp;

	if (GenerateEnum)
	{
#ifdef WIN32
		fp = _wfopen(m_EnumFile,L"w");
#else
		fp = fopen(m_EnumFile,"w");
#endif	
	}


	int ii;
	int jj;

	for (jj = 0; jj<NumStrValiddate; jj++)
	{
		bDuplicated[jj] = false;
	}

	int EnumCounter = 0;
	for (ii = 0; ii<NumStrValiddate; ii++)
	{
		//if (dup) break;
		bool dup = false;
		if (wcscmp(StrValidate[ii],L"")==0)
		{
			mywprintf(L"empty line : %d\n",ii+1);
		}
		for (jj = 0; jj<NumStrValiddate; jj++)
		{
			if (ii!=jj)
			{
				if (wcscmp(StrValidate[ii],StrValidate[jj])==0)
				{
					mywprintf(L"Duplicated name : %ls\n",StrValidate[ii]);
					dup = true;
					break;
				}
			}
		}

		if (GenerateEnum)
		{
			if (dup)
			{
				if (bDuplicated[jj] == false)
				{
					bDuplicated[jj] = true;
					fwprintf(fp, L"%d=%ls\n", EnumCounter, StrValidate[ii]);
					EnumCounter++;
				}
			}
			else
			{
				fwprintf(fp, L"%d=%ls\n", EnumCounter, StrValidate[ii]);
				EnumCounter++;
			}
		}
	}

	if (GenerateEnum)
	{
		fclose(fp);		
	}


	// final dest file handling
	LoadDict(m_EnumFile);


#ifdef WIN32
	CTextFileRead myfile5(TempDstFileFn);
	CTextFileWrite myfile6(DstFile, CTextFileWrite::UTF_8);	
#else	
	CTextFileRead myfile5(TempDstFileFn);	
	char TmpDstFile6[256];
	wcstombs(TmpDstFile6,DstFile,256);	
	CTextFileWrite myfile6(TmpDstFile6, CTextFileWrite::UTF_8);	
#endif	

	while(!myfile5.Eof())
	{
		CString StrTest;
		myfile5.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			// skip
		}
		else if (StrTestTrim == L"")
		{
			// skip
		}
		else
		{
			WCHAR StrContentBuf[MAX_LINE_BUF];
			wcscpy_x(StrContentBuf,StrTestTrim);
			scanner_set(StrContentBuf, wcslen(StrContentBuf));
			token tok;
			WCHAR* pRespStr;
			while (1)
			{
				tok = scanner();
				if (tok == TOKEN_EOF)
				{
					break;
				}
				if (tok == TOKEN_STRING)
				{
					if (NumStrValiddate < MAX_VALIDATE)
					{
						pRespStr = scanner_get_str();
						WCHAR StrContentBuf2[MAX_LINE_BUF];
						wcscpy(StrContentBuf2,pRespStr);
						token tok2 = scanner();
						if (tok2 == TOKEN_EQUAL)
						{
							WCHAR* RetDict = GetDictContent(StrContentBuf2);
							if (NULL == RetDict)
							{
								mywprintf(L"No found : %ls\n",StrContentBuf);
							}
							else
							{
								token tok3 = scanner_get_rest();
								if (TOKEN_STRING == tok3)
								{
									//int DestVarLoc = _wtoi(RetDict);
									int DestVarLoc = wcstol(RetDict, NULL, 10);
									pRespStr = scanner_get_str();
									wcscpy(m_DictNameTblDest[DestVarLoc].Content,pRespStr);
								}
								else
								{
									mywprintf(L"No string : %ls\n",StrContentBuf);
								}
							}
						}
						else
						{
							mywprintf(L"No equal : %ls\n",StrContentBuf);
						}
					}
					else
					{
						mywprintf(L"No free buffer\n");
					}
					break;
				}
			}
		}
	}

	myfile5.Close();

	for (jj = 0; jj<NumStrValiddate; jj++)
	{
		myfile6 << m_DictNameTblDest[jj].Content;
		myfile6 << '\n';
	}

	myfile6.Close();



	return RetVal;
}



#ifdef WIN32
int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
#else
int main(int argc, char* argv[])
#endif
{
	int nRetCode = 0;
	FILE* fp;

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


	if (argc != 9)
	{
		mywprintf(L"wrong parameter. for ex, %s tool_folder dictctrl.txt DSL-N55U HdrMapping.txt dict_src dict_dst output_enumfile output_msg\r\n",argv[0]);
		return 0;
	}

	mywprintf(L"Tool dir : %s\r\n",argv[1]);
	mywprintf(L"Ctrl file : %s\r\n",argv[2]);
	mywprintf(L"Mdl Name : %s\r\n",argv[3]);
	mywprintf(L"HdrMap Name : %s\r\n",argv[4]);
	mywprintf(L"Src dir : %s\r\n",argv[5]);
	mywprintf(L"Dst dir : %s\r\n",argv[6]);
	mywprintf(L"Enum file : %s\r\n",argv[7]);
	mywprintf(L"Output Msg file : %s\r\n",argv[8]);
#ifdef WIN32
	wcscpy(m_ToolDir, argv[1]);
	wcscpy(m_EnumFile, argv[7]);
	wcscpy(m_OutputMsgFile, argv[8]);
	DeleteFile(m_OutputMsgFile);
#else
	strcpy(m_ToolDir, argv[1]);
	strcpy(m_EnumFile, argv[7]);
	strcpy(m_OutputMsgFile, argv[8]);
	remove(m_OutputMsgFile);
#endif

	// add UTF-8 header
#ifdef WIN32
	fp = _wfopen(m_OutputMsgFile,L"w");
#else
	fp = fopen(m_OutputMsgFile,"w");
#endif	
	fwrite("\xef\xbb\xbf",1, 3, fp);
	fclose(fp);		



// fix isspace assert() when char is chinese word
    char* locale = setlocale( LC_ALL, "" );
	mywprintf(L"Load language header mapping : HdrMapping.txt\r\n");
#ifdef WIN32
	LoadHdrMapping(argv[4]);	
	GenerateLangHdr(argv[6],L"Lang_Hdr.txt");
#else
	char hdrmapfullpath[512];
	strcpy(hdrmapfullpath,argv[1]);
	strcat(hdrmapfullpath,"//");
	strcat(hdrmapfullpath,argv[4]);
	LoadHdrMapping(hdrmapfullpath);	
	GenerateLangHdr(argv[6],"Lang_Hdr.txt");
#endif
	TCHAR TagBuf[32];
	TCHAR ModelNameBuf[64];
	TCHAR DestFolder[MAX_PATH];
	TCHAR LineBuf[MAX_LINE_BUF];
	BOOL SkipGen = TRUE;
	
#ifdef WIN32
	CTextFileRead myfile(argv[2]);
#else
	char ctrlfilefullpath[512];
	strcpy(ctrlfilefullpath,argv[1]);
	strcat(ctrlfilefullpath,"//");
	strcat(ctrlfilefullpath,argv[2]);
	CTextFileRead myfile(ctrlfilefullpath);
#endif
	int NumDict = 0;
	while(!myfile.Eof())
	{
		CString line;
		myfile.ReadLine(line);
		line = line.Trim();
		if (line == L"[END]")
		{
			if (NumDict > 0)
			{
				int i;
				for (i=0; i<NumDict; i++)
				{
					int Ret = ConvDictPass1(m_FnList[i].SrcFn, DestFolder, ModelNameBuf);
					if (Ret == FALSE)
					{
						mywprintf(L"error conv, open not exist? %ls\r\n",m_FnList[i].SrcFn);
					}
					Ret = ConvDict(m_FnList[i].SrcFn, DestFolder, ModelNameBuf);
					if (Ret == FALSE)
					{
						mywprintf(L"error conv, open not exist? %ls\r\n",m_FnList[i].SrcFn);
					}
				}
			}
			break;
		}
		else if (line == L"[MODEL]")
		{
			if (NumDict > 0)
			{
				int i;
				for (i=0; i<NumDict; i++)
				{
					int Ret = ConvDictPass1(m_FnList[i].SrcFn, DestFolder, ModelNameBuf);
					if (Ret == FALSE)
					{
						mywprintf(L"error conv, open not exist? %ls\r\n",m_FnList[i].SrcFn);
					}
					Ret = ConvDict(m_FnList[i].SrcFn, DestFolder, ModelNameBuf);
					if (Ret == FALSE)
					{
						mywprintf(L"error conv, open not exist? %ls\r\n",m_FnList[i].SrcFn);
					}
				}
			}


			NumDict = 0;
			m_NumSupp = 0;
			CString line2;
			myfile.ReadLine(line2);
			line2 = line2.Trim();
			wcscpy_x(ModelNameBuf,line2);
			mywprintf(L"MODEL: ");
			mywprintf(ModelNameBuf);
			mywprintf(L"\r\n");

#ifdef WIN32
			if (wcscmp(argv[3],ModelNameBuf)!=0)
#else
			if (wcscmp_x(argv[3],ModelNameBuf)!=0)
#endif			
			{
				SkipGen = TRUE;
				mywprintf(L"gen model specified, skip\r\n");
			}
			else
			{
				SkipGen = FALSE;
				mywprintf(L"gen files for this one\r\n");
			}


			CString lineSrcFolder;
			//myfile.ReadLine(lineSrcFolder);
			//lineSrcFolder = lineSrcFolder.Trim();
			lineSrcFolder = argv[5];
			wcscpy_x(m_SrcFolder,lineSrcFolder);
#ifdef WIN32
			wcscat(m_SrcFolder,L"\\");
#else
			wcscat(m_SrcFolder,L"/");
#endif			
			CString line3;
			//myfile.ReadLine(line3);
			//line3 = line3.Trim();
			line3 = argv[6];
			wcscpy_x(DestFolder,line3);

		}
		else if (line == L"")
		{
			// empty line
			continue;
		}
		else if (line.Left(1) == L";")
		{
			// comment line
			continue;
		}
		else
		{
			mywprintf(L"wrong tag\r\n");
			break;
		}


		CString line4;
		myfile.ReadLine(line4);
		wcscpy_x(LineBuf,line4);

        scanner_set(LineBuf, wcslen(LineBuf));
		token tok;
		WCHAR* pRespStr;
        while (1)
        {
            tok = scanner();
            if (tok == TOKEN_EOF)
            {
                break;
            }
            if (tok == TOKEN_STRING)
            {
                pRespStr = scanner_get_str();
				if (SkipGen == TRUE)
				{
					//
				}
				else
				{
					wcscpy(m_FnList[NumDict].SrcFn, pRespStr);
					NumDict++;
					mywprintf(L"DICT: ");
					mywprintf(pRespStr);
					mywprintf(L"\r\n");
				}
            }
			if (tok == TOKEN_CAT)
			{
				//
				myfile.ReadLine(line4);
				wcscpy_x(LineBuf,line4);
		        scanner_set(LineBuf, wcslen(LineBuf));
			}

        }
	}



	myfile.Close();


	mywprintf(L"Done!");


	//int dummy;
	//scanf("%d",&dummy);

	return nRetCode;



}
