/////////////////////////////////////////////////////////////////////////////
//
// FILE: RS232Port.cpp : implementation file
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
// Implementation of the CRS232Port class.
// Requires MFC headder <afxmt.h>
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RS232Port.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
BOOL   DumpBuffer = FALSE;      // 2015-01-25 by Doug Lyons to handle dumping receive buffer on timeout

/*
// --------------------------------------- SaveDebugInfo ---------------------------
// STATIC : Debug Info to file by DL on 2015-01-10 & 2015-01-12 (dat to LogFile)
BOOL SaveDebugInfo(CString str)
{
CFileException ex;
CFile LogFile;
      // Try and open the file for output here
   if (!LogFile.Open("DEBUG.TXT", CFile::modeWrite | CFile::shareExclusive | CFile::modeCreate | CFile::modeNoTruncate, &ex) )
   {
      // complain if we get an error
      // no need to delete the ex object

      TCHAR szError[1024];
      ex.GetErrorMessage(szError, 1024);
      OutputDebugString( "Couldn't open destination file: ");
      OutputDebugString( szError);
      
      return FALSE;
   }
   //dat.Seek(0L, CFile::end);		// Seek to the end so we can append to the file
   LogFile.SeekToEnd();
   // print it out
   char *temp;                      // We want to take away the ending "\n" so this pointer is needed
   LPTSTR p = str.GetBuffer();      // This allows us to get a copy of the str that we can work with
   temp = strchr(p, '\n');          // This finds the ending "\n" position for us

   if(temp != NULL)                 // If the ending "\n" was found
    *temp = _T('\0');               // Then replace it with a "\0"
   str.ReleaseBuffer();             // Release the CString buffer
   str = str + "\r\n";              // Add the needed Windows "\r\n"
   LogFile.Write(str, strlen(str));     // Here we print all
   LogFile.Close();                     // Close the file at the end
   return TRUE;                     // Return TRUE for good results
} // SaveDebugInfo
*/


// --------------------------------------- WriteToFile ---------------------------
// WriteToFile by DL on 2016-12-26 which sends str to a DEBUG.TXT output file
// Replaces SaveDebugInfo and adds more capability to allow calling from one line
bool CRS232Port::WriteToFile(CString str)
{
CFileException ex;
CFile LogFile;
CString entry;

   if (m_CommsTimeShow)
   {
   SYSTEMTIME sysTime;
   GetLocalTime(&sysTime);
   entry.Format("%02d:%02d:%02d.%03d %s\n",
      sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds, str);
   }
   else
      entry = str;
   str = entry;
      // Try and open the file for output here
   if (!LogFile.Open("DEBUG.TXT", CFile::modeWrite | CFile::shareExclusive | CFile::modeCreate | CFile::modeNoTruncate, &ex) )
   {
      // complain if we get an error
      // no need to delete the ex object

      TCHAR szError[1024];
      ex.GetErrorMessage(szError, 1024);
      OutputDebugString( "Couldn't open destination file: ");
      OutputDebugString( szError);
      return FALSE;
   }
   //dat.Seek(0L, CFile::end);		// Seek to the end so we can append to the file
   LogFile.SeekToEnd();
   // print it out
   char *temp;                      // We want to take away the ending "\n" so this pointer is needed
   LPTSTR p = str.GetBuffer();      // This allows us to get a copy of the str that we can work with
   temp = strchr(p, '\n');          // This finds the ending "\n" position for us

   if(temp != NULL)                 // If the ending "\n" was found
    *temp = _T('\0');               // Then replace it with a "\0"
   str.ReleaseBuffer();             // Release the CString buffer
   str = str + "\r\n";              // Add the needed Windows "\r\n"
   LogFile.Write(str, strlen(str));     // Here we print all
   LogFile.Close();                     // Close the file at the end
   return TRUE;                     // Return TRUE for good results
}// WriteToFile

