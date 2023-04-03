/////////////////////////////////////////////////////////////////////////////
//
// FILE: RS232Port.cpp : headder file
//
// See _README.CPP
//
// interface for the CRS232Port class.
// Requires MFC headder <afxmt.h>
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_RS232PORT_H__19C837A9_9EB1_408D_83F7_6D85FDEF9739__INCLUDED_)
#define AFX_RS232PORT_H__19C837A9_9EB1_408D_83F7_6D85FDEF9739__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

extern CHAR * GetLongComPortName(LPCTSTR portName, LPSTR newName);
extern CHAR * FixComPortName(CHAR *portName);


#define RSPORTCURRENTLY_VOID        0
#define RSPORTCURRENTLY_READING     1
#define RSPORTCURRENTLY_WRITTING    2
#define RSPORTCURRENTLY_CLOSING     3
#define RSPORTCURRENTLY_OPENING     4
#define RSPORTCURRENTLY_IDLE        5


// server thread states
#define RS232_EX_PENDING         0
#define RS232_EX_SUSPENDED       1
#define RS232_EX_RUNNING         2
#define RS232_EX_TERMINATE       3
#define RS232_EX_TERMINATED      4


// # milliseconds to wait before discarding buffer contents
#define PORT_MAX_IDLETIME     2000 //after 2 seconds, kill all chars in the buffer // 2015-01-25 by DL Changed from 10

extern char commsParityStr[];   //NOPARITY ODDPARITY EVENPARITY MARKPARITY SPACEPARITY
extern char commsStopStr[][4];
extern char commsRTSStr[][6];   // Changed from 5 by D. Lyons on 2016-07-26

extern BOOL  m_SaveDebugToFile;			// Save Debug Output to file flag by DL on 2015-01-10
extern BOOL  m_CommsTimeShow;           // Added 2015-01-12 by DL to allow m_commsTimes to be used in RS232Port.cpp
//extern BOOL  m_CommsDecodeShow;			// Added 2016-12-28 by DL to allow use here
extern DWORD m_noiseLength;             // 2015-02-01 by DL to know we have data for Discarding Data on Timeout
extern BOOL  m_longTimeouts;			// 2016-10-15 by DL to get long timeout value

class CRS232Port : public CObject  
{
public:

   DECLARE_DYNAMIC(CRS232Port)

	CRS232Port();
	virtual ~CRS232Port();

   BOOL OpenPort(LPCTSTR oPortName);
   BOOL ConfigurePort(DWORD  baud, 
                      DWORD byteSize, 
                      DWORD parity, 
                      DWORD stopBits,
                      DWORD rts,
                      DWORD checkParity=TRUE,
					  DWORD longTimeout=FALSE);
   BOOL ClosePort();

   BOOL Purge();
   LONG Receive(DWORD *numberOfBytesRead, CHAR* bufferPtr, CHAR* debugStrPtr);
   LONG Send(int numberOfBytestoWrite, const BYTE* bufferPtr, CHAR* debugStrPtr);
   void UpdateModemStatus();

   // user must derive from this class and override this method.
//   virtual BOOL ProcessData(const CHAR *pBuffer, DWORD numBytes) = NULL;
   virtual void OnHWError(DWORD dwCommError);
   virtual BOOL OnProcessData(const CHAR *pBuffer, DWORD numBytes, BOOL *discardData) = NULL;

   // overridable notification functions
   virtual void RSStateChanged(DWORD state);
   virtual void RSDataDebugger(const BYTE * buffer, LONG length, BOOL transmit) = NULL;
   virtual void RSDataMessage(LPCTSTR msg) = NULL;
   virtual void RSModemStatus(DWORD status) = NULL;

   void Poll(CHAR * debugStr);
   UINT friend AsyncFriend(LPVOID pParam);

   // static members which implement some debugger Ceneric methods
   static void GenDataDebugger(const BYTE * buffer, LONG length, BOOL transmit);
   static bool CRS232Port::WriteToFile(CString str);

   BOOL ReConfigurePort();

   // diagnostic
#ifdef _DEBUG
   VOID Dump(CDumpContext& dc) const;
#endif

public:
   CString portNameS; // short (display) port name
   CEvent  m_threadStartupEvent;
   CEvent  m_threadDeadEvent;
   CWinThread *       m_pWorkerThread;

   DWORD m_debuggerStep;
   HANDLE   h232Port;   // a handle
   DWORD m_listenThreadStatus;
   BOOL  m_masterHasWork;        // if TRUE, then do not RX
   BOOL  keepPolling;
   DWORD m_lastCharIncommingtime;

private:
   CRITICAL_SECTION   critSec;   //sync object for this class
   CString  portName;   // long port name
   DCB      dcb;        // settings Device context block
   DWORD    m_lastModemStatus;

   BYTE  rxBuffer[1024];  //keep space for up to 2 messages
   DWORD rxBufferIndex;   // index to past last usable byte (is also the length)
};



#endif // !defined(AFX_RS232PORT_H__19C837A9_9EB1_408D_83F7_6D85FDEF9739__INCLUDED_)
