/////////////////////////////////////////////////////////////////////////////
//
// FILE: MODCommsProcessor.cpp : implementation file
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
// implementation of the CMOD232CommsProcessor class.
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ABCommsProcessor.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif



// English meanings for MODBUS (Exception) error codes
PCHAR MODBUSplcError[9] =     
{
   "No error",                // 0x0
   "Illegal Function",        // 0x1
   "Illegal Data Address",    // 0x2
   "Illegal Data value",      // 0x3
   "Slave Device failure",    // 0x4
   "Acknowledge",             // 0x5
   "Slave device busy",       // 0x6
   "Negative acknowledge",    // 0x7
   "Memory Parity error"      // 0x8
};  

BOOL CMODMessage::m_protocolEthernet = FALSE;   // default to serial
BOOL m_msgLenOK = FALSE;                        // Added 2015-01-25 by DL to hold Message Length Check
int FC = 0;                                     // Added 2015-01-25 by DL to hold FC across functions
DWORD m_noiseLength;                            // Added 2015-02-01 by DL

// ----------------------- constructor -----------------------------------
CMODMessage::CMODMessage(const CHAR * pMessageRX, DWORD len)
{
BYTE *pTelePtr;
BYTE *crcPtr;  // CRC bytes here
BYTE *crcStartPtr = (BYTE*)pMessageRX;
WORD  crc = 0;
static BYTE EthernetHeader[4]= {
         0,0,0,0
      };

   m_packError = FALSE;
   frameEthernet = m_protocolEthernet;
   //frameASCII = FALSE;   // Deleted 2015-Dec-19 by DL because it is not needed

   // break it down
   pTelePtr = (BYTE*)pMessageRX;
   totalLen = (WORD)len;
   count = 0;
   if (m_protocolEthernet)
   {
	  // Needs to handle short packets such as two bytes from Eth better. DL on 2016-09-14
      m_EthernetTransNum = *(WORD*)pTelePtr;
      pTelePtr += sizeof(EthernetHeader);
      // grab the TCP frame length from the actual headder
      m_frameLength = *(WORD*)pTelePtr;
      m_frameLength = SwapBytes(m_frameLength);
      pTelePtr += sizeof(WORD);
   }
   //Pre-Amble
   //if (frameASCII)     // Deleted Block 2015-Dec-19 by DL because it is not needed
   //{
   //   stationID =    (BYTE)UnPackASCIIField(&pTelePtr, 1, m_packError); // 2 char 
   //   functionCode = (BYTE)UnPackASCIIField(&pTelePtr, 1, m_packError); // 2 char
   //   address   = UnPackASCIIField(&pTelePtr, 2, m_packError); // 2 chars
   //}
   //else
   //{                   // End of Deleted Block 2015-Dec-19 by DL because it is not needed
      stationID =    (BYTE)UnPackField(&pTelePtr, 1); // 2 char 
      functionCode = (BYTE)UnPackField(&pTelePtr, 1); // 2 char & Maybe add test for len <= 4 to next statement for FC7
      address   = UnPackField(&pTelePtr, 2); // 2 chars
   //}                   // Deleted 2015-Dec-19 by DL because it is not needed

   switch (functionCode)
   {
      case MOD_WRITE_SINGLE_COIL /*0x05*/ : //Write single coils dont have count bytes
         byteCount = 1;              // Changed from "byteCount = 2" on 2015-01-21 by DL to fix writing coil 65536
         overalLen = 2;              // Changed from "overalLen = byteCount" on 2015-01-21 by DL to fix writing coil 65536
         break;
      case MOD_WRITE_MULTIPLE_COILS:
         byteCount = UnPackField(&pTelePtr, 2); // 2 chars, the count is in bits
         count = byteCount;
         overalLen = (WORD)ceil(byteCount/8.0);
         coilByteCount = *pTelePtr++; // increment past the #bytes byte which is the # bytes of data to expect (max 255)
         overalLen += 3;
         break;
      case MOD_READ_COILS  : 
      case MOD_READ_DIGITALS  : 
      case MOD_READ_REGISTERS : 
      case MOD_READ_HOLDING   :
      case MOD_READ_EXTENDED  :
         // byteCount= # bytes to read
         byteCount = UnPackField(&pTelePtr, 2); // 2 chars, the count is in REGISTERS
         overalLen = 2;
         m_msgLenOK = TRUE;     // Added 2015-01-25 by DL-Assume good length since short
         break;
      case MOD_WRITE_HOLDING:
      case MOD_WRITE_EXTENDED:
         // byteCount=# bytes if a write, else # bytes to read
         byteCount = UnPackField(&pTelePtr, 2)*2; // 2 chars, the count is in bytes
         overalLen = byteCount;
		 regByteCount = *pTelePtr; // Added 2017-01-01 by DL for testing bad BYTE count
         pTelePtr++; // increment past the #bytes byte which is the # bytes of data to expect (max 255)
         overalLen+=3;  // skip the 3 bytes for the req. size (byte) and length/quantity word
		 if (len == byteCount + 9)        // Added 2015-01-25 by DL to handle bad CRC
             m_msgLenOK = TRUE;           // Added 2015-01-25 by DL to handle bad CRC
         else                             // Added 2015-01-25 by DL to handle bad CRC
             m_msgLenOK = FALSE;          // Added 2015-01-25 by DL to handle bad CRC
         break;
      case MOD_WRITE_SINGLEHOLDING:
         byteCount = 2; // 2 chars, only 1 register
         overalLen = byteCount;
         m_msgLenOK = TRUE;     // Added 2015-01-25 by DL-Assume good length since short
         break;
      case MOD_MASKEDWRITE_HOLDING:
         byteCount = 4; // 2 masks
         overalLen = byteCount;
         m_andMask = UnPackField(&pTelePtr, 2); // 2 bytes, word
         m_orMask  = UnPackField(&pTelePtr, 2); // 2 bytes, word
         m_msgLenOK = TRUE;     // Added 2015-01-25 by DL-Assume good length since short
         break;
      case MOD_READ_EXCEPTION:   // Section added 2015-01-13 and revised 2015-01-25 by DL
         byteCount = 8;          // BIT registers to read
         overalLen = 2;          // Actual Length of command
         address = 0;            // BIT register beginning address
         m_msgLenOK = TRUE;      // Added 2015-01-25 by DL-Assume good length since short
         break;                  // End of Function Code 7 Additions 2015-01-25
	  case MOD_READ_SLAVEID:	 // Section added 2015-Dec-15 by DL
		 byteCount = 9;          // Bytes of data to return
		 overalLen = 2;          // Actual Length of command
		 m_msgLenOK = TRUE;      // Assume Good Length by DL
		 break;                  // End of Section Added 2015-Dec-15 by DL
      case MOD_READ_DIAGNOSTIC:  // Section added 2015-Dec-03 by DL
         byteCount = UnPackField(&pTelePtr, 2); // 2 chars that would normally be byte count
         overalLen = 6;          // Save as normal command (needs to be initialized on first run)
         m_msgLenOK = TRUE;      // Added 2015-Dec-03 by DL-Assume good length
         break;                  // Added 2015-Dec-03 by DL
	  case MOD_READWRITE_HOLDING:
         // byteCount=# bytes if a write, else # bytes to read
         byteCount = UnPackField(&pTelePtr, 2); // 2 chars, the count is in REGISTERS
		 overalLen = UnPackField(&pTelePtr, 2); // 2 chars, write start address
		 overalLen = UnPackField(&pTelePtr, 2);   // write length in words
		 overalLen = UnPackField(&pTelePtr, 1);   // write byte count
//         overalLen = byteCount;
         overalLen += 7;  // skip the 7 bytes for the req. size (byte) and length/quantity words
         break;
	  default   : //All other commands not supported
         //ASSERT (0);
         overalLen = 0;
         byteCount = 0;
         break;
   }
   if ((functionCode != 7) && (functionCode != 17)) // Added 2015-01-25 by DL to allow Read Exception to pass range-check code below
	                                              // Added 17 testing on 2015-Dec-15 by DL for Slave ID Reading
   overalLen += 4; //now it points to the CRC

   //Now  (at last) pTelePtr points to the data to read/write
   dataPtr = pTelePtr;  // data starts here
   
//   ASSERT(totalLen >= overalLen + MODBUS_CRC_LEN); // range-check here
   if (totalLen < overalLen + MODBUS_CRC_LEN)
   {
	   // turf this message it is duff!
      overalLen = totalLen - MODBUS_CRC_LEN;
      m_packError = TRUE;

   }
   if (frameEthernet)
   {
      ASSERT(totalLen >= overalLen - ETH_PREAMBLE_LENGTH); // range-check here
      // Ethernet frame does not have an embedded CRC
      if (totalLen < overalLen - ETH_PREAMBLE_LENGTH)
      {
         overalLen = totalLen - ETH_PREAMBLE_LENGTH;
         // turf this message it is duff!
         m_packError = TRUE;
      }
      else
         crcCheckedOK = TRUE;
   }
   else
   {
      // check the CRC
      crcPtr = (BYTE*)&pMessageRX[overalLen];
      crcStartPtr = (BYTE*)pMessageRX;

      crc = 0xffff;
      CalcCRC(crcStartPtr, overalLen, &crc);      // Only one buffer to calc crc of

      if (*(WORD *)crcPtr != crc)
      {
         // CRC did not match
         crcCheckedOK = FALSE;
      }
      else 
         crcCheckedOK = TRUE;
   }
} // CMODMessage