// ------------------------------ AsyncFriend ------------------------------
// Worker thread function, this is a friend of the CRS232Port class
//
UINT AsyncFriend(LPVOID pParam)
{
CHAR     debugStr2[MAX_DEBUG_STR_LEN];
CHAR     debugStr[MAX_DEBUG_STR_LEN];
CRS232Port* portPtr;

   portPtr = (CRS232Port*)pParam;
   try 
   { 
		
      // call the function the thread will run in
      if (portPtr->IsKindOf(RUNTIME_CLASS( CRS232Port)))
      {
         // wait untill the Application is ready for us
         CSingleLock lk(&portPtr->m_threadStartupEvent);
         lk.Lock(5000); // wait max 5 seconds

         portPtr->RSStateChanged(RSPORTCURRENTLY_VOID);
         
         // stays in a loop at this point
         portPtr->Poll(debugStr);
      }
      else
      {
         strcpy_s(debugStr2, sizeof(debugStr2), "CRS232Port AsyncFriend pointer corruption!!!!\n");
         OutputDebugString(debugStr2);
      }
   }
   catch (...) 
   {
      CString msg;
         msg.Format("INTERNAL APPLICATION ERROR FILE %s LINE: %d\n%s\n%s", 
            __FILE__, __LINE__, __MY_APPVERSION__, __DATE__);
      OutputDebugString(msg);

      portPtr->RSDataMessage(msg);
      //OutputDebugString( "Catch\n" );
      //sprintf(debugStr2, "CRS232Port AsyncFriend Exception !!!!\n");
      //OutputDebugString(debugStr2);
   }
   portPtr->m_listenThreadStatus = RS232_EX_TERMINATED;
   {
   CString d;
      d.Format("[Comms thread %4X Terminating.]\n", GetCurrentThreadId());
      OutputDebugString(d);
   }
   portPtr->m_threadDeadEvent.SetEvent();// CEvent
   OutputDebugString("Port object thread returning to System\n");
   
   //AfxEndThread(0);

   return(0);
} // AsyncFriend


char commsParityStr[] = "NOEMS";   // Noparity Oddparity Evenparity Markparity Spaceparity
char commsStopStr[][4] = {// ONESTOPBIT ONE5STOPBITS TWOSTOPBITS
   "1",
   "1.5",
   "2"
};

