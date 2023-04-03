// DDKSocket.cpp: implementation of the CDDKSocket class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//extern bool IsWindows7OrGreater();   // Added by DL on 2015-01-05
//extern static void SockDataDebugger(const CHAR * buffer, LONG length, dataDebugAttrib att);


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


// WSA initalized
BOOLEAN				CDDKSocket::m_wsaInitialized = FALSE;
// WSA interlocking
CRITICAL_SECTION    CDDKSocket::m_wsaCS;
CRITICAL_SECTION *	CDDKSocket::m_pwsaCS = NULL;

IMPLEMENT_DYNAMIC( CDDKSocket, CObject );
//IMPLEMENT_DYNCREATE( CDDKSocket, CObject );

CDDKSocket::CDDKSocket()
{
   m_socket = INVALID_SOCKET;
   m_serverObject = FALSE;
   // do WSA start-up
   Initialize();

   { // Create a Client socket

   }
}


CDDKSocket::CDDKSocket(DWORD timeout)
{
   m_socket = INVALID_SOCKET;
   m_serverObject = FALSE;
   // do WSA start-up
   Initialize();

   { // Create a Client socket

   }
}

// -------------------------------- CloseSocket -------------------------------
//  Close a socket
BOOL CDDKSocket::CloseSocket(BOOL gracefully /*= FALSE */, SOCKET sock/* = NULL*/)
{
SOCKET lSock;
linger lingerOpt;
int sockError=0;
unsigned long  numZero=0;      // Added 2016-09-19 by DL for use in ioctlsocket argp value to set blocking ON
   
   if (NULL == sock)
      lSock = *m_pSocket;
   else
      lSock = sock;
   // As of 2016-09-21 this was previously the path always taken because the previous call forced gracefully to TRUE
   // Now "gracefully" tracks the "SO_LINGER" checkbox selection on the Ethernet Settings Dialog Box
   if ((gracefully)&&(NULL != lSock))
   {
      // set up LINGER to do a delayed (LINGERing) close.
      lingerOpt.l_linger = 2;    // Changed on 2016-09-21 by DL from SO_LINGER (128) to 2 second linger
      lingerOpt.l_onoff = TRUE;
      sockError = setsockopt(lSock, SOL_SOCKET, SO_LINGER, (CHAR*)&lingerOpt, sizeof(lingerOpt));

	  // for TCP sockets this is a blocking call and all data will be sent before the FIN
      sockError = shutdown(lSock, SD_SEND ); // disable any more sends on the socket
      if (sockError)
         //sockError = wsaerrno;                     // Deleted on 2016-09-21 by DL to simplify
	     sockError = WSAGetLastError();              // Added on 2016-09-21 by DL to simplify
      //if (WSAENOTSOCK == sockError+WSABASEERR)     // Deleted on 2016-09-21 by DL to simplify
	  if (WSAENOTSOCK == sockError)                  // Added on 2016-09-21 by DL to simplify
         lSock = NULL;
      //
   }
   // As of 2016-09-21 this was a path not taken by default, but now it is the default path (SO_LINGER unchecked)
   // Beginning of changes to allow non-gracefull close by DL on 2016-09-20
   if ((!gracefully)&&(NULL != lSock))
   {
	  // Make the Winsock Functions blocking by using FIONBIO (Non-Blocking I/O) with argp of zero.
	  // This sets up back to the original way it was handled in code. now closesocket will work in background.
	  // Added ioctlsocket for Blocking on 2016-09-21 by DL
	  ioctlsocket(lSock, FIONBIO, &numZero);
	  // set up LINGER to do a non-LINGER close.
      lingerOpt.l_linger = 0;
      lingerOpt.l_onoff = FALSE;
      sockError = setsockopt(lSock, SOL_SOCKET, SO_LINGER, (CHAR*)&lingerOpt, sizeof(lingerOpt));
   }
   // End of changes to allow non-gracefull close by DL on 2016-09-20
   if (NULL != lSock)
	 sockError = closesocket(lSock);
   return (TRUE);
} // CloseSocket