// --------------------------- CMODMessage --------------------------
// PURPOSE: copy constructor used to build responses, does not actually 
// copy the message.
CMODMessage::CMODMessage(const CMODMessage & oldMODMessage) 
{
   m_packError = FALSE;

   //Copy in common stuff from both messages here!
   this->stationID    = oldMODMessage.stationID;
   this->functionCode = oldMODMessage.functionCode;
   this->address = oldMODMessage.address;       // where to copy data from
   this->byteCount = oldMODMessage.byteCount;   // length of data to copy
   this->m_andMask = oldMODMessage.m_andMask;
   this->m_orMask  = oldMODMessage.m_orMask;
   
   this->overalLen = 0;   //New message so 0 for now!
   
   this->dataPtr = (BYTE*)buffer; //Nice an fresh pointer to the beginning!
   this->m_EthernetTransNum = oldMODMessage.m_EthernetTransNum;
}

// ------------------------------ BuildMessagePreamble -------------------------
// PURPOSE: Builds the STN,FN and LEN bytes of the telegram.
// on completion dataPtr pointsto where the data must be packed in (if any)
CHAR * CMODMessage::BuildMessagePreamble(BOOL error, WORD errorCode)
{
BYTE *pWorkArea;
BYTE numBytesData;

   //
   pWorkArea = (BYTE*)buffer;
   *pWorkArea++ = (BYTE)stationID;
   if (error)
   { // error flag 80 + error meaning byte
      *pWorkArea++ = (BYTE)(functionCode|0x80);
      *pWorkArea++ = (BYTE)errorCode;
   }
   else
   {
      // normal processing
      *pWorkArea++ = (BYTE)functionCode;
      switch (functionCode)
      {
          case MOD_WRITE_HOLDING        : 
          case MOD_WRITE_EXTENDED       : 
             // HF fixed the return address.
             *pWorkArea++ = HIBYTE(address);
             *pWorkArea++ = LOBYTE(address);
             *pWorkArea++ = HIBYTE(byteCount/2);
             *pWorkArea++ = LOBYTE(byteCount/2);
             break;
          case MOD_WRITE_SINGLEHOLDING :
             *pWorkArea++ = HIBYTE(address); // CDB fixed return address rev 7.0
             *pWorkArea++ = LOBYTE(address);
             break;
          case MOD_MASKEDWRITE_HOLDING:
             *pWorkArea++ = HIBYTE(address);
             *pWorkArea++ = LOBYTE(address);
             //add the masks
             *pWorkArea++ = HIBYTE(m_andMask);
             *pWorkArea++ = LOBYTE(m_andMask);
             *pWorkArea++ = HIBYTE(m_orMask);
             *pWorkArea++ = LOBYTE(m_orMask);

             break;
          case MOD_WRITE_MULTIPLE_COILS : 
             *pWorkArea++ = HIBYTE(address);
             *pWorkArea++ = LOBYTE(address);
             *pWorkArea++ = HIBYTE(byteCount);  // # bits actually
             *pWorkArea++ = LOBYTE(byteCount);

             break;
          case MOD_WRITE_SINGLE_COIL    :
             *pWorkArea++ = HIBYTE(address);  // fixed thanks to Joan Lluch-Zorrilla
             *pWorkArea++ = LOBYTE(address);
             break;
          case MOD_READ_DIGITALS  : // in
          case MOD_READ_COILS     : // out
             numBytesData = (BYTE)ceil((float)byteCount/8.0);  // # registers*2
             *pWorkArea++ = numBytesData; 
             break;
         case MOD_READ_EXCEPTION : // 07 Added 2015-01-13 by DL
         case MOD_READ_DIAGNOSTIC : // 08 Added 2015-Dec-03 by DL
		 case MOD_READ_SLAVEID :    // 11 Added 2015-Dec-15 by DL
             break;                // 07 End of Add 2015-01-20 by DL
          case MOD_READ_REGISTERS : 
          case MOD_READ_HOLDING   :
          case MOD_READ_EXTENDED  : 
             numBytesData = byteCount*2;  // # registers*2
             *pWorkArea++ = numBytesData; 
             break;
      }
   }   
   dataPtr = pWorkArea; // must now point to 1st byte of data

   return (buffer);
} // BuildMessagePreamble

// ----------------------------- SetEthernetFrames --------------------------
// supply FALSE for normal serial 232 frames
BOOL CMODMessage::SetEthernetFrames(BOOL ethernetFrames/* = TRUE*/)
{
BOOL oldV = m_protocolEthernet;
   m_protocolEthernet = ethernetFrames;
   return (m_protocolEthernet);
}

// ------------------------------ BuildMessageEnd -------------------------------
// PURPOSE: glue a CRC onto the end of the message
// totalLen must be = the full telegram length (+CRC) when this is called.
CHAR * CMODMessage::BuildMessageEnd()
{
WORD length;
BYTE *pCrcStart = (BYTE*)buffer;
WORD crc = 0xFFFF;
BYTE *crcPtr;

   // Add the CRC bytes
   length = totalLen - MODBUS_CRC_LEN; //calc the CRC of all bytes but the 2 CRC bytes

   CalcCRC(pCrcStart, length, &crc);
   crcPtr = (BYTE*)&buffer[length];
   *(WORD *)crcPtr = crc;

   return (buffer);
} // BuildMessageEnd


// ------------------------------ GetAddressArea --------------------
// Returns:    A supported MEM area index for any MOD address class
// Parameter:  A modbus command (e.g. 3 =read holding register)
//
WORD CMODMessage::GetAddressArea(WORD classCode //  modbus command byte
                                )
{
   switch(classCode)
   {
      // read commands 
      case MOD_READ_COILS      : return(0); break;
      case MOD_READ_DIGITALS   : return(1); break;
      case MOD_READ_REGISTERS  : return(2); break;   
      case MOD_READ_HOLDING    : return(3); break;
      case MOD_READ_EXTENDED   : return(4); break;
      case MOD_READ_EXCEPTION  : return(0); break;      // Added 2015-01-13 by DL
	  case MOD_READ_SLAVEID    : return(0); break;      // Added 2015-Dec-15 by DL
      case MOD_READ_DIAGNOSTIC : return(0); break;      // Added 2015-Dec-19 by DL
      // write commands      
      case MOD_WRITE_HOLDING        : return(3); break;
      case MOD_WRITE_SINGLEHOLDING  : return(3); break;
      case MOD_MASKEDWRITE_HOLDING  : return(3); break;
      case MOD_WRITE_SINGLE_COIL    : return(0); break;
      case MOD_WRITE_MULTIPLE_COILS : return(0); break;
      case MOD_WRITE_EXTENDED       : return(4); break;
   }
   return(3); //Default here for now, Should never get here anyway!

} // GetAddressArea

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
IMPLEMENT_DYNAMIC( CMOD232CommsProcessor, SimulationSerialPort);