char commsRTSStr[][6] = { // Changed from 5 to 6 to handle 'R-off' by D. Lyons on 2016-07-26
   "R-off",       //  RTS_CONTROL_DISABLE // Changed from NULL by D. Lyons on 2016-07-26
   "R-on",        //  RTS_CONTROL_ENABLE  // Changed from 'R-en' by D. Lyons on 2016-07-26
   "R-hs",        //  RTS_CONTROL_HANDSHAKE
   "R-tg"         //  RTS_CONTROL_TOGGLE   
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC( CRS232Port, CObject);

CRS232Port::CRS232Port()
{
   m_debuggerStep = 0;
   m_masterHasWork = FALSE;

   h232Port = INVALID_HANDLE_VALUE;
   m_lastModemStatus = 0xFFFF;

   // create the listening thread
   m_listenThreadStatus = RS232_EX_PENDING;
   m_pWorkerThread      = AfxBeginThread((AFX_THREADPROC)AsyncFriend,
                                     this,
                                     THREAD_PRIORITY_TIME_CRITICAL, 0,
                                     CREATE_SUSPENDED
                                    );

   rxBufferIndex = 0;   // set up our RX buffer
   memset(rxBuffer, 0 , sizeof(rxBuffer));
   keepPolling = TRUE;
   m_lastCharIncommingtime = GetTickCount() + PORT_MAX_IDLETIME;  // 2015-02-01 by DL Removed multiplier of 5.
   InitializeCriticalSection(&critSec);
}

CRS232Port::~CRS232Port()
{
   ClosePort();
   // caller must wait for thread to die, if not already dead
   OutputDebugString("Port object deleted\n");
   TerminateThread(m_pWorkerThread,0);
}

#ifdef _DEBUG
VOID CRS232Port::Dump(CDumpContext& dc) const
{
   // call the base class first
   CObject::Dump(dc);

   // dump our object to the debuggers output
   // all important members can be dumped at this stage.
   dc << "RS232 Port: " << "\n";
} // Dump
#endif // _DEBUG

// ------------------------- RSStateChanged ----------------------------
void CRS232Port::RSStateChanged(DWORD state)
{
   // do nothing in the base class
} // RSStateChanged
/*
// ------------------------- RSDataDebugger ----------------------------
void CRS232Port::RSDataDebugger(const BYTE * buffer, LONG length, BOOL transmit)
{
CString prefix, ASCIIdata;
LONG index, i;
   if (transmit)
      prefix = "TX:";
   else
      prefix = "RX:";
   index = 0;
   while (index < length)
   {
      i=0;
      OutputDebugString("\n");
      OutputDebugString(prefix);
      while ((index+i < length)&&(i<8))
      {
         ASCIIdata.Format("%02X", buffer[index+i]);
         OutputDebugString(ASCIIdata);
         i++;
      }
      index +=8;
   }
} // RSDataDebugger
  */
// --------------------------------- OpenPort ---------------------------
BOOL CRS232Port::OpenPort(LPCTSTR oPortName)
{
CHAR longPortName[MAX_COMPORT_NAME];
BOOL  error;
CString msg;

   portNameS = oPortName;
   GetLongComPortName(portNameS, longPortName);  // get windows port name in long format   
   // for COM ports >COM9 Windows uses a different naming format, hence this 
   // little catch and we have to be compatible.

   portName = longPortName;
   h232Port =  CreateFile( portName,
                               GENERIC_READ | GENERIC_WRITE,
                               (DWORD)NULL,   // exclusive access
                               NULL,          // no security
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL           // hTemplate
                             );
   if (INVALID_HANDLE_VALUE ==h232Port)
   {
      AfxMessageBox("Error: Could not open the RS232 port!", MB_OK|MB_ICONEXCLAMATION);
      return FALSE;
   }

   msg.Format("Port %s opened.\n" , longPortName);
//   OutputDebugString(msg);
   RSDataMessage(msg);

   // set up the comms parameters now that we have a handle
   error = SetupComm(h232Port,
                     (DWORD)2048, (DWORD)2048);

   msg.Format("Port I/O buffers configured.\n");
   //OutputDebugString(msg);
   RSDataMessage(msg);
   RSStateChanged(RSPORTCURRENTLY_IDLE);

   m_lastCharIncommingtime = GetTickCount() + PORT_MAX_IDLETIME;  // 2015-02-01 by DL Removed multiplier of 5.
   return (TRUE);
} // OpenPort

void CRS232Port::OnHWError(DWORD dwCommError)
{
	// OnHWError was previously empty but now it will detect RS-232 errors - modified by DL on 2016-11-06
	// This handles displaying any COMx Port errors or messages by displaying them in the COMMS window
	BOOL fOVERRUN, fRXOVER, fRXPARITY, fTXFULL;     // define some bools to hold individual error flags
	BOOL fBREAK, fFRAME, fMODE;

	// Get error flags.
	fMODE = dwCommError & CE_MODE;                  // mask out individual bits to seperate error flags
	fBREAK = dwCommError & CE_BREAK;
	fFRAME = dwCommError & CE_FRAME;
	fRXOVER = dwCommError & CE_RXOVER;
	fTXFULL = dwCommError & CE_TXFULL;
	fOVERRUN = dwCommError & CE_OVERRUN;
	fRXPARITY = dwCommError & CE_RXPARITY;
	
	// Only look at RS-232 errors which means that we ignore LPT errors
	dwCommError = dwCommError & (CE_BREAK | CE_FRAME | CE_RXOVER | CE_TXFULL | CE_OVERRUN | CE_RXPARITY | CE_MODE);
	if (dwCommError)
	{
		CString errorout = "\0";

		if (fRXPARITY)
			errorout = "COM PORT ERROR: Parity Error";
		if (fMODE)
		{
			if (strlen(errorout) > 2)
				errorout = errorout + ", Mode Not Supported";
			else
				errorout = errorout + "COM PORT ERROR: Mode Not Supported";
		}
		if (fFRAME)
		{
			if (strlen(errorout) > 2)
				errorout = errorout + ", Framing Error";
			else
				errorout = errorout + "COM PORT ERROR: Framing Error";
		}
		if (fBREAK)
		{
			if (strlen(errorout) > 2)
				errorout = errorout + ", Break Received";
			else
				errorout = errorout +  "COM PORT ERROR: Break Received";
		}
		if (fRXOVER)
		{
			if (strlen(errorout) > 2)
				errorout = errorout + ", Receive Queue Overflow";
			else
				errorout = errorout +  "COM PORT ERROR: Receive Queue Overflow";
		}
		if (fOVERRUN)
		{
			if (strlen(errorout) > 2)
				errorout = errorout + ", Receive OverRun";
			else
				errorout = errorout +  "COM PORT ERROR: Receive OverRun";
		}
		if (fTXFULL)
		{
			if (strlen(errorout) > 2)
				errorout = errorout + ", TX queue is full";
			else
				errorout = errorout +  "COM PORT ERROR: TX queue is full";
		}
		if (strlen(errorout) > 2)                      // if we have a message by now then line terminate it and display it
		{
			errorout = errorout +  ".\n";
            if (m_SaveDebugToFile) WriteToFile(errorout);  // Added section on 2016-12-26 by DL to test before saving to Log file
			RSDataMessage(errorout);
		}
	}
	// End of modification for OnHWError by DL on 2016-11-06
}


// ----------------------- ReConfigurePort ------------------------------
//
BOOL CRS232Port::ReConfigurePort()
{
   return (ConfigurePort(dcb.BaudRate, dcb.ByteSize, dcb.Parity, dcb.fRtsControl, (NOPARITY==dcb.Parity?FALSE:TRUE),m_longTimeouts));
}

// ---------------------------------- ConfigurePort ---------------------
//
BOOL CRS232Port::ConfigurePort(DWORD  baud, 
                               DWORD byteSize, 
                               DWORD parity, 
                               DWORD stopBits,
                               DWORD rts,
                               DWORD checkParity,
							   DWORD longTimeouts)
{
BOOL error;
COMMTIMEOUTS timeout;

   if (INVALID_HANDLE_VALUE==h232Port)
   {
   CString errMsg("Error: Could not set up RS232 port. Port not open!");

      RSDataMessage(errMsg);
      AfxMessageBox(errMsg, MB_OK|MB_ICONEXCLAMATION);
      return FALSE;
   }
   
   m_lastModemStatus = 0xFFFF;

   // Set up the DCB with our settings
   // Get it first so we fill all members
   error = GetCommState(h232Port,
                           &dcb);
   dcb.BaudRate = baud;
   dcb.ByteSize = (BYTE)byteSize;
   dcb.Parity = (BYTE)parity;
   dcb.fParity = checkParity;
   dcb.StopBits = (BYTE)stopBits;

   // set the default parameters
   dcb.fOutxDsrFlow = FALSE;
//   ASSERT(0);
   dcb.fDsrSensitivity = FALSE;   // SCM22 appears to be DSR sensing = TRUE;//
   dcb.fOutxCtsFlow = FALSE; // TRUE;

   // set the port for hardware handshaking
   dcb.fRtsControl = rts;
   dcb.fDtrControl = DTR_CONTROL_ENABLE; //DTR_CONTROL_HANDSHAKE;   was DTR_CONTROL_DISABLE;//
   dcb.fAbortOnError = TRUE;
   dcb.fBinary = TRUE;
   dcb.fTXContinueOnXoff  = FALSE;
   dcb.fOutX = FALSE;
   dcb.fInX = FALSE;
   dcb.fErrorChar = 0;
   dcb.fNull = 0;
   dcb.fDummy2 = 0;

   // assign it
   error = SetCommState(h232Port,
                           &dcb);
   CString paramsSet;
   if (!error)
   {
   CHAR  errorMsg[MAX_ERROR_STR_LEN];
   CHAR *errorMsgPtr = errorMsg;
   DWORD ntError = GetLastError();

      // call the API using the default system language
      errorMsg[0] = '\0';
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 
                    NULL,                    // ignored, search the system only
                    ntError,                 // error code passed in
                    GetSystemDefaultLangID(),// English ,English US
                    errorMsg,                // message buffer
                    sizeof(errorMsg),        // length
                    NULL);                   // optional parameter list
      // 
      while(*errorMsgPtr)                    // Find any '\r' and replace with ' '
      {
         errorMsgPtr++;
         if (( *errorMsgPtr == '\r') | ( *errorMsgPtr == '\n'))
            *errorMsgPtr = ' ';  
      }
      paramsSet.Format("Error configuring port for %d,%d,%c,%s : (error %d) - %s", 
                                                      baud, 
                                                      byteSize,
                                                      commsParityStr[parity], 
                                                      commsStopStr[stopBits],
                                                      ntError,
                                                      errorMsg);
      RSDataMessage(paramsSet);
      AfxMessageBox( paramsSet, MB_OK | MB_ICONEXCLAMATION);
      ClosePort();
      return FALSE;
   }
   else
   {
      paramsSet.Format("Port configured %d,%d,%c,%s", baud, 
                                                      byteSize,
                                                      commsParityStr[parity], 
                                                      commsStopStr[stopBits]);
      RSDataMessage(paramsSet);
   }


   // Set up the timeouts to use, they are quite short, since we will loop anyway.
   // Do not make them zero, else we will have a CPU load problem. Too large a value, 
   // and we have to wait for comms to time out when shutting down

   // fill timeout structure
   GetCommTimeouts(h232Port, &timeout);
//   timeout.ReadIntervalTimeout = 100;           // 100ms between incomming chars.
//   timeout.ReadTotalTimeoutConstant = 500;
   timeout.ReadTotalTimeoutMultiplier = 0;      // #chars to read does not add to timeout amount
   timeout.WriteTotalTimeoutConstant = 2000;
   timeout.WriteTotalTimeoutMultiplier = 60;    // 60ms per char sent
   if (longTimeouts)
   {
      timeout.ReadIntervalTimeout = 250;           // 250ms between incomming chars.
      timeout.ReadTotalTimeoutConstant = 1000;
   }
   else
   {
      timeout.ReadIntervalTimeout = 100;           // 100ms between incomming chars.
      timeout.ReadTotalTimeoutConstant = 500;
   }

   error = SetCommTimeouts(h232Port, &timeout);
   if (! error)
   {
   CString errMsg("Error: Could not configure the timeouts.");  // 2015-01-31 by DL Added "not"

      RSDataMessage(errMsg);
      AfxMessageBox(errMsg, MB_OK|MB_ICONEXCLAMATION);
      ClosePort();
      return FALSE;
   }

   CString TimeOutBuffer;
   TimeOutBuffer.Format("Timeouts configured (%dms/%dms)\n", timeout.ReadIntervalTimeout, timeout.ReadTotalTimeoutConstant);
   RSDataMessage(TimeOutBuffer);

   // done
   m_lastCharIncommingtime = GetTickCount() + PORT_MAX_IDLETIME;  // 2015-02-01 by DL Removed multiplier of 5.
   return (TRUE);
} // ConfigurePort

