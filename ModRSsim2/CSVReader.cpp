// FILE: CSVReader.cpp
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
// class CCSVTextImporter
//         the class that imports a 16-column CSV file (was 17)
// [internal]
// class CCSVTextLine implementation
//       CCSVTextFile implementation
//       CCSVLineArray implementation
//
//////////////////////////////////////////////////////////////////////////////////////////////////////
//
//Example of Input File. These will be read into values and stored as IEEE-754 32-bit if FP's
//
//This line will be completely ignored and can contain anything such as a header. (Line 1)
//1,4,10,123.456          // Load value 123.456 into 400010/11 (Line 2)   // Revised 2015-Nov-15 by DL
//1,4,20,234.567          // Load value 234.567 into 400020/21 (line 3)   // Revised 2015-Nov-15 by DL
//1,3,1,1.234             // Load value 1.234 into 300001/2 (line 4)      // Added 2015-Nov-20 by DL
//0,4,50,100              // Load value 100 into 400050 (line 5)          // Added 2015-Nov-20 by DL
//0,0,50,1                // Load value 1 into 000001 (line 6)            // Added 2015-Nov-20 by DL
//0,1,100,0               // Load value 0 into 100100 (line 7)            // Added 2015-Nov-20 by DL
//.....                   // Lines Continue as needed                     // Revised 2015-Nov-15 by DL
//////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"


//////////////////////////////////////////////////////////////////////////////
// class CCSVTextLine
// implementation:  this class is an extension of CString, all it does is contruct from 
// a standard string. Then it allows you to interrogate the string which
// is seperated CSV style.
// --------------------------- CCSVTextLine::CCSVTextLine ------------------
CCSVTextImporter::CCSVTextLine::CCSVTextLine(LPCSTR string) : CString (string)
{
   m_init = FALSE;
   Parse();
}

CCSVTextImporter::CCSVTextLine::CCSVTextLine(CCSVTextLine& other)
{
   this->m_init = other.m_init;
   this->m_sizedouble = other.m_sizedouble;
   memcpy(this->m_values, other.m_values, sizeof(other.m_values));
}


// ----------------------------------- CCSVTextLine -------------------------
CCSVTextImporter::CCSVTextLine::CCSVTextLine() : CString ()
{
   m_init = FALSE;
} // CCSVTextLine

// ----------------------------------- GetElement ----------------------------
// return values in a row
double CCSVTextImporter::CCSVTextLine::GetElement(LONG index)
{
   ASSERT((index >=0) && (index <= MAX_CSVFILE_COLUMNS));
   return(m_values[index]);
}


// ----------------------------------- operator= -----------------------------
// compare operator for a other line
CCSVTextImporter::CCSVTextLine& CCSVTextImporter::CCSVTextLine::operator=(CCSVTextLine &otherLine)
{
   *((CString*)(this)) = *(CString*)&otherLine;
   return(*this);
} // operator=

// ----------------------------------- operator= -----------------------------
// compare operator for a CString
CCSVTextImporter::CCSVTextLine& CCSVTextImporter::CCSVTextLine::operator=(CString &otherString)
{
   *((CString*)(this)) = otherString; 
   return(*this);
} // operator=


// ----------------------------------- Parse ---------------------------------
// Fetch the variables out of a line of text
//
// This function uses a technique I call indirect addressing.
// it sets up a pointer to one variable, then increments it to point to the next.
// The trick is that the variables are not in an array, but just coded 
// consecutively so that the function does not have to take ages un-packing, since 
// this routine would get called many times.
//
void CCSVTextImporter::CCSVTextLine::Parse()
{
LONG  pos, last, next, fields=0;
CString temp;
double * varPtr = &m_values[0];
   
   m_init = TRUE;
   pos = Find(',');
   if (-1 == pos)
      return;
   last = pos+1;
   temp = Mid(0, pos);  //Double Register Flag

   sscanf(temp, "%d", &m_sizedouble);            // Rev by DL on 2015-Nov-18
   OutputDebugString(temp);
   OutputDebugString(" ");
   fields++;
   *varPtr++ = m_sizedouble;           // Rev by DL on 2015-Nov-18

   next = pos;
   // get the other fields in a loop
   while ((-1 != last)&&(-1 != next))
   {
      temp = Mid(last, GetLength()-last);
      next = temp.Find(',');
      if (-1!=next)
         temp = Mid(last, next);

      sscanf(temp, "%lg", varPtr);
      OutputDebugString(temp);
      OutputDebugString(" ");
      last = last+next+1;
      fields++;
      varPtr++;
      if (fields >= MAX_CSVFILE_COLUMNS)
      {
//         ASSERT(0);				// Deleted 2015-Nov-19 by DL
         break;  // forced break
      }
   }
   ASSERT(fields == MAX_CSVFILE_COLUMNS);
   OutputDebugString("\n");
} // Parse



