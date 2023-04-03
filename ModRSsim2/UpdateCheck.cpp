// UpdateCheck.cpp
// Mod_RSSim (c) Embedded Intelligence Ltd. 1993,2009
// AUTHOR: Conrad Braam.  http://www.plcsimulator.org
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UpdateCheck.h"
#include "resource.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CUpdateCheck::CUpdateCheck()
{

}

CUpdateCheck::~CUpdateCheck()
{
}

BOOL CUpdateCheck::GetFileVersion(DWORD &dwMS, DWORD &dwLS)
{
	TCHAR szModuleFileName[MAX_PATH];

    LPBYTE  lpVersionData; 

	if (GetModuleFileName(AfxGetInstanceHandle(), szModuleFileName, sizeof(szModuleFileName)) == 0) return FALSE;

    DWORD dwHandle;     
    DWORD dwDataSize = ::GetFileVersionInfoSize(szModuleFileName, &dwHandle); 
    if ( dwDataSize == 0 ) 
        return FALSE;

    lpVersionData = new BYTE[dwDataSize]; 
    if (!::GetFileVersionInfo(szModuleFileName, dwHandle, dwDataSize, (void**)lpVersionData) )
    {
		delete [] lpVersionData;
        return FALSE;
    }

    ASSERT(lpVersionData != NULL);

    UINT nQuerySize;
	VS_FIXEDFILEINFO* pVsffi;
    if ( ::VerQueryValue((void **)lpVersionData, _T("\\"),
                         (void**)&pVsffi, &nQuerySize) )
    {
		dwMS = pVsffi->dwFileVersionMS;
		dwLS = pVsffi->dwFileVersionLS;
		delete [] lpVersionData;
        return TRUE;
    }

	delete [] lpVersionData;
    return FALSE;

}

void CUpdateCheck::Check(UINT uiURL)
{
	CString strURL(MAKEINTRESOURCE(uiURL));
	Check(strURL);
}