// ----------------------------------- Purge ---------------------------
// remove all characters from the RX buffer (eliminates back-log)
//
BOOL CRS232Port::Purge()
{
BOOL ret;

   if (!keepPolling)
      return FALSE;
   ASSERT(INVALID_HANDLE_VALUE != h232Port);
   // interlock
   EnterCriticalSection(&critSec);

   ret = PurgeComm(h232Port, PURGE_RXCLEAR);
   LeaveCriticalSection(&critSec);
   return (ret);
} // Purge

// ---------------------------------- ClosePort -------------------------
// PURPOSE:
BOOL CRS232Port::ClosePort()
{
   
   // interlock
   EnterCriticalSection(&critSec);
   if (INVALID_HANDLE_VALUE != h232Port)
   {
      // tell dialog that we are closing it (this is so fast anyway)
      RSStateChanged(RSPORTCURRENTLY_CLOSING);
      RSDataMessage("Closing port\n");
      CloseHandle(h232Port);
   }
   // set HANDLE to closed so we know in future
   h232Port = INVALID_HANDLE_VALUE;
   LeaveCriticalSection(&critSec);
   return (TRUE);
} // ClosePort

// ---------------------------------- Send --------------------------------
// PURPOSE: This one should be clear
//
LONG CRS232Port::Send(int numberOfBytestoWrite, 
                      const BYTE* bufferPtr, 
                      CHAR* debugStrPtr
                     )
{
BOOL error;
DWORD numberOfBytesWritten=0;

   ASSERT (numberOfBytestoWrite>0);
   if (!keepPolling)
      return FALSE;
   // send the bytes out on the wire
   if (INVALID_HANDLE_VALUE==h232Port)
   {
      OutputDebugString("Error: Cannot send. Port closed!\n");   // busy shutting down the app
      return FALSE;
   }
   
   // flag our intention
   RSStateChanged(RSPORTCURRENTLY_WRITTING);
   // interlock
   EnterCriticalSection(&critSec);
   //error = EscapeCommFunction(h232Port, CLRDTR);
//   ASSERT(0);
   error = WriteFile(h232Port,
                      bufferPtr,
                      numberOfBytestoWrite,                 // nNumberOfBytesToWrite,
                      &numberOfBytesWritten,
                      NULL); 
  // Sleep(50);
//   error = EscapeCommFunction(h232Port, SETDTR);
   LeaveCriticalSection(&critSec);
   
   // output it in windows debugger
   RSDataDebugger(bufferPtr, numberOfBytesWritten, TRUE);
   return (TRUE);
} // Send

