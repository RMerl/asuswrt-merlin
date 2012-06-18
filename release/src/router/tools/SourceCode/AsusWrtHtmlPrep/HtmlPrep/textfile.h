// textfile.h: interface for the textfile class.
//
//////////////////////////////////////////////////////////////////////

#ifndef PEKSPRODUCTIONS_TEXTFILE
#define PEKSPRODUCTIONS_TEXTFILE



/*
	CTextFileDocument let you write and read text files with 
	different encodings (ASCII, UTF-8, Unicode 16 little/big 
	endian is supported). When you work with ASCII-files
	CTextFileDocument will help you convert strings to/from
	different code-pages.

	Let me now if you find something strange or just gets
	a clever idea...

	Get the latest version at 
	http://www.codeproject.com/file/textfiledocument.asp

	Version 1.22 - 21 May 2005
	 ! Reading a line before reading everything could add an
	   extra line break.
	 ! A member variable wasn't always initialized, could cause
	   problems when reading single lines.
	 ! A smarter/easier algorithm is used when reading single lines.

	Version 1.21 - 10 Apr 2005
	 ! Fix by sammyc: If it was not possible to open a file in techlevel 1,
	   IsOpen returned a bad result.

	Version 1.20 - 15 Jan 2005
	 ! Fixed some problems when converting multi-byte string to Unicode,
	   and vice versa.
	 + Improved conversion routines. It's now possible to define
	   which code-page to use.
	 + It's now possible to set which character to use when it's
	   not possible to convert an Unicode character to an multi-byte character.
	 + It's now possible to see if data was lost during conversion.
	 + Better support for other platforms, it's no longer necessary to use
	   MFC in Windows.
	 ! Reading very small files (1 byte) failed.

	Version 1.13 - 26 Dec 2004
	 ! Fixes by drinktea:
	 ! If a text file begun with an empty line, the file
	   wasn't read correctly (first empty line was ignored).
	 ! Fixes in CharToWstring and WcharToString.

	Version 1.12 - 17 Oct 2004
	 + Minor memory leak when open file failed, fixed.

	Version 1.11 - 28 Aug 2004
	 ! Calling WriteEndl() when writing an ASCII file could make
	   the file incorrectly written. Fixed.
	 + ASCII files is written faster.

	Version 1.10 - 13 Aug 2004
	Sorry about the quick update.
	 + Improved performance (much faster now, but code is more complicated :-/).
	 + Buffer is used when writing files.
	 + Buffer is used in non-mfc compilers

	Version 1.0	- 12 Aug 2004
	Initial version.

	PEK
  */


/*

If you are creating a console project that doesn't support
MFC in Visual Studio, you will probably need to define
techlevel to 0:
#define PEK_TX_TECHLEVEL 0

In other cases it usually not necessary to define which "tech-level" 
to use, the code below should do this for you. However, 
if you need to this is the difference:

#define PEK_TX_TECHLEVEL 0
You should use this if you running on a none-Windows 
platform. This uses fstream internally to read and 
write files. If you want to change codepage you should 
call setlocal.

#define PEK_TX_TECHLEVEL 1
Use this on Windows if you don't use MFC. This calls 
Windows API directly to read and write files. If 
something couldn't be read/written a CTextFileException 
is thrown. Unicode in filenames are supported. 
Codepages are supported.

#define PEK_TX_TECHLEVEL 2
Use this when you are using MFC. This uses CFile 
internally to read and write files. If data can't be 
read/written, CFile will throw an exception. Codepages 
are supported. Unicode in filenames are supported.
CString is supported.

*/

#ifndef PEK_TX_TECHLEVEL

//Autodetect which "tech level" to use
#ifdef _MFC_VER
	#define	PEK_TX_TECHLEVEL 2
#else
#ifdef _WIN32
	#define	PEK_TX_TECHLEVEL 1
#else
	#define	PEK_TX_TECHLEVEL 0
#endif
#endif
#endif


#if PEK_TX_TECHLEVEL > 0
	/*
		In windows it's possible to use Unicode in filenames,
		in unix it's not possible (afaik). FILENAMECHAR is the 
		charactertype.
	  */
	#include <afx.h>

	#ifndef _UNICODE
		typedef char FILENAMECHAR;
	#else
		typedef wchar_t FILENAMECHAR;
	#endif
#else
	#include <fstream>
	typedef char FILENAMECHAR;
#endif

#include <string>
using namespace std;

class CTextFileBase
{
public:
	enum TEXTENCODING { ASCII, UNI16_BE, UNI16_LE, UTF_8 };

	CTextFileBase();
	~CTextFileBase();

	//Is the file open?
	int IsOpen();
	
	//Close the file
	virtual void Close();

	//Return the encoding of the file (ASCII, UNI16_BE, UNI16_LE or UTF_8);
	TEXTENCODING GetEncoding() const;

	//Set which character that should be used when converting
	//Unicode->multi byte and an unknown character is found ('?' is default)
	void SetUnknownChar(const char unknown);

	//Returns true if data was lost
	//(happens when converting Unicode->multi byte string and an unmappable
	//characters is found).
	bool IsDataLost() const;
	
	//Reset the data lost flag
	void ResetDataLostFlag();

	#if PEK_TX_TECHLEVEL > 0