bool CUpdateCheck::Check(const CString& strURL)  // Changed from void to BOOL on 2016-07-14 by DL
{                                                // if True then successful test and if False then unsuccessful
	DWORD dwMS, dwLS;
	bool FoundSite;                              // Added 2016-07-13 by DL for Update Check Success or Not
	FoundSite = FALSE;                           // Initialize to Site Not Found on 2016-07-13 by DL
	const INT BufferSize = 36 * 1024;			 // Buffer Size for Reading Internet File into changed to 36K on 2016-07-16 by DL
	if (!GetFileVersion(dwMS, dwLS))
	{
		ASSERT(FALSE); // Check that application exe has a version resource defined (Open Resource editor, add new version resource)
		return FALSE;
	}
	CWaitCursor wait;
	HINTERNET hInet = InternetOpen(UPDATECHECK_BROWSER_STRING, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
	HINTERNET hUrl = InternetOpenUrl(hInet, strURL, NULL, -1L,
										 INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE |
										 INTERNET_FLAG_NO_CACHE_WRITE |WININET_API_FLAG_ASYNC, NULL);
	if (hUrl)
	{
		char szBuffer[BufferSize];  // Increased Buffer Size from 512 on 2015-07-26 by DL & from 6K to 8K again on 2016-07-13
		                            // My Internet Provider enforced larger header on website requiring larger read to find version. 2016-07-13 by DL
		                            // This causes previous Update Check code to fail unfortunately. 2015-07-13 by DL
		DWORD dwRead;
		//if (InternetReadWholeFile(hUrl, szBuffer, sizeof(szBuffer), &dwRead))   // We do not care if all not read, so delete this. 2016-07-13 by DL
		InternetReadWholeFile(hUrl, szBuffer, sizeof(szBuffer), &dwRead);         // This just gets beginning bytes up to BufferSize. Added 2016-07-13 by DL
			//if (dwRead >= BufferSize) dwRead = BufferSize -1;                   // First Try at fixing buffer => download 2016-07-15 by DL
			if (dwRead > 0)                                                       // If we read some bytes - Next IF by DL added on 2016-07-16
			{                                                                     // The next IF is needed because the buffer index starts at zero
				if (dwRead >= BufferSize)                                         // if we read equal to or more than (not possible) the buffer size
					szBuffer[BufferSize -1] = 0;                                  // We set the last byte of the buffer to zero
				else                                                              // If we read the whole web page without getting to BufferSize
					szBuffer[dwRead] = 0;                                         // then we set the byte after the last one read to zero
				CString strSubMS1;
				CString strSubMS2;
				CString strSubLS1;	// Added 2014-07-27 by DL storage for the Least Significant Minor Version
				CString strSubLS2;	// Added 2014-07-27 by DL storage for the Least Significant Minor Version
				CString strSub;
				CString buffer(szBuffer);
				DWORD dwMSWeb;
				DWORD dwLSWeb;	// Added 2014-07-27 by DL this gets the Least Significant Version
				int pos=0;
				int ch2;        // Begin Add for Web Version Update Check by DL on 2016-07-14
				int ch3;        // Define three consecutive characters to verify we are in the right place.
				int ch4;
				do {
				if (!(buffer[0] == 'V'))                    // If the first character is not a "V" then we need to Tokenize
					strSubMS1 = buffer.Tokenize("V", pos);	// Added 2014-07-26 by DL this finds the 'V' in 'Version' or other
				else
					pos = 1;                               // If the first character was a "V" then we need to index past it
				if ((DWORD)pos < dwRead - 20)              // If we found "V" and there is still room for our numbered version
					{
				ch2 = buffer[pos];     // "e" ?
				ch3 = buffer[pos+1];   // "r" ?
				ch4 = buffer[pos+2];   // "s" ?
					}
				}while (!(ch2 == 'e' && ch3 == 'r' && ch4 == 's') && ((DWORD)pos < dwRead));    // End Add for Web Version Update Check by DL on 2016-07-14
				//if (ch1 == 86 && ch2 == 101 && ch3 == 114)           // Left for testing 2016-07-13
				//	AfxMessageBox("Match", MB_OK|MB_ICONINFORMATION);  // Left for testing 2016-07-13
				if ((DWORD)pos < dwRead - 12)        // If we found "Vers" before buffer end and there is still room (pos will be > dwRead if not found)
				{
				   strSubMS1 = buffer.Tokenize(". ", pos);	// Added 2014-07-26 by DL this advances past the 'Version' to the numbers
				   strSubMS1 = buffer.Tokenize(". ", pos);	// Added 2014-07-26 by DL this gets the Most Significant Major Version
				   strSubMS2 = buffer.Tokenize(". ", pos);	// Added 2014-07-26 by DL this gets the Most Significant Minor Version
				   strSubLS1 = buffer.Tokenize(". ", pos);	// Added 2014-07-27 by DL this gets the Least Significant Major Version
				   strSubLS2 = buffer.Tokenize(". ", pos);	// Added 2014-07-27 by DL this gets the Least Significant Minor Version
				   dwMSWeb = MAKELONG((WORD) atol(strSubMS2), (WORD) atol(strSubMS1));
				   dwLSWeb = MAKELONG((WORD) atol(strSubLS2), (WORD) atol(strSubLS1));	// Added 2014-07-27 by DL Calc dwLSWeb

				   if (dwMSWeb > dwMS)
				   {
					   //strSub = buffer.Tokenize("|", pos);						// Deleted 2015-07-26 by DL
					   strSub = "http://sourceforge.net/projects/modrssim2";		// Added 2015-07-26 by DL
					   MsgUpdateAvailable(dwMS, dwLS, dwMSWeb, dwLSWeb, strSub);	// Revised 2015-07-27 by DL for Least Significant digits
					   FoundSite = TRUE;
				   }
				   else if ((dwMSWeb == dwMS) && (dwLSWeb > dwLS))	// Added 2015-07-26 by DL to handle only Least Significant digit update
				   {																// Added 2015-07-26 by DL
					   strSub = "http://sourceforge.net/projects/modrssim2";		// Added 2015-07-27 by DL
					   MsgUpdateAvailable(dwMS, dwLS, dwMSWeb, dwLSWeb, strSub);	// Added 2015-07-27 by DL for Least Significant digits
					   FoundSite = TRUE;
				   }																// Added 2015-07-26 by DL
				   else
				   {
					   MsgUpdateNotAvailable(dwMS, dwLS);
					   FoundSite = TRUE;
				   }
				}
			}
		InternetCloseHandle(hUrl);
	}
	InternetCloseHandle(hInet);
	return FoundSite;             // Added 2016-07-13 by DL for Update Check to return Success or Not
}
HINSTANCE CUpdateCheck::GotoURL(LPCTSTR url, int showcmd)
{
    TCHAR key[MAX_PATH + MAX_PATH];

    // First try ShellExecute()
    HINSTANCE result = ShellExecute(NULL, _T("open"), url, NULL,NULL, showcmd);

    // If it failed, get the .htm regkey and lookup the program
    if ((UINT)result <= HINSTANCE_ERROR) 
	{

        if (GetRegKey(HKEY_CLASSES_ROOT, _T(".htm"), key) == ERROR_SUCCESS) 
		{
            lstrcat(key, _T("\\shell\\open\\command"));

            if (GetRegKey(HKEY_CLASSES_ROOT,key,key) == ERROR_SUCCESS) 
			{
                TCHAR *pos;
                pos = _tcsstr(key, _T("\"%1\""));
                if (pos == NULL) {                     // No quotes found
                    pos = _tcsstr(key, _T("%1"));      // Check for %1, without quotes 
                    if (pos == NULL)                   // No parameter at all...
                        pos = key+lstrlen(key)-1;
                    else
                        *pos = '\0';                   // Remove the parameter
                }
                else
                    *pos = '\0';                       // Remove the parameter

                lstrcat(pos, _T(" "));
                lstrcat(pos, url);
				CString csKey(key);
                result = (HINSTANCE) WinExec((LPSTR)((LPCTSTR)csKey), showcmd);
            }
        }
    }

    return result;
}

LONG CUpdateCheck::GetRegKey(HKEY key, LPCTSTR subkey, LPTSTR retdata)
{
    HKEY hkey;
    LONG retval = RegOpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE, &hkey);

    if (retval == ERROR_SUCCESS) 
	{
        long datasize = MAX_PATH;
        TCHAR data[MAX_PATH];
        RegQueryValue(hkey, NULL, data, &datasize);
        lstrcpy(retdata,data);
        RegCloseKey(hkey);
    }

    return retval;
}