// ------------------------------ GenDataDebugger ------------------------------
/*static*/ 
void CRS232Port::GenDataDebugger(const BYTE * buffer, LONG length, int transmit)
{
if (length)                 // Added 'if (length)' to wrap whole routine by DL on 2015-Dec-15
{
CString debuggerString;
CString entry;				// Added 2015-01-12 by DL for handling Sending Times to Debug File
BYTE *data;
byte hiNib,loNib;

   //convert BIN-ary to ASCII for display
   data = new BYTE[(length*3)+1];
   BYTE* pChar = data;
   for (int i = 0; i < length; i++)
   {
       hiNib = ( *(buffer+i) >>4) & 0x0f;
       loNib = ( *(buffer+i)    ) & 0x0f;
       *pChar++ = ( (hiNib < 0x0A) ? ('0' + hiNib) : ('A' + hiNib-10) );
       *pChar++ = ( (loNib < 0x0A) ? ('0' + loNib) : ('A' + loNib-10) );
       *pChar++ = ' ';
   }
   --*pChar = '\0';
   //data[(length*2)] = '\0';

   if (transmit)
   {
      //if (length)                               // Deleted 2015-Dec-15 by DL because length=0 now handled at beginning
      pGlobalDialog->UpdateCharactersSent();
      debuggerString.Format("TX:%s\n", data);
   }
   else
   {
      //if (length)                               // Deleted 2015-Dec-15 by DL because length=0 now handled at beginning
      //{            // Added 2015-01-24 to put debuggerSting inside "if (length)" & Removed 2015-Dec-15 by DL
       pGlobalDialog->UpdateCharactersReceived();
       debuggerString.Format("RX:%s\n", data);
      //}            // Added 2015-01-24 to put debuggerSting inside "if (length)" & Removed 2015-Dec-15 by DL
   }
   // Send to debugger list-box
   //if (length)                               // Deleted 2015-Dec-15 by DL because length=0 now handled at beginning
   //{                                         // Deleted 2015-Dec-15 by DL because not needed without 'if (length)'
	pGlobalDialog->AddCommsDebugString(debuggerString);
   //}                                         // Deleted 2015-Dec-15 by DL because not needed without 'if (length)'
   if ((m_SaveDebugToFile) && (length)) // Added length check on 2015-01-13 by DL to keep spam out of Log file
   {
      WriteToFile(debuggerString);      // Added section on 2016-12-26 by DL to test before saving to Log file
   }
#ifdef _COMMS_DEBUGGING
   OutputDebugString(debuggerString);
#endif

   delete (data);
}
} // GenDataDebugger