//////////////////////////////////////////////////////////////////////////////
// class CCSVTextFile
// implementation

// PURPOSE: read the CSV text file in in one go.
// this class gets deleted after it has been used, in order to conserve RAM
// the contents get copied into an array.
CCSVTextImporter::CCSVTextFile::CCSVTextFile(LPCTSTR fileName, UINT flags) : CFile(fileName, flags)
{
   m_length = (DWORD)GetLength();
   m_data = (BYTE*)malloc(m_length + 1);

   Read((void*)m_data, m_length);
   m_data[m_length] = '\0';
}

// ----------------------------------- ~CCSVTextFile ------------------------------
CCSVTextImporter::CCSVTextFile::~CCSVTextFile()
{
   if (NULL != m_data)
      free(m_data);
} // ~CCSVTextFile

// ----------------------------------- Data ----------------------------------
// return a pointer to the data, this pointer becomes invalid once this 
// object is destroyed.
LPCTSTR CCSVTextImporter::CCSVTextFile::Data()
{
   return((LPCTSTR)m_data);
} // Data

//////////////////////////////////////////////////////////////////////////////
// class CCSVLineArray
// implementation

// ----------------------------------- ~CCSVLineArray --------------------------
CCSVTextImporter::CCSVLineArray::~CCSVLineArray()
{
CCSVTextLine * pLine;
LONG index, size = GetSize();

// Moved below buffer and MessageBox statment to UpdateRegisters so I could test "manual" on 2015-Nov-19 by DL
//char buffer[20];
// Deleted previous & next statement because it was holding up auto processing on 2015-Nov-16 by DL
//MessageBox(NULL,_itoa((int)size, buffer, 10), "Import Rows OK", MB_OK);      // prints number of active rows
// I added the under bar to the itoa function above because the C++ compiler asked for it. By DL on 2015-01-05

   if (size)
   {
      for (index = 0; index < size; index++)
      {
         pLine = GetAt(index);
         delete pLine;
      }
   }
   RemoveAll();
} // ~CCSVLineArray

// ----------------------------------- GetAt ---------------------------------
CCSVTextImporter::CCSVTextLine* CCSVTextImporter::CCSVLineArray::GetAt(LONG index)
{
   return((CCSVTextImporter::CCSVTextLine*)CObArray::GetAt(index));
} // GetAt

// ----------------------------------- Add -----------------------------------
LONG CCSVTextImporter::CCSVLineArray::Add(CCSVTextImporter::CCSVTextLine *pLine)
{
   CCSVTextImporter::CCSVTextLine lineA(*pLine);
   //lineA
   return(CObArray::Add((CObject*)pLine));
} // // ----------------------------------- Add


// ----------------------------------- operator [] ---------------------------
CCSVTextImporter::CCSVTextLine *CCSVTextImporter::CCSVLineArray::operator [](LONG index)
{
   return(GetAt(index));
} // operator []

//////////////////////////////////////////////////////////////////////////////
// class CCSVTextImporter
// implementation

// Wrapper class which uses all of the above classes to load the CSV file
// in CSV format. The constructor automaticaly loads the file and puts it 
// into an array.
CCSVTextImporter::CCSVTextImporter()
 : CObject()
{
   myArray = NULL;
   memset(&m_lastInterval, 0, sizeof(m_lastInterval));
}

CCSVTextImporter::~CCSVTextImporter()
{
   if (myArray)
      delete myArray;
}