//////////////////////////////////////////////////////////////////////
// constructor to open port
CMOD232CommsProcessor::CMOD232CommsProcessor(LPCTSTR portNameShort, 
                                     DWORD  baud, 
                                     DWORD byteSize, 
                                     DWORD parity, 
                                     DWORD stopBits,
                                     DWORD rts,
                                     int   responseDelay,
                                     BOOL  MOSCADchecks,
                                     BOOL modifyThenRespond,
                                     BOOL disableWrites,
									 BOOL longTimeouts) : SimulationSerialPort()
{
CString description;
 m_protocolName = "MODBUS RTU";

   InitializeCriticalSection(&stateCS);
   
   m_noiseLength = 0;
   
   description.Format("Starting comms emulation : %s", "MODBUS RS-232");
   RSDataMessage(description);
   if (m_SaveDebugToFile) CRS232Port::WriteToFile(description);  // Added statement on 2016-12-26 by DL to save to Log file
   
   // open the port etc...
   if (OpenPort(portNameShort))
   {
      ConfigurePort(baud, byteSize, parity, stopBits, rts, (NOPARITY==parity?FALSE:TRUE), longTimeouts);
   }
   m_responseDelay = responseDelay;

   SetEmulationParameters(MOSCADchecks, modifyThenRespond, disableWrites);
   m_pWorkerThread->ResumeThread(); //start thread off here

}


CMOD232CommsProcessor::~CMOD232CommsProcessor()
{

}

void CMOD232CommsProcessor::SetEmulationParameters(BOOL moscadChecks, 
                                                BOOL modifyThenRespond, 
                                                BOOL disableWrites)
{
   m_MOSCADchecks = moscadChecks;
   m_modifyThenRespond = modifyThenRespond;
   m_disableWrites = disableWrites;
}

// ------------------------------ RSDataDebugger ------------------------------
void CMOD232CommsProcessor::RSDataDebugger(const BYTE * buffer, LONG length, int transmit)
{
   CRS232Port::GenDataDebugger(buffer,length,transmit);
} // RSDataDebugger

// ------------------------------- RSStateChanged -----------------------
void CMOD232CommsProcessor::RSStateChanged(DWORD state)
{
   EnterCriticalSection(&stateCS);
   if (NULL==pGlobalDialog)
      return;
   pGlobalDialog->m_ServerRSState = state;
   LeaveCriticalSection(&stateCS);
} // RSStateChanged

// ------------------------------ RSDataMessage ------------------------------
void CMOD232CommsProcessor::RSDataMessage(LPCTSTR msg)
{
   EnterCriticalSection(&stateCS);
   OutputDebugString("##");
   if (NULL!=pGlobalDialog)
      pGlobalDialog->AddCommsDebugString(msg);
   LeaveCriticalSection(&stateCS);
}

// ------------------------------- RSModemStatus ---------------------------
void CMOD232CommsProcessor::RSModemStatus(DWORD status)
{
   EnterCriticalSection(&stateCS);
   if (NULL!=pGlobalDialog)
      pGlobalDialog->SendModemStatusUpdate(status);
   LeaveCriticalSection(&stateCS);
}


// ------------------------------ OnProcessData --------------------------------
BOOL CMOD232CommsProcessor::OnProcessData(const CHAR *pBuffer, DWORD numBytes, BOOL *discardData)
{
	if (*discardData && m_noiseLength)     // Added 2015-01-25 by DL to discard data after timeout
	{                                      // Added 2015-01-25 by DL
	   *discardData = TRUE;     // Added 2015-01-25 by DL
       m_noiseLength = 0;       // Added 2015-01-25 by DL
 
       CString msg;             // Added 2015-01-25 by DL
       msg.Format("Timeout on Message Receive. Dumping Data.\n");  // Added 2015-01-25 by DL
       OutputDebugString(msg);  // Added 2015-01-25 by DL
       RSDataMessage(msg);      // Added 2015-01-25 by DL
       if (m_SaveDebugToFile) CRS232Port::WriteToFile(msg);  // Added statement on 2016-12-26 by DL to save to Log file
       return(FALSE);           // Added 2015-01-25 by DL
	}                           // Added 2015-01-25 by DL - End of section to discard data after timeout
	// build noise telegram
   if (numBytes)
   { //append recieved bytes to the noise telegram and m_noiseLength if the length of the Accumulator buffer is OK
      if (m_noiseLength + numBytes >= sizeof(m_noiseBuffer))
      {
         RSDataMessage("OVERFLOW:Restarting interpretation.");
		 // Added statement on 2016-12-26 by DL to save to Log file
         if (m_SaveDebugToFile) CRS232Port::WriteToFile("OVERFLOW:Restarting interpretation.");

         m_noiseLength = 0;
         //SetEngineState(ENG_STATE_IDLE);
         return(TRUE);
      }
      memcpy(&m_noiseBuffer[m_noiseLength], pBuffer, numBytes);
      m_noiseLength += numBytes;
      *discardData = TRUE;
   }

//   if ((m_noiseLength < MODBUS_NORMAL_LEN)              // 2015-01-25 Deleted by DL for new check including FC7 below
   if ((numBytes == m_noiseLength) && (numBytes >=2))                                // If first bytes then try to get Function Code
   FC = (WORD)pBuffer[1];                                                            // Get Byte in buffer that is Function Code
   //if ((m_noiseLength < MODBUS_NORMAL_LEN) && (m_noiseLength == 4) && !(FC == 7))  // Test for Length unless FC7 or FV17 and len=4
   //if ((m_noiseLength < MODBUS_NORMAL_LEN) && !((FC == 17) && (m_noiseLength == 4)))// Test for Length unless FC7 and len=4
   if ((m_noiseLength < MODBUS_NORMAL_LEN) && !(((FC == 7) || (FC == 17)) && (m_noiseLength == 4)))// Test for Length unless FC7 and len=4
      return(FALSE);

   CMODMessage::SetEthernetFrames(FALSE);
   CMODMessage msg((char*)m_noiseBuffer, m_noiseLength);
   if (msg.CRCOK())
   {
   BOOL ret;
      // build a response etc
      ret = ProcessData((char*)m_noiseBuffer, msg.overalLen + 2);   //+2 for the CRC
      m_noiseLength = 0;                       // Resets the Accumulator buffer length
      return(ret);
   }
   else
   {
      // try to strip away leading byte "noise"?
// comment out the following 2 lines, from spaccabbomm [beta@mmedia.it]
/*
      m_noiseLength--;
      memmove(m_noiseBuffer, &m_noiseBuffer[1], m_noiseLength);
*/

   if (!m_msgLenOK)         // Added 2015-01-25 by DL to handle bad CRC messages
      return(TRUE);         // Added 2015-01-25 by DL
   else                     // Added 2015-01-25 by DL
   {                        // Added 2015-01-25 by DL
   *discardData = TRUE;     // Added 2015-01-25 by DL
   m_noiseLength = 0;       // Added 2015-01-25 by DL
 
   CString msg;             // Added 2015-01-25 by DL
   msg.Format("Malformed Modbus message for Function Code = x%02X\n", // Added 2015-01-25 by DL
	     (unsigned int)(FC & 0xFF));  // Revised for only two hex digits on 2016-12-28 by DL
   OutputDebugString(msg);  // Added 2015-01-25 by DL
   RSDataMessage(msg);      // Added 2015-01-25 by DL
   if (m_CommsDecodeShow) CRS232Port::WriteToFile(msg);  // Added statement on 2016-12-26 by DL to save to Log file

   return(FALSE);           // Added 2015-01-25 by DL - End of bad CRC message handling
   }

   }
   *discardData = FALSE;
   return(FALSE);
}