// -------------------------------- Initialize -------------------------------
void CDDKSocket::Initialize()
{
int errCode;
	
	// Init the WSA only once
	EnterWSA();
	if (!m_wsaInitialized)
	{
		m_wVersionRequested = MAKEWORD( 1, 1 );
		errCode = WSAStartup( m_wVersionRequested, &(m_WSAData) );
      if ( errCode != 0 )
      {
         // ? Tell the user that we could not find a usable WinSock DLL.
         OutputDebugString("Unable to find a useable WinSock DLL"); 
      }
	}
	LeaveWSA();

   // init all internal variables
	m_CallBackFuncPtr = NULL; // set the CB pointer to null
	m_listenThreadStatus  = SOCKET_EX_TERMINATED;  
	m_listenThreadCreated = FALSE;
	m_pWorkerThread       = NULL;
   memset((void * ) &(m_destSockaddr_in), 0, sizeof (m_destSockaddr_in));
   memset((void * ) &(m_localSockaddr_in), 0, sizeof (m_localSockaddr_in));
   m_socket = INVALID_SOCKET;
   m_serverBufferSize = 2048;
   m_buffer = NULL;
}


// ------------------------------- ~CDDKSocket --------------------------
CDDKSocket::~CDDKSocket()
{
   //OutputDebugString("In Base socket destructor\n");
   if (NULL!=m_socket)
   {
      OutputDebugString("Base: Client socket closing\n");
      closesocket(m_socket); //kill accepted instance immediately
   }

//	EnterWSA();
//	if (m_wsaInitialized)
//	{
//		WSACleanup();
//	}
//	LeaveWSA();
   OutputDebugString("[DEBUGSTEP]\n");
   if (NULL != m_buffer)
      delete m_buffer;
   //OutputDebugString("Leaving Base socket destructor\n");

}

#ifdef _DEBUG
VOID CDDKSocket::Dump(CDumpContext& dc) const
{
   // call the base class first
   CObject::Dump(dc);

   // dump our object to the debuggers output
   // all important members can be dumped at this stage.
   dc << "Socket: " << "\n";
} // Dump
#endif // _DEBUG


// ------------------------- SockStateChanged ----------------------------
void CDDKSocket::SockStateChanged(DWORD state)
{
   // do nothing in the base class
} // SockStateChanged

void CDDKSocket::SockDataMessage(LPCTSTR msg)
{

}

void CDDKSocket::SockDataDebugger(const CHAR * buffer, LONG length, dataDebugAttrib att)  // Added 2015-01-08
{
	CRS232Port::GenDataDebugger((BYTE*)buffer, length, att);  // Added 2015-01-08
}
/*
// ------------------------- SockDataDebugger ----------------------------
void CDDKSocket::SockDataDebugger(const CHAR * buffer, LONG length, dataDebugAttrib att)
{
// Appears not to be used. The RS232Port.cpp version of the GenDataDebugger is the one used. DL on 2015-01-05
	char buffer1[10];
	MessageBox(NULL, itoa((int)att, buffer1, 10), "DataDebugger", MB_OK);      // Left for Debug Purposes by DL on 2015-01-06
CString prefix, ASCIIdata;
LONG index, i;

   if (dataDebugTransmit == att)
      prefix.Format("(%d)TX:", (int)*m_pSocket);    // Changed from TX to RX by DL on 2015-01-02 & Back on 2015-01-05
   else
      if (dataDebugReceive == att)
         prefix.Format("(%d)RX:", (int)*m_pSocket);    // Changed from RX to TX by DL on 2015-01-02 & Back on 2015-01-05
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
   if (dataDebugOther == att)
      OutputDebugString("]");
} // SockDataDebugger
*/

// --------------------------------- GetSockError -------------------------
void CDDKSocket::GetSockError(CHAR * errString, BOOL reset/*=true*/)
{
LONG wx = wsaerrno;

   sprintf(errString,sys_wsaerrlist[wx]);
    if (reset) WSASetLastError(0);
} // GetSockError

// -------------------------------- EnterWSA -------------------------------
void CDDKSocket::EnterWSA()
{
   // If the critical section has not been instantiated,
   // instantiate and initialize and enter
   if( NULL != m_pwsaCS)
      EnterCriticalSection(m_pwsaCS);
   else
   {
      m_pwsaCS = &m_wsaCS;
      InitializeCriticalSection(m_pwsaCS);
      EnterCriticalSection(m_pwsaCS);
   }
} // EnterWSA


// -------------------------------- LeaveWSA -------------------------------
void CDDKSocket::LeaveWSA()
{
	LeaveCriticalSection(m_pwsaCS);
}