void CCSVTextImporter::Open(LPCTSTR fileName)
{
CCSVTextFile * file;
CString data;
LONG pos,curPos;

   if (myArray)
      delete myArray;
   myArray = new CCSVLineArray;
   TRY
   {
      file = new CCSVTextFile(fileName, CFile::modeRead| CFile::shareDenyNone);
      data = file->Data();
      pos = data.Find('\n');
      if (-1 != pos)
      {
         // strip off the column headings
         data = data.Mid(pos+1, data.GetLength()-pos-1);

         while (-1 != pos)
         {
         CString curLine;
            pos = data.Find('\n');
            if (-1 != pos)
            {
               curLine = data.Mid(0, pos-1);
               data = data.Mid(pos+1, data.GetLength()-pos-1);
            }
            else
               curLine = data;
            // make double sure this is just one line
            curPos = curLine.Find('\n');
            if (-1 != curPos)
            {
               curLine = curLine.Mid(0, curPos-1);
            }
            if (curLine.GetLength())
            {
               // put the line into the array
               CCSVTextLine *pString;
                  pString = new CCSVTextLine(curLine);
                  //*pString = curLine;
               myArray->Add(pString);
			   //char buffer[20]=" ";                       // Deleted 2015-Nov-15 by D. Lyons
			   //MessageBox(NULL, *pString, buffer, MB_OK); // Deleted 2015-Nov-15 by D. Lyons
            }
         }
      }
      //printf(data);
      delete file;
   }
   CATCH (CFileException, e)
   {
   CHAR msg[MAX_DEBUG_STR_LEN];   

      sprintf(msg, "Error %d opening CSV file", e->m_cause);
      OutputDebugString(msg);
   }
   END_CATCH

   // initialisation completed 
} // CCSVTextImporter

// ----------------------------------- LoadedOK ------------------------------
BOOL CCSVTextImporter::LoadedOK()
{
   if (myArray->GetSize())
      return(TRUE);
   return(FALSE);
} // LoadedOK

// ----------------------------------- HandleTimer ----------------------------
// return TRUE if we have data to dump into registers
//
BOOL CCSVTextImporter::HandleTimer(LPCTSTR importFolder, CRegisterUpdaterIF *pParentInterface)
{

   m_parentInterface = pParentInterface;
   ASSERT(m_parentInterface );
   
   // Verify that my invented data algorithm is same as Vinay's approach.
   {
	//COleDateTime dtCurrent	      =	COleDateTime::GetCurrentTime();
	//CString CurrentDate		=	dtCurrent.Format("%Y%m%d");
	//CString CurrentTime		=	dtCurrent.Format("%H%M");

	//CString strTime  = CurrentTime.Mid(2,2);
	//CString strFile;
   }

   //Test for new CSV file
   CString fileName;
   SYSTEMTIME  sysTimeExpect, currentTime;
   CString fullFileName;

      GetLocalTime(&currentTime);
      sysTimeExpect = currentTime;

      //sysTimeExpect.wMinute = (sysTimeExpect.wMinute/15)*15;            // Deleted 2015-Nov-16 by DL
	  //The above statement limits the testing of the CSV file name to only once every 15 minutes
      fileName.Format("%04d%02d%02d\\%02d%02d.csv", sysTimeExpect.wYear, 
                                                    sysTimeExpect.wMonth, 
                                                    sysTimeExpect.wDay, 
                                                    sysTimeExpect.wHour, 
                                                    sysTimeExpect.wMinute);
      fullFileName = importFolder;
      fullFileName+= '\\';
      fullFileName+= fileName;

      // prevent re-processing the same file twice
      // todo: this may need to be cleverer, although it is not a real problem on start-up either.
      if ((fileName != m_lastProcessed) &&(ExistFile(fullFileName)))
      {
      CWaitCursor wait;    // put up a wait cursor

         if (ImportFile(fullFileName))
         {
            m_lastProcessed = fileName;
            return(true);
         }
      }
   if (fileName != m_lastProcessed)
   {
      // check for no CSV found/skipped cases
      CTime    localTime(currentTime);
      CTime    lastTime(m_lastInterval);
      CTimeSpan elapsedTime = localTime - lastTime;
      LONGLONG stale = elapsedTime.GetTotalMinutes();
   
      if (stale > 15)
      {
         m_lastInterval = sysTimeExpect;
         CString msg;
         msg.Format("The expected CSV import file '%s' was not found!", fullFileName);
         m_parentInterface->DebugMessage(msg);
      }
   }
   return(false);
} // HandleTimer