/**********************************************************/
// this patch provided by 	John-Lucas Brown
bool CUpdateCheck::InternetReadWholeFile(HINTERNET hUrl,LPVOID lpBuffer,DWORD dwNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead)
{
DWORD dwRead=0;
BYTE szBuffer[512];
memset(lpBuffer,0,dwNumberOfBytesToRead);
*lpNumberOfBytesRead = 0;
do{
if (!InternetReadFile(hUrl,szBuffer,sizeof(szBuffer),&dwRead))
dwRead = 0;
if (dwRead!=0){
if (*lpNumberOfBytesRead + dwRead > dwNumberOfBytesToRead) return false;//too much data for the buffer
memcpy(&((char *)lpBuffer)[*lpNumberOfBytesRead],szBuffer,dwRead);
*lpNumberOfBytesRead+=dwRead;
}
}while (dwRead != 0 && strchr((char *)lpBuffer,EOF)==NULL);
return strchr((char *)lpBuffer,EOF)==NULL;
}

// override these methods as needed.
void CUpdateCheck::MsgUpdateAvailable(DWORD dwMSlocal, DWORD dwLSlocal, DWORD dwMSWeb, DWORD dwLSWeb, const CString& strURL)
{
	CString strMessage;
	// Added Least Significant (LS) to both local and Web versionings for statement below on 2015-07-27 by DL
	strMessage.Format(IDS_UPDATE_AVAILABLE, HIWORD(dwMSlocal), LOWORD(dwMSlocal), 
		HIWORD(dwLSlocal), LOWORD(dwLSlocal), HIWORD(dwMSWeb), LOWORD(dwMSWeb),HIWORD(dwLSWeb), LOWORD(dwLSWeb));

	if (AfxMessageBox(strMessage, MB_YESNO|MB_ICONINFORMATION) == IDYES)
		GotoURL(strURL, SW_SHOW);
}

void CUpdateCheck::MsgUpdateNotAvailable(DWORD dwMSlocal, DWORD dwLSlocal)
{
	AfxMessageBox(IDS_UPDATE_NO, MB_OK|MB_ICONINFORMATION);
}

void CUpdateCheck::MsgUpdateNoCheck(DWORD dwMSlocal, DWORD dwLSlocal)
{
	AfxMessageBox(IDS_UPDATE_NOCHECK, MB_OK|MB_ICONINFORMATION);
}