// --------------------------- Receive -------------------------------
// RETURNS : -1 if error, or the number actually read from socket
LONG CDDKSocket::Recieve(SOCKET ackSock, //The accepted socket
                           int    numberOfBytesToRead,// -> number of bytes to read.
                           CHAR * BufferPtr,       //<- Data read from socket    
                           CHAR * debugStrPtr       //Any errors 
                          )
{
int numread;
int temp;


   
   //call recv func
   int check1 = 0x1234;
   numread =  recv(ackSock,                    // Our precious socket
                   BufferPtr,//buffPtr,                  // RxBuffer 
                   (INT) numberOfBytesToRead,
                   (INT) NULL                  // no fancy flaggies
                  ); 
   int check2 = 0x5678;

   if (numread != 0xffffffff)
      CDDKSocket::SockDataDebugger(BufferPtr, numread, dataDebugReceive);// 2015-01-05 revised from below by DL on 2015-01-05

   // SockDataDebugger(BufferPtr, numread, dataDebugReceive);// a little debugger msg // Original deleted by DL on 2015-01-05
   // The above actually calls the GenDataDebugger in RS232Port.cpp instead of the one in this program. DL on 2015-01-05
   // Discovered by putting a MessageBox at the function in thie program that did not get called.

   //on sock_error
   if (SOCKET_ERROR == numread) 
   {
      m_socketStatus = SOCKET_UNHEALTHY;
      temp = wsaerrno;
      (temp >= 0) ? (sprintf(debugStrPtr, "Read Error on Listen socket : %s\n",sys_wsaerrlist[temp]))
                     : (sprintf(debugStrPtr, "Unknown Read error on Listen socket\n"));
      WSASetLastError(0);
      OutputDebugString(debugStrPtr);
      CloseSocket(TRUE, ackSock);       // TRUE forces "graceful" close
      return(-1);
   }
   //on mismatch
   if (numread != numberOfBytesToRead)
   {
       m_socketStatus = SOCKET_UNHEALTHY;
       sprintf(debugStrPtr, "Read Timeout (%ld/%ld) ", numread,numberOfBytesToRead);
       OutputDebugString(debugStrPtr);
       return(numread);
       //I expect the caller to initiatate retries or whatever!
   }
   else
      m_socketStatus = SOCKET_HEALTHY;
   return(numread);
}

// --------------------------- Send -------------------------------
LONG CDDKSocket::Send(SOCKET ackSock, //The accepted socket
                           int    numberOfBytesToWrite,// -> number of bytes to write
                           CHAR * BufferPtr,       //<- Data read from socket    
                           CHAR * debugStrPtr       //Any errors 
                          )
{
//returns -1 if error, or the number actualy read from socket
int temp;
int numberOfBytesWritten; 

   numberOfBytesWritten = send(ackSock,
                               BufferPtr,
                               (INT) numberOfBytesToWrite,
                               (INT) NULL
                              );
   if (SOCKET_ERROR == numberOfBytesWritten) 
   {
      // We have a socket transmit error, so time to start counting down to a socket reset
      numberOfBytesWritten = 0;
      m_socketStatus = SOCKET_UNHEALTHY;
      temp = wsaerrno;
      (temp >= 0) ? (sprintf(debugStrPtr, "Write Error on Listen socket : %s\n",sys_wsaerrlist[temp]))
                     : (sprintf(debugStrPtr, "Unknown Write error on Listen socket\n"));
      WSASetLastError(0);
      OutputDebugString(debugStrPtr);
      SockDataMessage(debugStrPtr);
   }


   // a leettel debugger msg
   CDDKSocket::SockDataDebugger(BufferPtr, numberOfBytesWritten, dataDebugTransmit);
   // The above actually calls the GenDataDebugger in RS232Port.cpp instead of the one in this program. DL on 2015-01-05
   // Discovered by putting a MessageBox at the function in thie program that did not get called.

   // on mismatch
   if (numberOfBytesWritten != numberOfBytesToWrite)
   {
       m_socketStatus = SOCKET_UNHEALTHY;
       sprintf(debugStrPtr, "Write Timeout (%ld/%ld) ", numberOfBytesWritten,numberOfBytesToWrite);
	   //2015-01-30 by DL first variable was numberOfBytesToWrite and I changed it to numberOfBytesWritten
       //OutputDebugString(debugStrPtr);
       //I expect the caller to initiatate retries or whatever!
   }
   else
      m_socketStatus = SOCKET_HEALTHY;
   return(numberOfBytesWritten);
} // Send