// --------------------------- ImportFile -----------------------------
// returns: TRUE if the file was read without error, else FALSE
//
LONG CCSVTextImporter::ImportFile(LPCTSTR csvName, BOOL manual, CRegisterUpdaterIF *pParentInterface/* =0*/)
{
CString msg, msgFormat;

   if (pParentInterface)
      m_parentInterface = pParentInterface;
   ASSERT(m_parentInterface);
   if (!manual)
      msgFormat.LoadString(IDS_MSGCSVSTARTPROCESSING);
   else
      msgFormat.LoadString(IDS_MSGCSVSTARTPROCESSING_MAN);
   msg.Format(msgFormat, csvName);
   m_parentInterface->DebugMessage(msg);                                                                                                                                                  
   {
      // Process CSV
      // open file and load all in here
      Open(csvName);
      
      // return if error occurs
      if (!LoadedOK())
      {
         msg.Format("Processing CSV failed.");
         m_parentInterface->DebugMessage(msg);                                                                                                                                                                                                                                   
         return(0);
      }
      // Viola, job done
   }

   msg.Format("End processing OK");
   m_parentInterface->DebugMessage(msg);

 // These next two statements are used to update the main form after reading registers. By DL on 2015-01-01
 // Moved into UpdateRegisters section on 2015-Nov-16 by DL
 //   if (pGlobalDialog)
 //       pGlobalDialog->RedrawListItems(3,0,6554); // repaint all of the 4000 registers by DL on 2015-01-01

   return(1);
   //LONG NewCount;
   //NewCount = this->LineCount();

   //return(NewCount);

}

//#define CSV_REGISTERSIZE   2	         // Deleted 2015-Nov-15 by D. Lyons because we now load one register per line
//#define CSV_CLONEMODBUS    (false)     // Deleted 2015-Nov-19 by D. Lyons - not needed

