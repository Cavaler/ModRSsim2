#ifndef CSVREADER_H_INCLUDED
#define CSVREADER_H_INCLUDED

// include the instrument#
#define MAX_CSVFILE_COLUMNS   4	// Changed from 17 to 1 for better readability by DL on 2015-01-02
// Changed from 1 to 4 for using first number as index and second number as floating point value on 2015-Nov-15 by DL
#define MAX_DEBUG_STR_LEN     256


class CRegisterUpdaterIF
{
public:
   virtual void DebugMessage(LPCTSTR message)=0;
   virtual BOOL SetRegister(LONG index, WORD value)=0;
   virtual BOOL SetAnalogs(LONG index, WORD value)=0;   // Added 2015-Nov-15 by DL
   virtual BOOL SetCoils(LONG index, WORD value)=0;     // Added 2015-Nov-16 by DL
   virtual BOOL SetInputs(LONG index, WORD value)=0;    // Added 2015-Nov-16 by DL
   virtual BOOL ModbusClone() = 0;
};


//////////////////////////////////////////////////////////////////////////////
// CCSVTextImporter
class CCSVTextImporter : public CObject
{
public:
   CCSVTextImporter();
   virtual ~CCSVTextImporter();


private:
   //friend class
   //////////////////////////////////////////////////////////////////////////////
   // CCSVTextLine
   class CCSVTextLine : public CString
   {
   //DECLARE_DYNAMIC(CCSVTextLine)
   friend class CCSVLineArray;

   public:
      CCSVTextLine(LPCSTR string);
      CCSVTextLine(CCSVTextLine& other);
      CCSVTextLine();

      CCSVTextLine & operator = (CCSVTextLine &otherLine);
      CCSVTextLine & operator = (CString &otherString);

      // functions to get the values out
      double GetElement(LONG index);

      void  Parse();

   private:
      //double m_double;

      // misc variables
      BOOL     m_init;
      INT      m_sizedouble;
      double   m_values[MAX_CSVFILE_COLUMNS];

   };

   //////////////////////////////////////////////////////////////////////////////
   // CCSVLineArray
   class CCSVLineArray : public CObArray
   {
   friend class CCSVTextLine;
   public:
      ~CCSVLineArray();
   
      LONG Add(CCSVTextLine *pLine);
      CCSVTextLine *operator [](LONG index);
      CCSVTextLine *GetAt(LONG index);
   };


   //////////////////////////////////////////////////////////////////////////////
   // CCSVTextFile
   class CCSVTextFile : public CFile
   {
   public:
      CCSVTextFile(LPCTSTR fileName, UINT flags);
      ~CCSVTextFile();

      LPCTSTR Data();
      
   private:
      DWORD  m_length;
      BYTE * m_data;
   };

// main class
   friend class CCSVLineArray;
public:

   LONG ImportFile(LPCTSTR csvName, BOOL manual=FALSE, CRegisterUpdaterIF * =0);
   BOOL HandleTimer(LPCTSTR importFolder, CRegisterUpdaterIF *pParentInterface);

   double GetElement(LONG line, LONG index);
   LONG LineCount() {return((myArray?myArray->GetSize():0));}; // # instruments

   void Open(LPCTSTR fileName);
   BOOL  LoadedOK();

   LONG UpdateRegisters(BOOL manual);
   CCSVLineArray *myArray;

private:
   CString m_lastProcessed;
   SYSTEMTIME  m_lastInterval;

   //CCSVLineArray *myArray;
   CRegisterUpdaterIF *m_parentInterface;
};

#endif // CSVREADER_H_INCLUDED