// ---------------------------- UpdateModemStatus -------------------------
void CRS232Port::UpdateModemStatus()
{
DWORD status;
DWORD errornum;       // Added 2016-11-06 by DL for checking GetCommModemStatus

   if (!keepPolling)
      return;
   // get ready to read bytes
   if (INVALID_HANDLE_VALUE==h232Port)
   {
      OutputDebugString("Error: Cannot get modem. Port closed!\n");   // busy shutting down the app
      Sleep(0);
      return;
   }
  
   //GetCommModemStatus(h232Port,  &status);           // Deleted 2016-11-06 and replaced by statements below by DL
   errornum = GetCommModemStatus(h232Port,  &status);  // Added 2016-11-06 by DL for checking GetCommModemStatus
   // Returned value non-zero indicates success        // Added 2016-11-06 by DL for checking GetCommModemStatus
   if (errornum != 0)                                  // Added 2016-11-06 by DL for checking GetCommModemStatus
   {                                                   // Added 2016-11-06 by DL for checking GetCommModemStatus
	status = status & 0xF0;                            // Added 2016-11-06 by DL for filtering out non-Modem values
	RSModemStatus(status);

	// display on scope when status changes
	if (m_lastModemStatus != status)
	{
	CString modemDescr;
      modemDescr.Format("Modem status : [CTS %c] [DSR %c] [RING %c]",
                           (status & MS_CTS_ON ? 'X' : '_'),
                           (status & MS_DSR_ON ? 'X' : '_'),
                           (status & MS_RING_ON? 'X' : '_')
                        );
      RSDataMessage(modemDescr);

      m_lastModemStatus = status;
	}
   }                                                   // Added 2016-11-06 by DL for checking GetCommModemStatus
}