// --------------------------------- TestMessage ------------------------
//
BOOL CMOD232CommsProcessor::TestMessage(CMODMessage &modMsg, 
                                        WORD &startRegister, 
                                        WORD &endRegister, 
                                        WORD &MBUSerrorCode,
                                        WORD &requestMemArea,
                                        WORD &numBytesInReq
                                        )
{
BOOL MBUSError = FALSE;

   if (!modMsg.CRCOK())
   {
      // bail
   }

   //Get memory area which to update or retrieve from
   requestMemArea = modMsg.GetAddressArea(modMsg.functionCode);
   if (requestMemArea >= MAX_MOD_MEMTYPES)
   {
      // TO DO!
      // handle the error
      Beep(2000,200);
      requestMemArea = 3;  // for now just default to "Holding" for now!
   }

   // validate the request is a valid command code
   startRegister = modMsg.address;
   if ((MOD_WRITE_SINGLE_COIL == modMsg.functionCode) || (MOD_READ_EXCEPTION == modMsg.functionCode) // 2015-07-18 DL
	   || (MOD_READ_SLAVEID == modMsg.functionCode) || (MOD_READ_DIAGNOSTIC == modMsg.functionCode)) // Added 2016-12-28 by DL
	   endRegister = startRegister;					// 2015-07-18 DL
   else												// 2015-07-18 DL
   //endRegister = startRegister + modMsg.byteCount;    // Previously deleted by CDB (per DL on 2016-12-28)
   //endRegister = startRegister + modMsg.byteCount/2;  // CDB rev 7.0 // Deleted 2016-12-28 by DL

   {                                                          // Code section added 2016-12-28 by DL
	   if ((modMsg.functionCode == MOD_READ_COILS)||          // 01
           (modMsg.functionCode == MOD_READ_DIGITALS)||       // 02
		   (modMsg.functionCode == MOD_READ_EXCEPTION) ||     // 07
		   (modMsg.functionCode == MOD_WRITE_MULTIPLE_COILS)) // 15
		   endRegister = startRegister + modMsg.byteCount;    // Old code was correct for bits but not regs
	   else
           endRegister = startRegister + modMsg.byteCount/2;  // Had rev to "/2" previously for all cases but correct for regs only
   }                                                          // End of Code section added 2016-12-28 by DL

   if ((modMsg.functionCode == MOD_READ_COILS)||       // 01
       (modMsg.functionCode == MOD_READ_DIGITALS)||    // 02
       (modMsg.functionCode == MOD_READ_REGISTERS)||   // 04
       (modMsg.functionCode == MOD_READ_HOLDING)||     // 03
       (modMsg.functionCode == MOD_READ_EXCEPTION) ||  // 07  // Added 2015-01-13 by DL
       (modMsg.functionCode == MOD_READ_DIAGNOSTIC) || // 08  // Added 2015-Dec-03 by DL
       (modMsg.functionCode == MOD_READ_EXTENDED)||    // 20
	   (modMsg.functionCode == MOD_READ_SLAVEID) ||    // 17  // Added 2015-Dec-15 by DL
       (modMsg.functionCode == MOD_WRITE_SINGLE_COIL)||     // 05
       (modMsg.functionCode == MOD_WRITE_MULTIPLE_COILS)||  // 0F
       (modMsg.functionCode == MOD_WRITE_HOLDING)||         // 10
       (modMsg.functionCode == MOD_WRITE_SINGLEHOLDING)||   // 06
       (modMsg.functionCode == MOD_MASKEDWRITE_HOLDING)||   // 22
       (modMsg.functionCode == MOD_WRITE_EXTENDED) ||       // 21
	   (modMsg.functionCode == MOD_READWRITE_HOLDING)      // 23
      )
   {
	     CString deb;
         deb.Format("m_PDUSize is %d \n", m_PDUSize);
         OutputDebugString(deb);
         RSDataMessage(deb);


      // Check the request length against our PDU size.
      if ((modMsg.functionCode == MOD_READ_COILS)||      // 01
          (modMsg.functionCode == MOD_READ_DIGITALS)||   // 02
		  (modMsg.functionCode == MOD_READ_EXCEPTION) ||  // 07   Added 2015-01-13 by DL
          (modMsg.functionCode == MOD_WRITE_MULTIPLE_COILS))  // 0F
         numBytesInReq = modMsg.byteCount/8; // # bits
      else
         numBytesInReq = modMsg.byteCount*2; // # registers
	  // Added "+5" on 2017-01-02 by DL to account for the 3 beginning bytes (ID, FC, Count) and 2 CRC ending bytes
	  // Also added cast "(DWORD)" to avoid compiler C4018 warning on 2017-01-02 by DL
      if (((DWORD)numBytesInReq + 5 > m_PDUSize * 2) && (modMsg.functionCode != MOD_READ_DIAGNOSTIC)) // Added MOD_READ_DIAGNOSTIC check 2015-Dec-03 by DL
      {
         MBUSError = TRUE;
         MBUSerrorCode = MOD_EXCEPTION_ILLEGALVALUE;   // too long data field (error > 62)
      }
      else
         MBUSError = FALSE;
   }
   else
   {
      MBUSError = TRUE;
      MBUSerrorCode = MOD_EXCEPTION_ILLEGALFUNC;   // 01
   }
   
/*   if (modMsg.m_packError)
   {
      // request message has a corrupted field somewhere
      MBUSError = TRUE;
      MBUSerrorCode = MOD_EXCEPTION_ILLEGALVALUE;   // too long data field
   }*/

   // 3. build response

   if ((m_MOSCADchecks)&& // Is a (Analog/holding/extended register)
       ((requestMemArea == 2)||(requestMemArea == 3)||(requestMemArea == 4))
      )
   {
   WORD startTable,endTable;     // table #
   WORD startCol,endCol;         // col #
   
      endTable = MOSCADTABLENUMBER(endRegister); // MOSCAD specify register # on the wire for the formula
      endCol = MOSCADCOLNUMBER(endRegister);
      startTable = MOSCADTABLENUMBER(startRegister);
      startCol = MOSCADCOLNUMBER(startRegister);
      // test that this request does not bridge 2 columns.
      // , else we cannot job/request them together.
      if ((endTable != startTable) ||
          (endCol != startCol))
      {
         MBUSError = TRUE;
         MBUSerrorCode = MOD_EXCEPTION_ILLEGALADDR;   // 02
      }
   }
   
   // if we want to disable all writes
   if ((m_disableWrites) &&
       ((modMsg.functionCode == MOD_WRITE_SINGLE_COIL) ||
        (modMsg.functionCode == MOD_WRITE_SINGLEHOLDING) ||
        (modMsg.functionCode == MOD_MASKEDWRITE_HOLDING) ||
        (modMsg.functionCode == MOD_WRITE_MULTIPLE_COILS) ||
        (modMsg.functionCode == MOD_WRITE_HOLDING) ||
        (modMsg.functionCode == MOD_WRITE_EXTENDED) 
       )
      )
   {
   CString deb;
      MBUSError = TRUE;
      MBUSerrorCode = MOD_EXCEPTION_ILLEGALFUNC;   // 02
      deb.Format("Writing data or I/O currently disabled, see Advanced Settings!\n");
      OutputDebugString(deb);
      RSDataMessage(deb);
	  if (m_SaveDebugToFile) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
   }

   if (modMsg.functionCode == MOD_WRITE_MULTIPLE_COILS) 
   {
      if (modMsg.coilByteCount != (modMsg.overalLen-7))
      {    
      CString deb;
         MBUSError = TRUE;
		 // Next line changed 2017-01-01 by DL
         MBUSerrorCode = MOD_EXCEPTION_ILLEGALADDR;   // 02 // Was MOD_EXCEPTION_ILLEGALFUNC should be MOD_EXCEPTION_ILLEGALADDR
         deb.Format("Invalid BYTE Count. Modbus Exception Code 02h\n");
         OutputDebugString(deb);
         RSDataMessage(deb);
		 if (m_SaveDebugToFile) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
		 return(MBUSError);  // Added 2017-01-01 by DL because we do not want the final Exception Code message to appear from here.
      }
   }

   if (modMsg.functionCode == MOD_WRITE_HOLDING)   // Section added 2017-01-01 by DL to detect bad value in write multiple holding regs
   {
      if (modMsg.regByteCount != (modMsg.overalLen-7))
      {    
      CString deb;
         MBUSError = TRUE;
		 // Next line changed 2017-01-01 by DL
         MBUSerrorCode = MOD_EXCEPTION_ILLEGALVALUE;   // 03 
         deb.Format("Invalid BYTE Count. Modbus Exception Code 03h\n");
         OutputDebugString(deb);
         RSDataMessage(deb);
		 if (m_SaveDebugToFile) CRS232Port::WriteToFile(deb);
		 return(MBUSError);
      }
   }                                               // End of Section added 2017-01-01 by DL

   // do an address+length range check too
   // Added tests for WRITE_MULTIPLE_COILS & WRITE_HOLDING on 2017-01-01 by DL
   if ((!MBUSError)&& ((modMsg.functionCode == MOD_WRITE_MULTIPLE_COILS) || (modMsg.functionCode == MOD_WRITE_HOLDING)))
      if ((PLCMemory[requestMemArea].GetSize() < endRegister) && (modMsg.functionCode != MOD_READ_DIAGNOSTIC)) // Added MOD_READ_DIAGNOSTIC check 2015-Dec-03 by DL
      {
         MBUSError = TRUE;
		 // Next line added 2017-01-01 by DL because of bad Exception Code and following two lines commented out
		 MBUSerrorCode = MOD_EXCEPTION_ILLEGALADDR;
         //MBUSerrorCode = (PLCMemory[requestMemArea].GetSize() < startRegister ?
         //                    MOD_EXCEPTION_ILLEGALADDR:MOD_EXCEPTION_ILLEGALVALUE);   // 02:03
      }

   if (MBUSError)
   {
   CString msg;
      msg.Format("Modbus message in error. Exception Code= x%02X\n", MBUSerrorCode);
      OutputDebugString(msg);
      RSDataMessage(msg);
	  if (m_CommsDecodeShow) CRS232Port::WriteToFile(msg);  // Added statement on 2016-12-26 by DL to save to Log file
   }
   return(MBUSError);
}