// ------------------------------ UpdateRegisters -----------------------------------
// allows us to call this function to update data a while after reading the file
// so that we can slice up the time used if needed
LONG CCSVTextImporter::UpdateRegisters(BOOL manual)
{
LONG count=0;
//  Initialize Bools for Using register blocks to false
bool UsingCoils = false;                // Added 2015-Nov-18 by D. Lyons    
bool UsingInputs = false;               // Added 2015-Nov-18 by D. Lyons    
bool UsingAnalogs = false;              // Added 2015-Nov-18 by D. Lyons    
bool UsingRegisters = false;            // Added 2015-Nov-18 by D. Lyons    

   ASSERT(m_parentInterface);
   
   //int col, row, registerNum;  // Deleted 2015-Nov-15 by DL
   int row, prefix, registerNum;    // Added 2015-Nov-15 by DL
   bool typ;                // Added 2015-Nov-15 by DL
   for (row=0; row < this->LineCount(); row++)
      //for (col = 0; col < MAX_CSVFILE_COLUMNS;col++)   // Deleted 2015-Nov-15 by D. Lyons
	  // Revised so that the first number is a double register flag and the second number is the Modbus area number
	  // Revised so that the third number is the register number to load and the fourth number is the value to load
   {                                                                     // Added 2015-Nov-15 by D. Lyons                                                            
   registerNum = (int)myArray->GetAt(row)->GetElement(2) - 1;            // Added 2015-Nov-15 by D. Lyons
   if (registerNum < 0) registerNum = 0;                                 // Added 2015-Nov-15 by D. Lyons
   prefix = (int) myArray->GetAt(row)->GetElement(1);					 // Added 2015-Nov-15 by D. Lyons
   typ = (bool) myArray->GetAt(row)->GetElement(0);			             // Get Type (False = Integer, True = Float)
   if (typ)         // Type false(0) = Integer & true(1) = Float. Added 2015-Nov-15 by D. Lyons
      {                                                                  // Added 2015-Nov-15 by D. Lyons                                                
		 if (registerNum > 65534) registerNum = 65534;                   // Added 2015-Nov-15 by D. Lyons
         //registerNum = (row* CSV_REGISTERSIZE* MAX_CSVFILE_COLUMNS)+ (CSV_REGISTERSIZE*col); // Deleted 2015-Nov-15 by D. Lyons
         float value = (float)myArray->GetAt(row)->GetElement(3);        // Revised 2015-Nov-15 "col" changed to "3" by D. Lyons
         DWORD lowhigh;
         float*pFloatValue;
         pFloatValue= (float *)&lowhigh;
         *pFloatValue = value;

         //if (CSV_CLONEMODBUS)      // Deleted 2015-Nov-19 by D. Lyons - not used & not needed
         //   SwopWords(&lowhigh);   // Deleted 2015-Nov-19 by D. Lyons
		 if (prefix == 4)
		 {
         m_parentInterface->SetRegister(registerNum, LOWORD(lowhigh));
         m_parentInterface->SetRegister(registerNum+1, HIWORD(lowhigh));
		 //pGlobalDialog->RedrawListItems(3,0,6554);                     // repaint all of the Holding Regs by DL on 2015-Nov-16
		 UsingRegisters = true;
		 count++;
		 }
		 if (prefix == 3)
		 {
		 m_parentInterface->SetAnalogs(registerNum, LOWORD(lowhigh));
         m_parentInterface->SetAnalogs(registerNum+1, HIWORD(lowhigh));
		 //pGlobalDialog->RedrawListItems(2,0,6554);                     // repaint all of the Analog Regs by DL on 2015-Nov-16
		 UsingAnalogs = true;
		 count++;
		 }
      }                                                                // Added 2015-Nov-15 by D. Lyons 
   else                                                                // Added 2015-Nov-15 by D. Lyons
      {                                                                // Added 2015-Nov-15 by D. Lyons
	     if (registerNum > 65535) registerNum = 65535;                 // Added 2015-Nov-15 by D. Lyons
	     int value = (int)myArray->GetAt(row)->GetElement(3);          // Added 2015-Nov-15 by D. Lyons
		 if (prefix == 4)                                              // Added 2015-Nov-16 by D. Lyons
		 {
	     m_parentInterface->SetRegister(registerNum,  value);          // Added 2015-Nov-15 by D. Lyons
		 UsingRegisters = true;
		 }
		 if (prefix == 3)                                              // Added 2015-Nov-16 by D. Lyons
		 {
		 m_parentInterface->SetAnalogs(registerNum,  value);           // Added 2015-Nov-16 by D. Lyons
		 UsingAnalogs = true;
		 }
		 if (prefix == 0)                                              // Added 2015-Nov-16 by D. Lyons
		 {
		 m_parentInterface->SetCoils(registerNum, value);              // Added 2015-Nov-16 by D. Lyons
		 UsingCoils = true;
		 }
		 if (prefix == 1)                                              // Added 2015-Nov-16 by D. Lyons
		 {
		 m_parentInterface->SetInputs(registerNum, value);             // Added 2015-Nov-16 by D. Lyons
		 UsingInputs = true;
		 }
         if ((prefix >= 0) && (prefix <= 4) && (prefix != 2))count++;  // Added 2015-Nov-15 by D. Lyons
	  }                                                                // Added 2015-Nov-15 by D. Lyons
   }
   // we can only call on the GUI thread once we have un-locked
   if (pGlobalDialog)
   {
       if (UsingCoils) pGlobalDialog->RedrawListItems(0,0,6554);		// Updated all by DL on 2015-Nov-18
       if (UsingInputs) pGlobalDialog->RedrawListItems(1,0,6554);		// Updated all by DL on 2015-Nov-18
       if (UsingAnalogs) pGlobalDialog->RedrawListItems(2,0,6554);		// Updated all by DL on 2015-Nov-18
       if (UsingRegisters) pGlobalDialog->RedrawListItems(3,0,6554);	// Updated all by DL on 2015-Nov-18
   }
   // Added next three statements to show Import Line Count if Manual Import on 2015-Nov-19 by DL
   // Moved into Import Status Field in CSVFileImportDlg.cpp on 2015-Nov-26 by DL
   //char buffer[20];
   //if (manual)
   //MessageBox(NULL,_itoa((int)count, buffer, 10), "Import Rows OK", MB_OK);      // prints number of active rows
   return(count);
}