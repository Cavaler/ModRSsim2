// DDKSrvSocket.cpp: implementation of the CDDKSrvSocket class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// # of times (seconds) a connected server socket will loop for without getting any RX data 
// before it closes and gets ready to accept new connections again.
//#define MAX_IDLE_LOOPS   100   // 2015-01-30 Deleted by DL after replacing with m_SockTO below
extern DWORD  m_SockTO;          // 2015-01-30 Added by DL for Ethernet Socket Timeout
extern BOOL SaveDebugInfo(CString str);   // 2015-01-31 Added by DL to use Save to File
extern BOOL m_CommsTimeShow;              // Added 2015-01-31 by DL to allow m_commsTimes to be used in DDKSrvSocket.cpp
extern BOOL m_SaveDebugToFile;            // Added 2015-02-14 by DL to allow m_SaveDebugToFile to be used in DDKSrvSocket.cpp
extern INT WSAERROR;					  // Added 2014-11-03 by DL to remember showing WSA Error Message
DWORD PortSave;                           // Added 2015-02-15 by DL to save Port for Later use

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC( CDDKSrvSocket, CDDKSocket);

// This server socket object will construct a listen socket of accept a pointer to an existing socket.
CDDKSrvSocket::CDDKSrvSocket(unsigned short port,unsigned long IPAddrULONG /*= ADDR_ANY*/, SOCKET * pServerSocket /*=NULL*/) : CDDKSocket()
{
int error;
CHAR errStr[180],debugStr[180];
INT      sockoptEnable = TRUE;   

   m_serverObject = TRUE;
   m_pSocket      = NULL;

   int check1 = 0x1234;
   m_buffer = new char[m_serverBufferSize];
   int check2 = 0x1234;

   // server stuff
   // Create a Thread and then a Server-end socket to listen on later

   // create the listening thread
   m_listenThreadStatus = SOCKET_EX_PENDING;
   m_pWorkerThread      = AfxBeginThread((AFX_THREADPROC)SockAsyncFriend,
                                     this,
                                     THREAD_PRIORITY_TIME_CRITICAL, 1024*1024*64,
                                     CREATE_SUSPENDED
                                    );
   // construct with IP and port or with an existing socket
   if (NULL == pServerSocket)
   {

      // Setup local addressing for listen socket
      m_localSockaddr_in.sin_family           = PF_INET;
      m_localSockaddr_in.sin_addr.S_un.S_addr = INADDR_ANY; //usually default
      if ((m_localSockaddr_in.sin_port = htons((u_short) port)) == 0)
      {
         sprintf(debugStr, "Cannot use port %ld", port);
         OutputDebugString(debugStr);

         // Fail the connection process
         m_socketStatus = SOCKET_UNCONFIGURED;
         return;
      }
	  else
	  {		// Added 2015-02-15 by DL to save Socket in use.
		  PortSave = port;
	  }     // End of added section 2015-02-15 by DL to save Socket in use.

      // Map protocol name to protocol number
      if ((m_ppe = getprotobyname("tcp")) == NULL)
      {
         GetSockError(errStr);
         sprintf(debugStr,"Driver cannot connect for listen :%s",errStr);
         OutputDebugString(debugStr);
         // Fail the connection process
         m_socketStatus = SOCKET_UNCONFIGURED;
         return;
      }

      // Allocate a listen socket
      m_socket = socket(PF_INET, SOCK_STREAM, m_ppe->p_proto); 
      // If we could not allocate a socket, we must fail the connection process
      if (INVALID_SOCKET == m_socket)  // recommended NT error check
      {
         GetSockError(errStr);
         sprintf(debugStr, "Cannot create Listen socket :%s",errStr);
         OutputDebugString(debugStr);
          // Fail the connection process
         m_socketStatus = SOCKET_UNCONFIGURED;
         return;
      }
      // now to bind socket to local address+port
      if (0 != bind(m_socket, (sockaddr *)&(m_localSockaddr_in), sizeof(m_localSockaddr_in) ) )
      {
         GetSockError(errStr);
         sprintf(debugStr, "Cannot Bind to Listen socket :%s",errStr);
         OutputDebugString(debugStr);
         // Fail the connection process
         m_socketStatus = SOCKET_UNCONFIGURED;
         return;
      }
      // Set the socket to not delay any sends 
      error = setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (CHAR FAR * ) &m_sockoptEnable, sizeof (INT));
      // If we could not set the socket parameters, we must fail the connection process
      if (error == (LONG) SOCKET_ERROR)
      {
         int lastError = WSAGetLastError();
         sprintf(debugStr, "Cannot setsockopt error (%ld)", lastError);
         OutputDebugString(debugStr);
         // Fail the connection process
         m_socketStatus = SOCKET_UNCONFIGURED;
         return;
      }
      m_pSocket = &m_socket;
      m_socketStatus = SOCKET_INITIALISED;
   }
   else
   {
      // use a socket provided to us.
      // do this when many threads listen and accept connections on the same socket.
      m_pSocket = pServerSocket;
   }
   m_socketStatus = SOCKET_INITIALISED;

   strcpy(debugStr, "Socket created OK.");
   
   SockDataMessage(debugStr);
   //if (m_SaveDebugToFile) CRS232Port::WriteToFile(debugStr);  // Added statement on 2016-12-26 by DL to save to Log file

   m_listenThreadStatus = SOCKET_EX_RUNNING;
}