// --------------------------------- ProcessData -----------------------------
// Interpret MODBUS request pBuffer, and respond to it.
//
BOOL CMOD232CommsProcessor::ProcessData(const CHAR *pBuffer, DWORD numBytes)
{
BYTE  telegramBuffer[MAX_RX_MESSAGELENGTH+MODBUS_FRAME_LENGTH_MAX];
CHAR  debugStr[160];
BYTE  *pDataPortion;
int   i=0;
//WORD  guardword1=1;
WORD  requestMemArea;   // telegram read/write are being referenced=0..MAX_MOD_MEMTYPES
//WORD  guardword2=2;
WORD  startRegister, endRegister, MBUSerrorCode=0;
WORD  seperationOffset; // offset added to each address, due to stations having seperate reg.s
BOOL  MBUSError = TRUE;
WORD  numBytesInReq;
WORD  numRegs;
CString deb;

   m_debuggerStep = 100;
   // inc counter
   pGlobalDialog->PacketsReceivedInc();

   // simulate the I/O and network delays of a real PLC
   Sleep(m_responseDelay);

   //
   // Parse the telegram
   //

   // 1. break up the telegram
   memcpy(telegramBuffer, pBuffer, numBytes);
   CMODMessage::SetEthernetFrames(FALSE);
   CMODMessage  modMsg((CHAR*)telegramBuffer, numBytes);
   
   ActivateStationLED(modMsg.stationID);
   if (!StationIsEnabled(modMsg.stationID))
      return(TRUE);

   // 2. parse it, by testing it first
   CMODMessage  responseModMsg(modMsg); //Call copy constructor


   MBUSError = TestMessage(modMsg, 
                           startRegister,  
                           endRegister,    
                           MBUSerrorCode,  
                           requestMemArea, 
                           numBytesInReq   
                          );
   if (pGlobalDialog->m_seperateRegisters)
   {
      seperationOffset = (WORD)(pGlobalDialog->m_numSeperate * (modMsg.stationID-1)); // Added -1 on 2017-01-06 by DL
      if ((PLCMemory[requestMemArea].GetSize() < seperationOffset+endRegister) ||
          (endRegister > pGlobalDialog->m_numSeperate))
      {
         MBUSError = TRUE;
		 // Next line added 2017-01-01 by DL because of bad Exception Code and following two lines commented out
		 MBUSerrorCode = MOD_EXCEPTION_ILLEGALADDR;
         //MBUSerrorCode = (PLCMemory[requestMemArea].GetSize() < seperationOffset + startRegister ?
         //                    MOD_EXCEPTION_ILLEGALADDR:MOD_EXCEPTION_ILLEGALVALUE);   // 02:03
      }
   }
   else
      seperationOffset = 0;

                        // 1st 3 bytes + any others up to data get 
                        // added in at this time 
   responseModMsg.BuildMessagePreamble(MBUSError,
                                       MBUSerrorCode); 

   // A read must now pack into dataPtr onwards! and calc len etc!
   //   writes must update our mem areas accordingly.
   if (!MBUSError)
   {
      // 4. fill in the data portion of telegram
      switch (modMsg.functionCode)
      {
	  case MOD_READ_DIAGNOSTIC: // 08 READ DIAGNOSTIC - Start of Function Code 8 Subfunction 00 Changes on 2015-Dec-03 by DL
	  if (modMsg.address == 0)  // 08 Diagnostic Functiod Code 8 with Subfunction Code 00
	  {
		 pDataPortion = responseModMsg.dataPtr;   // Get offset to fill in data
         *(WORD*)pDataPortion = modMsg.address;   // This is Subfunction Code 00 here
         modMsg.dataPtr +=2;   // inc pointer by 2 bytes
         pDataPortion += 2;
		 *(WORD*)pDataPortion = SwapBytes(modMsg.byteCount); // This is the 2-byte Query Data field to be echoed back here
         modMsg.dataPtr +=2;   // inc pointer by 2 bytes
         pDataPortion += 2;
		 // Print Debugging Information
		 deb.Format("Respond to Function Code 8, Subfunction 00 with Echo with %d bytes.\n", 8); // Spelling fix by DL 2016-11-02
		 OutputDebugString(deb);
         RSDataMessage(deb);
         if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
         break;
	  }                               // End of Function Code 8 Subfunction 00 Changes on 2015-Dec-03 by DL
      case MOD_READ_COILS     : // 01 READ
      case MOD_READ_DIGITALS  : // 02 READ
      case MOD_READ_EXCEPTION : // 07 READ   Added 2015-01-13 by DL
         pDataPortion = responseModMsg.dataPtr; //Get offset to fill in data
         if (MAX_MOD_MEMWORDS >= modMsg.address + modMsg.byteCount)
         {
         WORD memValueTemp;
         WORD bitOffset;

		    if (modMsg.functionCode == MOD_READ_COILS)           // Added 2015-01-08 by VL
               deb.Format("Read Output Coils from %d for %d bits.\n", modMsg.address, modMsg.byteCount);// Added 2015-01-08 by DL
		    if (modMsg.functionCode == MOD_READ_DIGITALS)       // Added 2015-01-13 by DL
			   deb.Format("Read Inputs from %d for %d bits.\n", modMsg.address, modMsg.byteCount);// Added 2015-01-08 by DL
//          deb.Format("Read In/output from %d for %d bits.\n", modMsg.address, modMsg.byteCount);// Deleted 2015-01-08 by DL
		    if (modMsg.functionCode == MOD_READ_EXCEPTION)      // Added 2015-01-13 by DL
			   deb.Format("Read Outputs as Exception from %d for %d bits.\n", modMsg.address, modMsg.byteCount);// Added 2015-01-13 by DL

            OutputDebugString(deb);
            RSDataMessage(deb);
			if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

            // pack SIM memory: one WORD of sim memory for every BIT in the data.
            // Ie the sim uses one word for each I/O bit in modbus.
            numBytesInReq = modMsg.byteCount/8;
            if (modMsg.byteCount%8)  // if we overflow the byte
               numBytesInReq++;
            for (i=0; i <numBytesInReq;i++)
            {
               // grab the memory now
               memValueTemp = 0;
               for (bitOffset=0;bitOffset<8;bitOffset++)
               {
                  if ((i*8)+bitOffset < modMsg.byteCount)
                     if (PLCMemory[requestMemArea][(seperationOffset + modMsg.address)+(i*8)+bitOffset])
                        memValueTemp += (0x01<<bitOffset);
                     // else bit is off
               }
               *(BYTE*)pDataPortion =  (BYTE)memValueTemp;
               pDataPortion +=1;
            }

         }
         else
         {
            // pack the exception code into the message
            ASSERT(0); // this is supposed to be caught in TestMessage()
            responseModMsg.buffer[1] |= 0x80;
            responseModMsg.buffer[2] = 0x02;    // exception code here (could also use 0x03)
         }
         break;

      case MOD_READ_REGISTERS : 
      case MOD_READ_HOLDING   :
      case MOD_READ_EXTENDED  :
         pDataPortion = responseModMsg.dataPtr; //Get offset to fill in data
         if (MAX_MOD_MEMWORDS >= modMsg.address + modMsg.byteCount)
         {
            WORD memValueTemp;

		    if (modMsg.functionCode == MOD_READ_REGISTERS)           // Added 2015-01-09 by DL
               deb.Format("Read Input Regs from %d for %d.\n", modMsg.address, modMsg.byteCount);// Added 2015-01-09 by DL
		    else                                                 // Added 2015-01-08 by DL
			   deb.Format("Read Holding Regs from %d for %d.\n", modMsg.address, modMsg.byteCount);// Added 2015-01-09 by DL
//          deb.Format("Read Register from %d for %d .\n", modMsg.address, modMsg.byteCount); // Removed 2015-01-09 by DL
            OutputDebugString(deb);
            RSDataMessage(deb);
            if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

            for (i=0; i <modMsg.byteCount;i++)
            {
               // grab the memory now
               memValueTemp = PLCMemory[requestMemArea][(seperationOffset + modMsg.address)+i];
               *(WORD*)pDataPortion =  SwapBytes( memValueTemp );
               pDataPortion += 2;
            }
         }
         else
         {
            // pack the exception code into the message
            ASSERT(0); // this is supposed to be caught in TestMessage()
            responseModMsg.buffer[1] |= 0x80;
            responseModMsg.buffer[2] = 0x02;    // exception code here (could also use 0x03)

            deb.Format("Read register past %d error x%02X!\n", MAX_MOD_MEMWORDS, (BYTE)responseModMsg.buffer[2]);
            OutputDebugString(deb);
            RSDataMessage(deb);
            if (m_SaveDebugToFile) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

         }
         break;
      default :
          // Writes acks are all built in copy constructor
          // But the update is done here!
          if (MAX_MOD_MEMWORDS >= modMsg.address + modMsg.byteCount)
          {
          // lock memory for writting
          CMemWriteLock lk(PLCMemory.GetMutex());
      
              pDataPortion = responseModMsg.dataPtr; //Get offset to fill in data

              if (!lk.IsLocked())
                 //...Update
                 switch (modMsg.functionCode)
                 { 
                 case MOD_WRITE_SINGLE_COIL     :
                     {
                     CString deb;
                        deb.Format("Write single output coil %d.\n", modMsg.address);
                        OutputDebugString(deb);
                        RSDataMessage(deb);
			            if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
                     }
                     //data gets copied in now
                     if (m_modifyThenRespond)
                        PLCMemory.SetAt(requestMemArea, seperationOffset +modMsg.address, (*(WORD*)modMsg.dataPtr?1:0));

//                     pDataPortion = responseModMsg.dataPtr; //Get offset to fill in data
                     *pDataPortion++ = (PLCMemory[requestMemArea][(seperationOffset +modMsg.address)+i] ? 0xFF : 0x00);
                     *pDataPortion++ = 0x00;
                     if (!m_modifyThenRespond)
                        PLCMemory.SetAt(requestMemArea, seperationOffset +modMsg.address, (*(WORD*)modMsg.dataPtr?1:0));
                     
                     numRegs = 1;   // repaint 1 item

                     break;
                 case MOD_WRITE_MULTIPLE_COILS  :
                    // unpack into the SIMul memory on WORD of sim memory for every BIT in the data 
                     {
                        int coilCount = modMsg.byteCount;
                        numBytesInReq = modMsg.count/8;
                        if (modMsg.count%8)  // if we overflow a byte
                           numBytesInReq++;

                        {
                        CString deb;
                           deb.Format("Write multiple outputs coils from %d for %d bits.\n", modMsg.address, modMsg.count);
                           OutputDebugString(deb);
                           RSDataMessage(deb);
			               if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

                        }
                        numRegs = numBytesInReq * 8 ;   // repaint X bits // Multiplied by 8 to get total BITS on 2015-01-19 by DL
                     
                        while (coilCount>0)
                        {
                           for (i=0;i<numBytesInReq;i++)
                           {
                           WORD bitOffset;

                              for (bitOffset=0;bitOffset<8;bitOffset++)
                              {
                                 if (coilCount >0)
                                 {
                                    coilCount--;
                                    if (*(BYTE*)modMsg.dataPtr & (0x01<<bitOffset))
                                       PLCMemory.SetAt(requestMemArea, seperationOffset +modMsg.address+(i*8)+bitOffset, 1);
                                    else
                                       PLCMemory.SetAt(requestMemArea, seperationOffset +modMsg.address+(i*8)+bitOffset, 0);
                                 }
                              }
                              modMsg.dataPtr++;
                           }
                        }
                     }
                     break;

                 case MOD_WRITE_HOLDING : //WRITE multiple holdings
                 case MOD_WRITE_EXTENDED:
                     numRegs = modMsg.byteCount/2;

                     {
                     CString deb;
		             if (modMsg.functionCode == MOD_WRITE_HOLDING)           // Added 2016-04-19 by DL
                        deb.Format("Write multiple Holding Registers from %d for %d.\n", modMsg.address, numRegs);// Added 2016-04-19 by DL
		             else                                                 // Added 2016-04-19 by DL
			            deb.Format("Write multiple Extended Registers from %d for %d.\n", modMsg.address, numRegs);// Added 2016-04-19 by DL
//                      deb.Format("Write multiple Extended registers from %d for %d registers.\n", modMsg.address, numRegs); // Deleted 2016-04-19
                     OutputDebugString(deb);
                     RSDataMessage(deb);
			         if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
                     }

                     for (i=0;i<numRegs;i++)
                     {
                        PLCMemory.SetAt(requestMemArea, seperationOffset + modMsg.address + i, SwapBytes(*(WORD*)modMsg.dataPtr));
                        modMsg.dataPtr +=2;   // inc pointer by 2 bytes
                     }
                     break;

                 case MOD_WRITE_SINGLEHOLDING : //WRITE single holding reg.
                     {
                     CString deb;
                     WORD memValueTemp;
                        deb.Format("Write single Holding register %d.\n", modMsg.address);
                        OutputDebugString(deb);
                        RSDataMessage(deb);
			            if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

                        numRegs = 1;   //repaint 1 register

                        if (m_modifyThenRespond)
                           PLCMemory.SetAt(requestMemArea, seperationOffset + modMsg.address , SwapBytes(*(WORD*)modMsg.dataPtr));

                        memValueTemp = PLCMemory[requestMemArea][(seperationOffset +modMsg.address)];
                        *(WORD*)pDataPortion =  SwapBytes( memValueTemp );

                        if (!m_modifyThenRespond)
                           PLCMemory.SetAt(requestMemArea, seperationOffset + modMsg.address , SwapBytes(*(WORD*)modMsg.dataPtr));
                        modMsg.dataPtr +=2;   // inc pointer by 2 bytes
                        pDataPortion += 2;

                     }
                     break;

                 case MOD_MASKEDWRITE_HOLDING : // WRITE with MASK single holding reg.
                     {
                     CString deb;
                     WORD memValueTemp, memValueResult;
                     WORD orMask, andMask;

                        deb.Format("Write Mask Holding register %d.\n", modMsg.address);
                        OutputDebugString(deb);
                        RSDataMessage(deb);
			            if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

						// fetch it
                        memValueTemp = PLCMemory[requestMemArea][(seperationOffset +modMsg.address)];
                        // do the changes requested
						modMsg.dataPtr -= sizeof(WORD) * 2;			// Added Statement by DL for Pointer Fix on 2014-12-28
                        andMask = SwapBytes(*(WORD*)modMsg.dataPtr);
                        modMsg.dataPtr += sizeof(WORD);
                        orMask = SwapBytes(*(WORD*)modMsg.dataPtr);
                        modMsg.dataPtr += sizeof(WORD);
                        memValueResult = ( memValueTemp & andMask) | ( orMask & (~andMask));

                        deb.Format("In=%04X And=%04X Or=%04X Out=%04X.\n", memValueTemp, andMask, orMask, memValueResult);
                        OutputDebugString(deb);
                        RSDataMessage(deb);
			            if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file

                        numRegs = 1;   //repaint 1 register

                        //PLCMemory.SetAt(requestMemArea, seperationOffset + modMsg.address , SwapBytes(memValueResult)); // Was
                        PLCMemory.SetAt(requestMemArea, seperationOffset + modMsg.address , memValueResult);  // DL Fix 2014-12-28
                     }
                     break;
                 default:
                    ASSERT(0);
                    break;
                 }
          }
		  else                            // Added 2015-01-21 by DL to catch bad counts found in testing writing single coil 65536
			  return (FALSE);             // Added 2015-01-21 by DL to catch bad counts found in testing writing single coil 65536
          // we can only call on the GUI thread once we have un-locked
          if (pGlobalDialog)
		  {
			 if (!pGlobalDialog->m_seperateRegisters)			// If not seperate registers redraw only small areas
			 {
             int cols = pGlobalDialog->GetListDisplayedWidth();
             pGlobalDialog->RedrawListItems(modMsg.GetAddressArea(modMsg.functionCode), 
                                            (seperationOffset +modMsg.address)/(cols), 
                                            (seperationOffset +modMsg.address+(numRegs-1))/(cols)
                                           ); // repaint only the needed rows
			 }
			 else
			 {
			  // repaint all of the screen areas registers by DL on 2015-07-18
				pGlobalDialog->RedrawListItems(modMsg.GetAddressArea(modMsg.functionCode),0,6554);
			 }
          }
          break;
      case MOD_READ_SLAVEID:
		 pDataPortion = responseModMsg.dataPtr;   // Get offset to fill in data
         *pDataPortion = 9;   // # of Bytes
         modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 *pDataPortion = 9;   // 9 for 984
		 modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
         *pDataPortion = 255;   // Running
         modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 *pDataPortion = 1;   // 4k sectors of Page 0 memory
		 modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
         *pDataPortion = 2;   // 1k sectors of state RAM
         modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 *pDataPortion = 1;   // Count of segments of User Logic
		 modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
         *pDataPortion = 1;   // Bitmapped
         modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 *pDataPortion = 160; // Bitmapped
		 modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 *pDataPortion = 0;   // Bitmapped Error Code
         modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 *pDataPortion = 0;   // Bitmapped Error Code
		 modMsg.dataPtr ++;   // inc pointer
         pDataPortion ++;
		 // Print Debugging Information
		 deb.Format("Respond to Function Code 17 with %d bytes.\n", 14);  // Spelling fix by DL 2016-11-02
		 OutputDebugString(deb);
         RSDataMessage(deb);
         if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
         break;
      }
   }
   else
   { // error occurred
      pDataPortion = responseModMsg.dataPtr; //Get offset to fill in data
   }
   if (0 == modMsg.stationID)   // broadcast, don't respond at all.
   {							// Write to debug window this was Station #0 (broadcast). No Response required.
		CString deb;			// 2015-07-22 by DL Added Debug Message to Form Comms window for Station #0
		deb.Format("Station #0 so no response allowed.\n");
		//OutputDebugString(deb);	// This writes to the Output Window when debugging
		RSDataMessage(deb);			// This writes to the Window Form's Comms page
        if (m_CommsDecodeShow) CRS232Port::WriteToFile(deb);  // Added statement on 2016-12-26 by DL to save to Log file
		return (TRUE);
   }							// 2015-07-22 by DL End of Debug Message for Station #0 Add
   
   // finnish building the response
   responseModMsg.totalLen = (WORD)((LONG)pDataPortion-(LONG)responseModMsg.buffer);
   responseModMsg.totalLen += MODBUS_CRC_LEN;

   // 5. append the CRC
   //OutputDebugString("Calculate CRC\n");
   {
      responseModMsg.BuildMessageEnd();
   }
   
   // 6. send it back
   m_debuggerStep = 102;

#ifdef _COMMS_DEBUGGING
   sprintf(debugStr, "Send %d bytes\n", responseModMsg.totalLen);
   OutputDebugString(debugStr);
#endif
   
   RSStateChanged(RSPORTCURRENTLY_WRITTING);
   // Send it on the wire , but first kill any incomming messages as well so we don't overflow or anything
   Purge();
   if (!m_NoiseSimulator.ErrorsEnabled())
      Send(responseModMsg.totalLen, (BYTE*)responseModMsg.buffer, debugStr);
   else
   {
      // disabled v7.7
      m_NoiseSimulator.InjectErrors((CRS232Port*)this, (BYTE*)responseModMsg.buffer, responseModMsg.totalLen, debugStr);
      RSDataMessage("Comm-Error injection/simulation active.\n");
	  // Added statement on 2016-12-26 by DL to save to Log file
	  if (m_SaveDebugToFile) CRS232Port::WriteToFile("Comm-Error injection/simulation active.\n"); 
   }
   m_debuggerStep = 103;
   
   // inc our counter
   pGlobalDialog->PacketsSentInc();

   return (TRUE);
} // ProcessData