// ------------------------------ Receive ------------------------------------
// return TRUE if a telegram was read.
// return false if no packet was read, this routine will keep trying to build a 
// full telegram.
LONG CRS232Port::Receive(DWORD * numberOfBytesLong, CHAR* bufferPtr, CHAR* debugStrPtr)
{
BOOL error;
DWORD numberOfBytesWritten=0;
DWORD readAheadLength;        // # bytes we probably need (depends on what we have in the buffer so far)
DWORD numberOfBytesRead, dwCommError, tickCount;
BYTE * rxBuffPtr;
CString debuggerStr;

   RSStateChanged(RSPORTCURRENTLY_READING);
   if (!keepPolling)
      return FALSE;
   // get ready to read bytes
   if (INVALID_HANDLE_VALUE==h232Port)
   {
      OutputDebugString("Error: Cannot RX. Port closed!\n");   // busy shutting down the app
      Sleep(0);
      return FALSE;
   }

   // check if port has been idle for too long
   tickCount = GetTickCount();
   if (((LONG)tickCount - (LONG)m_lastCharIncommingtime) > PORT_MAX_IDLETIME)
   {
      m_lastCharIncommingtime = tickCount + PORT_MAX_IDLETIME;
	  DumpBuffer = TRUE;                  // 2015-01-25 by DL Used to empty noise buffer
      if (rxBufferIndex)
      {
      CString msg;
         msg = "Port idle, buffer cleared!\n";   // idle
         OutputDebugString(msg);
         RSDataMessage(msg);
         //OnHWError(0);
      }
      // start recieving from fresh again
      rxBufferIndex = 0;   // set up our RX buffer
      memset(rxBuffer, 0 , sizeof(rxBuffer));
      OnHWError(0);
   }

   rxBuffPtr = &rxBuffer[rxBufferIndex];
   *numberOfBytesLong = 0;

   // Work out our read-ahead length
   //
   readAheadLength = SimulationSerialPort::CalculateReadAheadLength(rxBuffer, rxBufferIndex);

   ASSERT(rxBufferIndex + readAheadLength < sizeof(rxBuffer));
#ifdef _COMMS_DEBUGGING
//   debuggerStr.Format("Read-ahead %d bytes\n", readAheadLength);
//   OutputDebugString(debuggerStr);
#endif
   
   // read what-ever is on the port, up to a maximum of readAheadLength.
   numberOfBytesRead = 0;
   EnterCriticalSection(&critSec);
   error = ReadFile(h232Port,
                      rxBuffPtr,
                      readAheadLength,                 // nNumberOfBytesToWrite,
                      &numberOfBytesRead,
                      NULL);
   LeaveCriticalSection(&critSec);
   rxBufferIndex+=numberOfBytesRead;
 
#ifdef _COMMS_DEBUGGING
//   debuggerStr.Format("Bytes Read %d bytes\n", numberOfBytesRead);     // 2015-01-22 removed comments by DL
//   OutputDebugString(debuggerStr);                                   // 2015-01-22 removed comments by DL
#endif

   // display the chars in the debugger   // It looks like this should test 'if (numberOfBytesRead != 0) before doing this. by DL on 2015-Dec-15
   RSDataDebugger(rxBuffPtr, numberOfBytesRead, FALSE);
   // keep idle tick count alive since we just got a char(s)
   if (numberOfBytesRead)
   {
      m_lastCharIncommingtime = GetTickCount();
//	  OutputDebugString("GetTickCount\n");
   }
   if (0==error)
   {
      // clear it
	  ClearCommError(h232Port, &dwCommError, NULL);
      // clear the buffers
      rxBufferIndex = 0;
      numberOfBytesRead = 0;
      OnHWError(dwCommError);
   }
   // done reading
   RSStateChanged(RSPORTCURRENTLY_IDLE);
   if (rxBufferIndex)
   {
      *numberOfBytesLong = numberOfBytesRead;
      memcpy(bufferPtr, rxBuffPtr, numberOfBytesRead);
      return(TRUE);
   }
//   OutputDebugString("Return FALSE\n");
   return (FALSE);   // no data avail yet
} // Receive