CDDKSrvSocket::~CDDKSrvSocket()
{
   // get the thread to die off
   //delete(m_buffer);
   //OutputDebugString("In Server socket destructor\n");
   m_listenThreadStatus = SOCKET_EX_TERMINATE;
   
   // abort any listen in progress, this only executes on 1st comms thread's socket, since it created it
   if (INVALID_SOCKET != m_socket)
   {
      closesocket(m_socket);
      m_socket = INVALID_SOCKET;
   }
   // wait for thread to die off
   CSingleLock lk(&m_threadDeadEvent);
   lk.Lock(5000); // wait max 5 seconds

   if (NULL != AcceptedAsyncSocket)
   {
      closesocket(AcceptedAsyncSocket); //kill accepted instance immediately
      accepted = FALSE;
      OutputDebugString("Server socket closing normally.\n");
      SockStateChanged(SOCKETCURRENTLY_CLOSING);
   }
}

#ifdef _DEBUG
VOID CDDKSrvSocket::Dump(CDumpContext& dc) const
{
   // call the base class first
   CDDKSocket::Dump(dc);

   // dump our object to the debuggers output
   // all important members can be dumped at this stage.
   dc << "Server socket: " << "\n";
} // Dump
#endif // _DEBUG


// ------------------------------ SockSockAsyncFriend ------------------------------
UINT SockAsyncFriend(LPVOID pParam)
{
CHAR     debugStr2[MAX_DEBUG_STR_LEN];
CHAR     debugStr[MAX_DEBUG_STR_LEN];
CDDKSrvSocket* DDKSockPtr;

   Sleep(500);
   // OK now since everyone will be asking me what is this sleep doing here. So I am
   // going to tell U. We are being naughty, this thread actually starts executing 
   // while the parent class is constructing, consequently all calls to virtual functions
   // are made before the parent has initialized completely (constructor body has not yet run)
   // and U end up using un-initialized variables EEK.

   DDKSockPtr = (CDDKSrvSocket*)pParam;
   try 
   { 
		
      // call the function the thread will run in
      if (DDKSockPtr->IsKindOf(RUNTIME_CLASS( CDDKSrvSocket)))
      {
      CString msgStartup;
         // wait untill the Application is ready for us
         CSingleLock lk(&DDKSockPtr->m_threadStartupEvent);
         lk.Lock(5000); // wait max 5 seconds
         msgStartup.Format("Socket %d listen thread ID=[%d] running", 
                          DDKSockPtr->m_socket, 
                          GetCurrentThreadId());
         DDKSockPtr->SockDataMessage(msgStartup);
		 if (m_SaveDebugToFile) CRS232Port::WriteToFile(msgStartup);  // Added statement on 2016-12-26 by DL to save to Log file
         DDKSockPtr->SockStateChanged(SOCKETCURRENTLY_VOID);
               
         DDKSockPtr->Poll(debugStr);
      }
      else
      {
         sprintf(debugStr2, "CDDKSrvSocket SockAsyncFriend pointer corruption!!!!\n");
         OutputDebugString(debugStr2);
      }
   }
   catch (...) 
   {
      OutputDebugString( "Catch\n" );
      sprintf(debugStr2, "CDDKSrvSocket SockAsyncFriend Exception !!!!\n");
      OutputDebugString(debugStr2);
   }
   try
   {
      DDKSockPtr->m_listenThreadStatus = SOCKET_EX_TERMINATED;
      {
      CString d;
         d.Format("[Thread %4d Terminating.]\n", GetCurrentThreadId());
         OutputDebugString(d);
      }
      DDKSockPtr->m_threadDeadEvent.SetEvent();// CEvent
   }
   catch (...)
   {
      CString msg;
         msg.Format("INTERNAL APPLICATION ERROR FILE %s LINE: %d\n%s\n%s", 
            __FILE__, __LINE__, __MY_APPVERSION__, __DATE__);
      OutputDebugString(msg);
   }
   //AfxEndThread(0);

   return(0);
} // SockAsyncFriend