// ----------------------------- LoadRegistersIMP -----------------------------
// STATIC : implements the function to load register values from file
CString CMOD232CommsProcessor::LoadRegistersIMP()  // Was BOOL but I wanted to return the error. Changed 2016-08-06 by DL
{
CFileException ex;
CFile dat;
LONG area;
DWORD wordIndex;

   if (!dat.Open("MODDATA.DAT", CFile::modeRead|CFile::shareDenyRead, &ex) )
   {
      // complain if an error happened
      // no need to delete the exception object

      TCHAR szError[1024];
	  CString deb;
      ex.GetErrorMessage(szError, 1024);
	  deb.Format("Couldn't open source file: %s\n", szError);
      OutputDebugString(deb);
	  return deb;   // was "return FALSE. Changed on 2016-08-07 by DL
   }
   // read it in
   for (area=0;area < MAX_MOD_MEMTYPES - 1;area++) // added -1 on 2016-08-06 by D. Lyons. This was loading more than required
   {
   DWORD numRead;  //,totalRead=0;    // Removed as unused on 2016-08-06 by DL
   WORD dataWord;
   DWORD maxIndex = PLCMemory[area].GetSize();

      // lock the memory for reading
//      CMemWriteLock lk(pGlobalDialog->m_pMemWriteSync);
      CMemWriteLock  lk(PLCMemory.GetMutex());
      // loop thru all registers (WORD)
      if (!lk.IsLocked())
      {
         for (wordIndex=0; wordIndex < maxIndex/*MAX_MOD_MEMWORDS*/; wordIndex++)
         {
            numRead = dat.Read((BYTE*)&dataWord, sizeof(WORD));
            PLCMemory.SetAt(area, wordIndex, dataWord);
            //totalRead +=numRead;          // Removed as unused on 2016-08-06 by DL
            if (numRead != sizeof(WORD))
			   return "Reading Needed MODDATA.DAT Failed!";   // was "return FALSE". Changed on 2016-08-07 by DL
         }
         // Read past the rest of the block
         while (wordIndex < MAX_MOD_MEMWORDS)
         {
            numRead = dat.Read((BYTE*)&dataWord, sizeof(WORD));
            //totalRead +=numRead;            // Removed as unused on 2016-08-06 by DL
            if (numRead != sizeof(WORD))
			   return "Reading Extra Unneeded MODDATA.DAT Failed!";   // was "return FALSE". Changed on 2016-08-07 by DL
            wordIndex++;
         }
      }
      else
      {
      CString errorMsg;
         //error
         errorMsg.LoadString(IDS_SYNC_READING);
         AfxMessageBox(errorMsg, MB_ICONEXCLAMATION);
		 return errorMsg;   // was "return FALSE" changed on 2016-08-07 by DL
      }
   }
   return "";   // was "return TRUE" for successful completion. Changed on 2016-08-07 by DL
}