	/* Note!
	   The codepage is only used when converting from multibyte
	   to Unicode or vice versa. It is not used when reading
	   ANSI-files in none-Unicode strings, or reading
	   Unicode-files in Unicode strings.

	   This means that if you want to read a ANSI-textfile
	   (with some code page) to an non-Unicode string you
	   must do the conversion yourself. But this is easy :-).
	   Read the file with the codepage to a wstring, then use
	   ConvertCharToWstring to convert the wstring to a
	   string.

	*/
	//Set codepage to use when working with none-Unicode strings
	void SetCodePage(const UINT codepage);

	//Get codepage to use when working with none-Unicode strings
	UINT GetCodePage() const;

	//Convert char* to wstring
	static void ConvertCharToWstring(const char* from, wstring &to,	UINT codepage=CP_ACP);

	//Convert wchar_t* to string
	static void ConvertWcharToString(const wchar_t* from, string &to, UINT codepage=CP_ACP, bool* datalost=NULL, char unknownchar=0);


	#else

	//Convert char* to wstring
	static void ConvertCharToWstring(const char* from, wstring &to);

	//Convert wchar_t* to string
	static void ConvertWcharToString(const wchar_t* from, string &to, bool* datalost=NULL, char unknownchar='a');

	#endif


protected:
	//Convert char* to wstring
	void CharToWstring(const char* from, wstring &to) const;
	//Convert wchar_t* to string
	void WcharToString(const wchar_t* from, string &to);
		
	//The enocoding of the file
	TEXTENCODING m_encoding;

	//Buffersize
	#define BUFFSIZE 1024


#if PEK_TX_TECHLEVEL == 0
	//Use fstream
	fstream m_file;
#elif PEK_TX_TECHLEVEL == 1
	HANDLE m_hFile;
#else
	//In windows we are using CFile
	CFile* m_file;
	bool m_closeAndDeleteFile;
#endif
	
	//These controls the buffer for reading/writing

	//True if end of file
	bool m_endoffile;
	//Readingbuffer
	char m_buf[BUFFSIZE];
	//Bufferposition
	int m_buffpos;
	//Size of buffer
	int m_buffsize;

	//Character used when converting Unicode->multi byte and an unknown character was found
	char m_unknownChar;

	//Is true if data was lost when converting Unicode->multi-byte
	bool m_datalost;

#if PEK_TX_TECHLEVEL > 0
	UINT m_codepage;
#endif


	

};

class CTextFileWrite : public CTextFileBase
{

public:
	CTextFileWrite(const FILENAMECHAR* filename, TEXTENCODING type=ASCII);
#if PEK_TX_TECHLEVEL == 2
	CTextFileWrite(CFile* file, TEXTENCODING type=ASCII);
#endif
	~CTextFileWrite();

	//Write routines
	void Write(const char* text);
	void Write(const wchar_t* text);
	void Write(const string& text);
	void Write(const wstring& text);
	
	CTextFileWrite& operator << (const char c);
	CTextFileWrite& operator << (const char* text);
	CTextFileWrite& operator << (const string& text);

	CTextFileWrite& operator << (const wchar_t wc);
	CTextFileWrite& operator << (const wchar_t* text);
	CTextFileWrite& operator << (const wstring& text);
	
	//Write new line (two characters, 13 and 10)
	void WriteEndl();

	//Close the file
	virtual void Close();

private:
	//Write and empty buffer
	void Flush();

	//Write a single one wchar_t, convert first
	void WriteWchar(const wchar_t ch);

	//Write one byte
	void WriteByte(const unsigned char byte);

	//Write a c-string in ASCII-format
	void WriteAsciiString(const char* s);

	//Write byte order mark
	void WriteBOM();
};


class CTextFileRead : public CTextFileBase
{

public:
	CTextFileRead(const FILENAMECHAR* filename);
#if PEK_TX_TECHLEVEL == 2
	CTextFileRead(CFile* file);
#endif

	//Returns false if end-of-file was reached
	//(line will not be changed). If returns true,
	//it means that last line ended with a line break.
	bool ReadLine(string& line);
	bool ReadLine(wstring& line);

	//Returns everything from current position.
	bool Read(string& all, const string newline="\r\n");
	bool Read(wstring& all, const wstring newline=L"\r\n");

#if PEK_TX_TECHLEVEL == 2
	bool ReadLine(CString& line);
	bool Read(CString& all, const CString newline=_T("\r\n"));
#endif

	//End of file?
	bool Eof() const;

private:
	//Guess the number of characters in the file
	int GuessCharacterCount();

	//Read line to wstring
	bool ReadWcharLine(wstring& line);

	//Read line to string
	bool ReadCharLine(string& line);

	//Reset the filepointer to start
	void ResetFilePointer();

	//Read one wchar_t
	void ReadWchar(wchar_t& ch);

	//Read one byte
	void ReadByte(unsigned char& ch);

	//Detect encoding
	void ReadBOM();

	//Use extra buffer. Sometimes we read one character to much, save it.
	bool m_useExtraBuffer;

	//Used to read see if the first line in file is to read 
	//(so we know how to handle \n\r)
	bool m_firstLine;

	//Extra buffer. It's ok to share the memory
	union
	{
		char m_extraBuffer_char;
		wchar_t m_extraBuffer_wchar;
	};

};


#if PEK_TX_TECHLEVEL == 1

//This is only used in Windows mode (no MFC)
//An exception is thrown will data couldn't be read or written
class CTextFileException
{
public:
	CTextFileException(DWORD err)
	{
		m_errorCode = err;
	}

	//Value returned by GetLastError()
	DWORD m_errorCode;
};

#endif

#endif //PEKSPRODUCTIONS_TEXTFILE