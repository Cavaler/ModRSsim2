/////////////////////////////////////////////////////////////////////////////
//
// FILE: EditMemoryDlg.cpp : implementation file
//
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
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EditMemoryDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// This function was added to test for illegal HEX characters by DL on 2016-07-10
BOOL CheckHex(const char *text)
{
	for (size_t i = 0; i < strlen(text); i++)
	{
	if (!isxdigit(text[i]))
		return false;
	}
	return true;
}


/////////////////////////////////////////////////////////////////////////////
// CEditMemoryDlg dialog


CEditMemoryDlg::CEditMemoryDlg(LPCTSTR formatting, 
                               LPCTSTR registerName, // the 'name' if this is a juice-plant register
                               DWORD memoryValue, 
                               WORD valueType, 
                               LPCTSTR description,
                               CWnd* pParent /*=NULL*/)
	: CDialog(CEditMemoryDlg::IDD, pParent)
{
   m_formatting = formatting; // HEX etc.
   m_description = description;
   m_value = memoryValue;
   m_valueType = valueType;
   m_registerName = registerName;

	//{{AFX_DATA_INIT(CEditMemoryDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CEditMemoryDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEditMemoryDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEditMemoryDlg, CDialog)
	//{{AFX_MSG_MAP(CEditMemoryDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEditMemoryDlg message handlers

BOOL CEditMemoryDlg::OnInitDialog() 
{
CString valStr;
LONG  *pLongVal;
float *pFloatVal;

	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
   switch (m_valueType)
   {
   case CMemoryEditorList::VIEWFORMAT_DECIMAL:
   case CMemoryEditorList::VIEWFORMAT_LONG:
      pLongVal = (LONG*)&m_value;
      valStr.Format(m_formatting, m_value);
      break;
   case CMemoryEditorList::VIEWFORMAT_HEX:
   case CMemoryEditorList::VIEWFORMAT_WORD:
   case CMemoryEditorList::VIEWFORMAT_DWORD:
      valStr.Format(m_formatting, m_value);
      break;
   case CMemoryEditorList::VIEWFORMAT_FLOAT:
      pFloatVal = (float*)&m_value;
      valStr.Format(m_formatting, *pFloatVal);
      break;
   case CMemoryEditorList::VIEWFORMAT_CHAR:
      ConvertWordToASCIICS(valStr, (WORD)m_value);
      break;
   default:
      ASSERT(0);
      break;
   }
   SetDlgItemText(IDC_VALUE, valStr);
   SetWindowText(m_description);

   SetDlgItemText(IDC_REGISTERNAME, m_registerName);

   ((CEdit*)GetDlgItem(IDC_VALUE))->SetSel(0, -1);
   GetDlgItem(IDC_VALUE)->SetFocus();
   return (FALSE);
	//return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// Extensive modification were made to the functions below for data verification by DL on 2016-07-10

void CEditMemoryDlg::OnOK() 
{
CString text;
LONG  *pLongVal;
//float *pFloatVal;    // commented out by DL on 2016-07-10
INT64 save_value;
CString valStr;
INT64 *pLongLongVal;
INT64 MyNum;

save_value = m_value;   // this makes the value available later to save back the original value if needed

   GetDlgItemText(IDC_VALUE, text);
     
   switch (m_valueType)
   {
   case CMemoryEditorList::VIEWFORMAT_DECIMAL:
	  //pLongVal = (LONG*)&m_value;
	  pLongLongVal = (LONGLONG*)&m_value;
      //sscanf(text,m_formatting, pLongVal);
	  sscanf(text,m_formatting, pLongLongVal);
	  MyNum = _atoi64(text);
      //if ((*pLongVal > (long) 32767) || (*pLongVal < (long) -32768))  // error trap
	  if ((MyNum > (LONGLONG) 32767) || (MyNum < (LONGLONG) -32768))  // error trap
         {
         CString caption;
		 //if (*pLongVal < (long) -32768)
		 if (MyNum < (LONGLONG) -32768)
            caption.Format("%s is less than -32768. ", text);
		 else
			 caption.Format("%s is greater than 32767. ", text);
         AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
	     valStr.Format(m_formatting, save_value);
	     //sscanf(valStr,m_formatting, pLongVal);
		 sscanf(valStr,m_formatting, pLongLongVal);
         }
      break;
//   case CMemoryEditorList::VIEWFORMAT_LONG:
//      pLongVal = (LONG*)&m_value;
//      valStr.Format(m_formatting, m_value);
//      break;
   case CMemoryEditorList::VIEWFORMAT_LONG:
	  pLongLongVal = (LONGLONG*)&m_value;
      sscanf(text,m_formatting, pLongLongVal);
	  MyNum = _atoi64(text);
      if ((MyNum > (LONGLONG) 2147483647) || (MyNum < _atoi64("-2147483648")))  // error trap
         {
         CString caption;
		 if (MyNum < _atoi64("-2147483648"))
            caption.Format("%s is less than -2147483648. ", text); //dwValue = 2147483647  dwValue = 2147418112
		 else
			 caption.Format("%s is greater than 2147483647. ", text);
         AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
	     valStr.Format(m_formatting, save_value);
	     sscanf(valStr,m_formatting, pLongLongVal);
	     }
      break;
   case CMemoryEditorList::VIEWFORMAT_WORD:
	  //pLongVal = (LONG*)&m_value;
	  pLongLongVal = (LONGLONG*)&m_value;
      //sscanf(text,m_formatting, pLongVal);
	  sscanf(text,m_formatting, pLongLongVal);
	  MyNum = _atoi64(text);
      if ((MyNum > (LONGLONG) 65535) || (MyNum < (LONGLONG) 0))  // error trap
         {
         CString caption;
		 if (MyNum < (LONGLONG) 0)
            caption.Format("%s is less than 0. ", text);
		 else
			 caption.Format("%s is greater than 65535. ", text);
         AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
	     valStr.Format(m_formatting, save_value);
	     //sscanf(valStr,m_formatting, pLongVal);
		 sscanf(valStr,m_formatting, pLongLongVal);
	     }
      break;
   case CMemoryEditorList::VIEWFORMAT_DWORD:
	  pLongLongVal = (INT64*)&m_value;
      sscanf(text,m_formatting, pLongLongVal);
	  MyNum = _atoi64(text);
      if ((MyNum > (INT64) 4294967295) || (MyNum < (INT64) 0))  // error trap
         {
         CString caption;
		 if (MyNum < (INT64) 0)
            caption.Format("%s is less than 0. ", text);
		 else
			 caption.Format("%s is greater than 4294967295. ", text);
         AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
	     valStr.Format(m_formatting, save_value);
	     sscanf(valStr,m_formatting, pLongLongVal);
	     }
      break;
   case CMemoryEditorList::VIEWFORMAT_HEX:
	  pLongVal = (LONG*)&m_value;
	  if (CheckHex(text))
		 {if (strlen(text)> 4)
		 {
		 CString caption;
		 caption.Format("\"%s\" has more than 4 chars. Only first 4 will be used. ", text);
		 AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
		 }
		 sscanf(text, m_formatting, &m_value);
		 }
	  else
	  {
		CString caption;
		caption.Format("\"%s\" has Non-HEX characters. ", text);
		AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
		valStr.Format(m_formatting, save_value);
	    sscanf(valStr,m_formatting, pLongVal);
	  }
      break;
   case CMemoryEditorList::VIEWFORMAT_FLOAT:
      //pFloatVal = (float*)&m_value;         // commented out on 2016-07-10 by DL
	  sscanf(text, m_formatting, &m_value);
      break;
   case CMemoryEditorList::VIEWFORMAT_CHAR:
      WORD w;
      ConvertASCIIToWord(text, w);
	  if ((strlen(text)> 2) && (w == 0)&& text != "x00 x00")
		 {
		 CString caption;
		 caption.Format("\"%s\" has more than 2 chars. Only first 2 will be used. ", text);
		 AfxMessageBox(caption, MB_OK|MB_ICONINFORMATION);
		 text = text.Left(2);
		 ConvertASCIIToWord(text, w);
		 } 
      m_value = w;
      break;
   default:
      ASSERT(0);
      break;
   }
	CDialog::OnOK();
}