// ----------------------------- LoadRegisters -----------------------------
// load binary dump of the register values from file.
BOOL CMOD232CommsProcessor::LoadRegisters()
{
BOOL retstatus;	// Return value True = Good. Added 2016-08-06 by DL
// The next statement previously returned a BOOL, but I wanted to show the error from the Subroutine call
CString ret = LoadRegistersIMP();  // Was "BOOL ret =". Changed 2016-08-06 by DL

   if (ret=="")                    // ret=="" means that everything worked fine
   {
      RSDataMessage("Register values loaded OK\n");
	  // Added statement on 2016-12-26 by DL to save to Log file
	  if (m_SaveDebugToFile) CRS232Port::WriteToFile("Register values loaded OK\n"); 
	  retstatus = TRUE;            // Added 2016-08-06 by DL
   }
   else                            // Added next six statements on 2016-08-06 by DL to display errors
   {
	  RSDataMessage(ret);
	  AfxMessageBox(ret, MB_OK|MB_ICONINFORMATION);
	  retstatus = FALSE;
   }                               // End of added statements on 2016-08-06 by DL
   return (retstatus);             // was return (ret). Changed on 2016-08-06 by DL
} // LoadRegisters


// --------------------------------------- SaveRegisters ---------------------------
// save a binary dump of the values to file.
BOOL CMOD232CommsProcessor::SaveRegisters()
{
BOOL retstatus;	// Return value True = Good. Added 2016-08-06 by DL
// The next statement previously returned a BOOL, but I wanted to show the error from the Subroutine call
CString ret = SaveRegistersIMP();  // Was "BOOL ret =". Changed 2016-08-06 by DL

   if (ret=="")                    // ret=="" means that everything worked fine
   {
      RSDataMessage("Register values saved OK\n");
	  // Added statement on 2016-12-26 by DL to save to Log file
	  if (m_SaveDebugToFile) CRS232Port::WriteToFile("Register values saved OK\n"); 
	  retstatus = TRUE;            // Added 2016-08-06 by DL
   }
   else                            // Added next six statements on 2016-08-06 by DL to display errors
   {
	  RSDataMessage(ret);
	  AfxMessageBox(ret, MB_OK|MB_ICONINFORMATION);
	  retstatus = FALSE;
   }                               // End of added statements on 2016-08-06 by DL
   return (retstatus);             // was return (ret). Changed on 2016-08-06 by DL
}