// -------------------------------------- Poll ------------------------------
// The main simulator thread loop
//
void CRS232Port::Poll(CHAR * debugStr)
{
DWORD numBytes;
CHAR buffer[MAX_MODBUS_MESSAGELEN];
CHAR debugMsg[80];
BOOL discard;

   while (keepPolling)  // check to see if we must quit this loop
   {
      discard = FALSE;

      UpdateModemStatus();
      if ((! m_masterHasWork) && (Receive(&numBytes, buffer, debugMsg)) )
      {
         // test to see if they want this thread to die.
         if (!keepPolling)
            break;
         //CMODMessage msg(buffer, numBytes);
         //if (msg.CRCOK())
         //{
            // build a response etc
            OnProcessData(buffer, numBytes, &discard);   //
         //}
      }
      else
      {
         m_masterHasWork = FALSE;

		 if (DumpBuffer && m_noiseLength)                  // 2015-01-25 by DL to empty noise buffer. Added m_noiseLength 2015-02-01
		 {                                                 // 2015-01-25 by DL to empty noise buffer
		    discard = TRUE;                                // 2015-01-25 by DL to empty noise buffer
		    OutputDebugString("Timeout on Message Recieve. Discarding Data\n");        // 2015-01-25 by DL to empty noise buffer
		    DumpBuffer = FALSE;                            // 2015-01-25 by DL to empty noise buffer
            if (m_SaveDebugToFile)                         // Section Added on 2015-02-02 by DL to send to Log File
            {
              WriteToFile("Timeout on Message Receive. Discarding Data.\n");
		    }
		 }// 2015-01-25 by DL to empty noise buffer
		 else                                           // 2015-01-25 by DL to empty noise buffer
			 discard = FALSE;                           // 2015-01-25 by DL to empty noise buffer

         if (! OnProcessData(buffer, 0 , &discard))
            Sleep(10);  // allow this thread some "space" when it is not doing anything
                        // only needed at times when the port is not available.
      }
      if (discard)
         rxBufferIndex = 0;
   }
   OutputDebugString("Poll loop exiting\n");
} // Poll


