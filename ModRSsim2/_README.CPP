/////////////////////////////////////////////////////////////////////////////
//
// FILE: README.cpp
//
// Mod_RSSim (c) Embedded Intelligence Ltd. 1993,2009
// AUTHOR: Conrad Braam.  
// Homepage: http://www.plcsimulator.org
// email: zaphodikus@hotmail.com
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
//
// In the words of Paul DiLascia: If this code works, it was 
// written by Conrad Braam.
// If not, I don't know who wrote it.
// Compiles with Visual C++ 6.0, VC 9.0 . Runs on NT4, Windows 2000, XP, 98b and probably 95 too.
//
// LIMITATIONS:
// Not all of this code is production code, and as such is not fully tested. 
// Use at your own risk.
//
// PURPOSE:
// This simulator was written to allow the testing of a MODBUS RTU driver without
// having to get 256 little MODBUS PLC's into one office. It is not by any means 
// a full implementation of the protocol, implementing only the common functions
// used in MODBUS. This simulator actually originated on Windows SocketsAPI, for 
// the Texas Instruments protcol, but that is another story. It surpasses 
// limitations in other programs we have found in the past by:
// 1. Allowing to simulate more than one RTU at once.
// 2. Allowing the editing+display of all the Registers 
// 3. Allowing the scanning of registers not actually in the display.
// Missing:
// 1. running on >1 comm ports, (you have to run 2.. copies of the program)
// 2. ASCII protocol support
// 3. Display statuses for all outstations simulated.
//
// MORE:
// This program is not a released work, and can be freely distributed, so 
// long as this header is immortalized. (I.e. After I die, any copies of this 
// must still hold my name.)
// home : www.plcsimulator.org 
//  . I dig the code that Paul DiLascia writes.
//
//
// PROGRAM STRUCTURE:
// By way of a class diagram, the program is based on a Dialog app.
// Diagram symbols used :
// * =many         (collection)
// o =encapsulate  (has-a)
// %>=derived from (is-a) or %<
//
// CWinApp %< CMOD_simApp                     ;application object
// CDialog %< CMOD_simDlg                     ;main window
//                        o> CResizer         ;window resizing
//                        o> CServerRS232Array *> CMODRS232CommsProcessor %> CRS232Port
//                                                                        o> CRS232Noise
//                        o> CServerSockArray *> CMODEthCommsProcessor %> CDDKServSocket %> CDDKSocket
//                                                                     o> CRS232Noise
//                        o> CServerRS232Array *> CAB232CommsProcessor %> CPLCApplication %> CABCommsProcessor %> CCommsProcessor %> CRS232Port
//                                                                                                                                o> CRS232Noise
//
//                        o> CControllerMemory *> CControllerMemoryArea *> WORD
// CDialog %< CAboutDlg                                        ; application about window
//                      o> CEasterDlg                          ; Easter egg
//                                    o> CStarWarsCtrl %> CStatic
// CStatic %< CStaticLink                                      ; click-able web URL link
// CButton %< CMicroTick                                       ; clickable colored tick-box
// CMemWriteLock %< CSingleLock                                ; Mutex to control PLC RAM accesses
// CButton %< CCommsLamp                                       ; activity indicator LED
// CListCtrl %< CMemoryEditorList                              ; list-control for registers
// CObject %< DRegKey                                          ; registry class
// CComboBox %< CColorPickerCB                                 ; combo list of colors
// CObject %< CMODMessage                                      ; telegram parsing/building class
// CObject %< CRS232Noise                                      ; comms fault simulator
//
/////////////////////////////////////////////////////////////////////////////////////
//
// REVISIONS:
// Version 2.3b    xx/xx/xx   CDB Initial rev submitted to CodeGuru site
// Version 2.4     22/07/02   CDB Fixed and support Fn 10h and 15h (Write multiple 
//                            register/extended registers)
// Version 2.5     28/07/02   CDB conditional compilation macros _COMMS_DEBUGGING and
//                            _UI_DEBUGGING added, specify in build options to enable 
//                            more debugg'n messages. Display of data and addresses 
//                            format is selectable now.
// Version 2.6     29/07/02   CDB Reading digital I/O showed me a bug where the
//                            CMODCommsProcessor was reading the wrong address, see 
//                            UnPackField()
// Version 2.8     07/08/02   CDB Web-link to modicon.com. Tooltips added. Also allow 
//                            64K registers instead of 9999 (10K) uses more RAM now.
//                            Window "stay-on-top" added.
// Version 2.9     14/08/02   CDB Tooltips work in all dialogs except value edit dialog.
//                            This dialog still has some bugs in it.
// Version 3.1     27/10/02   CDB Devices can all start "off", regsiters can load at start-up.
//                            EasterEgg added to "About" box.
// Version 3.2     02/11/02   HF : (Hirofumi Fudoudou) Hirofumi Fudoudou's changes added
//                            CDB : Easter-credits updated
// Version 3.3     08/11/02   submitted by HF : Integrated transparency index bitmap fixes. PLC write 
//                            address returned in telegram corrected too.
// Version 3.4     11/11/02   CDB : Function 05 (write single coil) is now supported and tested.
// Version 3.4a    12/11/02   CDB : remember the register area type between load-ups
// Version 3.4b    21/11/02   CDB : Can now set upper limit to # registers.
// Version 3.4c    25/11/02   CDB : Write multiple coils 0F is supported now, # registers 
//                                  per req. is settable too.
//         3.4d    27/11/02   CDB : RTS can now be set for enable/dis/h-shake/toggle.
// version 3.5     18/12/02   CDB +AP : Renamed the MODCommsProcessor class to CMODCommsProcessor
//                            (Álvaro Palma) Fixed the read-ahead length calculation. 
//                 20/01/03   CDB focus rectangle for the tick-boxes added, tab order here fixed too.
// version 3.6     23/01/03   CDB Keyboard only friendly hotkeys added to get to the other dialogs.
//                            [duplicate toolbar]
// version 4.0     22/03/03   CDB station activity indicators all accessible now, comms analyser 
//                            screen added. CMicroTick over-painting of parent Wnd by 1 pixel fixed.
//                            Class header files tidied up. Registers list redraw & CPU load when 
//                            doing writes reduced.
// Version 4.1     24/03/03   CDB Option so that coils modify before the response is sent- see eMulation 
//                            dialog
// Version 4.2     02/04/03   CDB Emulations settings 'tick boxes' update the comms class, new tick 
//                            for disallowing writes to PLC memory.
// Version 4.3     04/04/03   CDB If scrolling thru stations, the comms engine was fetching the wrong 
//                            station enabled tickbox to see if it was enabled.
// Version 4.4     11/04/03   CDB dialog can now be re-sized, thanks to CResizer class by Dmitry Kochin 
//                            <dco@mail.ru>
// Version 5.0     02/05/03   CDB Merged Ethernet TCP functions into the sim/ Added training simulation.
// Version 5.1     05/05/03   CDB Splash screen- Training sim bugs sorted out.
// Version 5.2     XX/XX/XX   CDB Added tooltips support for diabled controls to trainer dialog 
//                            Ray Almog [almog@notes.iet.co.il] from - http://www.codeguru.com/controls/tooltip_for_disabled_control.shtml
// Version 5.3     15/05/03   CDB Icon for application changes from Modbus Ethernet to Serial.
//                            <ENTER> key trapped in main dialog.
// Version 6.0     20/05/03   CDB Allen-Bradley DF1 protocol added
// Version 6.1     09/06/03   CDB BCC and CRC option on Allen-Bradley added
// Version 6.2     24/06/03   CDB Modbus RS232 disabled station stations no longer respond as it should be.
//         6.3     06/07/03   CDB debugger screen data was not hex coded correctly, Eth. message frame trans # bytes are handled correctly too.
//                            fault reported by BMG  
// Version 6.4     28/08/03   CDB When funny chars come in on serial, we sometimes had crashes, if port settings are invalid, there 
//                            is now a warning messagebox!
//         6.5     08/09/03   Time license + : Patrick SOFORCADA fixed length calc. for force multiple coils - patrick.saforcada@ineo.com
//         6.6     08/10/03   CDB Help updated, Joy protocol seriously tweaked, new setup dialog for joy
//         6.7     16/10/03   CDB Modbus Ethernet - COMMS screen error message if station ID not active.
// Submitted 6.7 to submit@codeguru.com on 21/10/03
//         6.8     26/10/03   CDB Splash is now "programmed" in text-format, About text changed a bit+ mouse cursor over 
//                            links fixed.
//         6.9     06/12/03   CDB still need to sort out write multiple coils when writting 16. modem status lights added.
//         7.0     14/12/03   CDB cleaned up include statements; multiple stations have seperate modbus RTU registers. 
//                            write multiple coils fixed; Ctrl-J injects random char.
//         7.1     06/08/04   CDB Digital items now display with a darker background when "on" '1' to make them easier to spot.
//                            requested by Canada customer
//         7.2     30/10/04   CDB you can now specify a # of bits that is not a multiple of 16 for modbus coils and relays (in fact any bit-area)
//         7.3     29/10/06   CDB toolbar button to open/close port
//         7.4     22/01/07   CDB Write multiple coils wrote one too many, fixed. (reported by nilsfenner@web.de)
//         7.4b               Write single coil was dividing the returned coil address by 2, fixed now too
//                            __TEST_INJECTMSG macro added to allow testing of any message
//         <branch>
//         7.3     30/09/05   CDB Creep fix: Script engine/parser was persisting the script text all of the time
//         7.4     16/02/06   CDB ASCII/text display mode to allow string debugging; e-mail address changed
//         7.5     25/03/06   CDB When TCP/IP requests Queue up too quickly, the simulator now processes them all instead of just the first one,
//                            comments tidied a little; modbus TCP/IP can also simulate some errors now.
//         7.6     11/04/06   CDB script run interval is more intuitive now.
//         <merge>
//         7.7     22/07/07   CDB merged branched code, added HTML output GUI feature; disabled the ERROR-simulation function for now.
//                            Nag-screen no longer 'spams' and locks the UI.
//         7.8     09/09/07   CDB - floating-point editing values in the table "never really worked", was displaying 0 and putting 0 in.
//                            telegram Data to have spaces between bytes 00 00 00 = (seems to be in now)
//         7.9     03/02/08   Baud Rate is 57600 not 64800 (Ken Robbins)
//
//         8.0     10/2/08 - started CSV importer for Vinay Kumar
//         8.1     28/3/08 - improved debugging msgs for VB script scriptlet errors
//         8.2     05/05/08 - CDB : Added RS232 Modbus support for function 16 Write mask holding register
//         8.3                CDB , ExistFile() moved from csvreader.cpp into mylib.lib
//         8.4     15/06/08 - CDB __TEST_INJECTMSG macro removed now that I have a integration-test app. Adding commandline support
//                            for Leendert in't Veld . Demo interval increased from 30 to 45 minutes.
//         8.5     17/06/08 - CDB Toggling channel closed then opening a dialog raised an exception, reported by Steve Bardsley. fixed.
//                            Help text updated to show command-line stuff from rev 8.04
//         8.6     26/06/08 - CDB released a debug build by mistake. Fixed ASSERTion in the animation dialog. Fixed Easter-egg scroller only
//                            showling first 11 lines of text.
//         8.7     30/06/08 - CDB Added RTS indication on serial modbus protocol title-bar text R-en/R-tg...
//         8.8     14/08/08 - CDB Removed SCRIPTTEXT_ISPERSISTENT flag from scripting site since it caused creep (it now has a CPU load hit)
//         8.09    25/11/08 - CDB addresses as displayed in the edit item dialog are now 400000 not 40000 to adjust for >10000 registers.
//         8.10    29/03/09 - CDB force multiple coils no longer forces a multiple of 8 coils (thanks to Alex Shinkin)
//         8.11    16/10/09 - CDB Convert project to VS9 mesage-handlers prototypes fixed - lots of CRT_SECURE type warnings to still patch-up
//                            station tickbox array overflow bug fixed. Sourceforge hosted release of a VS2008 project.
//         8.12    27/10/09 - CDB splash updated. Removed my old Company email address and domain from most of the code.
//		   8.13    01/11/09 - thanks to Joan Lluch-Zorrilla write single coil response is now correct. Compile options set for compile size,
//                            resources trimmed slightly to shrink EXE a little. Fixed a small intermittent stack corruption
//                            and tidied the CRT warnings a little bit.
//       8.14    14/12/09 - Animation refresh counting not taking effect. Help screenshots updated.
//       8.15    18/12/09 - Version update check added to About Box. Application version resource added. Internal help updated
//       8.16    27/12/09 - GPL v3. 
//       8.17    18/04/10 - Shutdown does not hesitate waiting for HTML publisher shutdown. CTRL-Insert copies debugger contents to clipboard.
//                          Registry key setting move from HKLM to HKCU
//       8.18    27/04/10 - (ver 8.17) UAC - should now be Vista compliant, Registration dialog screen added.
//       8.19    03/05/10 - Source cleaned up, eliminated timer/CPU starvation for animation updates updating all registers
//       8.20    10/01/11 - COM port name UNC prefix \\.\ expanding changed to work with com0com ports with alternative naming convention
//Previous Revisions by Conrad Braam
//
//Following Revisions by Doug Lyons
//   8.21.0.5    01/11/15 - Changed date format for revision notes from DD/MM/YY to MM/DD/YY for all revisions following.
//   8.21.0.9    02/01/15 - Fix crash when trying to open a script. This fix was thanks to James Slootweg from the SourceForge Support site.
//                          He created a support comment on 2013-10-03. MemoryEditorList fix to allow editing CHAR and HEX data.
//                          Allow "Clone" to reverse Byte Order for Floating Point display. Fix Function Code 22 Masked Write for RS-232
//                          Implement Function Code 22 Masked Write for Modbus TCP/IP. Made Changing "Clone" always redraw display.
//                          Changed CSV Read Max Columns from 17 to 1 and read as float. Only one value per line followed by comma.
//                          Otherwise we were forced to write a minimum of the number of Max Columns regardless of fewer values on the line.
//                          Changed CSV Read to write to every other memory instead of skipping 4 for each value for 32-bit Floating Support.
//                          Added code to update display after registers are read into the values after CSV Import.
//                          Included Example of CSV Read in CSVReader.cpp in comments. Added saving of Data View Format ("Fmt") to registry.
//                          This means that once selected when you open the program again, it will remembe your selection of Format.
//                          Added Windows Version Check to Minimum Form Width. Win7 was squeezing out Toolbar when width minimized.
//                          Made all Modbus addresses 6-digits and widened Address column to handle this. This fixes bad address ranges.
//                          Removed spaces for I/O Display to get a better display of the values. Now shows "Holding Regs (400000)".
//                          Made Floating Point default to normal Modicon A984-131 word/byte order and "Clone" reverses the words.
//                          Reversed "TX" and "RX" in Comms Debug Messages because these seem to have been reversed in some simulations.
//                          Changed the Address Column for Modbus Inputs (000001 - 065536) so that they show with leading zeroes.
//                          Changed the VIEWFORMAT_FLOAT from %f to %g for better readability of very large and very small numbers.
//                          Fixed SockDataDebugger Call from DDKSocket.cpp so that it forwards to CRS232Port::GenDataDebugger like RS232.
//                          Added Debug Comms output text to show Inputs, Outputs, Input Regs, or Holding Regs for RS-232 and Ethernet.
//                          Widened I/O Listbox and changed it to display examples to 6-digit addressing to make it match other changes.
//                          Added Hotkey <Ctrl-F> (for File) to toggle sending characters to the DEBUG.TXT output file. Added Hours to output.
//                          Added "Debug" check box on the main screen to turn on and off the DEBUG output and show its status.
//                          Added the ability to turn on and off the Time Stamps to the Debug Log File like the Debug Window.
//                          Fixed Crash when writing to coil bit 65536 and increased all Modbus limits to a maximum of 65536.
//                          Added Support for Function Code 7 to output first 8 coils (1-8) similar to most Modicon PLC's.
//                          Modified the MOD_WRITE_MULTIPLE_COILS function to update 8 times the number of bytes to the screen (BITS).
//                          Changed the registry read checking order to try HKCU first and then HKLM because writes to registry go there.
//                          Added Socket Timeout Setting to Ethernet Settings Dialog Box & Added Connected IP Address:Port to debugger out.
//                          Fix sending "New Connection from IP Address.." to DEBUG.TXT when LOG not checked.
//                          Added my name into the About and Easter Egg Display with reference to Revision 8.21.
//                          This revision is by Doug Lyons  without permission from the original author. He did not answer my emails.
//                          This revision is created in Visual Studio 2010 Professional for the first time. Previously it was in VS 2008.
//     8.21.1.0 07/15/15    Revisions to the IDD_TI_SIM_DIALOG for larger display and better proportions in Windows 7.
//                          Extended maximum default Modbus registers from 65535 to 65536 to allow all registers to be used.
//     8.21.1.1 07/15/15    Changed Station Display Scrolls to scroll better (first station is 0,50,100,...).
//     8.21.1.2 07/26/15    Revision to Allow Station #0 messages to show in Active Station Display. They did not show previously.
//                          Removed PortA.gif used for Background in Help html file for easier reading.
//                          Updated index.html used for help to include more information on Advanced Emulation using Separate Registers.
//                          Advanced Emulation changes to display Address in popup edit window instead of I/O using Separate Registers.
//                          Fix for Modbus Function Code 7 using Serial port to match Ethernet code.
//                          Fix to update screen registers when writes done remotely and using Advanced Emulation & Separate Registers.
//                          Added "Load Register Values at Startup" & "Units are all off at startup" to Ethernet Settings dialog.
//                          Corrected Station #0 Ethernet responses so none are returned (This is broadcast address).
//                          Added "Rx:" and "Tx:" labels to Receive and Transmit LED's at the top center of the form.
//                          Changed Web references for Update Testing, Downloads and About Box to SourceForge/projects/modrssim2.
//     8.21.1.3 11/02/15    Changed Web reference for Update Testing to www.douglyons.us because SourceForge was unpredictable.
//                          Added MessageBox on starting to warn user if another Modbus TCP/IP server may already be connected.
//     8.21.1.4 11/21/15    Revised CSV Import for more flexibility to set register type, number and value one-per-line.
//                          Improved CSV Automatic Importer to allow updates every minute. Previously this was every 15 minutes.
//                          On CSV Importer Dialog Box moved "Load File.." to show up prominently at the top left of the form.
//                          Fixed "Load Register Values.." & "Units are all..." to save settings on "OK" from Ethernet Settings box.
//                          Update to Help file to show Modbus Supported Commands and document CSV Importing Functionality.
//                          Suppressed MessageBox on Automatic CSV Import that showed Rows Imported which interrupted automation.
//                          Fix editing of 4-digit hex value (Total column) for 16 Coils/Inputs at once in correct bit order.
//     8.21.1.5 11/27/15    Provided a Status Box for CSV Import that will warn about long times for importing large files.
//                          Update Status Box instead of using a MessageBox to display lines successfully manually imported.
//                          Removed annoying automatic Window Minimize when Right Click Read-Only toggle when Read-Only is set.
//                          Corrected some lost charactesrs in the Help file for "Load" and "Automation" lines.
//     8.21.1.6 12/05/15    Re-enable Noise Testing for Modbus RS232 (Beta Only) but not for Modbus Ethernet yet.
//                          Added Support for Modbus Function Code 8 Subfunction 00 - Return Query Data (Loopback/Echo) for RS232.
//                          Update Help file to include new Modbus Function Code 8 Subfunction 00 in listing.
//     8.21.1.7 12/17/15    Added Support for Modbus Function Code 8 Subfunction 00 - Return Query Data (Loopback/Echo) for Ethernet.
//                          Added Support for Modbus Function Code 17 - Report Slave ID for RS232 and Ethernet as Modicon A984-131.
//                          Update Help file to include new "Modbus Function Code 17 - Report Slave ID" in Function Codes Supported.
//     8.21.1.8 04/23/16    Modified GetLastVisibleStation to allow station number of 255 (FF Hex) to show in Active Station display.
//                          Fix RS232 Writing Multiple Holding Regs showing as Writing Multiple Extended Registers in Comms display.
//                          Added Script Function clarifications about parenthesis requirements and examples in <F1> Help.
//                          Corrected Script Station Number range check to use ">=" instead of ">" when checking STATIONTICKBOXESMAX.
//                          Changed Script Function DisplayAbout to Sleep as delay test and reverted because this stopped comms.
//                          Modified Splash screen text for better readability and changed the WEB address to new forked site.
//                          Removed Version History from <F1> Help Display. This is available from the websites.
//     8.21.1.9 08/02/16    If enabled, add "Using Seperate Registers" to New Connection info line showing IP Address and Port.
//                          Also added MessageBox when program started to warn of "Using Seperate Registers for Each Station".
//                          Changed m_lampRX.SetupLamp/m_lampTX.SetupLamp to 600 because sometimes lamps were not showing activity.
//                          EditMemoryDlg edited to do some input verification for DECIMAL, LONG, HEX, WORD, CHAR & DWORD entries.
//                          Modified the Easter Egg display click to open the WEB address for the new forked site at SourceForge.
//                          Changes to Update Check because Internet Provider for www.douglyons.us increased header size requirements.
//                          Changed update check URL to SourceForge Project Wiki page, then douglyons.us, then SourceForge Readme.txt.
//                          Added "R-off" when using RS-232 RTS=off to Title to show RTS state. Previously this displayed as blank.
//     8.21.2.0 08/20/16    Added Error Messages to Comms Window for Load/Save All Regs. Ex: No MODDATA.DAT file or Read-Only File.
//                          Added MessageBoxes for Load All Registers and Save All Registers to display any error messages.
//                          Changed Loading and Saving registers to use only 640K instead of 768K of disk space. Rest was not used.
//                          Increased ABDF1 File Number maximum from 254 to 255 because 255 is available in Allen-Bradley SLC's.
//                          Allow blanking script file name so built-in animation in CMOD_simDlg::DoPlantSimulation can run.
//                          Main Dialog fonts changed from 8->10 & rearranged second selection row for better viewing. Hex->H & Dec->D.
//                          Reworked IDD_TI_SIM_DIALOG & IDB_TOOLBARID location for better layout and viewing after font change to 10.
//                          Column auto resize for registers added when resizing main window which evenly divides the 10 columns.
//                          Testing & changes to "HTML Output GUI" for types of DECIMAL, HEX, WORD16, WORD32, LONG32, FLOAT and CHAR's.
//                          In AnimationSettings.cpp removed "UpdateData(TRUE);" because it was giving double errors on invalid data.
//                          Updated Help file to include "HTML Output GUI" Example and some explanations.
//     8.21.2.1 09/22/16    Check for Socket Close or Reset from Client side and close our socket. Neither was previously handled.
//                          Change SO_LINGER which previously had 128 seconds of linger (SO_LINGER value) to 2 seconds.
//     8.21.2.2 11/06/16    Added option "Use Long Timeouts" changing RS-232 Timeouts from default of 100ms/500ms to 250ms/1000ms.
//                          Revised built-in <F1> Help to show the new "Use Long Timeouts" checkbox and give explanation of its use.
//                          Removed legacy code that checked for saved settings in HKLM. Now we only use HKCU for saving these.
//                          Removed some Registration Reminder Screen code to begin cleanup of code involving registration.
//                          Added RS-232 Serial debugging messages for Bad Parity, Overflows, Overruns, Framing Errors, etc.
//                          RS-232 Modem Status check with GetCommModemStatus now has error checking which quiets spam output.
//     8.21.2.3 01/02/17    Added <CTRL>+<INS> into to HTML help file and About box. It sends Comms Debug text to the Windows Clipboard.
//                          Better readability on narrow widths by compressing main form items and putting Clone above the Log checkbox.
//                          New checkbox for "Log Decode" is now shown below the "Show Times" checkbox in the Comms Debugging screen.
//                          This is an added option for Logging function decode explanations to the Disk Log File for troubleshooting.
//                          With this option selected most of what is shown in the Comms Debugging window goes to the "DEBUG.TXT" file.
//                          Because of the greatly increased disk activity, some data may be missed in the Log file under heavy loads.
//                          Conversion of SaveDebugInfo function to CRS232Port::WriteToFile class with built-in check for time display.
//                          Made default PDU Size 256 for serial and 2048 for Ethernet and both can be overridden once selection made.
//                          Changed Modbus Exception Codes from 03 to 02 to reflect actual tests on Modbus PLC for out-of-range reads.
//                          Tests added for Modbus Exception Code 03 around PDU Size and Byte Count mismatch on write multiples.
//                          Next to last button tooltip changed from "PLC: Emulation Settings" to "Adv. PLC Emul. Settings"
//                          <F1> Help file "Protocols" changed to "Protocols - Advanced Emulation Settings" for better description.
//     8.21.2.4 01/08/17    Fixed layout and auto-sizing to work better with different screen resolutions. Problem reported by Godra.
//                          Changed "Use Seperate Set of Registers for Each Station" to start at Station #1 not Stations #0.
//     8.21.2.5 01/19/18    Changed Toolbar location code so that left side tracks Protocol Selection Box right side in OnSize.
//                          Problem reported by Rob Gustavsson and fixed with his very helpful feedback and testing.
//     8.21.2.6 12/03/18    Experimental Support for Ethernet Function Code 23 Read/Write Holding Registers.
//                          Changed Station Number Scroll Bar to Up and Down Buttons for better ReactOS support.
//                          Fix for Writing Multiple Coils overwriting one more than desired.
//     8.21.2.7 03/06/18    Fix handling of PDU size so that it is treated at 16-bit words instead of 8-bit bytes.
//                          Changed malformed C++ Variable names for header guards to be compliant with standard.
//
//
//
//
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"  // ha ha

char lpsMyAppVersion[10] = "n.n.n.n";