int HexToBin(const char * str, byte *pBuffer)
{
	int len=0;
	while (*str)
	{
		ConvertASCIIToByte(str, *pBuffer++);
		*str++;
		if (*str)
			*str++;
		if (*str)
			*str++;
		if (*str)
			*str++;
		len++;
	}
	return(len);
}


// ------------------------------- Poll -----------------------------------
//
void CDDKSrvSocket::Poll(CHAR * debugStr)
{
CHAR *      msgPtr = m_buffer; //rxMessage;
CHAR *      debugStrPtr = debugStr;
BOOL        incommingMsgOK = FALSE;
BOOL        outgoingMsgOK = FALSE;
// Socket "notification" setup variables
u_int       retCode;
u_int       errWSALast;               // Added 2016-09-19 by DL for use in recv to detect socket errors
int         errorCode;
BOOL        exitFlag;
sockaddr    acceptedAsyncSocketAddr, ackSocketAddr;
int         addrlength;
unsigned long  numBytes=0;
//unsigned long  numZero=0;           // Added 2016-09-19 by DL for use in ioctlsocket argp value to set blocking ON
unsigned long  numOne=1;              // Added 2016-09-19 by DL for use in ioctlsocket argp value to set blocking OFF
CString     debuggerString;
LONG        numIdleLoops = m_SockTO;  // 2015-01-30 changed from MAX_IDLE_LOOPS by DL for Ethernet Socket Timeouts
DWORD		bufSize;

CString entry;				          // Added 2015-01-31 by DL for handling Sending Times to Debug File

   m_debuggerStep = 0;
   accepted = FALSE;
   exitFlag = false;
   *debugStr = NULL;//zero string

   if (SOCKET_EX_TERMINATE == m_listenThreadStatus) //termination routine 
   {
      sprintf(debugStr, "EX_TERMINATE recieved, terminating!");
      OutputDebugString(debugStr);
      exitFlag = true;
   }
   AcceptedAsyncSocket = INVALID_SOCKET;

   // We want to read the asynchronous data responses
   // ..........Thread Control Loop
   while ( (SOCKET_EX_RUNNING == m_listenThreadStatus) 
            && (false == exitFlag)
         )
   {
      exitFlag = false;
      m_debuggerStep = 1;

      if (SOCKET_EX_TERMINATE == m_listenThreadStatus) //termination routine 
      {
         exitFlag = true;
         break;
      }
//#ifndef _TEST
//	bufSize = HexToBin("x06 x7D x00 x00 x00 x06 x01 x05 x00 x01 xFF x00", (byte*)msgPtr);
//	ProcessData(AcceptedAsyncSocket, msgPtr, bufSize);
//#endif
      if (!accepted)
      {
         m_debuggerStep = 2;
         numIdleLoops = m_SockTO;        // 2015-01-30 changed from MAX_IDLE_LOOPS by DL for Custom Ethernet Socket Timeout
         SockStateChanged(SOCKETCURRENTLY_LISTENING);
         debuggerString.Format("[%4d] Thread listening for connection..\n", GetCurrentThreadId()); // 2015-02-15 by DL "Thread"
		 //debuggerString.Format("[%4d] Listen for connection on Port: %d...\n", GetCurrentThreadId(), PortSave);
         OutputDebugString(debuggerString);
         // Send to debugger list-box
         SockDataMessage(debuggerString);
		 if (m_SaveDebugToFile) CRS232Port::WriteToFile(debuggerString);  // Added statement on 2016-12-26 by DL to save to Log file

         errorCode = listen(*m_pSocket, SOMAXCONN); //listen with backlog maximum reasonable
      
         if (SOCKET_ERROR == errorCode)
         {
            sprintf(debugStr, "Listen returned with a socket error : %s\n", sys_wsaerrlist[wsaerrno]);
            OutputDebugString(debugStr);
            exitFlag = true;
         }
         //now listening - no error !!!!

         addrlength = sizeof(acceptedAsyncSocketAddr);//for now
         WSASetLastError(0);
         if (AcceptedAsyncSocket == INVALID_SOCKET)
            accepted = FALSE;

         //------------------------- Listen Control Loop -------------------//
         do  
         {
            m_debuggerStep = 3;
             //Beep(3000,50);
            debuggerString.Format("[%4d] Accept connection...\n", GetCurrentThreadId());
            OutputDebugString(debuggerString);
            AcceptedAsyncSocket = accept(*m_pSocket,  //listen always returns immediately
                                          &acceptedAsyncSocketAddr,
                                          &addrlength
                                         );
            if (INVALID_SOCKET !=AcceptedAsyncSocket)
            {
               m_debuggerStep = 4;
			   // Added text "(Socket)[Thread]" for better documentation on 2016-09-17 by DL
               debuggerString.Format("(%d)[%4d] (Socket)[Thread] Connection accepted.\n", *m_pSocket, GetCurrentThreadId());
               OutputDebugString(debuggerString);
               SockDataMessage(debuggerString);
			   if (m_SaveDebugToFile) CRS232Port::WriteToFile(debuggerString);  // Added statement on 2016-12-26 by DL to save to Log file
                // copy the IP address
               memcpy(&ackSocketAddr, &acceptedAsyncSocketAddr, sizeof(acceptedAsyncSocketAddr));

// Section added 2015-01-31 by DL for showing IP Address connected & Revised 2015-02-15 to show Port used.
			   // Modified 2016-05-22 by DL to remove ending period and "\n" to allow showing "Using Seperate Registers" or not
               debuggerString.Format("New Connection from IP Address:Port = %d.%d.%d.%d:%d",  // Added ":%d" 2015-02-15 by DL
                (BYTE)acceptedAsyncSocketAddr.sa_data[2],
                (BYTE)acceptedAsyncSocketAddr.sa_data[3],
                (BYTE)acceptedAsyncSocketAddr.sa_data[4],
                (BYTE)acceptedAsyncSocketAddr.sa_data[5], PortSave); // Added PortSave 2015-02-15 by DL for show Port used
			   
			   // Added next 4 lines 2016-05-22 by DL to show "Using Seperate Registers" if true when connection made
			   if (pGlobalDialog->m_seperateRegisters)
                  debuggerString = debuggerString + " Using Seperate Registers.\n";
               else
                  debuggerString = debuggerString + ".\n";

               SockDataMessage(debuggerString);
			   if (m_SaveDebugToFile) CRS232Port::WriteToFile(debuggerString);  // Added statement on 2016-12-26 by DL to save to Log file
/*
			   if (m_SaveDebugToFile)         // Added 2015-02-14 by DL to see if we need to send to Debug Log File
			   {
                   if (m_CommsTimeShow)       // Section added 2015-01-12 by DL to allow adding Times to Debug Log File
                   {
                   SYSTEMTIME sysTime;
                   GetLocalTime(&sysTime);
                   entry.Format("%02d:%02d:%02d.%03d %s", sysTime.wHour, sysTime.wMinute, sysTime.wSecond,
                   sysTime.wMilliseconds, debuggerString);   // Added Hours on 2015-01-11 by DL
			       }
                   else
                   entry = debuggerString;
	           SaveDebugInfo(entry);               // Code by DL on 2015-01-10 & 2015-01-12 for saving Debug info to file.
			   }
*/
// End of section added 2015-01-31 by DL for showing IP Address connected.

               int test = h_errno;
               if ( (INVALID_SOCKET == AcceptedAsyncSocket) //check for fails
                      && (h_errno != WSAEWOULDBLOCK) //no connection avail to be accepted
                   ) 
               {
                   m_debuggerStep = 5;
                   sprintf(debugStr, "Listen Accept returned with a socket error : %s\n",sys_wsaerrlist[wsaerrno]);
                   OutputDebugString(debugStr);                    
                   exitFlag = true;
                   continue;   //skip the rest of this loopy
               }
               accepted = TRUE;   
            }
            else
            {
            LONG temp;
               
               // get the error.
               m_debuggerStep = 6;
               temp = wsaerrno;

               (temp >= 0) ? (sprintf(debugStrPtr, "Accept() Error on Listen socket : %s\n",sys_wsaerrlist[temp]))
                              : (sprintf(debugStrPtr, "Unknown Accept() error on Listen socket\n"));
               OutputDebugString(debugStrPtr);
               SockDataMessage(debugStrPtr);
			   if (m_SaveDebugToFile) CRS232Port::WriteToFile(debugStrPtr);  // Added statement on 2016-12-26 by DL to save to Log file
               WSASetLastError(0);
			   if (temp == 22 && WSAERROR == 0)			// 2015-11-02 added by DL for another server warning
				   {
					   WSAERROR = 1;
					   MessageBox(NULL, "There is probably another Modbus Server running!",
						   "Already Modbus Server Error", MB_OK);
				   }			                        // 2015-11-02 added by DL for another server warning end of adds
            }
         }//Listen
         while ( (SOCKET_EX_RUNNING == m_listenThreadStatus) 
                  && (false == exitFlag)
                  && (AcceptedAsyncSocket == INVALID_SOCKET)
                  && (h_errno == WSAEWOULDBLOCK)
               );
      }
      if (INVALID_SOCKET !=AcceptedAsyncSocket)
      {
      DWORD initialTicks = GetTickCount();
         m_debuggerStep = 7;
         debuggerString.Format("%4d Currently connected.\n", GetCurrentThreadId());
         OutputDebugString(debuggerString);

		 // Make the Winsock Functions non-blocking by using FIONBIO (Non-Blocking I/O) with argp non-zero.
		 // Added ioctlsocket for Non-Blocking on 2016-09-19 by DL
		 ioctlsocket(AcceptedAsyncSocket, FIONBIO, &numOne);

         do
         {
            // Find out how many bytes are currently available for reading on the socket
            m_debuggerStep = 8;
			// numBytes has the count of bytes ready to be read and retCode has SOCKET_ERROR (-1) or zero for success
            retCode = ioctlsocket(AcceptedAsyncSocket, FIONREAD, &numBytes);

// Beginning of Adds on 2016-09-19 by DL to capture close & reset socket requests
			// Do a recv on our socket so we can test errorCode for close or reset
            errorCode =  recv(AcceptedAsyncSocket,    // Our socket
                   msgPtr,                            // Receive Buffer 
                   (INT) 0,                           // Zero because we don't want to read anything, but only check for socket close
                   (INT) MSG_OOB);                    // Use Out Of Band so we do not affect normal reads
			errWSALast = WSAGetLastError();           // Save result for use if needed after socket close
            //debuggerString.Format("errorCode = %d, WSAGetLastError = %d, retCode = %d.\n", errorCode, WSAGetLastError(), retCode);
            //OutputDebugString(debuggerString);

            // If errorCode equals zero (success) and no bytes in buffer then a close request was received
	        // If retCode is WSAECONNRESET then the connection was RESET by the remote client
		    if ((errorCode == 0) && numBytes == 0 || (errWSALast == WSAECONNRESET))
		    {
               // close the server socket
               closesocket(AcceptedAsyncSocket); // kill accepted instance immediately
			   //debuggerString.Format("(%d)[%4d] (Socket)[Thread] Connection Closed.\n", *m_pSocket, GetCurrentThreadId());
               //OutputDebugString(debuggerString);
			   //SockDataMessage(debuggerString);
               accepted = FALSE;
			   if (errWSALast == WSAECONNRESET)
			   {
                  debuggerString.Format("(%d)[%4d] (Socket)[Thread] Connection Closed. Connection RESET by Remote Client.\n",
					  *m_pSocket, GetCurrentThreadId());
				  //OutputDebugString("Connection RESET by Remote Client.\n");
                  //SockDataMessage  ("Connection RESET by Remote Client.\n");
			   }
			   else
			   {
                  debuggerString.Format("(%d)[%4d] (Socket)[Thread] Connection Closed. Closing socket normally.\n",
					  *m_pSocket, GetCurrentThreadId());
				  //OutputDebugString("Closing socket normally.\n");
                  //SockDataMessage  ("Closing socket normally.\n");
			   }
               OutputDebugString(debuggerString);
			   SockDataMessage(debuggerString);
			   if (m_SaveDebugToFile) CRS232Port::WriteToFile(debuggerString);  // Added statement on 2016-12-26 by DL to save to Log file
               SockStateChanged(SOCKETCURRENTLY_CLOSING);
		    }
// End of Adds on 2016-09-19 by DL to capture close & reset socket requests

            if (0==numBytes)
               Sleep(1);
            else
               SockStateChanged(SOCKETCURRENTLY_READING);
            if (m_serverBufferSize <= numBytes)
               numBytes = m_serverBufferSize-1; // -1 because we add a NULL char to the buffer contents latter

           if ((GetTickCount() - initialTicks) > 1000)	// 2015-01-30 changed from 10000 by DL so m_SockTO is seconds
            {                                           // This occurs whenever a socket times out once per second now
 //              ASSERT(0);  //                         // Removed 2015-01-30 by DL because it was not needed
               break;   // time out for one second
            }

            if (SOCKET_EX_TERMINATE == m_listenThreadStatus) //termination routine 
            {
               exitFlag = true;
               break;
            }
         }
         while (0 == numBytes); //delay until an actual data bearing packet is brought in 
                                //remembering only 1 packet per transaction allways

         if ( (SOCKET_ERROR != retCode)
               && (0 != numBytes)
            )
         { // Yes got have some bytes available on the socket
            m_debuggerStep = 9;
            numIdleLoops = m_SockTO;    // 2015-01-30 by DL changed from MAX_IDLE_LOOPS for Custom Ethernet Socket Timeout
            debuggerString.Format("%4d Server socket thread reading.\n", GetCurrentThreadId());
            OutputDebugString(debuggerString);

            // ...read them out
            incommingMsgOK = (Recieve(AcceptedAsyncSocket, numBytes, msgPtr, debugStrPtr)==(LONG)numBytes);
            m_debuggerStep = 10;
            if (SOCKET_EX_RUNNING != m_listenThreadStatus) //termination routine 
            {
               //exitFlag = true;
               break;
            }

            // process this message as valid data from a PLC
            if (incommingMsgOK)
            {

               m_debuggerStep = 11;
               bufSize = numBytes;     // ok so all bytes were read
               msgPtr[bufSize] = '\0'; // append a null for neatness
               m_debuggerStep = 12;
               if (FALSE == ProcessData(AcceptedAsyncSocket, msgPtr, bufSize))
               {
                  m_debuggerStep = 13;
                  
                  // close the server socket
                  closesocket(AcceptedAsyncSocket); // kill accepted instance immediately
                  accepted = FALSE;
                  OutputDebugString("Closing socket normally.\n");
                  SockDataMessage("Closing socket normally.\n");     // Added 2015-01-30 by DL for better debugger messages
				  if (m_SaveDebugToFile) CRS232Port::WriteToFile("Closing socket normally.\n");  // Added statement on 2016-12-26 by DL to save to Log file
                  SockStateChanged(SOCKETCURRENTLY_CLOSING);
                  //SockDataMessage(debuggerString);
                  // 13 Rembrandt st Petervale
               }
               m_debuggerStep = 14;
            }
         }
         else
         { //We may have a problem please be a patient           
            if (SOCKET_ERROR == retCode && accepted)    // Added "&& accepted" by DL on 2016-09-16 for better cleanup on normal socket close
            {
               sprintf(debugStr, "Listen Poll Failed on rx, BytesRx'd : %d\n",numBytes);
               OutputDebugString(debugStr);
               // close this PLC connection so that new data still comes in
               closesocket(AcceptedAsyncSocket); //kill accepted instance immediatly
               accepted = FALSE;
               OutputDebugString("Closing socket after error.\n");
               SockStateChanged(SOCKETCURRENTLY_CLOSING);
            }
            else
            {  // no problem at all, socket was just idle.
               numIdleLoops--;
               if (numIdleLoops > 0)
               {
                  debuggerString.Format("[%4d] Listen Poll idle.\n", GetCurrentThreadId());
                  OutputDebugString(debuggerString);
                  SockStateChanged(SOCKETCURRENTLY_IDLE);
               }
               else
               {
                  debuggerString.Format("[%4d] Closing socket, idle for too long.\n", GetCurrentThreadId());
                  OutputDebugString(debuggerString);
                  SockDataMessage(debuggerString);
				  if (m_SaveDebugToFile) CRS232Port::WriteToFile(debuggerString);  // Added statement on 2016-12-26 by DL to save to Log file

                  closesocket(AcceptedAsyncSocket); //kill accepted instance immediatly
                  accepted = FALSE;
                  SockStateChanged(SOCKETCURRENTLY_CLOSING);
				}
            }
         }
      }
   } //Thread
       
   //This is where we go to Terminate the Thread.

   m_listenThreadStatus = SOCKET_EX_TERMINATED;
} // Poll