// --------------------------------------- SaveRegistersIMP ---------------------------
// STATIC : save all register values to a flat file.
CString CMOD232CommsProcessor::SaveRegistersIMP()  // Was BOOL but I wanted to return the error. Changed 2016-08-06 by DL
{
CFileException ex;
CFile dat;
LONG area;
DWORD wordIndex;

   if (!dat.Open("MODDATA.DAT", CFile::modeWrite | CFile::shareExclusive | CFile::modeCreate, &ex) )
   {
      // complain if an error happened
      // no need to delete the ex object
      TCHAR szError[1024];
	  CString deb;
      ex.GetErrorMessage(szError, 1024);
	  deb.Format("Couldn't open source file: %s\n", szError);
      OutputDebugString(deb);

	  return deb;   // was "return FALSE". Changed on 2016-08-07 by DL
   }
   // read it in
   for (area=0;area < MAX_MOD_MEMTYPES - 1;area++)  // added "-1" on 2016-08-06 by D. Lyons. This was saving more than required
   {
   WORD wordData;
   DWORD maxIndex = PLCMemory[area].GetSize();
      // lock the memory for writting
      CMemWriteLock lk(PLCMemory.GetMutex());
      // loop thru all registers (WORD)
      if (!lk.IsLocked())
      {
         for (wordIndex=0; wordIndex < maxIndex/*MAX_MOD_MEMWORDS*/; wordIndex++)
         {
            wordData = PLCMemory[area][wordIndex];
            dat.Write((BYTE*)&wordData, sizeof(WORD));
         }
         // Fill the rest with NULLs. This makes the file the same size and layout regardless of the # of registers defined
         while (wordIndex < MAX_MOD_MEMWORDS)
         {
            wordData = 0;
            dat.Write((BYTE*)&wordData, sizeof(WORD));
            wordIndex++;
         }
      }
      else
      {
      CString errorMsg;
         errorMsg.LoadString(IDS_SYNC_WRITTING);
         AfxMessageBox(errorMsg, MB_ICONEXCLAMATION);
         // error
         return errorMsg;   // was "return FALSE". Changed on 2016-08-07 by DL
      }
   }
   return "";   // was "return TRUE" for successful completion. Changed on 2016-08-07 by DL
} // SaveRegistersIMP

// ------------------------------- ActivateStation ---------------------------
void CMOD232CommsProcessor::ActivateStationLED(LONG stationID)
{
   if (stationID>=0 && stationID<STATIONTICKBOXESMAX)	// 2015-07-22 by DL added "=" for Active Station #0
   {
      //start the counter for this station at the beginning
      pGlobalDialog->m_microTicksCountDown[stationID] = pGlobalDialog->GetAnimationOnPeriod();
      // it will count down untill it extinguishes
   }
} // ActivateStation

// ------------------------------- StationIsEnabled ---------------------------
// Return TRUE if station is enabled
BOOL CMOD232CommsProcessor::StationIsEnabled(LONG stationID)
{
   if (stationID>=0 && stationID<STATIONTICKBOXESMAX)	// 2015-07-22 by DL added "=" for Active Station #0
   {
      return (pGlobalDialog->StationEnabled(stationID));//m_microTickState==1);
   }
   return TRUE;
} // StationIsEnabled
