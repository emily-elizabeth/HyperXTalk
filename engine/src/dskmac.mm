/* Copyright (C) 2003-2015 LiveCode Ltd.
 
 This file is part of LiveCode.
 
 LiveCode is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License v3 as published by the Free
 Software Foundation.
 
 LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.
 
 You should have received a copy of the GNU General Public License
 along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "osxprefix.h"
#include "osxprefix-legacy.h"

// ARM/modern macOS: CoreServices provides AE types, FSRef, OSErr etc.
// Carbon/Carbon.h is NOT included - it is unavailable on arm64.
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#import <AppKit/AppKit.h>    // NSRunningApplication, NSWorkspace, NSAppleScript
#include <sys/mount.h>
#include <sys/stat.h>

// OSAID is a Carbon/OSA type - define stub for arm64
#if defined(__arm64__) || defined(__aarch64__)
typedef long OSAID;
// AESend is in Carbon/HIToolbox - unavailable on arm64
// AppleEvent sending via AESend is stubbed out
static inline OSStatus AESend(const AppleEvent *p, AppleEvent *r, AESendMode m, AESendPriority p2, SInt32 t, void *i, void *f)
{ (void)p; (void)r; (void)m; (void)p2; (void)t; (void)i; (void)f; return -1708; } // errAEEventNotHandled
#endif

#include "parsedef.h"
#include "filedefs.h"
#include "globdefs.h"
#include "objdefs.h"


#include "exec.h"
#include "globals.h"
#include "system.h"
#include "osspec.h"
#include "mcerror.h"
#include "util.h"
#include "mcio.h"
#include "stack.h"
#include "handler.h"
#include "dispatch.h"
#include "card.h"
#include "group.h"
#include "button.h"
#include "param.h"
#include "mode.h"
#include "securemode.h"
#include "text.h"
#include "socket.h"

#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/xattr.h>  // ARM: for XATTR_FINDERINFO_NAME / setxattr / getxattr

// SN-2014-12-09: [[ Bug 14001 ]] Update the module loading for Mac server
#include <dlfcn.h>

#include "foundation.h"

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#include <mach-o/dyld.h>


#define ENTRIES_CHUNK 1024

#define SERIAL_PORT_BUFFER_SIZE  16384 //set new buffer size for serial input port
#include <termios.h>
#define B16600 16600

#include <pwd.h>
#include <inttypes.h>

#define USE_FSCATALOGINFO

///////////////////////////////////////////////////////////////////////////////

#define keyReplyErr 'errn'
#define keyMCScript 'mcsc'  //reply from apple event

#define AETIMEOUT                60.0

uint1 *MClowercasingtable = NULL;
uint1 *MCuppercasingtable = NULL;

static bool GetProcessIsTranslated()
{
	static int s_state = -1;
	if (s_state == -1)
	{
		int ret = 0;
		size_t size = sizeof(ret);
		if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1)
		{
			if (errno == ENOENT)
			{
				s_state = 0;
			}
		}
		else
		{
			s_state = ret;
		}
	}

	return s_state == 1;
}


inline FourCharCode FourCharCodeFromString(const char *p_string)
{
	return MCSwapInt32HostToNetwork(*(FourCharCode *)p_string);
}

bool FourCharCodeFromString(MCStringRef p_string, uindex_t p_start, FourCharCode& r_four_char_code)
{
    MCAutoStringRefAsCString t_temp;
    uint32_t t_four_char_code;
    if (!t_temp.Lock(p_string))
        return false;
    
    memcpy(&t_four_char_code, *t_temp + p_start, 4);
    r_four_char_code = MCSwapInt32HostToNetwork(t_four_char_code);
    
    return true;
}

inline char *FourCharCodeToString(FourCharCode p_code)
{
	char *t_result;
	t_result = new (nothrow) char[5];
	*(FourCharCode *)t_result = MCSwapInt32NetworkToHost(p_code);
	t_result[4] = '\0';
	return t_result;
}

bool FourCharCodeToStringRef(FourCharCode p_code, MCStringRef& r_string)
{
    return MCStringCreateWithCStringAndRelease(FourCharCodeToString(p_code), r_string);
}

struct triplets
{
	AEEventClass		theEventClass;
	AEEventID		theEventID;
	AEEventHandlerProcPtr	theHandler;
	AEEventHandlerUPP	theUPP;
};

typedef struct triplets triplets;

typedef struct
{
    MCStringRef compname;
    OSType compsubtype;
    void *compinstance; // ARM: unused stub (was ComponentInstance)
}
OSAcomponent;

static OSAcomponent *osacomponents = NULL;
static uint2 osancomponents = 0;

#define MINIMUM_FAKE_PID (1 << 29)

static int4 curpid = MINIMUM_FAKE_PID;
static AEKeyword replykeyword;   // Use in DoSpecial & other routines
static MCStringRef AEReplyMessage;
static MCStringRef AEAnswerData;
static MCStringRef AEAnswerErr;
static const AppleEvent *aePtr; //current apple event for mcs_request_ae()

/***************************************************************************
 * utility functions used by this module only		                   *
 ***************************************************************************/

static OSStatus getDescFromAddress(MCStringRef address, AEDesc *retDesc);
static OSStatus getDesc(short locKind, MCStringRef zone, MCStringRef machine, MCStringRef app, AEDesc *retDesc);
static OSStatus getAEAttributes(const AppleEvent *ae, AEKeyword key, MCStringRef &r_result);
static OSStatus getAEParams(const AppleEvent *ae, AEKeyword key, MCStringRef &r_result);
static OSStatus getAddressFromDesc(AEAddressDesc targetDesc, char *address);

static void getosacomponents();
static OSStatus osacompile(MCStringRef s, void *compinstance, OSAID &id);
static OSStatus osaexecute(MCStringRef& r_string, void *compinstance, OSAID id);

// SN-2014-10-07: [[ Bug 13587 ]] Update to return an MCList
static bool fetch_ae_as_fsref_list(MCListRef &r_list);

static OSStatus MCS_mac_pathtoref(MCStringRef p_path, FSRef& r_ref);
static bool MCS_mac_fsref_to_path(FSRef& p_ref, MCStringRef& r_path);

/***************************************************************************/

///////////////////////////////////////////////////////////////////////////////

// SN-2014-08-07: [[ MERG-6.7 ]] Porting updates from osxspec.cpp
OSErr MCAppleEventHandlerDoSpecial(const AppleEvent *ae, AppleEvent *reply, long refCon)
{
	// MW-2013-08-07: [[ Bug 10865 ]] If AppleScript is disabled (secureMode) then
	//   don't handle the event.
	if (!MCSecureModeCanAccessAppleScript())
		return errAEEventNotHandled;
    
	OSErr err = errAEEventNotHandled;  //class, id, sender
	DescType rType;
	Size rSize;
	AEEventClass aeclass;
	AEGetAttributePtr(ae, keyEventClassAttr, typeType, &rType, &aeclass, sizeof(AEEventClass), &rSize);
    
	AEEventID aeid;
	AEGetAttributePtr(ae, keyEventIDAttr, typeType, &rType, &aeid, sizeof(AEEventID), &rSize);
    
	if (aeclass == kTextServiceClass)
	{
		err = errAEEventNotHandled;
		return err;
	}
	//trap for the AEAnswer event, let DoAEAnswer() to handle this event
	if (aeclass == kCoreEventClass)
	{
		if (aeid == kAEAnswer)
			return errAEEventNotHandled;
	}
	AEAddressDesc senderDesc;
	//
	char *p3val = new (nothrow) char[128];
	//char *p3val = new (nothrow) char[kNBPEntityBufferSize + 1]; //sender's address 105 + 1
	if (AEGetAttributeDesc(ae, keyOriginalAddressAttr,
	                       typeWildCard, &senderDesc) == noErr)
	{
		getAddressFromDesc(senderDesc, p3val);
		AEDisposeDesc(&senderDesc);
	}
	else
		p3val[0] = '\0';
    
	aePtr = ae; //saving the current AE pointer for use in mcs_request_ae()
	MCParameter p1, p2, p3;
	MCAutoStringRef s1;
	MCAutoStringRef s2;
    MCAutoStringRef s3;
    
    /* UNCHECKED */ FourCharCodeToStringRef(aeclass, &s1);
    /* UNCHECKED */ MCStringCreateWithCString(p3val, &s3);
	
	p1.setvalueref_argument(*s1);
	p1.setnext(&p2);
	
    /* UNCHECKED */ FourCharCodeToStringRef(aeid, &s2);
	
	p2.setvalueref_argument(*s2);
	p2.setnext(&p3);
	p3.setvalueref_argument(*s3);
	/*for "appleEvent class, id, sender" message to inform script that
     there is an AE arrived */
	Exec_stat stat = MCdefaultstackptr->getcard()->message(MCM_apple_event, &p1);
	if (stat != ES_PASS && stat != ES_NOT_HANDLED)
	{ //if AE is handled by MC
		if (stat == ES_ERROR)
		{ //error in handling AE in MC
			err = errAECorruptData;
			if (reply->dataHandle != NULL)
			{
				int16_t e = err;
				AEPutParamPtr(reply, keyReplyErr, typeSInt16, (Ptr)&e, sizeof(short));
			}
		}
		else
		{ //ES_NORMAL
			if (AEReplyMessage == NULL) //no reply, will return no error code
				err = noErr;
			else
			{
				if (reply->descriptorType != typeNull && reply->dataHandle != NULL)
				{
					MCAutoStringRefAsUTF8String t_reply;
                    /* UNCHECKED */ t_reply.Lock(AEReplyMessage);
                    err = AEPutParamPtr(reply, replykeyword, typeUTF8Text, *t_reply, t_reply.Size());
					if (err != noErr)
					{
						int16_t e = err;
						AEPutParamPtr(reply, keyReplyErr, typeSInt16, (Ptr)&e, sizeof(short));
					}
				}
			}
            
            MCValueRelease(AEReplyMessage);
            AEReplyMessage = NULL;
		}
	}
	else
		if (aeclass == kAEMiscStandards
            && (aeid == kAEDoScript || aeid == 'eval'))
		{
			if ((err = AEGetParamPtr(aePtr, keyDirectObject, typeUTF8Text, &rType, NULL, 0, &rSize)) == noErr)
			{
				byte_t *sptr = new (nothrow) byte_t[rSize + 1];
				AEGetParamPtr(aePtr, keyDirectObject, typeUTF8Text, &rType, sptr, rSize, &rSize);
                MCExecContext ctxt(MCdefaultstackptr -> getcard(), nil, nil);
                MCAutoStringRef t_sptr;
                /* UNCHECKED */ MCStringCreateWithBytesAndRelease(sptr, rSize, kMCStringEncodingUTF8, false, &t_sptr);
				if (aeid == kAEDoScript)
				{
                    MCdefaultstackptr->getcard()->domess(*t_sptr);
                    MCAutoValueRef t_value;
                    MCAutoStringRef t_string;
                    MCAutoStringRefAsUTF8String t_utf8_string;
                    /* UNCHECKED */ MCresult->eval(ctxt, &t_value);
                    /* UNCHECKED */ ctxt . ConvertToString(*t_value, &t_string);
                    /* UNCHECKED */ t_utf8_string.Lock(*t_string);
                    AEPutParamPtr(reply, '----', typeUTF8Text, *t_utf8_string, t_utf8_string.Size());
				}
				else
                {
					MCAutoValueRef t_val;
					MCAutoStringRef t_string;
                    MCAutoStringRefAsUTF8String t_utf8;

					MCdefaultstackptr->getcard()->eval(ctxt, *t_sptr, &t_val);
					/* UNCHECKED */ ctxt.ConvertToString(*t_val, &t_string);
                    /* UNCHECKED */ t_utf8.Lock(*t_string);

                    AEPutParamPtr(reply, '----', typeUTF8Text, *t_utf8, t_utf8.Size());
				}
			}
		}
		else
			err = errAEEventNotHandled;
	// do nothing if the AE is not handled,
	// let the standard AE dispacher to dispatch this AE
	delete[] p3val;
	return err;
}

OSErr MCAppleEventHandlerDoOpenDoc(const AppleEvent *theAppleEvent, AppleEvent *reply, long refCon)
{ //Apple Event for opening documnets, in our use is to open stacks when user
	//double clicked on a MC stack icon

    // MW-2013-08-07: [[ Bug 10865 ]] If AppleScript is disabled (secureMode) then
	//   don't handle the event.
	if (!MCSecureModeCanAccessAppleScript())
		return errAEEventNotHandled;
    
	AEDescList docList; //get a list of alias records for the documents
	errno = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList);
	if (errno != noErr)
		return errno;
	long count;
	//get the number of docs descriptors in the list
	AECountItems(&docList, &count);
	if (count < 1)     //if there is no doc to be opened
		return errno;
	AEKeyword rKeyword; //returned keyword
	DescType rType;    //returned type
	
	FSRef t_doc_fsref;
	
	Size rSize;        //returned size, atual size of the docName
	long item;
	// get a FSSpec record, starts from count==1
	for (item = 1; item <= count; item++)
	{
		errno = AEGetNthPtr(&docList, item, typeFSRef, &rKeyword, &rType, &t_doc_fsref, sizeof(FSRef), &rSize);
		if (errno != noErr)
			return errno;
		// extract FSSpec record's info & form a file name for MC to use
        MCAutoStringRef t_full_path_name;
		MCS_mac_fsref_to_path(t_doc_fsref, &t_full_path_name);
        
		if (MCModeShouldQueueOpeningStacks())
		{
			MCU_realloc((char **)&MCstacknames, MCnstacks, MCnstacks + 1, sizeof(MCStringRef));
			MCstacknames[MCnstacks++] = MCValueRetain(*t_full_path_name);
		}
		else
		{
			MCStack *stkptr;  //stack pointer
			if (MCdispatcher->loadfile(*t_full_path_name, stkptr) == IO_NORMAL)
				stkptr->open();
		}
	}
	AEDisposeDesc(&docList);
	return noErr;
}

OSErr MCAppleEventHandlerDoAEAnswer(const AppleEvent *ae, AppleEvent *reply, long refCon)
{
	// MW-2013-08-07: [[ Bug 10865 ]] If AppleScript is disabled (secureMode) then
	//   don't handle the event.
	if (!MCSecureModeCanAccessAppleScript())
		return errAEEventNotHandled;
    
    //process the repy(answer) returned from a server app. When MCS_send() with
	// a reply, the reply is handled in this routine.
	// This is different from MCS_reply()
	//check if there is an error code
	DescType rType; //returned type
	Size rSize;
    
	/*If the handler returns a result code other than noErr, and if the
     client is waiting for a reply, it is returned in the keyErrorNumber
     parameter of the reply Apple event. */
	if (AEGetParamPtr(ae, keyErrorString, typeUTF8Text, &rType, NULL, 0, &rSize) == noErr)
	{
		byte_t* t_utf8 = new (nothrow) byte_t[rSize + 1];
		AEGetParamPtr(ae, keyErrorString, typeUTF8Text, &rType, t_utf8, rSize, &rSize);
		/* UNCHECKED */ MCStringCreateWithBytesAndRelease(t_utf8, rSize, kMCStringEncodingUTF8, false, AEAnswerErr);
	}
	else
	{
		int16_t e;
		if (AEGetParamPtr(ae, keyErrorNumber, typeSInt16, &rType, (Ptr)&e, sizeof(short), &rSize) == noErr
            && e != noErr)
		{
			/* UNCHECKED */ MCStringFormat(AEAnswerErr, "Got error %d when sending Apple event", e);
		}
		else
		{
			if (AEAnswerData != NULL)
            {
                MCValueRelease(AEAnswerData);
                AEAnswerData = NULL;
            }
			if ((errno = AEGetParamPtr(ae, keyDirectObject, typeUTF8Text, &rType, NULL, 0, &rSize)) != noErr)
			{
				if (errno == errAEDescNotFound)
				{
					AEAnswerData = MCValueRetain(kMCEmptyString);
					return noErr;
				}
                /* UNCHECKED */ MCStringFormat(AEAnswerErr, "Got error %d when receiving Apple event", errno);
                return errno;
			}
			byte_t *t_utf8 = new (nothrow) byte_t[rSize + 1];
			AEGetParamPtr(ae, keyDirectObject, typeUTF8Text, &rType, t_utf8, rSize, &rSize);
			/* UNCHECKED */ MCStringCreateWithBytesAndRelease(t_utf8, rSize, kMCStringEncodingUTF8, false, AEAnswerData);
		}
	}
	return noErr;
}

/// END HERE

///////////////////////////////////////////////////////////////////////////////

static void MCS_launch_set_result_from_lsstatus(void)
{
	int t_error;
	t_error = 0;
    
	switch(errno)
	{
		case kLSUnknownErr:
		case kLSNotAnApplicationErr:
		case kLSLaunchInProgressErr:
		case kLSServerCommunicationErr:
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1040
		case kLSAppInTrashErr:
		case kLSIncompatibleSystemVersionErr:
		case kLSNoLaunchPermissionErr:
		case kLSNoExecutableErr:
		case kLSNoClassicEnvironmentErr:
		case kLSMultipleSessionsNotSupportedErr:
#endif
			t_error = 2;
            break;
            
		case kLSDataUnavailableErr:
		case kLSApplicationNotFoundErr:
		case kLSDataErr:
			t_error = 3;
            break;
	}
	
	switch(t_error)
	{
        case 0:
            MCresult -> clear();
            break;
            
        case 1:
            MCresult -> sets("can't open file");
            break;
            
        case 2:
            MCresult -> sets("request failed");
            break;
            
        case 3:
            MCresult -> sets("no association");
            break;
	}
    
}

///////////////////////////////////////////////////////////////////////////////

IO_stat MCS_mac_shellread(int fd, char *&buffer, uint4 &buffersize, uint4 &size)
{
	MCshellfd = fd;
	size = 0;
	while (True)
	{
		int readsize = 0;
		ioctl(fd, FIONREAD, (char *)&readsize);
		readsize += READ_PIPE_SIZE;
		if (size + readsize > buffersize)
		{
			MCU_realloc((char **)&buffer, buffersize,
                        buffersize + readsize + 1, sizeof(char));
			buffersize += readsize;
		}
		errno = 0;
		int4 amount = read(fd, &buffer[size], readsize);
		if (amount <= 0)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
				break;
			if (!MCS_poll(SHELL_INTERVAL, 0))
				if (!MCnoui && MCscreen->wait(SHELL_INTERVAL, False, True))
				{
					MCshellfd = -1;
					return IO_ERROR;
				}
		}
		else
			size += amount;
	}
	MCshellfd = -1;
	return IO_NORMAL;
}

///////////////////////////////////////////////////////////////////////////////

// Special Folders

// MW-2012-10-10: [[ Bug 10453 ]] Added 'mactag' field which is the tag to use in FSFindFolder.
//   This allows macfolder to be 0, which means don't alias the tag to the specified disk.
typedef struct
{
	MCNameRef *token;
	unsigned long macfolder;
	OSType domain;
	unsigned long mactag;
}
sysfolders;

// MW-2008-01-18: [[ Bug 5799 ]] It seems that we are requesting things in the
//   wrong domain - particularly for 'temp'. See:
// http://lists.apple.com/archives/carbon-development/2003/Oct/msg00318.html

static sysfolders sysfolderlist[] = {
    {&MCN_desktop, 'desk', OSType(kOnAppropriateDisk), 'desk'},
    {&MCN_fonts,'font', OSType(kOnAppropriateDisk), 'font'},
    {&MCN_preferences,'pref', OSType(kUserDomain), 'pref'},
    {&MCN_temporary,'temp', OSType(kUserDomain), 'temp'},
    {&MCN_system, 'macs', OSType(kOnAppropriateDisk), 'macs'},
    // TS-2007-08-20: Added to allow a common notion of "home" between all platforms
    {&MCN_home, 'cusr', OSType(kUserDomain), 'cusr'},
    // MW-2007-09-11: Added for uniformity across platforms
    {&MCN_documents, 'docs', OSType(kUserDomain), 'docs'},
    // MW-2007-10-08: [[ Bug 10277 ] Add support for the 'application support' at user level.
    // FG-2014-09-26: [[ Bug 13523 ]] This entry must not match a request for "asup"
    {&MCN_support, 0, OSType(kUserDomain), 'asup'},
};

static bool MCS_mac_specialfolder_to_mac_folder(MCStringRef p_type, uint32_t& r_folder, OSType& r_domain)
{
	for (uindex_t i = 0; i < ELEMENTS(sysfolderlist); i++)
	{
		if (MCStringIsEqualTo(p_type, MCNameGetString(*(sysfolderlist[i].token)), kMCStringOptionCompareCaseless))
		{
			r_folder = sysfolderlist[i].mactag;
			r_domain = sysfolderlist[i].domain;
            return true;
		}
	}
    return false;
}

/********************************************************************/
/*                        Serial Handling                           */
/********************************************************************/

// Utilities

static void parseSerialControlStr(MCStringRef setting, struct termios *theTermios)
{
    int baud = 0;
    MCAutoStringRef t_property, t_value;
    if (MCStringDivideAtChar(setting, '=', kMCCompareExact, &t_property, &t_value))
    {
        if (MCStringIsEqualToCString(*t_property, "baud", kMCCompareCaseless))
        {
            integer_t baudrate;
            /* UNCHECKED */ MCStringToInteger(*t_value, baudrate);
            baud = baudrate;
			cfsetispeed(theTermios, baud);
			cfsetospeed(theTermios, baud);
            
        }
        
        else if (MCStringIsEqualToCString(*t_property, "parity", kMCCompareCaseless))
        {
            char first;
            first = MCStringGetNativeCharAtIndex(*t_value, 0);
            if (first == 'N' || first == 'n')
				theTermios->c_cflag &= ~(PARENB | PARODD);
			else if (first == 'O' || first == 'o')
				theTermios->c_cflag |= PARENB | PARODD;
			else if (first == 'E' || first == 'e')
				theTermios->c_cflag |= PARENB;
        }
        
        else if (MCStringIsEqualToCString(*t_property, "data", kMCCompareCaseless))
        {
            integer_t data;
            /* UNCHECKED */ MCStringToInteger(*t_value, data);
			switch (data)
			{
                case 5:
                    theTermios->c_cflag |= CS5;
                    break;
                case 6:
                    theTermios->c_cflag |= CS6;
                    break;
                case 7:
                    theTermios->c_cflag |= CS7;
                    break;
                case 8:
                    theTermios->c_cflag |= CS8;
                    break;
			}
        }
        
        else if (MCStringIsEqualToCString(*t_property, "stop", kMCCompareCaseless))
        {
            double stopbit;
            /* UNCHECKED */ MCStringToDouble(*t_value, stopbit);
			if (stopbit == 1.0)
				theTermios->c_cflag &= ~CSTOPB;
			else if (stopbit == 1.5)
				theTermios->c_cflag &= ~CSTOPB;
			else if (stopbit == 2.0)
				theTermios->c_cflag |= CSTOPB;
        }
    }
}

static void configureSerialPort(int sRefNum)
{/****************************************************************************
  *parse MCserialcontrolstring and set the serial output port to the settings*
  *defined by MCserialcontrolstring accordingly                              *
  ****************************************************************************/
	//initialize to the default setting
	struct termios	theTermios;
	if (tcgetattr(sRefNum, &theTermios) < 0)
	{
		// TODO: handle error appropriately
	}
	cfsetispeed(&theTermios,  B9600);
	theTermios.c_cflag = CS8;
 
    // Split the string on the spaces
    MCAutoArrayRef t_settings;
    /* UNCHECKED */ MCStringSplit(MCserialcontrolsettings, MCSTR(" "), nil, kMCCompareExact, &t_settings);
    uindex_t nsettings = MCArrayGetCount(*t_settings);
    
    for (int i = 0 ; i < nsettings ; i++)
    {
        // Note: 't_settings' is an array of strings
        MCValueRef t_settingval = nil;
        /* UNCHECKED */ MCArrayFetchValueAtIndex(*t_settings, i + 1, t_settingval);
        MCStringRef t_setting = (MCStringRef)t_settingval;
        parseSerialControlStr(t_setting, &theTermios);
    }
    //configure the serial output device
	if (tcsetattr(sRefNum, TCSANOW, &theTermios) < 0)
	{
		// TODO: handle error appropriately
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//  REFACTORED FROM TEXT.CPP
//

// ARM: UnicodeInfoRecord / fetch_unicode_info removed.
// Carbon TextEncoding/TextToUnicodeInfo APIs are unavailable on arm64.
// Charset conversion uses CFString (see MCS_unicodetomultibyte/MCS_multibytetounicode).

///////////////////////////////////////////////////////////////////////////////


static void MCS_mac_setfiletype(MCStringRef p_new_path)
{
    // ARM replacement: FSCatalogInfo/FSSetCatalogInfo are unavailable on arm64.
    // Set HFS type/creator via com.apple.FinderInfo extended attribute instead.
    MCAutoStringRefAsUTF8String t_utf8_path;
    if (!t_utf8_path.Lock(p_new_path))
        return;

    // Read existing FinderInfo xattr (32 bytes) or zero-initialise
    uint8_t t_finder_info[32];
    memset(t_finder_info, 0, sizeof(t_finder_info));
    getxattr(*t_utf8_path, XATTR_FINDERINFO_NAME, t_finder_info, sizeof(t_finder_info), 0, 0);

    // Bytes 0-3 = file type, 4-7 = file creator (big-endian FourCharCodes)
    FourCharCode t_file_type, t_file_creator;
    FourCharCodeFromString(MCfiletype, 4, t_file_type);
    FourCharCodeFromString(MCfiletype, 0, t_file_creator);
    t_file_type     = OSSwapHostToBigInt32(t_file_type);
    t_file_creator  = OSSwapHostToBigInt32(t_file_creator);
    memcpy(t_finder_info,     &t_file_type,    4);
    memcpy(t_finder_info + 4, &t_file_creator, 4);

    setxattr(*t_utf8_path, XATTR_FINDERINFO_NAME, t_finder_info, sizeof(t_finder_info), 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

extern "C"
{
#include	<IOKit/IOKitLib.h>
#include	<IOKit/serial/IOSerialKeys.h>
#include	<IOKit/IOBSD.h>
}

static kern_return_t FindSerialPortDevices(io_iterator_t *serialIterator, mach_port_t *masterPort)
{
    kern_return_t	kernResult;
    CFMutableDictionaryRef classesToMatch;
    if ((kernResult = IOMasterPort(0, masterPort)) != KERN_SUCCESS)
        return kernResult;
    if ((classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue)) == NULL)
        return kernResult;
    CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey),
                         CFSTR(kIOSerialBSDRS232Type));
    //kIOSerialBSDRS232Type filters KeySpan USB modems use
    //kIOSerialBSDModemType to get 'real' serial modems for OSX
    //computers with real serial ports - if there are any!
    kernResult = IOServiceGetMatchingServices(*masterPort, classesToMatch,
                                              serialIterator);
    return kernResult;
}

static void getIOKitProp(io_object_t sObj, const char *propName,
                         char *dest, uint2 destlen)
{
	CFTypeRef nameCFstring;
	dest[0] = 0;
	nameCFstring = IORegistryEntryCreateCFProperty(sObj,
                                                   CFStringCreateWithCString(kCFAllocatorDefault, propName,
                                                                             kCFStringEncodingASCII),
                                                   kCFAllocatorDefault, 0);
	if (nameCFstring)
	{
		CFStringGetCString((CFStringRef)nameCFstring, (char *)dest, (long)destlen,
                           (unsigned long)kCFStringEncodingASCII);
		CFRelease(nameCFstring);
	}
}

///////////////////////////////////////////////////////////////////////////////

//for setting serial port use
typedef struct
{
	short baudrate;
	short parity;
	short stop;
	short data;
}
SerialControl;

//struct
SerialControl portconfig; //serial port configuration structure

// ARM: SwapQDTextFlags (QuickDraw) removed - QD is unavailable on arm64.

static void configureSerialPort(int sRefNum);
static bool getResourceInfo(MCListRef p_list, ResType p_type);
static void parseSerialControlStr(MCStringRef set, struct termios *theTermios);


// ARM: Carbon TextEncoding convertor statics removed.
// Charset conversion implemented via CFString in MCS_unicodetomultibyte/MCS_multibytetounicode.
// init_utf8_converters is a no-op stub below.
static void init_utf8_converters(void) { /* no-op on ARM */ }
///////////////////////////////////////////////////////////////////////////////

/********************************************************************/
/*                        File Handling                             */
/********************************************************************/

// File opening and closing

// This function checks that a file really does exist at the given location.
// The path is expected to have been resolved but in native encoding.
static bool MCS_file_exists_at_path(MCStringRef p_path)
{
    MCAutoStringRefAsUTF8String t_new_path;
	/* UNCHECKED */ t_new_path . Lock(p_path);
    
    bool t_found;
    
	struct stat buf;
	t_found = (stat(*t_new_path, (struct stat *)&buf) == 0);
	if (t_found)
        if (S_ISDIR(buf . st_mode))
            t_found = false;
    
    return t_found;
}

// MW-2014-09-17: [[ Bug 13455 ]] Attempt to redirect path. If p_is_file is false,
//   the path is taken to be a directory and is always redirected if is within
//   Contents/MacOS. If p_is_file is true, then the file is only redirected if
//   the original doesn't exist, and the redirection does.
bool MCS_apply_redirect(MCStringRef p_path, bool p_is_file, MCStringRef& r_redirected)
{
    // If the original file exists, do nothing.
    if (p_is_file && MCS_file_exists_at_path(p_path))
        return false;
    
    uindex_t t_engine_path_length;
    if (!MCStringLastIndexOfChar(MCcmd, '/', UINDEX_MAX, kMCStringOptionCompareExact, t_engine_path_length))
        t_engine_path_length = MCStringGetLength(MCcmd);
    
    // If the length of the path is less than the folder prefix of the exe, it
    // cannot be inside <bundle>/Contents/MacOS/
    if (MCStringGetLength(p_path) < t_engine_path_length)
        return false;
    
    // If the prefix of path is not the same as MCcmd up to the folder, it
    // cannot be inside <bundle>/Contents/MacOS/
    if (!MCStringSubstringIsEqualToSubstring(p_path, MCRangeMake(0, t_engine_path_length), MCcmd, MCRangeMake(0, t_engine_path_length), kMCCompareCaseless))
        return false;
    
    // If the final component is not MacOS then it is not inside the relevant
    // folder.
    if (MCStringGetLength(p_path) != t_engine_path_length &&
        MCStringGetCodepointAtIndex(p_path, t_engine_path_length) != '/')
        return false;
    
    // Construct the new path from the path after MacOS/ inside Resources/_macos.
    MCAutoStringRef t_new_path;
	MCRange t_cmd_range = MCRangeMake(0, t_engine_path_length - 6);
	uindex_t t_path_end = MCStringGetLength(p_path);
	bool t_success = true;
	
	if (MCStringGetCodepointAtIndex(p_path, t_path_end) == '/')
		t_path_end--;
	
	if (t_engine_path_length == t_path_end)
	{
		t_success = MCStringFormat(&t_new_path, "%*@/Resources/_MacOS", &t_cmd_range, MCcmd);
	}
	else
	{
	    MCRange t_path_range = MCRangeMakeMinMax(t_engine_path_length + 1, t_path_end);
		// AL-2014-09-19: Range argument to MCStringFormat is a pointer to an MCRange.
		t_success = MCStringFormat(&t_new_path, "%*@/Resources/_MacOS/%*@", &t_cmd_range, MCcmd, &t_path_range, p_path);
	}
	
    if (!t_success || (p_is_file && !MCS_file_exists_at_path(*t_new_path)))
        return false;

    r_redirected = MCValueRetain(*t_new_path);
    return true;
}

/* LEGACY */
extern char *path2utf(char *);

static void handle_signal(int sig)
{
	MCHandler handler(HT_MESSAGE);
	switch (sig)
	{
        case SIGUSR1:
            MCsiguser1++;
            break;
        case SIGUSR2:
            MCsiguser2++;
            break;
        case SIGTERM:
            if (MCdefaultstackptr)
            {
                switch (MCdefaultstackptr->getcard()->message(MCM_shut_down_request))
                {
                    case ES_NORMAL:
                        return;
                    case ES_PASS:
                    case ES_NOT_HANDLED:
                        MCdefaultstackptr->getcard()->message(MCM_shut_down);
                        MCquit = True; //set MC quit flag, to invoke quitting
                        return;
                    default:
                        break;
                }
            }
            
            MCS_killall();
            exit(-1);
            
            // MW-2009-01-29: [[ Bug 6410 ]] If one of these signals occurs, we need
            //   to return, so that the OS can CrashReport away.
        case SIGILL:
        case SIGBUS:
        case SIGSEGV:
        {
            MCAutoStringRefAsUTF8String t_utf8_MCcmd;
            /* UNCHECKED */ t_utf8_MCcmd.Lock(MCcmd);
            fprintf(stderr, "%s exiting on signal %d\n", *t_utf8_MCcmd, sig);
            // Only call MCS_killall() if we are not already in the quit
            // sequence.  During X_close(), MCprocesses is freed before
            // delete MCdispatcher; if a crash occurs inside the destructor
            // the re-entrant MCS_killall() would access the freed array and
            // cause a secondary crash / MCAssert failure.
            if (!MCquit)
                MCS_killall();
            // Use _exit so the OS can generate a crash report. Returning
            // from a SIGSEGV/SIGILL/SIGBUS handler is undefined behaviour.
            _exit(-1);
        }
            
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGIOT:
            if (MCnoui)
                exit(1);
            MCabortscript = True;
            break;
        case SIGFPE:
            errno = EDOM;
            break;
        case SIGCHLD:
            MCS_checkprocesses();
            break;
        case SIGALRM:
            MCalarm = True;
            break;
        case SIGPIPE:
        default:
            break;
	}
	return;
}

///////////////////////////////////////////////////////////////////////////////

// The external list of environment vars (terminated by NULL).
extern char **environ;

// Check to see if two environment var definitions are for the same variable.
static bool same_var(const char *p_left, const char *p_right)
{
    const char *t_left_sep, *t_right_sep;
    t_left_sep = strchr(p_left, '=');
    if (t_left_sep == NULL)
        t_left_sep = p_left + strlen(p_left);
    t_right_sep = strchr(p_right, '=');
    if (t_right_sep == NULL)
        t_right_sep = p_right + strlen(p_right);
    
    if (t_left_sep - p_left != t_right_sep - p_right)
        return false;
    
    if (strncmp(p_left, p_right, t_left_sep - p_left) != 0)
        return false;
    
    return true;
}

// [[ Bug 13622 ]] On Yosemite, there can be duplicate environment variable
//    entries in the environ global list of vars. This is what is passed through
//    to child processes and it seems default behavior is for the second value
//    in the list to be taken - however, the most recently set value by this process
//    will be first in the list (it seems). Therefore we just remove any duplicates
//    before passing on to execle.
static char **fix_environ(void)
{
    char **t_new_environ;
    if (MCmajorosversion > MCOSVersionMake(10,9,0))
    {
        // Build a new environ, making sure that each var only takes the
        // first definition in the list. We don't have to care about memory
        // in particular, as this process is being wholesale replaced by an
        // exec.
        int t_new_length;
        t_new_environ = NULL;
        t_new_length = 0;
        for(int i = 0; environ[i] != NULL; i++)
        {
            bool t_found;
            t_found = false;
            for(int j = 0; j < t_new_length; j++)
            {
                if (same_var(t_new_environ[j], environ[i]))
                {
                    t_found = true;
                    break;
                }
            }
            
            if (!t_found)
            {
                t_new_environ = (char **)realloc(t_new_environ, (t_new_length + 2) * sizeof(char *));
                if (t_new_environ == NULL)
                    _exit(-1);
                t_new_environ[t_new_length++] = environ[i];
            }
        }
        
        // Terminate the new environment list.
        t_new_environ[t_new_length] = NULL;
        
        return t_new_environ;
    }
    
    return environ;
}

///////////////////////////////////////////////////////////////////////////////

// MW-2005-08-15: We have two types of process starting in MacOS X it seems:
//   MCS_startprocess is called by MCLaunch with a docname
//   MCS_startprocess is called by MCOpen without a docname
// Thus, we will fork two methods - and dispatch from MCS_startprocess
static void MCS_startprocess_unix(MCNameRef name, MCStringRef doc, Open_mode mode, Boolean elevated);
static void MCS_startprocess_launch(MCNameRef name, MCStringRef docname, Open_mode mode);

///////////////////////////////////////////////////////////////////////////////

// MW-2005-02-22: Make this global scope for now to enable opensslsocket.cpp
//   to access it.
real8 curtime;

///////////////////////////////////////////////////////////////////////////////

bool MCS_mac_is_link(MCStringRef p_path)
{
	struct stat buf;
    MCAutoStringRefAsUTF8String t_utf8_path;
    /* UNCHECKED */ t_utf8_path.Lock(p_path);
	return (lstat(*t_utf8_path, &buf) == 0 && S_ISLNK(buf.st_mode));
}

bool MCS_mac_readlink(MCStringRef p_path, MCStringRef& r_link)
{
	struct stat t_stat;
	ssize_t t_size;
	MCAutoNativeCharArray t_buffer;
    MCAutoStringRefAsUTF8String t_utf8_path;
    /* UNCHECKED */ t_utf8_path.Lock(p_path);
	if (lstat(*t_utf8_path, &t_stat) == -1 ||
		!t_buffer.New(t_stat.st_size))
		return false;
    
	t_size = readlink(*t_utf8_path, (char*)t_buffer.Chars(), t_stat.st_size);
    
	return (t_size == t_stat.st_size) && t_buffer.CreateStringAndRelease(r_link);
}

Boolean MCS_mac_nodelay(int4 p_fd)
{
	return fcntl(p_fd, F_SETFL, (fcntl(p_fd, F_GETFL, 0) & O_APPEND) | O_NONBLOCK)
    >= 0;
}

///////////////////////////////////////////////////////////////////////////////

// ARM replacement: use POSIX stat instead of FSPathMakeRef
static OSStatus MCS_mac_pathtoref(MCStringRef p_path, FSRef& r_ref)
{
    MCAutoStringRef t_auto_path;
    MCAutoStringRef t_redirected_path;
    MCAutoStringRefAsUTF8String t_path;

    if (!MCS_resolvepath(p_path, &t_auto_path))
        return memFullErr;

    if (!MCS_apply_redirect(*t_auto_path, true, &t_redirected_path))
        t_redirected_path = *t_auto_path;

    if (!t_path.Lock(*t_redirected_path))
        return memFullErr;

    return FSPathMakeRef((const UInt8 *)(*t_path), &r_ref, NULL);
}

static OSErr MCS_mac_pathtoref_and_leaf(MCStringRef p_path, FSRef& r_ref, UniChar*& r_leaf, UniCharCount& r_leaf_length)
{
	OSErr t_error;
	t_error = noErr;
    
	MCAutoStringRef t_resolved_path;
    MCAutoStringRef t_redirected_path;
    
    if (!MCS_resolvepath(p_path, &t_resolved_path))
        // TODO assign relevant error code
        t_error = fnfErr;

    // SN-2015-01-26: [[ Merge-6.7.2-rc-2 ]]
    if (!MCS_apply_redirect(*t_resolved_path, true, &t_redirected_path))
        t_redirected_path = *t_resolved_path;

    
    MCAutoStringRef t_folder, t_leaf;
    uindex_t t_leaf_index;
    if (MCStringLastIndexOfChar(*t_redirected_path, '/', UINDEX_MAX, kMCStringOptionCompareExact, t_leaf_index))
    {
        if (!MCStringDivideAtIndex(*t_redirected_path, t_leaf_index, &t_folder, &t_leaf))
            t_error = memFullErr;
    }
    else
        t_error = fnfErr;
    
	MCAutoStringRefAsUTF8String t_utf8_auto;
    
    if (t_error == noErr)
        if (!t_utf8_auto.Lock(*t_folder))
            t_error = fnfErr;
    
	if (t_error == noErr)
		t_error = FSPathMakeRef((const UInt8 *)*t_utf8_auto, &r_ref, NULL);
	
	// Convert the leaf from MacRoman to UTF16.
	if (t_error == noErr)
	{
		unichar_t *t_utf16_leaf;
		uint4 t_utf16_leaf_length;
        /* UNCHECKED */ MCStringConvertToUnicode(*t_leaf, t_utf16_leaf, t_utf16_leaf_length);
		r_leaf = (UniChar *)t_utf16_leaf;
		r_leaf_length = (UniCharCount)t_utf16_leaf_length;
	}
    
	return t_error;
}

// ARM: FSSpec / FSGetCatalogInfo removed. MCS_mac_fsref_to_fsspec is a no-op stub.
static OSErr MCS_mac_fsref_to_fsspec(const FSRef *p_fsref, void *r_fsspec)
{
    (void)p_fsref; (void)r_fsspec;
    return unimpErr; // FSSpec unavailable on arm64
}

void MCS_mac_closeresourcefile(ResFileRefNum p_ref)
{
    // ARM: resource fork APIs removed. This is a no-op stub.
    // Resource fork support has been replaced with xattr-based storage.
}

static bool MCS_mac_fsref_to_path(FSRef& p_ref, MCStringRef& r_path)
{
	MCAutoArray<byte_t> t_buffer;
	if (!t_buffer.New(PATH_MAX))
		return false;
	FSRefMakePath(&p_ref, (UInt8*)t_buffer.Ptr(), PATH_MAX);
    
	t_buffer.Shrink(strlen((const char*)t_buffer.Ptr()));
    
	return MCStringCreateWithBytes(t_buffer.Ptr(), t_buffer.Size(), kMCStringEncodingUTF8, false, r_path);
}

#ifndef __64_BIT__
#ifndef __arm64__
bool MCS_mac_FSSpec2path(FSSpec *fSpec, MCStringRef& r_path)
{
    MCAutoNativeCharArray t_path, t_name;
    MCAutoStringRef t_filename;
    MCAutoStringRef t_filename_std;
    char *t_char_ptr;
    
    t_path.New(PATH_MAX + 1);
    t_name.New(PATH_MAX + 1);
    
    t_char_ptr = (char*)t_path.Chars();
    
	CopyPascalStringToC(fSpec->name, (char*)t_name.Chars());
    
    /* UNCHECKED */ t_name . Shrink(MCCStringLength((const char *)t_name . Chars()));
    /* UNCHECKED */ t_name.CreateStringAndRelease(&t_filename_std);
	/* UNCHECKED */ MCS_pathfromnative(*t_filename_std, &t_filename);

	char oldchar = fSpec->name[0];
	Boolean dontappendname = False;
	fSpec->name[0] = '\0';
    
	FSRef ref;
    
	// MW-2005-01-21: Removed the following two lines - function would not work if file did not already exist
    
	/* fSpec->name[0] = oldchar;
     dontappendname = True;*/
    
	if ((errno = FSpMakeFSRef(fSpec, &ref)) != noErr)
	{
		if (errno == nsvErr)
		{
			fSpec->name[0] = oldchar;
			if ((errno = FSpMakeFSRef(fSpec, &ref)) == noErr)
			{
				errno = FSRefMakePath(&ref, (unsigned char *)t_char_ptr, PATH_MAX);
				dontappendname = True;
			}
			else
				t_char_ptr[0] = '\0';
		}
		else
			t_char_ptr[0] = '\0';
	}
	else
		errno = FSRefMakePath(&ref, (unsigned char *)t_char_ptr, PATH_MAX);
    
    MCAutoStringRef t_path_str;
    /* UNCHECKED */ MCStringCreateWithBytes((const byte_t*)t_char_ptr, strlen(t_char_ptr), kMCStringEncodingUTF8, false, &t_path_str);
    
	if (!dontappendname)
	{
		/* UNCHECKED */ MCStringFormat(r_path, "%@/%@", *t_path_str, *t_filename);
	}
    else
    {
        r_path = MCValueRetain(*t_path_str);
    }
	return true;
}
#endif // __64_BIT__
#endif // __arm64__

// ============================================================
// ARM Phase 3: Resource fork replacement using extended attributes
//
// macOS resource fork APIs (FSOpenResFile, CloseResFile, UseResFile,
// AddResource, GetResource, etc.) are unavailable on arm64.
//
// We replace the entire resource system with an xattr-based store.
// Resources are stored under the extended attribute:
//   "com.livecode.resource.<type>.<id>"
// where <type> is the 4-char FourCharCode and <id> is the ResID.
//
// This preserves the LiveCode scripting surface (getResource, setResource,
// copyResource, deleteResource, getResources) while using a supported API.
// ============================================================

// Build the xattr key for a resource: "com.livecode.resource.TYPE.ID"
static bool MCS_xattr_resource_key(FourCharCode p_type, short p_id, MCStringRef& r_key)
{
    char t_type_str[5];
    uint32_t t_swapped = OSSwapHostToBigInt32(p_type);
    memcpy(t_type_str, &t_swapped, 4);
    t_type_str[4] = '\0';
    return MCStringFormat(r_key, "com.livecode.resource.%.4s.%d", t_type_str, (int)p_id);
}

// List all xattr resource keys for a given file path
static bool MCS_xattr_list_resource_keys(const char *p_utf8_path, MCAutoArray<MCStringRef>& r_keys)
{
    ssize_t t_list_size = listxattr(p_utf8_path, NULL, 0, 0);
    if (t_list_size <= 0)
        return true;

    MCAutoArray<char> t_buf;
    if (!t_buf.New((uindex_t)t_list_size))
        return false;
    listxattr(p_utf8_path, t_buf.Ptr(), (size_t)t_list_size, 0);

    const char *t_ptr = t_buf.Ptr();
    const char *t_end = t_ptr + t_list_size;
    while (t_ptr < t_end)
    {
        if (strncmp(t_ptr, "com.livecode.resource.", 22) == 0)
        {
            MCStringRef t_key;
            if (MCStringCreateWithCString(t_ptr, t_key))
                /* UNCHECKED */ r_keys.Push(t_key);
        }
        t_ptr += strlen(t_ptr) + 1;
    }
    return true;
}

// Stub: MCS_mac_openresourcefile_with_path now just verifies the file exists.
// The ResFileRefNum returned is unused - operations go direct via xattr.
bool MCS_mac_openresourcefile_with_path(MCStringRef p_path, SInt8 p_permissions, bool p_create, ResFileRefNum& r_fileref_num, MCStringRef& r_error)
{
    MCAutoStringRefAsUTF8String t_utf8;
    if (!t_utf8.Lock(p_path))
        return MCStringCreateWithCString("can't lock path", r_error);

    struct stat t_stat;
    if (stat(*t_utf8, &t_stat) != 0)
    {
        if (!p_create)
            return MCStringCreateWithCString("file not found", r_error);
        // File doesn't exist - touch it
        int t_fd = open(*t_utf8, O_CREAT | O_RDWR, 0644);
        if (t_fd < 0)
            return MCStringCreateWithCString("can't create file", r_error);
        close(t_fd);
    }
    r_fileref_num = 1; // dummy value - not used by xattr operations
    return true;
}

// Stub: resource fork open (LoadResFile / SaveResFile / CopyResourceFork paths).
// We return a dummy handle; actual data goes via xattr key "com.livecode.resfork".
static void MCS_mac_openresourcefork_with_path(MCStringRef p_path, SInt8 p_permission, bool p_create, FSIORefNum *r_fork_ref, MCStringRef& r_error)
{
    // No-op stub on ARM: raw fork I/O replaced by xattr in LoadResFile/SaveResFile
    *r_fork_ref = 0;
}

static void MCS_openresourcefork_with_fsref(FSRef *p_ref, SInt8 p_permission, bool p_create, FSIORefNum *r_fork_ref, MCStringRef& r_error)
{
    *r_fork_ref = 0;
}

///////////////////////////////////////////////////////////////////////////////

// Resource utility functions

// Build a list of resource info lines from xattrs matching searchType (0 = all)
static bool getResourceInfo(MCListRef p_list, ResType searchType)
{
    // ARM: stub - resource enumeration now done inline in GetResources via xattr listing
    return true;
}

/********************************************************************/
/*                       Resource Handling                          */
/********************************************************************/

// ARM: MCAutoResourceFileHandle is a no-op RAII wrapper since resource forks
// have been replaced with xattr-based storage and ResFileRefNum is a dummy.
class MCAutoResourceFileHandle
{
public:
    MCAutoResourceFileHandle()  { m_res_file = 0; }
    ~MCAutoResourceFileHandle() { /* no-op on ARM */ }

    ResFileRefNum operator = (ResFileRefNum p_res_file)
    {
        m_res_file = p_res_file;
        return m_res_file;
    }
    ResFileRefNum operator * (void) { return m_res_file; }
    ResFileRefNum& operator & (void) { return m_res_file; }

private:
    ResFileRefNum m_res_file;
};

class MCStdioFileHandle: public MCSystemFileHandle
{
public:
    
    MCStdioFileHandle(FILE *p_fptr, bool p_is_serial_port = false)
    {
        m_stream = p_fptr;
        m_is_eof = false;
        m_is_serial_port = p_is_serial_port;
    }
	
	virtual void Close(void)
	{
       if (m_stream != NULL)
            fclose(m_stream);
        
        delete this;
	}
	
	virtual bool Read(void *p_ptr, uint32_t p_length, uint32_t& r_read)
	{
        uint4 nread;
        
        // MW-2010-08-26: Taken from the Linux source, this changes the previous code
        //   to take into account pipes and such.
        char *sptr = (char *)p_ptr;
        uint4 toread = p_length;
        uint4 offset = 0;
        errno = 0;
        while ((nread = fread(&sptr[offset], 1, toread, m_stream)) != toread)
        {
            offset += nread;
            r_read = offset;
            if (feof(m_stream))
            {
                m_is_eof = true;
                return true;
            }
            if (ferror(m_stream))
            {
                clearerr(m_stream);
                m_is_eof = false;
                
                if (errno == EAGAIN)
                    return true;
                
                if (errno == EINTR)
                {
                    toread -= nread;
                    continue;
                }
                
                // A "real" error occurred
                return false;
            }
            
            m_is_eof = false;
            return false;
        }
        r_read = nread;
        return true;
	}
    
	virtual bool Write(const void *p_buffer, uint32_t p_length)
	{
        bool t_success;
        
        // SN-2014-05-21 [[ Bug 12246 ]]
        // When writing to a serial port, we can hit EAGAIN error
        if (m_is_serial_port)
        {
            integer_t t_last_writing;
            integer_t t_length;
            uint32_t t_offset;
            
            t_length = p_length;
            t_offset = 0;
            t_success = true;
            
            while (t_length && t_success)
            {
                t_last_writing = fwrite((char*)p_buffer + t_offset, 1, t_length, m_stream);
                t_length -= t_last_writing;
                t_offset += t_last_writing;
                
                // Avoid to get stuck in a loop in case writing failed
                if (t_last_writing == 0 && t_length != 0 && errno != EAGAIN)
                    t_success = false;
            }
        }
        else
            t_success = fwrite(p_buffer, 1, p_length, m_stream) == p_length;
        
        return t_success;
	}
    
    virtual bool IsExhausted(void)
    {
        if (m_is_eof)
            return true;
        
        return false;
    }
	
	virtual bool Seek(int64_t offset, int p_dir)
	{
        // TODO Add MCSystemFileHandle::SetStream(char *newptr) ?
		if (m_is_eof)
		{
			clearerr(m_stream);
			m_is_eof = false;
		}
		return fseeko(m_stream, offset, p_dir < 0 ? SEEK_END : (p_dir > 0 ? SEEK_SET : SEEK_CUR)) == 0;
	}
	
	virtual bool Truncate(void)
	{
		return ftruncate(fileno(m_stream), ftell(m_stream)) == 0;
	}
	
	virtual bool Sync(void)
	{
        if (m_stream != NULL)
        {
            int64_t t_pos;
            t_pos = ftello(m_stream);
            return fseeko(m_stream, t_pos, SEEK_SET) == 0;
        }
        return true;
	}
	
	virtual bool Flush(void)
	{
        if (m_stream != NULL)
            return fflush(m_stream) == 0;
        
        return true;
	}
	
	virtual bool PutBack(char p_char)
	{
        if (m_stream == NULL)
            return Seek(-1, 0);
        
        if (ungetc(p_char, m_stream) != p_char)
            return false;
        
        return true;
	}
	
	virtual int64_t Tell(void)
	{
		return ftello(m_stream);
	}
	
	virtual uint64_t GetFileSize(void)
	{
		struct stat t_info;
		if (fstat(fileno(m_stream), &t_info) != 0)
			return 0;
		return t_info . st_size;
	}
	
	virtual void *GetFilePointer(void)
	{
		return m_stream;
	}
	
	FILE *GetStream(void)
	{
		return m_stream;
	}
    
    virtual bool TakeBuffer(void*& r_buffer, size_t& r_length)
    {
        return false;
    }
	
private:
	FILE *m_stream;
    bool m_is_eof;
    bool m_is_serial_port;
};

struct MCMacSystemService: public MCMacSystemServiceInterface//, public MCMacDesktop
{
    virtual bool SetResource(MCStringRef p_source, MCStringRef p_type, MCStringRef p_id, MCStringRef p_name, MCStringRef p_flags, MCStringRef p_value, MCStringRef& r_error)
    {
        // ARM: xattr-based resource storage
        // Key format: "com.livecode.resource.TYPE.ID"
        MCAutoStringRefAsUTF8String t_utf8_path;
        if (!t_utf8_path.Lock(p_source))
            return MCStringCreateWithCString("can't lock path", r_error);

        FourCharCode t_type_code;
        if (!FourCharCodeFromString(p_type, 0, t_type_code))
            return MCStringCreateWithCString("invalid type", r_error);

        short t_rid = 0;
        if (!MCStringIsEmpty(p_id))
            /* UNCHECKED */ MCStringToInt16(p_id, t_rid);
        if (t_rid == 0)
        {
            // Auto-assign ID: find highest existing and increment
            MCAutoArray<MCStringRef> t_keys;
            MCS_xattr_list_resource_keys(*t_utf8_path, t_keys);
            for (uindex_t i = 0; i < t_keys.Size(); i++)
            {
                MCAutoStringRefAsCString t_ckey;
                t_ckey.Lock(t_keys[i]);
                int t_id_val;
                char t_type_buf[8];
                if (sscanf(*t_ckey, "com.livecode.resource.%4s.%d", t_type_buf, &t_id_val) == 2)
                    if (t_id_val >= t_rid) t_rid = (short)(t_id_val + 1);
                MCValueRelease(t_keys[i]);
            }
            if (t_rid == 0) t_rid = 128;
        }

        MCAutoStringRef t_key;
        if (!MCS_xattr_resource_key(t_type_code, t_rid, &t_key))
            return MCStringCreateWithCString("can't build key", r_error);

        MCAutoStringRefAsCString t_key_cstr;
        t_key_cstr.Lock(*t_key);

        MCAutoStringRefAsUTF8String t_value_utf8;
        t_value_utf8.Lock(p_value);
        if (setxattr(*t_utf8_path, *t_key_cstr, *t_value_utf8, t_value_utf8.Size(), 0, 0) != 0)
            return MCStringCreateWithCString("can't set resource", r_error);

        return true;
    }

    virtual bool GetResource(MCStringRef p_source, MCStringRef p_type, MCStringRef p_name, MCStringRef& r_value, MCStringRef& r_error)
    {
        // ARM: xattr-based resource lookup by ID or name (name treated as ID on ARM)
        MCAutoStringRefAsUTF8String t_utf8_path;
        if (!t_utf8_path.Lock(p_source))
            return MCStringCreateWithCString("can't lock path", r_error);

        FourCharCode t_type_code;
        if (!FourCharCodeFromString(p_type, 0, t_type_code))
            return MCStringCreateWithCString("invalid type", r_error);

        integer_t t_rid;
        if (!MCStringToInteger(p_name, t_rid))
            return MCStringCreateWithCString("resource not found", r_error);

        MCAutoStringRef t_key;
        if (!MCS_xattr_resource_key(t_type_code, (short)t_rid, &t_key))
            return MCStringCreateWithCString("can't build key", r_error);

        MCAutoStringRefAsCString t_key_cstr;
        t_key_cstr.Lock(*t_key);

        ssize_t t_size = getxattr(*t_utf8_path, *t_key_cstr, NULL, 0, 0, 0);
        if (t_size < 0)
        {
            MCresult->sets("can't find specified resource");
            return MCStringCreateWithCString("can't find specified resource", r_error);
        }

        MCAutoArray<byte_t> t_buf;
        if (!t_buf.New((uindex_t)t_size))
            return MCStringCreateWithCString("out of memory", r_error);
        getxattr(*t_utf8_path, *t_key_cstr, t_buf.Ptr(), (size_t)t_size, 0, 0);
        return MCStringCreateWithBytes(t_buf.Ptr(), (uindex_t)t_size, kMCStringEncodingUTF8, false, r_value);
    }

    virtual bool GetResources(MCStringRef p_source, MCStringRef p_type, MCListRef& r_list, MCStringRef& r_error)
    {
        // ARM: enumerate xattr resource keys on the file
        MCAutoStringRefAsUTF8String t_utf8_path;
        if (!t_utf8_path.Lock(p_source))
            return MCStringCreateWithCString("can't lock path", r_error);

        MCAutoArray<MCStringRef> t_keys;
        if (!MCS_xattr_list_resource_keys(*t_utf8_path, t_keys))
            return MCStringCreateWithCString("can't list resources", r_error);

        MCAutoListRef t_list;
        if (!MCListCreateMutable('\n', &t_list))
            return false;

        const char *t_filter_type = NULL;
        MCAutoStringRefAsCString t_type_cstr;
        if (p_type != nil && !MCStringIsEmpty(p_type))
        {
            t_type_cstr.Lock(p_type);
            t_filter_type = *t_type_cstr;
        }

        for (uindex_t i = 0; i < t_keys.Size(); i++)
        {
            MCAutoStringRefAsCString t_ckey;
            t_ckey.Lock(t_keys[i]);
            char t_type_buf[8] = {};
            int t_id_val;
            if (sscanf(*t_ckey, "com.livecode.resource.%4s.%d", t_type_buf, &t_id_val) == 2)
            {
                if (t_filter_type == NULL || strncmp(t_type_buf, t_filter_type, 4) == 0)
                {
                    ssize_t t_size = getxattr(*t_utf8_path, *t_ckey, NULL, 0, 0, 0);
                    MCAutoStringRef t_line;
                    /* UNCHECKED */ MCStringFormat(&t_line, "%4s,%d,,%ld,\n", t_type_buf, t_id_val, (long)t_size);
                    /* UNCHECKED */ MCListAppend(*t_list, *t_line);
                }
            }
            MCValueRelease(t_keys[i]);
        }

        return MCListCopy(*t_list, r_list);
    }

    virtual bool CopyResource(MCStringRef p_source, MCStringRef p_dest, MCStringRef p_type, MCStringRef p_name, MCStringRef p_newid, MCStringRef& r_error)
    {
        // ARM: copy a resource xattr from source to dest file
        MCAutoStringRefAsUTF8String t_src_utf8, t_dst_utf8;
        if (!t_src_utf8.Lock(p_source) || !t_dst_utf8.Lock(p_dest))
            return MCStringCreateWithCString("can't lock path", r_error);

        FourCharCode t_type_code;
        if (!FourCharCodeFromString(p_type, 0, t_type_code))
            return MCStringCreateWithCString("invalid type", r_error);

        integer_t t_src_id;
        if (!MCStringToInteger(p_name, t_src_id))
            return MCStringCreateWithCString("invalid resource id", r_error);

        integer_t t_dst_id = t_src_id;
        if (p_newid != NULL && !MCStringIsEmpty(p_newid))
            /* UNCHECKED */ MCStringToInteger(p_newid, t_dst_id);

        MCAutoStringRef t_src_key, t_dst_key;
        MCS_xattr_resource_key(t_type_code, (short)t_src_id, &t_src_key);
        MCS_xattr_resource_key(t_type_code, (short)t_dst_id, &t_dst_key);

        MCAutoStringRefAsCString t_src_ckey, t_dst_ckey;
        t_src_ckey.Lock(*t_src_key);
        t_dst_ckey.Lock(*t_dst_key);

        ssize_t t_size = getxattr(*t_src_utf8, *t_src_ckey, NULL, 0, 0, 0);
        if (t_size < 0)
            return MCStringCreateWithCString("can't find source resource", r_error);

        MCAutoArray<byte_t> t_buf;
        if (!t_buf.New((uindex_t)t_size))
            return MCStringCreateWithCString("out of memory", r_error);
        getxattr(*t_src_utf8, *t_src_ckey, t_buf.Ptr(), (size_t)t_size, 0, 0);

        if (setxattr(*t_dst_utf8, *t_dst_ckey, t_buf.Ptr(), (size_t)t_size, 0, 0) != 0)
            return MCStringCreateWithCString("can't write destination resource", r_error);

        return true;
    }

    virtual bool DeleteResource(MCStringRef p_source, MCStringRef p_type, MCStringRef p_name, MCStringRef& r_error)
    {
        // ARM: remove a resource xattr
        MCAutoStringRefAsUTF8String t_utf8_path;
        if (!t_utf8_path.Lock(p_source))
            return MCStringCreateWithCString("can't lock path", r_error);

        FourCharCode t_type_code;
        if (!FourCharCodeFromString(p_type, 0, t_type_code))
            return MCStringCreateWithCString("invalid type", r_error);

        integer_t t_rid;
        if (!MCStringToInteger(p_name, t_rid))
            return MCStringCreateWithCString("invalid resource id", r_error);

        MCAutoStringRef t_key;
        MCS_xattr_resource_key(t_type_code, (short)t_rid, &t_key);
        MCAutoStringRefAsCString t_key_cstr;
        t_key_cstr.Lock(*t_key);

        if (removexattr(*t_utf8_path, *t_key_cstr, 0) != 0)
            return MCStringCreateWithCString("can't find the resource specified", r_error);

        return true;
    }

    virtual void CopyResourceFork(MCStringRef p_source, MCStringRef p_destination)
    {
        // ARM: copy all "com.livecode.resource.*" xattrs from source to dest
        MCAutoStringRefAsUTF8String t_src_utf8, t_dst_utf8;
        if (!t_src_utf8.Lock(p_source) || !t_dst_utf8.Lock(p_destination))
            return;

        MCAutoArray<MCStringRef> t_keys;
        MCS_xattr_list_resource_keys(*t_src_utf8, t_keys);

        for (uindex_t i = 0; i < t_keys.Size(); i++)
        {
            MCAutoStringRefAsCString t_ckey;
            t_ckey.Lock(t_keys[i]);
            ssize_t t_size = getxattr(*t_src_utf8, *t_ckey, NULL, 0, 0, 0);
            if (t_size > 0)
            {
                MCAutoArray<byte_t> t_buf;
                if (t_buf.New((uindex_t)t_size))
                {
                    getxattr(*t_src_utf8, *t_ckey, t_buf.Ptr(), (size_t)t_size, 0, 0);
                    setxattr(*t_dst_utf8, *t_ckey, t_buf.Ptr(), (size_t)t_size, 0, 0);
                }
            }
            MCValueRelease(t_keys[i]);
        }

        // Also copy raw resource fork blob if present
        const char *t_fork_key = "com.livecode.resfork";
        ssize_t t_fork_size = getxattr(*t_src_utf8, t_fork_key, NULL, 0, 0, 0);
        if (t_fork_size > 0)
        {
            MCAutoArray<byte_t> t_buf;
            if (t_buf.New((uindex_t)t_fork_size))
            {
                getxattr(*t_src_utf8, t_fork_key, t_buf.Ptr(), (size_t)t_fork_size, 0, 0);
                setxattr(*t_dst_utf8, t_fork_key, t_buf.Ptr(), (size_t)t_fork_size, 0, 0);
            }
        }
    }

    virtual void LoadResFile(MCStringRef p_filename, MCStringRef& r_data)
    {
        // ARM: load raw resource fork blob from xattr "com.livecode.resfork"
        if (!MCSecureModeCanAccessDisk())
        {
            r_data = MCValueRetain(kMCEmptyString);
            MCresult->sets("can't open file");
            return;
        }

        MCAutoStringRef t_redirected;
        if (!MCS_apply_redirect(p_filename, true, &t_redirected))
            t_redirected = p_filename;

        MCAutoStringRefAsUTF8String t_utf8;
        if (!t_utf8.Lock(*t_redirected))
        {
            MCresult->sets("can't open file");
            return;
        }

        const char *t_key = "com.livecode.resfork";
        ssize_t t_size = getxattr(*t_utf8, t_key, NULL, 0, 0, 0);
        if (t_size < 0)
        {
            r_data = MCValueRetain(kMCEmptyString);
            MCresult->clear(False);
            return;
        }

        MCAutoArray<byte_t> t_buf;
        if (!t_buf.New((uindex_t)t_size))
        {
            MCresult->sets("can't create data buffer");
            return;
        }
        getxattr(*t_utf8, t_key, t_buf.Ptr(), (size_t)t_size, 0, 0);
        /* UNCHECKED */ MCStringCreateWithNativeChars((const char_t *)t_buf.Ptr(), (uindex_t)t_size, r_data);
        MCresult->clear(False);
    }

    virtual void SaveResFile(MCStringRef p_path, MCDataRef p_data)
    {
        // ARM: save raw resource fork blob as xattr "com.livecode.resfork"
        if (!MCSecureModeCanAccessDisk())
        {
            MCresult->sets("can't open file");
            return;
        }

        MCAutoStringRefAsUTF8String t_utf8;
        if (!t_utf8.Lock(p_path))
        {
            MCresult->sets("can't open file");
            return;
        }

        const char *t_key = "com.livecode.resfork";
        if (setxattr(*t_utf8, t_key, MCDataGetBytePtr(p_data), MCDataGetLength(p_data), 0, 0) != 0)
            MCresult->sets("error writing file");
        else
            MCresult->clear();
    }

    
    // MW-2006-08-05: Vetted for Endian issues
    virtual void Send(MCStringRef p_message, MCStringRef p_program, MCStringRef p_eventtype, Boolean p_reply)
    {
        //send "" to program "" with/without reply
        if (!MCSecureModeCheckAppleScript())
            return;
        
        
        AEAddressDesc receiver;
        errno = getDescFromAddress(p_program, &receiver);
        if (errno != noErr)
        {
            AEDisposeDesc(&receiver);
            MCresult->sets("no such program");
            return;
        }
        AppleEvent ae;
        if (p_eventtype == NULL)
            MCStringCreateWithCString("miscdosc", p_eventtype);
        
        AEEventClass ac;
        AEEventID aid;
        
        /* UNCHECKED */ FourCharCodeFromString(p_eventtype, 0, ac);
        /* UNCHECKED */ FourCharCodeFromString(p_eventtype, 4, aid);
               
        AECreateAppleEvent(ac, aid, &receiver, kAutoGenerateReturnID,
                           kAnyTransactionID, &ae);
        AEDisposeDesc(&receiver); //dispose of the receiver description record
        // if the ae message we are sending is 'odoc', 'pdoc' then
        // create a document descriptor of type fypeFSS for the document
        
        Boolean docmessage = False; //Is this message contains a document descriptor?
        AEDescList files_list, file_desc;
        // ARM: AliasHandle removed - use typeFSRef directly instead of typeAlias
        
        if (aid == 'odoc' || aid == 'pdoc')
        {
            FSRef t_fsref;
            if (MCS_mac_pathtoref(p_message, t_fsref) == noErr)
            {
                AECreateList(NULL, 0, false, &files_list);
                // ARM: send FSRef directly instead of creating an Alias Handle
                AECreateDesc(typeFSRef, &t_fsref, sizeof(FSRef), &file_desc);
                AEPutDesc(&files_list, 0, &file_desc);
                AEPutParamDesc(&ae, keyDirectObject, &files_list);
                docmessage = True;
            }
        }
        //non document related massge, assume it's typeChar message
        if (!docmessage && MCStringGetLength(p_message))
        {
            MCAutoStringRefAsUTF8String t_utf8;
            /* UNCHECKED */ t_utf8.Lock(p_message);
            AEPutParamPtr(&ae, keyDirectObject, typeUTF8Text, *t_utf8, t_utf8.Size());
        }
        
        //Send the Apple event
        AppleEvent answer;
        if (p_reply == True)
            errno = AESend(&ae, &answer, kAEQueueReply, kAENormalPriority,
                           kAEDefaultTimeout, NULL, NULL); //no reply
        else
            errno = AESend(&ae, &answer, kAENoReply, kAENormalPriority,
                           kAEDefaultTimeout, NULL, NULL); //reply comes in event queue
        if (docmessage)
        {
            // ARM: no AliasHandle to dispose
            AEDisposeDesc(&file_desc);
            AEDisposeDesc(&files_list);
        }
        AEDisposeDesc(&ae);
        if (errno != noErr)
        {
            char *buffer = new (nothrow) char[6 + I2L];
            sprintf(buffer, "error %d", errno);
            MCresult->copysvalue(buffer);
            delete[] buffer;
            return;
        }
        if (p_reply == True)
        { /* wait for a reply in a loop.  The reply comes in
           from regular event handling loop
           and is handled by an Apple event handler*/
            real8 endtime = curtime + AETIMEOUT;
            while (True)
            {
                if (MCscreen->wait(READ_INTERVAL, False, True))
                {
                    MCresult->sets("user interrupt");
                    return;
                }
                if (curtime > endtime)
                {
                    MCresult->sets("timeout");
                    return;
                }
                if (AEAnswerErr != NULL || AEAnswerData != NULL)
                    break;
            }
            if (AEAnswerErr != NULL)
            {
                MCresult->setvalueref(AEAnswerErr);
                MCValueRelease(AEAnswerErr);
                AEAnswerErr = NULL;
            }
            else
            {
                MCresult->setvalueref(AEAnswerData);
                MCValueRelease(AEAnswerData);
                AEAnswerData = NULL;
            }
            AEDisposeDesc(&answer);
        }
        else
            MCresult->clear(False);
    }
    
    // MW-2006-08-05: Vetted for Endian issues
    virtual void Reply(MCStringRef p_message, MCStringRef p_keyword, Boolean p_error)
    {
        MCValueAssign(AEReplyMessage, p_message);
        
        //at any one time only either keyword or error is set
        if (p_keyword != NULL)
        {
            /* UNCHECKED */ FourCharCodeFromString(p_keyword, 0, replykeyword);
        }
        else
        {
            if (p_error)
                replykeyword = 'errs';
            else
                replykeyword = '----';
        }
    }
    
    // MW-2006-08-05: Vetted for Endian issues
    virtual void RequestAE(MCStringRef p_message, uint2 p_ae, MCStringRef& r_value)
    {
        if (aePtr == NULL)
        {
            /* UNCHECKED */ MCStringCreateWithCString("No current Apple event", r_value); //as specified in HyperTalk
            return;
        }
        errno = noErr;
        
        switch (p_ae)
        {
            case AE_CLASS:
            {
                if ((errno = getAEAttributes(aePtr, keyEventClassAttr, r_value)) == noErr)
                    return;
                break;
            }
            case AE_DATA:
            {
                if (MCStringIsEmpty(p_message))
                { //no keyword, get event parameter(data)
                    DescType rType;
                    Size rSize;  //actual size returned
                    /*first let's find out the size of incoming event data */
                    
                    // On Snow Leopard check for a coercion to a file list first as otherwise
                    // we get a bad URL!
                    if (MCmajorosversion >= MCOSVersionMake(10,6,0))
                    {
                        // SN-2014-10-07: [[ Bug 13587 ]] fetch_as_as_fsref_list updated to return an MCList
                        MCAutoListRef t_list;
                        
                        if (fetch_ae_as_fsref_list(&t_list))
                        {
                            /* UNCHECKED */ MCListCopyAsString(*t_list, r_value);
                            return;
                        }
                    }
                    
                    if ((errno = AEGetParamPtr(aePtr, keyDirectObject, typeUTF8Text, &rType, NULL, 0, &rSize)) == noErr)
                    {
                        byte_t *t_utf8 = new (nothrow) byte_t[rSize + 1];
                        AEGetParamPtr(aePtr, keyDirectObject, typeUTF8Text, &rType, t_utf8, rSize, &rSize);
                        /* UNCHECKED */ MCStringCreateWithBytesAndRelease(t_utf8, rSize, kMCStringEncodingUTF8, false, r_value);
                    }
                    else
                    {
                        // SN-2014-10-07: [[ Bug 13587 ]] fetch_ae_as_frsef_list updated to return an MCList
                        MCAutoListRef t_list;
                        if (fetch_ae_as_fsref_list(&t_list))
                            /* UNCHECKED */ MCListCopyAsString(*t_list, r_value);
                        else
                            /* UNCHECKED */ MCStringCreateWithCString("file list error", r_value);
                    }
                    return;
                }
                else
                {
                    AEKeyword key;
                    /* UNCHECKED */ FourCharCodeFromString(p_message, MCStringGetLength(p_message) - sizeof(AEKeyword), key);
                    
                    if (key == keyAddressAttr || key == keyEventClassAttr
                        || key == keyEventIDAttr || key == keyEventSourceAttr
                        || key == keyInteractLevelAttr || key == keyMissedKeywordAttr
                        || key == keyOptionalKeywordAttr || key == keyOriginalAddressAttr
                        || key == keyReturnIDAttr || key == keyTimeoutAttr
                        || key == keyTransactionIDAttr)
                    {
                        if ((errno = getAEAttributes(aePtr, key, r_value)) == noErr)
                            return;
                    }
                    else
                    {
                        if ((errno = getAEParams(aePtr, key, r_value)) == noErr)
                            return;
                    }
                }
            }
                break;
            case AE_ID:
            {
                if ((errno = getAEAttributes(aePtr, keyEventIDAttr, r_value)) == noErr)
                    return;
                break;
            }
            case AE_RETURN_ID:
            {
                if ((errno = getAEAttributes(aePtr, keyReturnIDAttr, r_value)) == noErr)
                    return;
                break;
            }
            case AE_SENDER:
            {
                AEAddressDesc senderDesc;
                char *sender = new (nothrow) char[128];
                
                if ((errno = AEGetAttributeDesc(aePtr, keyOriginalAddressAttr,
                                                typeWildCard, &senderDesc)) == noErr)
                {
                    errno = getAddressFromDesc(senderDesc, sender);
                    AEDisposeDesc(&senderDesc);
                    /* UNCHECKED */ MCStringCreateWithCStringAndRelease(sender, r_value);
                    return;
                }
                delete[] sender;
                break;
            }
        }  /* end switch */
        
        if (errno == errAECoercionFail) //data could not display as text
        {
            /* UNCHECKED */ MCStringCreateWithCString("unknown type", r_value);
            return;
        }
        
        /* UNCHECKED */ MCStringCreateWithCString("not found", r_value);
    }
    
    // MW-2006-08-05: Vetted for Endian issues
    virtual bool RequestProgram(MCStringRef p_message, MCStringRef p_program, MCStringRef& r_value)
    {
        AEAddressDesc receiver;
        errno = getDescFromAddress(p_program, &receiver);
        if (errno != noErr)
        {
            AEDisposeDesc(&receiver);
            MCresult->sets("no such program");
            r_value = MCValueRetain(kMCEmptyString);
            return false;
        }
        AppleEvent ae;
        errno = AECreateAppleEvent('misc', 'eval', &receiver,
                                   kAutoGenerateReturnID, kAnyTransactionID, &ae);
        AEDisposeDesc(&receiver); //dispose of the receiver description record
        //add parameters to the Apple event
        MCAutoStringRefAsUTF8String t_message;
        /* UNCHECKED */ t_message.Lock(p_message);
        AEPutParamPtr(&ae, keyDirectObject, typeUTF8Text, *t_message, t_message.Size());
        //Send the Apple event
        AppleEvent answer;
        errno = AESend(&ae, &answer, kAEQueueReply, kAENormalPriority,
                       kAEDefaultTimeout, NULL, NULL); //no reply
        AEDisposeDesc(&ae);
        AEDisposeDesc(&answer);
        if (errno != noErr)
        {
            char *buffer = new (nothrow) char[6 + I2L];
            sprintf(buffer, "error %d", errno);
            MCresult->copysvalue(buffer);
            delete[] buffer;
            
            r_value = MCValueRetain(kMCEmptyString);
            return false;
        }
        real8 endtime = curtime + AETIMEOUT;
        while (True)
        {
            if (MCscreen->wait(READ_INTERVAL, False, True))
            {
                MCresult->sets("user interrupt");
                r_value = MCValueRetain(kMCEmptyString);
                return false;
            }
            if (curtime > endtime)
            {
                MCresult->sets("timeout");
                r_value = MCValueRetain(kMCEmptyString);
                return false;
            }
            if (AEAnswerErr != NULL || AEAnswerData != NULL)
                break;
        }
        if (AEAnswerErr != NULL)
        {
            MCresult->setvalueref(AEAnswerErr);
            MCValueRelease(AEAnswerErr);
            AEAnswerErr = NULL;
            r_value = MCValueRetain(kMCEmptyString);
            return true;
        }
        else
        {
            MCresult->clear(False);
            r_value = AEAnswerData;     // Pass on the reference
            AEAnswerData = NULL;
            return false;
        }
    }
};

#define CATALOG_MAX_ENTRIES 16
static bool MCS_getentries_for_folder(MCStringRef p_folder, MCSystemListFolderEntriesCallback p_callback, void *x_context)
{
    OSStatus t_os_status;
    
    Boolean t_is_folder;
    FSRef t_current_fsref;
    
    MCAutoStringRefAsUTF8String t_utf8_folder;
    /* UNCHECKED */ t_utf8_folder . Lock(p_folder);
    
    t_os_status = FSPathMakeRef((const UInt8 *)*t_utf8_folder, &t_current_fsref, &t_is_folder);
    if (t_os_status != noErr || !t_is_folder)
        return false;
    
    // Create the iterator, pass kFSIterateFlat to iterate over the current subtree only
    FSIterator t_catalog_iterator;
    t_os_status = FSOpenIterator(&t_current_fsref, kFSIterateFlat, &t_catalog_iterator);
    if (t_os_status != noErr)
        return false;
    
    uint4 t_entry_count;
    t_entry_count = 0;
    
    ItemCount t_max_objects, t_actual_objects;
    t_max_objects = CATALOG_MAX_ENTRIES;
    t_actual_objects = 0;
    FSCatalogInfo t_catalog_infos[CATALOG_MAX_ENTRIES];
    HFSUniStr255 t_names[CATALOG_MAX_ENTRIES];
    
    FSCatalogInfoBitmap t_info_bitmap;
    t_info_bitmap = kFSCatInfoAllDates |
    kFSCatInfoPermissions |
    kFSCatInfoUserAccess |
    kFSCatInfoFinderInfo |
    kFSCatInfoDataSizes |
    kFSCatInfoRsrcSizes |
    kFSCatInfoNodeFlags;
    
    OSErr t_oserror;
    do
    {
        t_oserror = FSGetCatalogInfoBulk(t_catalog_iterator, t_max_objects, &t_actual_objects, NULL, t_info_bitmap, t_catalog_infos, NULL, NULL, t_names);
        if (t_oserror != noErr && t_oserror != errFSNoMoreItems)
        {	// clean up and exit
            FSCloseIterator(t_catalog_iterator);
            return false;
        }
        
        for(uint4 t_i = 0; t_i < (uint4)t_actual_objects; t_i++)
        {
            MCSystemFolderEntry t_entry;
            
            MCStringRef t_unicode_name;
            bool t_is_entry_folder;
            
            t_is_entry_folder = t_catalog_infos[t_i] . nodeFlags & kFSNodeIsDirectoryMask;
            
            // MW-2008-02-27: [[ Bug 5920 ]] Make sure we convert Finder to POSIX style paths
            for(uint4 i = 0; i < t_names[t_i] . length; ++i)
                if (t_names[t_i] . unicode[i] == '/')
                    t_names[t_i] . unicode[i] = ':';
            
            if (t_names[t_i] . length != 0)
                MCStringCreateWithChars(t_names[t_i] . unicode, t_names[t_i] . length, t_unicode_name);
            else
                t_unicode_name = (MCStringRef)MCValueRetain(kMCEmptyString);
            
            FSPermissionInfo *t_permissions;
            t_permissions = (FSPermissionInfo *)&(t_catalog_infos[t_i] . permissions);
            
            uint32_t t_creator;
            uint32_t t_type;
            char t_filetype[9];
            
            t_creator = 0;
            t_type = 0;
            
            if (!t_is_entry_folder)
            {
                FileInfo *t_file_info;
                t_file_info = (FileInfo *) &t_catalog_infos[t_i] . finderInfo;
                uint4 t_file_creator;
                t_file_creator = MCSwapInt32NetworkToHost(t_file_info -> fileCreator);
                uint4 t_file_type;
                t_file_type = MCSwapInt32NetworkToHost(t_file_info -> fileType);
                
                if (t_file_info != NULL)
                {
                    memcpy(t_filetype, (char*)&t_file_creator, 4);
                    memcpy(&t_filetype[4], (char *)&t_file_type, 4);
                    t_filetype[8] = '\0';
                }
                else
                    t_filetype[0] = '\0';
            }
            else
                strcpy(t_filetype, "????????"); // this is what the "old" getentries did
            
            CFAbsoluteTime t_creation_time;
            UCConvertUTCDateTimeToCFAbsoluteTime(&t_catalog_infos[t_i] . createDate, &t_creation_time);
            t_creation_time += kCFAbsoluteTimeIntervalSince1970;
            
            CFAbsoluteTime t_modification_time;
            UCConvertUTCDateTimeToCFAbsoluteTime(&t_catalog_infos[t_i] . contentModDate, &t_modification_time);
            t_modification_time += kCFAbsoluteTimeIntervalSince1970;
            
            CFAbsoluteTime t_access_time;
            UCConvertUTCDateTimeToCFAbsoluteTime(&t_catalog_infos[t_i] . accessDate, &t_access_time);
            t_access_time += kCFAbsoluteTimeIntervalSince1970;
            
            CFAbsoluteTime t_backup_time;
            if (t_catalog_infos[t_i] . backupDate . highSeconds == 0 && t_catalog_infos[t_i] . backupDate . lowSeconds == 0 && t_catalog_infos[t_i] . backupDate . fraction == 0)
                t_backup_time = 0;
            else
            {
                UCConvertUTCDateTimeToCFAbsoluteTime(&t_catalog_infos[t_i] . backupDate, &t_backup_time);
                t_backup_time += kCFAbsoluteTimeIntervalSince1970;
            }
            
            t_entry.name = t_unicode_name;
            t_entry.data_size = t_catalog_infos[t_i] . dataLogicalSize;
            t_entry.resource_size = t_catalog_infos[t_i] . rsrcLogicalSize;
            t_entry.creation_time = (uint32_t)t_creation_time;
            t_entry.modification_time = (uint32_t) t_modification_time;
            t_entry.access_time = (uint32_t) t_access_time;
            t_entry.backup_time = (uint32_t) t_backup_time;
            t_entry.user_id = (uint32_t) t_permissions -> userID;
            t_entry.group_id = (uint32_t) t_permissions -> groupID;
            t_entry.permissions = (uint32_t) t_permissions->mode & 0777;
            t_entry.file_creator = t_creator;
            t_entry.file_type = t_filetype;
            t_entry.is_folder = t_catalog_infos[t_i] . nodeFlags & kFSNodeIsDirectoryMask;
            
            p_callback(x_context, &t_entry);
            
            MCValueRelease(t_unicode_name);
        }
    } while(t_oserror != errFSNoMoreItems);
    
    FSCloseIterator(t_catalog_iterator);
    
    return true;
}

struct MCMacDesktop: public MCSystemInterface, public MCMacSystemService
{
	virtual bool Initialize(void)
    {
        IO_stdin = MCsystem -> OpenFd(0, kMCOpenFileModeRead);
        IO_stdout = MCsystem -> OpenFd(1, kMCOpenFileModeWrite);
        IO_stderr = MCsystem -> OpenFd(2, kMCOpenFileModeWrite);
        struct sigaction action;
        memset((char *)&action, 0, sizeof(action));
        action.sa_handler = handle_signal;
        action.sa_flags = SA_RESTART;
        sigaction(SIGHUP, &action, NULL);
        sigaction(SIGINT, &action, NULL);
        sigaction(SIGQUIT, &action, NULL);
        sigaction(SIGIOT, &action, NULL);
        sigaction(SIGPIPE, &action, NULL);
        sigaction(SIGALRM, &action, NULL);
        sigaction(SIGTERM, &action, NULL);
        sigaction(SIGUSR1, &action, NULL);
        sigaction(SIGUSR2, &action, NULL);
        sigaction(SIGFPE, &action, NULL);
        action.sa_flags |= SA_NOCLDSTOP;
        sigaction(SIGCHLD, &action, NULL);
        
        // MW-2009-01-29: [[ Bug 6410 ]] Make sure we cause the handlers to be reset to
        //   the OS default so CrashReporter will kick in.
        action.sa_flags = SA_RESETHAND;
        sigaction(SIGSEGV, &action, NULL);
        sigaction(SIGILL, &action, NULL);
        sigaction(SIGBUS, &action, NULL);
        
        MCValueAssign(MCshellcmd, MCSTR("/bin/sh"));
        
#ifndef _MAC_SERVER
        // MW-2010-05-11: Make sure if stdin is not a tty, then we set non-blocking.
        //   Without this you can't poll read when a slave process.
        if (!IsInteractiveConsole(0))
            MCS_mac_nodelay(0);
        
		// Internally, LiveCode assumes sorting orders etc are those of en_US.
		// Additionally, the "native" string encoding for Mac is MacRoman
		// (even though the BSD components of the system are likely UTF-8).
		const char *t_internal_locale = "en_US";
		setlocale(LC_ALL, "");
		setlocale(LC_CTYPE, t_internal_locale);
		setlocale(LC_COLLATE, t_internal_locale);
        
        _CurrentRuneLocale->__runetype[202] = _CurrentRuneLocale->__runetype[201];
        
        MCinfinity = HUGE_VAL;
        
        SInt32 t_major, t_minor, t_bugfix;
        if (Gestalt(gestaltSystemVersionMajor, &t_major) == noErr &&
            Gestalt(gestaltSystemVersionMinor, &t_minor) == noErr &&
            Gestalt(gestaltSystemVersionBugFix, &t_bugfix) == noErr)
        {
			MCmajorosversion = MCOSVersionMake(t_major, t_minor, t_bugfix);
        }
		
        MCaqua = True; // Move to MCScreenDC
        
        init_utf8_converters();
        //
        
        MCAutoStringRef dptr; // Check to see if this can ever happen anymore - if not, remove
        if (!GetCurrentFolder(&dptr))
			dptr = kMCEmptyString;
        if (MCStringGetLength(*dptr) <= 1)
        {
            // The current directory is the root dir, which normally indicates
            // that the application was launched through Finder. Get the path to
            // the main app bundle and use it as the current directory instead.
            CFBundleRef t_main_bundle = CFBundleGetMainBundle();
            
            // Get the path to the bundle
            CFURLRef t_bundle_url = CFBundleCopyBundleURL(t_main_bundle);
            CFStringRef t_fs_path = CFURLCopyFileSystemPath(t_bundle_url, kCFURLPOSIXPathStyle);
            
            // Change the current folder
            MCAutoStringRef t_path;
            if (MCStringCreateWithCFStringRef(t_fs_path, &t_path))
                /* UNCHECKED */ SetCurrentFolder(*t_path);
            
            CFRelease(t_bundle_url);
            CFRelease(t_fs_path);
        }
        
        MCS_reset_time();
        // END HERE
        
        SInt32 response;
        if (Gestalt('ICAp', &response) == noErr)
        {
            OSStatus err;
            ICInstance icinst;
            ICAttr icattr;
            err = ICStart(&icinst, 'MCRD');
            if (err == noErr)
            {
                Str255 proxystr;
                Boolean useproxy;
                
                long icsize = sizeof(useproxy);
                err = ICGetPref(icinst,  kICUseHTTPProxy, &icattr, &useproxy, &icsize);
                if (err == noErr && useproxy == True)
                {
                    icsize = sizeof(proxystr);
                    err = ICGetPref(icinst, kICHTTPProxyHost ,&icattr, proxystr, &icsize);
                    if (err == noErr)
                    {
                        /* UNCHECKED */ MCStringCreateWithBytes(proxystr+1, *proxystr, kMCStringEncodingMacRoman, false, MChttpproxy);
                    }
                }
                ICStop(icinst);
            }
        }
        
        // MW-2005-04-04: [[CoreImage]] Load in CoreImage extension
        extern void MCCoreImageRegister(void);
        MCCoreImageRegister();
        
        // END HERE
		
        if (!MCnoui)
        {
            setlinebuf(stdout);
            setlinebuf(stderr);
        }
#endif // _MAC_SERVER

        // Initialize our case mapping tables. We always use the MacRoman locale.
        CFStringRef t_raw;
        CFMutableStringRef t_lower, t_upper;
        CFIndex t_ignored;
        MCuppercasingtable = new (nothrow) uint8_t[256];
        MClowercasingtable = new (nothrow) uint8_t[256];
        for(uindex_t i = 0; i < 256; ++i)
            MCuppercasingtable[i] = uint8_t(i);
        t_raw = CFStringCreateWithBytes(NULL, MCuppercasingtable, 256, kCFStringEncodingMacRoman, false);
        t_lower = CFStringCreateMutableCopy(NULL, 0, t_raw);
        t_upper = CFStringCreateMutableCopy(NULL, 0, t_raw);
        CFStringLowercase(t_lower, NULL);
        CFStringUppercase(t_upper, NULL);
        CFStringGetBytes(t_lower, CFRangeMake(0, 256), kCFStringEncodingMacRoman, '?', false, MClowercasingtable, 256, &t_ignored);
        CFStringGetBytes(t_upper, CFRangeMake(0, 256), kCFStringEncodingMacRoman, '?', false, MCuppercasingtable, 256, &t_ignored);
        CFRelease(t_raw);
        CFRelease(t_lower);
        CFRelease(t_upper);
        
        return true;
    }
    
	virtual void Finalize(void)
    {
#ifndef _MAC_SERVER
        uint2 i;
        
        // Move To MCScreenDC
        // MW-2005-04-04: [[CoreImage]] Unload CoreImage extension
        extern void MCCoreImageUnregister(void);
        MCCoreImageUnregister();
        // End
        
        // ARM: Carbon UnicodeToTextInfo / TextToUnicodeInfo removed - no-op
        
        for (i = 0; i< osancomponents; i++)
        {
            MCValueRelease(osacomponents[i].compname);
            // ARM: compinstance is void* stub, no CloseComponent needed
        }
        delete[] osacomponents;
#endif
        // End
    }
	
    virtual MCServiceInterface *QueryService(MCServiceType p_type)
    {
        if (p_type == kMCServiceTypeMacSystem)
            return (MCMacSystemServiceInterface *)this;
        return nil;
    }
    
	virtual void Debug(MCStringRef p_string)
    {
		CFStringRef t_string;
		if (MCStringConvertToCFStringRef(p_string, t_string))
        {
			NSLog(@"%@", (__bridge NSString *)t_string);
            CFRelease(t_string);
        }
    }
	
	virtual real64_t GetCurrentTime(void)
    {
        struct timezone tz;
        struct timeval tv;
        
        gettimeofday(&tv, &tz);
        curtime = tv.tv_sec + (real8)tv.tv_usec / 1000000.0;
        
        return curtime;
    }
    
    virtual void ResetTime(void)
    {
        // Nothing
    }
    
	virtual bool GetVersion(MCStringRef& r_version)
    {
        SInt32 t_major, t_minor, t_bugfix;
        Gestalt(gestaltSystemVersionMajor, &t_major);
        Gestalt(gestaltSystemVersionMinor, &t_minor);
        Gestalt(gestaltSystemVersionBugFix, &t_bugfix);
        return MCStringFormat(r_version, "%d.%d.%d", t_major, t_minor, t_bugfix);
    }
	virtual bool GetMachine(MCStringRef& r_string)
    {
		// PM-2015-07-21: [[ Bug 15623 ]] machine() returns "unknown" in OSX because of Gestalt being deprecated
		size_t t_len = 0;
		sysctlbyname("hw.model", NULL, &t_len, NULL, 0);

		if (t_len)
		{
			char *t_model = new (nothrow) char[t_len];
			if (nil == t_model)
				return false;
			sysctlbyname("hw.model", t_model, &t_len, NULL, 0);

			if (!MCStringCreateWithCStringAndRelease(t_model, r_string))
			{
				free(t_model);
				return false;
			}
			return true;
		}

		return MCStringCopy(MCNameGetString(MCN_unknown), r_string); //in case model name can't be read
    }

    virtual bool GetAddress(MCStringRef& r_address)
    {
        static struct utsname u;
        uname(&u);
        return MCStringFormat(r_address, "%s:%@", u.nodename, MCcmd);
    }
    
	virtual uint32_t GetProcessId(void)
    {
        return getpid();
    }
	
	virtual void Alarm(real64_t p_when)
    {
    }
    
	virtual void Sleep(real64_t p_duration)
    {
        unsigned long finalTicks;
        Delay((unsigned long)p_duration * 60, &finalTicks);
    }
	
	virtual void SetEnv(MCStringRef p_name, MCStringRef p_value)
    {
        MCAutoStringRefAsUTF8String t_name, t_value;
        /* UNCHECKED */ t_name . Lock(p_name);
        
        if (p_value == NULL)
            unsetenv(*t_name);
        else
        {
            /* UNCHECKED */ t_value . Lock(p_value);
            setenv(*t_name, *t_value, True);
        }
    }
    
	virtual bool GetEnv(MCStringRef p_name, MCStringRef& r_value)
    {
        MCAutoStringRefAsUTF8String t_name;
        if (!t_name . Lock(p_name))
			return false;
        
        const char* t_env;
        t_env = getenv(*t_name);
        
        // We want to avoid returning something in case there was the environment variable
        // doesn't exist
        return t_env != nil && MCStringCreateWithBytes((byte_t*)t_env, strlen(t_env), kMCStringEncodingUTF8, false, r_value); //always returns NULL under CodeWarrier env.
    }
	
	virtual Boolean CreateFolder(MCStringRef p_path)
    {
        MCAutoStringRefAsUTF8String t_path;
        if (!t_path.Lock(p_path))
            return False;
        
        if (mkdir(*t_path, 0777) != 0)
            return False;
            
        return True;
    }
    
	virtual Boolean DeleteFolder(MCStringRef p_path)
    {
        MCAutoStringRefAsUTF8String t_path;
        if (!t_path.Lock(p_path))
            return False;
        
        if (rmdir(*t_path) != 0)
            return False;
        
        return True;
    }
    
//    /* LEGACY */
//    virtual bool DeleteFile(const char *p_path)
//    {
//        char *newpath = path2utf(MCS_resolvepath(p_path));
//        Boolean done = remove(newpath) == 0;
//        delete newpath;
//        return done;
//    }
	
	virtual Boolean DeleteFile(MCStringRef p_path)
    {
        MCAutoStringRefAsUTF8String t_path;
        if (!t_path.Lock(p_path))
            return False;
        
        if (remove(*t_path) != 0)
            return False;
        
        return True;
    }
	
	virtual Boolean RenameFileOrFolder(MCStringRef p_old_name, MCStringRef p_new_name)
    {
        MCAutoStringRefAsUTF8String t_old_name, t_new_name;
        
        if (!t_old_name.Lock(p_old_name) || !t_new_name.Lock(p_new_name))
            return False;
        
        if (rename(*t_old_name, *t_new_name) != 0)
            return False;
        
        return True;
    }
	
    // MW-2007-07-16: [[ Bug 5214 ]] Use rename instead of FSExchangeObjects since
    //   the latter isn't supported on all FS's.
    // MW-2007-12-12: [[ Bug 5674 ]] Unfortunately, just renaming the current stack
    //   causes all Finder meta-data to be lost, so what we will do is first try
    //   to FSExchangeObjects and if that fails, do a rename.
	virtual Boolean BackupFile(MCStringRef p_old_name, MCStringRef p_new_name)
    {
        bool t_error;
        t_error = false;
        
        FSRef t_src_ref;
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = MCS_mac_pathtoref(p_old_name, t_src_ref);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        FSRef t_dst_parent_ref;
        FSRef t_dst_ref;
        UniChar *t_dst_leaf;
        t_dst_leaf = NULL;
        UniCharCount t_dst_leaf_length;
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = MCS_mac_pathtoref(p_new_name, t_dst_ref);
            if (t_os_error == noErr)
                FSDeleteObject(&t_dst_ref);
			
            // Get the information to create the file
            t_os_error = MCS_mac_pathtoref_and_leaf(p_new_name, t_dst_parent_ref, t_dst_leaf, t_dst_leaf_length);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        // ARM: FSCatalogInfo/FSCreateFileUnicode unavailable.
        // Create the destination file with FSCreateFileUnicode stub, set type/creator via xattr.
        bool t_created_dst;
        t_created_dst = false;
        if (!t_error)
        {
            OSErr t_os_error;
            // Create file without FinderInfo catalog (kFSCatInfoNone)
            t_os_error = FSCreateFileUnicode(&t_dst_parent_ref, t_dst_leaf_length, t_dst_leaf,
                                             kFSCatInfoNone, NULL, &t_dst_ref, NULL);
            if (t_os_error == noErr)
                t_created_dst = true;
            else
                t_error = true;
        }
        
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = FSExchangeObjects(&t_src_ref, &t_dst_ref);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        if (t_error && t_created_dst)
            FSDeleteObject(&t_dst_ref);
        
        if (t_dst_leaf != NULL)
            delete t_dst_leaf;
		
        if (t_error)
            t_error = !RenameFileOrFolder(p_old_name, p_new_name);
		
        if (t_error)
            return False;
        
        return True;
    }
    
	virtual Boolean UnbackupFile(MCStringRef p_old_name, MCStringRef p_new_name)
    {
        bool t_error;
        t_error = false;
        
        FSRef t_src_ref;
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = MCS_mac_pathtoref(p_old_name, t_src_ref);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        FSRef t_dst_ref;
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = MCS_mac_pathtoref(p_new_name, t_dst_ref);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        // It appears that the source file here is the ~file, the backup file.
        // So copy it over to p_dst_path, and delete it.
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = FSExchangeObjects(&t_src_ref, &t_dst_ref);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        if (!t_error)
        {
            OSErr t_os_error;
            t_os_error = FSDeleteObject(&t_src_ref);
            if (t_os_error != noErr)
                t_error = true;
        }
        
        if (t_error)
            t_error = !RenameFileOrFolder(p_old_name, p_new_name);
		
        if (t_error)
            return False;
        
        return True;
    }
	
	virtual Boolean CreateAlias(MCStringRef p_target, MCStringRef p_alias)
    {
        // ARM replacement: use NSFileManager symbolic link instead of Carbon resource-fork alias.
        // HFS+ alias files with resource forks are not creatable on arm64 without Carbon.
        // Symlinks are functionally equivalent for LiveCode's use of aliases.
        MCAutoStringRefAsUTF8String t_src_utf8, t_dst_utf8;
        if (!t_src_utf8.Lock(p_target) || !t_dst_utf8.Lock(p_alias))
            return False;

        // Destination must not already exist
        struct stat t_stat;
        if (lstat(*t_dst_utf8, &t_stat) == 0)
            return False;

        if (symlink(*t_src_utf8, *t_dst_utf8) != 0)
            return False;

        return True;
    }
    
	// NOTE: 'ResolveAlias' returns a standard (not native) path.
	virtual Boolean ResolveAlias(MCStringRef p_target, MCStringRef& r_resolved_path)
    {
        FSRef t_fsref;
        
        OSErr t_os_error;
        t_os_error = MCS_mac_pathtoref(p_target, t_fsref);
        if (t_os_error != noErr)
        {
            MCresult -> sets("file not found");
            return False;
        }
        
        Boolean t_is_folder;
        Boolean t_is_alias;
        
        t_os_error = FSResolveAliasFile(&t_fsref, TRUE, &t_is_folder, &t_is_alias);
        if (t_os_error != noErr || !t_is_alias) // this always seems to be false
        {
            MCresult -> sets("can't get alias");
            return False;
        }
        
        if (!MCS_mac_fsref_to_path(t_fsref, r_resolved_path))
        {
            MCresult -> sets("can't get alias path");
            return False;
        }
        
        return True;
    }
	
	virtual bool GetCurrentFolder(MCStringRef& r_path)
    {
		char *t_cwd_sys;
		errno = 0;
		t_cwd_sys = getcwd (NULL, 0);
		
		if (NULL == t_cwd_sys)
		{
			// TODO: Report errno appropriately.
			return false;
		}
		
		if (!MCStringCreateWithBytesAndRelease((byte_t*)t_cwd_sys, strlen(t_cwd_sys), kMCStringEncodingUTF8, false, r_path))
		{
			free(t_cwd_sys);
            return false;
		}
		
        return true;
    }
    
    // MW-2006-04-07: Bug 3201 - MCS_resolvepath returns NULL if unable to find a ~<username> folder.
	virtual Boolean SetCurrentFolder(MCStringRef p_path)
    {
        MCAutoStringRefAsUTF8String t_utf8_string;
        if (!t_utf8_string.Lock(p_path))
            return False;
        
        if (chdir(*t_utf8_string) != 0)
            return False;
        
        return True;
    }
	
	// NOTE: 'GetStandardFolder' returns a standard (not native) path.
	virtual Boolean GetStandardFolder(MCNameRef p_type, MCStringRef& r_folder)
    {
        uint32_t t_mac_folder = 0;
        OSType t_domain = kOnAppropriateDisk;
        bool t_found_folder = false;
        
        
        // SN-2014-08-08: [[ Bug 13026 ]] Fix ported from 6.7
        if (MCNameIsEqualToCaseless(p_type, MCN_engine)
                // SN-2015-04-20: [[ Bug 14295 ]] If we are here, we are a standalone
                // so the resources folder is the redirected engine folder
                || MCNameIsEqualToCaseless(p_type, MCN_resources))
        {
            MCAutoStringRef t_engine_folder;
            uindex_t t_last_slash;
            
            if (!MCStringLastIndexOfChar(MCcmd, '/', UINDEX_MAX, kMCStringOptionCompareExact, t_last_slash))
                t_last_slash = MCStringGetLength(MCcmd);

            if (!MCStringCopySubstring(MCcmd, MCRangeMake(0, t_last_slash), &t_engine_folder))
                return False;

            if (MCNameIsEqualToCaseless(p_type, MCN_resources))
            {
                if (!MCS_apply_redirect(*t_engine_folder, false, r_folder))
                    return False;
            }
            else
                r_folder = MCValueRetain(*t_engine_folder);

            return True;
        }
        
        if (MCS_mac_specialfolder_to_mac_folder(MCNameGetString(p_type), t_mac_folder, t_domain))
            t_found_folder = true;
        else if (MCStringGetLength(MCNameGetString(p_type)) == 4 &&
                 MCStringIsNative(MCNameGetString(p_type)))
        {
            t_mac_folder = MCSwapInt32NetworkToHost(*((uint32_t*)MCStringGetNativeCharPtr(MCNameGetString(p_type))));
            t_domain = kOnAppropriateDisk;
            t_found_folder = true;
        }
        
        FSRef t_folder_ref;
        if (t_found_folder)
        {
            OSErr t_os_error;
            Boolean t_create_folder;
            t_create_folder = t_domain == kUserDomain ? kCreateFolder : kDontCreateFolder;
            t_os_error = FSFindFolder(t_domain, t_mac_folder, t_create_folder, &t_folder_ref);
            t_found_folder = t_os_error == noErr;
        }
        
        if (!t_found_folder)
        {
            r_folder = MCValueRetain(kMCEmptyString);
            return True;
        }
		
        if (!MCS_mac_fsref_to_path(t_folder_ref, r_folder))
            return False;
        
        return True;
    }
	
	virtual Boolean FileExists(MCStringRef p_path)
    {
        if (MCStringGetLength(p_path) == 0)
            return False;
        
        // SN-2015-01-05: [[ Bug 14043 ]] Apply the fix to MCS_exists
        MCAutoStringRef t_redirected;
        if (!MCS_apply_redirect(p_path, true, &t_redirected))
            t_redirected = p_path;
        
        MCAutoStringRefAsUTF8String t_utf8_path;
        if (!t_utf8_path.Lock(*t_redirected))
            return False;
        
        bool t_found;
        struct stat buf;
        t_found = stat(*t_utf8_path, (struct stat *)&buf) == 0;
        if (t_found)
            t_found = !S_ISDIR(buf.st_mode);
        
        if (!t_found)
            return False;
        
        return True;
    }
    
	virtual Boolean FolderExists(MCStringRef p_path)
    {
        if (MCStringGetLength(p_path) == 0)
            return False;
        
        MCAutoStringRefAsUTF8String t_utf8_path;
        if (!t_utf8_path.Lock(p_path))
            return False;
        
        bool t_found;
        struct stat buf;
        t_found = stat(*t_utf8_path, (struct stat *)&buf) == 0;
        if (t_found)
            t_found = S_ISDIR(buf.st_mode);
        
        if (!t_found)
            return False;
        
        return True;
    }
    
	virtual Boolean FileNotAccessible(MCStringRef p_path)
    {
        return False;
    }
	
	virtual Boolean ChangePermissions(MCStringRef p_path, uint2 p_mask)
    {
        return True;
    }
    
	virtual uint2 UMask(uint2 p_mask)
    {
        return umask(p_mask);
    }
	
	// NOTE: 'GetTemporaryFileName' returns a standard (not native) path.
	virtual bool GetTemporaryFileName(MCStringRef& r_tmp_name)
    {
        bool t_success = true;
        MCAutoStringRef t_temp_file_auto;
        FSRef t_folder_ref;
        char* t_temp_file_chars;
        
        t_temp_file_chars = nil;        
        
        if (t_success && FSFindFolder(kOnSystemDisk, kTemporaryFolderType, TRUE, &t_folder_ref) == noErr)
        {
            MCAutoStringRef t_path;
            int t_fd;
            t_success = MCS_mac_fsref_to_path(t_folder_ref, &t_path);
            
            if (t_success)
                t_success = MCStringFormat(&t_temp_file_auto, "%@/tmp.%d.XXXXXXXX", *t_path, getpid());
            
            if (t_success)
            {
                MCAutoPointer<char> temp;
                /* UNCHECKED */ MCStringConvertToCString(*t_temp_file_auto, &temp);
                t_success = MCMemoryAllocateCopy(*temp, strlen(*temp) + 1, t_temp_file_chars);
                
            }
            
            if (t_success)
            {
                t_fd = mkstemp(t_temp_file_chars);
                t_success = t_fd != -1;
            }
            
            if (t_success)
            {
                close(t_fd);
                t_success = unlink(t_temp_file_chars) == 0;
            }
        }
        
        if (t_success)
            t_success = MCStringCreateWithCString(t_temp_file_chars, r_tmp_name);
        
        if (!t_success)
            r_tmp_name = MCValueRetain(kMCEmptyString);
        
        MCMemoryDeallocate(t_temp_file_chars);
        
        return t_success;
    }
    
#define CATALOG_MAX_ENTRIES 16
	virtual bool ListFolderEntries(MCStringRef p_folder, MCSystemListFolderEntriesCallback p_callback, void *x_context)
    {
        
        MCAutoStringRef t_path, t_redirect;
        bool t_success;
        t_success = true;
		if (p_folder == nil)
			MCS_getcurdir(&t_path);
		else
			&t_path = MCValueRetain (p_folder);
        // MW-2014-09-17: [[ Bug 13455 ]] First list in the usual path.
        t_success = MCS_getentries_for_folder(*t_path, p_callback, x_context);
        
        bool *t_files = (bool *)x_context;
        // MW-2014-09-17: [[ Bug 13455 ]] If we are fetching files, and the path is inside MacOS, then
        //   merge the list with files from the corresponding path in Resources/_MacOS.
        // NOTE: the overall operation should still succeed if the redirect doesn't exist
        if (t_success && *t_files &&
            MCS_apply_redirect(*t_path, false, &t_redirect))
            t_success = MCS_getentries_for_folder(*t_redirect, p_callback, x_context) || t_success;
        
        return t_success;
    }
    
    virtual real8 GetFreeDiskSpace()
    {
        char t_defaultfolder[PATH_MAX + 1];
        getcwd(t_defaultfolder, PATH_MAX);
        
        // ARM replacement: FSCatalogInfo / FSGetVolumeInfo replaced with statfs()
        // ARM replacement: FSCatalogInfo / FSGetCatalogInfo / FSGetVolumeInfo unavailable.
        // Use statfs() to get free disk space instead.
        struct statfs t_sfs;
        real8 t_free_space = 0.;
        if (statfs(t_defaultfolder, &t_sfs) == 0)
            t_free_space = (real8)t_sfs.f_bavail * (real8)t_sfs.f_bsize;
		
        return t_free_space;
    }
    
    virtual Boolean GetDevices(MCStringRef& r_devices)
    {
        MCAutoListRef t_list;
        io_iterator_t SerialPortIterator = 0;
        mach_port_t masterPort = 0;
        io_object_t thePort;
        if (FindSerialPortDevices(&SerialPortIterator, &masterPort) != KERN_SUCCESS)
        {
            char *buffer = new (nothrow) char[6 + I2L];
            sprintf(buffer, "error %d", errno);
            MCresult->copysvalue(buffer);
            delete[] buffer;
            return false;
        }
        if (!MCListCreateMutable('\n', &t_list))
            return false;
        
        uint2 portCount = 0;
        
        bool t_success = true;
        if (SerialPortIterator != 0)
        {
            while (t_success && (thePort = IOIteratorNext(SerialPortIterator)) != 0)
            {
                char ioresultbuffer[256];
                
                MCAutoListRef t_result_list;
                MCAutoStringRef t_result_string;
                
                t_success = MCListCreateMutable(',', &t_result_list);
                
                if (t_success)
                {
                    getIOKitProp(thePort, kIOTTYDeviceKey, ioresultbuffer, sizeof(ioresultbuffer));
                    t_success = MCListAppendCString(*t_result_list, ioresultbuffer);//name
                }
                if (t_success)
                {
                    getIOKitProp(thePort, kIODialinDeviceKey, ioresultbuffer, sizeof(ioresultbuffer));
                    t_success = MCListAppendCString(*t_result_list, ioresultbuffer);//TTY file
                }
                if (t_success)
                {
                    getIOKitProp(thePort, kIOCalloutDeviceKey, ioresultbuffer, sizeof(ioresultbuffer));
                    t_success = MCListAppendCString(*t_result_list, ioresultbuffer);//TTY file
                }
                
                if (t_success)
                    t_success = MCListCopyAsString(*t_result_list, &t_result_string);
                
                if (t_success)
                    t_success = MCListAppend(*t_list, *t_result_string);
                
                IOObjectRelease(thePort);
                portCount++;
            }
            IOObjectRelease(SerialPortIterator);
        }
        
        if (t_success && MCListCopyAsString(*t_list, r_devices))
            return True;
        
        return False;
    }
    
    virtual Boolean GetDrives(MCStringRef& r_drives)
    {
        MCAutoListRef t_list;
        if (!MCListCreateMutable('\n', &t_list))
            return false;
        
        OSErr t_err;
        ItemCount t_index;
        bool t_first;
        
        t_index = 1;
        t_err = noErr;
        t_first = true;
        
        // To list all the mounted volumes on the system we use the FSGetVolumeInfo
        // API with first parameter kFSInvalidVolumeRefNum and an index in the
        // second parameter.
        // This call will return nsvErr when it reaches the end of the list of
        // volumes, other errors being returned if there's a problem getting the
        // information.
        // Due to this, it is perfectly possible that the first index will not be
        // the first volume we put into the list - so we need a boolean flag (t_first)
        while(t_err != nsvErr)
        {
            HFSUniStr255 t_unicode_name;
            t_err = FSGetVolumeInfo(kFSInvalidVolumeRefNum, t_index, NULL, kFSVolInfoNone, NULL, &t_unicode_name, NULL);
            if (t_err == noErr)
            {
                MCAutoStringRef t_volume_name;
                if (!MCStringCreateWithChars(t_unicode_name . unicode, t_unicode_name . length, &t_volume_name))
                    return false;
                if (!MCListAppend(*t_list, *t_volume_name))
                    return false;
            }
            t_index += 1;
        }
        
        return MCListCopyAsString(*t_list, r_drives) ? True : False;
    }

	bool PathToNative(MCStringRef p_path, MCStringRef& r_native)
	{
        return MCStringCopy(p_path, r_native);
	}
	
	bool PathFromNative(MCStringRef p_native, MCStringRef& r_path)
	{
        return MCStringCopy(p_native, r_path);
	}
    
	virtual bool ResolvePath(MCStringRef p_path, MCStringRef& r_resolved_path)
    {
        if (MCStringGetLength(p_path) == 0)
            return GetCurrentFolder(r_resolved_path);
        
        MCAutoStringRef t_tilde_path;
        if (MCStringGetCharAtIndex(p_path, 0) == '~')
        {
            uindex_t t_user_end;
            if (!MCStringFirstIndexOfChar(p_path, '/', 0, kMCStringOptionCompareExact, t_user_end))
                t_user_end = MCStringGetLength(p_path);
            
            // Prepend user name
            struct passwd *t_password;
            if (t_user_end == 1)
                t_password = getpwuid(getuid());
            else
            {
                MCAutoStringRef t_username;
                if (!MCStringCopySubstring(p_path, MCRangeMakeMinMax(1, t_user_end), &t_username))
                    return false;
                MCAutoStringRefAsUTF8String t_utf8_username;
                /* UNCHECKED */ t_utf8_username . Lock(*t_username);
                t_password = getpwnam(*t_utf8_username);
            }
            
            if (t_password != NULL)
            {
                if (!MCStringCreateMutable(0, &t_tilde_path) ||
                    !MCStringAppendNativeChars(*t_tilde_path, (char_t*)t_password->pw_dir, MCCStringLength(t_password->pw_dir)) ||
                    !MCStringAppendSubstring(*t_tilde_path, p_path, MCRangeMakeMinMax(t_user_end, MCStringGetLength(p_path))))
                    return false;
            }
            else
                t_tilde_path = p_path;
        }
        else
            t_tilde_path = p_path;
        
        MCAutoStringRef t_fullpath;
        if (MCStringGetCharAtIndex(*t_tilde_path, 0) != '/')
        {
            MCAutoStringRef t_folder;
            if (!GetCurrentFolder(&t_folder))
				t_folder = kMCEmptyString;
            
            MCAutoStringRef t_resolved;
            if (!MCStringMutableCopy(*t_folder, &t_fullpath) ||
                !MCStringAppendChar(*t_fullpath, '/') ||
                !MCStringAppend(*t_fullpath, *t_tilde_path))
                return false;
        }
        else
            t_fullpath = *t_tilde_path;
        
        if (!MCS_mac_is_link(*t_fullpath))
            return MCStringCopy(*t_fullpath, r_resolved_path);
        
        // SN-2015-06-08: [[ Bug 15432 ]] Use realpath to solve the symlink
        MCAutoStringRefAsUTF8String t_utf8_path;
        bool t_success;
        t_success = true;

        if (t_success)
            t_success = t_utf8_path . Lock(*t_fullpath);

        char *t_resolved_path;

        t_resolved_path = realpath(*t_utf8_path, NULL);

        // If the does not exist, then realpath will fail: we want to
        // return something though, so we keep the input path (as it
        // is done for desktop).
        if (t_resolved_path != NULL)
            t_success = MCStringCreateWithBytes((const byte_t*)t_resolved_path, strlen(t_resolved_path), kMCStringEncodingUTF8, false, r_resolved_path);
        else
            t_success = false;

        MCMemoryDelete(t_resolved_path);

        return t_success;
    }
	
    virtual IO_handle DeployOpen(MCStringRef p_path, intenum_t p_mode)
    {
        if (p_mode != kMCOpenFileModeCreate)
            return OpenFile(p_path, p_mode, False);
        
        FILE *fptr;
        IO_handle t_handle;
        t_handle = NULL;
        
        MCAutoStringRefAsUTF8String t_path_utf;
        if (!t_path_utf.Lock(p_path))
            return NULL;
        
        fptr = fopen(*t_path_utf, IO_CREATE_MODE);

        if (fptr != nil)
            t_handle = new (nothrow) MCStdioFileHandle(fptr);
        
        return t_handle;
    }
    
	virtual IO_handle OpenFile(MCStringRef p_path, intenum_t p_mode, Boolean p_map)
    {
		FILE *fptr;
        IO_handle t_handle;
        t_handle = NULL;
		//opening regular files
		//set the file type and it's creator. These are 2 global variables
        
        MCAutoStringRef t_redirected;
        if (p_mode != kMCOpenFileModeRead || !MCS_apply_redirect(p_path, true, &t_redirected))
            t_redirected = p_path;
        
        
        MCAutoStringRefAsUTF8String t_path_utf;
        if (!t_path_utf.Lock(*t_redirected))
            return NULL;
        
        //////////
        // Copied from Linuxdesktop::OpenFile.
        // Using a memory mapped file now also possible for Mac
        if (p_map && MCmmap && p_mode == kMCOpenFileModeRead)
        {
            int t_fd = open(*t_path_utf, O_RDONLY);
            struct stat t_buf;
            if (t_fd != -1 && !fstat(t_fd, &t_buf))
            {
				// The length of a file could be > 32-bit, so we have to check that
				// the file size fits into a 32-bit integer as that is what mmap expects.
                off_t t_len = t_buf.st_size;
                if (t_len != 0 && t_len < UINT32_MAX)
                {
                    char *t_buffer = (char *)mmap(NULL, t_len, PROT_READ, MAP_SHARED,
                                                  t_fd, 0);
                    // MW-2013-05-02: [[ x64 ]] Make sure we use the MAP_FAILED constant
                    //   rather than '-1'.
                    if (t_buffer != MAP_FAILED)
                    {
                        t_handle = new (nothrow) MCMemoryMappedFileHandle(t_fd, t_buffer, t_len);
                        return t_handle;
                    }
                }
                close(t_fd);
            }
        }
        //
        //////////
        
        fptr = fopen(*t_path_utf, IO_READ_MODE);
        
        Boolean created = True;
        
        if (fptr != NULL)
        {
            created = False;
            if (p_mode != kMCOpenFileModeRead)
            {
                fclose(fptr);
                fptr = NULL;
            }
        }
        
        if (fptr == NULL)
        {
            switch(p_mode)
            {
                case kMCOpenFileModeRead:
                    fptr = fopen(*t_path_utf, IO_READ_MODE);
                    break;
                case kMCOpenFileModeUpdate:
                    fptr = fopen(*t_path_utf, IO_UPDATE_MODE);
                    break;
                case kMCOpenFileModeAppend:
                    fptr = fopen(*t_path_utf, IO_APPEND_MODE);
                    break;
                case kMCOpenFileModeWrite:
                    fptr = fopen(*t_path_utf, IO_WRITE_MODE);
                    break;
                default:
                    fptr = NULL;
            }
        }
        
        if (fptr == NULL && p_mode != kMCOpenFileModeRead)
            fptr = fopen(*t_path_utf, IO_CREATE_MODE);
        
        if (fptr != NULL && created)
            MCS_mac_setfiletype(p_path);
        
		if (fptr != NULL)
            t_handle = new (nothrow) MCStdioFileHandle(fptr);
        
        return t_handle;
    }
    
	virtual IO_handle OpenFd(uint32_t p_fd, intenum_t p_mode)
    {
		FILE *t_stream;
        t_stream = NULL;
        
        switch (p_mode)
        {
            case kMCOpenFileModeAppend:
                t_stream = fdopen(p_fd, IO_APPEND_MODE);
                break;
            case kMCOpenFileModeRead:
                t_stream = fdopen(p_fd, IO_READ_MODE);
                break;
            case kMCOpenFileModeUpdate:
                t_stream = fdopen(p_fd, IO_UPDATE_MODE);
                break;
            case kMCOpenFileModeWrite:
                t_stream = fdopen(p_fd, IO_WRITE_MODE);
                break;
            default:
                break;
        }
        
		if (t_stream == NULL)
			return NULL;
		
		// MH-2007-05-17: [[Bug 3196]] Opening the write pipe to a process should not be buffered.
        if (p_mode == kMCOpenFileModeWrite)
			setvbuf(t_stream, NULL, _IONBF, 0);
		
		IO_handle t_handle;
		t_handle = new (nothrow) MCStdioFileHandle(t_stream);
		
		return t_handle;
    }
    
	virtual IO_handle OpenDevice(MCStringRef p_path, intenum_t p_mode)
    {
		FILE *fptr;
        
        IO_handle t_handle;
        t_handle = NULL;
		//opening regular files
		//set the file type and it's creator. These are 2 global variables
        
        MCAutoStringRefAsUTF8String t_path_utf;
        if (!t_path_utf.Lock(p_path))
            return NULL;
        
        // SN-2014-05-02 [[ Bug 12246 ]] Enable an opening mode different from IO_READ...
        switch (p_mode)
        {
            case kMCOpenFileModeAppend:
                fptr = fopen(*t_path_utf, IO_APPEND_MODE);
                break;
            case kMCOpenFileModeRead:
                fptr = fopen(*t_path_utf, IO_READ_MODE);
                break;
            case kMCOpenFileModeUpdate:
                fptr = fopen(*t_path_utf, IO_UPDATE_MODE);
                break;
            case kMCOpenFileModeWrite:
                fptr = fopen(*t_path_utf, IO_WRITE_MODE);
                break;
            default:
                fptr = NULL;
                break;
        }
        
		if (fptr != NULL)
        {
            setbuf(fptr, nullptr);
            int val;
            int t_serial_in;
            
            t_serial_in = fileno(fptr);
            val = fcntl(t_serial_in, F_GETFL);
            val |= O_NONBLOCK |  O_NOCTTY;
            fcntl(t_serial_in, F_SETFL, val);
            configureSerialPort((short)t_serial_in);
            
            // SN-2014-05-02 [[ Bug 12246 ]] Serial I/O fails on write
            // The serial port number is never used in the 6.X engine... and switching to an STDIO file
            // is enough to have the serial devices working perfectly.
            t_handle = new (nothrow) MCStdioFileHandle(fptr, true);
        }
        
        return t_handle;
    }
	
	virtual bool LongFilePath(MCStringRef p_path, MCStringRef& r_long_path)
    {
        return MCStringCopy(p_path, r_long_path);
    }
    
	virtual bool ShortFilePath(MCStringRef p_path, MCStringRef& r_short_path)
    {
        return MCStringCopy(p_path, r_short_path);
    }
    
	virtual uint32_t TextConvert(const void *p_string, uint32_t p_string_length, void *r_buffer, uint32_t p_buffer_length, uint32_t p_from_charset, uint32_t p_to_charset)
    {
        uint32_t t_return_size;
        t_return_size = 0;
        if (p_from_charset == LCH_UNICODE) // Unicode to multibyte
        {
//            TextConvert(const void *p_string, uint32_t p_string_length, void *r_buffer, uint32_t p_buffer_length, uint32_t p_from_charset, uint32_t p_to_charset)
            //            (const char *s, uint4 len, char *d, uint4 destbufferlength, uint4 &destlen, uint1 charset)
            char* t_dest_ptr = (char*) r_buffer;
            char* t_src_ptr = (char*)p_string;
            // ARM replacement: delegate to CFString-based MCS_unicodetomultibyte
            if (!p_buffer_length)
            {
                return p_string_length; // conservative upper bound
            }
            uint4 t_dest_len = 0;
            MCS_unicodetomultibyte((const char *)p_string, p_string_length,
                                   (char *)r_buffer, p_buffer_length,
                                   t_dest_len, p_to_charset);
            t_return_size = t_dest_len;
        }
        else if (p_to_charset == LCH_UNICODE) // Multibyte to unicode
        {
//            (const char *s, uint4 len, char *d, uint4 destbufferlength, uint4 &destlen, uint1 charset)
            // ARM replacement: delegate to CFString-based MCS_multibytetounicode
            if (!p_buffer_length)
            {
                return p_string_length << 1;
            }
            uint4 t_dest_len2 = 0;
            MCS_multibytetounicode((const char *)p_string, p_string_length,
                                   (char *)r_buffer, p_buffer_length,
                                   t_dest_len2, p_from_charset);
            t_return_size = t_dest_len2;
        }
        return t_return_size;
    }
    
	virtual bool TextConvertToUnicode(uint32_t p_input_encoding, const void *p_input, uint4 p_input_length, void *p_output, uint4& p_output_length, uint4& r_used)
    {
        if (p_input_length == 0)
        {
            r_used = 0;
            return true;
        }
        
        int4 t_encoding;
        t_encoding = -1;
        
        if (p_input_encoding >= kMCTextEncodingWindowsNative)
        {
            struct { uint4 codepage; int4 encoding; } s_codepage_map[] =
            {
                {437, kTextEncodingDOSLatinUS },
                {850, kTextEncodingDOSLatinUS },
                {932, kTextEncodingDOSJapanese },
                {949, kTextEncodingDOSKorean },
                {1361, kTextEncodingWindowsKoreanJohab },
                {936, kTextEncodingDOSChineseSimplif },
                {950, kTextEncodingDOSChineseTrad },
                {1253, kTextEncodingWindowsGreek },
                {1254, kTextEncodingWindowsLatin5 },
                {1258, kTextEncodingWindowsVietnamese },
                {1255, kTextEncodingWindowsHebrew },
                {1256, kTextEncodingWindowsArabic },
                {1257, kTextEncodingWindowsBalticRim },
                {1251, kTextEncodingWindowsCyrillic },
                {874, kTextEncodingDOSThai },
                {1250, kTextEncodingWindowsLatin2 },
                {1252, kTextEncodingWindowsLatin1 }
            };
            
            for(uint4 i = 0; i < sizeof(s_codepage_map) / sizeof(s_codepage_map[0]); ++i)
                if (s_codepage_map[i] . codepage == p_input_encoding - kMCTextEncodingWindowsNative)
                {
                    t_encoding = s_codepage_map[i] . encoding;
                    break;
                }
			
            // MW-2008-03-24: [[ Bug 6187 ]] RTF parser doesn't like ansicpg1000
            if (t_encoding == -1 && (p_input_encoding - kMCTextEncodingWindowsNative >= 10000))
                t_encoding = p_input_encoding - kMCTextEncodingWindowsNative - 10000;
			
        }
        else if (p_input_encoding >= kMCTextEncodingMacNative)
            t_encoding = p_input_encoding - kMCTextEncodingMacNative;
        
        // ARM replacement: use CFString for legacy encoding → Unicode
        CFStringEncoding t_cf_enc = (CFStringEncoding)t_encoding;
        CFStringRef t_cf_str = CFStringCreateWithBytes(kCFAllocatorDefault,
            (const UInt8 *)p_input, (CFIndex)p_input_length, t_cf_enc, false);
        if (t_cf_str == NULL)
        {
            r_used = 0;
            return true;
        }
        CFIndex t_char_count = CFStringGetLength(t_cf_str);
        CFIndex t_byte_count = t_char_count * (CFIndex)sizeof(UniChar);
        if (t_byte_count > (CFIndex)p_output_length)
            t_byte_count = (CFIndex)p_output_length;
        CFStringGetCharacters(t_cf_str,
            CFRangeMake(0, t_byte_count / (CFIndex)sizeof(UniChar)),
            (UniChar *)p_output);
        CFRelease(t_cf_str);
        r_used = (uint4)t_byte_count;
        
        return true;
    }
    
    virtual void CheckProcesses(void)
    {
        uint2 i;
        int wstat;
        for (i = 0 ; i < MCnprocesses ; i++)
            if (MCprocesses[i].pid != 0 && MCprocesses[i].pid != -1
		        && waitpid(MCprocesses[i].pid, &wstat, WNOHANG) > 0)
            {
                if (MCprocesses[i].ihandle != NULL)
                {
                    MCStdioFileHandle *t_handle;
                    t_handle = static_cast<MCStdioFileHandle *>(MCprocesses[i].ihandle);
                    clearerr(t_handle -> GetStream());
                }
                MCprocesses[i].pid = 0;
                MCprocesses[i].retcode = WEXITSTATUS(wstat);
            }
    }
    
    virtual uint32_t GetSystemError(void)
    {
        return errno;
    }

    virtual bool Shell(MCStringRef p_command, MCDataRef& r_data, int& r_retcode)
    {
        IO_cleanprocesses();
        int tochild[2];
        int toparent[2];
        int4 index = MCnprocesses;

        if (pipe(tochild) == 0)
        {
            if (pipe(toparent) == 0)
            {
                MCU_realloc((char **)&MCprocesses, MCnprocesses,
                            MCnprocesses + 1, sizeof(Streamnode));
                MCprocesses[MCnprocesses].name = (MCNameRef)MCValueRetain(MCM_shell);
                MCprocesses[MCnprocesses].mode = OM_NEITHER;
                MCprocesses[MCnprocesses].ohandle = NULL;
                MCprocesses[MCnprocesses].ihandle = NULL;
                if ((MCprocesses[MCnprocesses++].pid = fork()) == 0)
                {
                    // [[ Bug 13622 ]] Make sure environ is appropriate (on Yosemite it can
                    //    be borked).
                    environ = fix_environ();
                    
                    close(tochild[1]);
                    close(0);
                    dup(tochild[0]);
                    close(tochild[0]);
                    close(toparent[0]);
                    close(1);
                    dup(toparent[1]);
                    close(2);
                    dup(toparent[1]);
                    close(toparent[1]);
                    MCAutoStringRefAsUTF8String t_shellcmd;
                    /* UNCHECKED */ t_shellcmd . Lock(MCshellcmd);
                    
                    // Use execl and pass our new environ through to it.
                    execl(*t_shellcmd, *t_shellcmd, "-s", NULL);
                    _exit(-1);
                }
                CheckProcesses();
                close(tochild[0]);
                
                MCAutoStringRefAsUTF8String t_utf_path;
                /* UNCHECKED */ t_utf_path . Lock(p_command);
                write(tochild[1], *t_utf_path, t_utf_path . Size());

                write(tochild[1], "\n", 1);
                close(tochild[1]);
                close(toparent[1]);
                MCS_mac_nodelay(toparent[0]);
                if (MCprocesses[index].pid == -1)
                {
                    if (MCprocesses[index].pid > 0)
                        Kill(MCprocesses[index].pid, SIGKILL);
                    MCprocesses[index].pid = 0;
                    MCeerror->add(EE_SHELL_BADCOMMAND, 0, 0, p_command);
                    return false;

                }
            }
            else
            {
                close(tochild[0]);
                close(tochild[1]);
                MCeerror->add(EE_SHELL_BADCOMMAND, 0, 0, p_command);
                return false;
            }
        }
        else
        {
            MCeerror->add(EE_SHELL_BADCOMMAND, 0, 0, p_command);
            return false;
        }
        char *buffer;
        uint4 buffersize;
        buffer = (char *)malloc(4096);
        buffersize = 4096;
        uint4 size = 0;
        if (MCS_mac_shellread(toparent[0], buffer, buffersize, size) != IO_NORMAL)
        {
            MCeerror->add(EE_SHELL_ABORT, 0, 0);
            close(toparent[0]);
            if (MCprocesses[index].pid != 0)
                Kill(MCprocesses[index].pid, SIGKILL);
            
            // SN-2015-07-15: [[ Bug 15592 ]] Do not copy the buffer as we want
            //  to take ownership of it - and ensure to free it in any case.
            if (!MCDataCreateWithBytesAndRelease((char_t*)buffer, size, r_data))
                free(buffer);

            return false;
        }
        // SN-2015-07-15: [[ Bug 15592 ]] Do not copy the buffer as we want
        //  to take ownership of it - and ensure to free it in any case.
        if (!MCDataCreateWithBytesAndRelease((char_t*)buffer, size, r_data))
            free(buffer);

        close(toparent[0]);
        CheckProcesses();
        if (MCprocesses[index].pid != 0)
        {
            uint2 count = SHELL_COUNT;
            while (count--)
            {
                if (MCscreen->wait(SHELL_INTERVAL, False, False))
                {
                    if (MCprocesses[index].pid != 0)
                        Kill(MCprocesses[index].pid, SIGKILL);
                    // SN-2015-01-29: [[ Bug 14462 ]] Should return a boolean
                    return false;
                }
                if (MCprocesses[index].pid == 0)
                    break;
            }
            if (MCprocesses[index].pid != 0)
            {
                MCprocesses[index].retcode = -1;
                Kill(MCprocesses[index].pid, SIGKILL);
            }
        }
        
        r_retcode = MCprocesses[index].retcode;        
        
        return true;
    }
    
    virtual bool StartProcess(MCNameRef p_name, MCStringRef p_doc, intenum_t p_mode, Boolean p_elevated)
    {
        // SN-2014-04-22 [[ Bug 11979 ]] IDE fails to launch when installed on a Unicode path
        // p_doc might be empty when startprocess_launch is targetted
        if (MCStringEndsWithCString(MCNameGetString(p_name), (const char_t *)".app", kMCStringOptionCompareCaseless) || (p_doc != nil))
            MCS_startprocess_launch(p_name, p_doc, (Open_mode)p_mode);
        else
            MCS_startprocess_unix(p_name, kMCEmptyString, (Open_mode)p_mode, p_elevated);
        
        return true;
    }
    
    virtual bool ProcessTypeIsForeground(void)
    {
        ProcessSerialNumber t_psn = { 0, kCurrentProcess };
        
        CFDictionaryRef t_info;
        t_info = ProcessInformationCopyDictionary(&t_psn, kProcessDictionaryIncludeAllInformationMask);
        
        bool t_result;
        t_result = true;
        if (t_info != NULL)
        {
            CFBooleanRef t_value;
            t_value = (CFBooleanRef)CFDictionaryGetValue(t_info, CFSTR("LSBackgroundOnly"));
            if (t_value != NULL && CFBooleanGetValue(t_value) == TRUE)
                t_result = false;
            CFRelease(t_info);
        }
        
        return t_result;
    }
    
    virtual bool ChangeProcessType(bool p_to_foreground)
    {
        // We can only switch from background to foreground. So check to see if
        // we are foreground already, we are only asking to go to foreground.
        if (ProcessTypeIsForeground())
        {
            if (p_to_foreground)
                return true;
            return false;
        }
        
        // Actually switch to foreground.
        ProcessSerialNumber t_psn = { 0, kCurrentProcess };
        TransformProcessType(&t_psn, kProcessTransformToForegroundApplication);
        SetFrontProcess(&t_psn);
        
        return true;
    }
    
    virtual void CloseProcess(uint2 p_index)
    {
        if (MCprocesses[p_index].ihandle != NULL)
        {
            MCprocesses[p_index].ihandle -> Close();
            MCprocesses[p_index].ihandle = NULL;
        }
        if (MCprocesses[p_index].ohandle != NULL)
        {
            MCprocesses[p_index].ohandle -> Close();
            MCprocesses[p_index].ohandle = NULL;
        }
        MCprocesses[p_index].mode = OM_NEITHER;
    }
    
    virtual void Kill(int4 p_pid, int4 p_sig)
    {
        if (p_pid == 0)
            return;
        
        uint2 i;
        for (i = 0 ; i < MCnprocesses ; i++)
            if (p_pid == MCprocesses[i].pid && (MCprocesses[i].sn.highLongOfPSN != 0 || MCprocesses[i].sn.lowLongOfPSN != 0))
            {
                AppleEvent ae, answer;
                AEDesc pdesc;
                AECreateDesc(typeProcessSerialNumber, &MCprocesses[i].sn,
                             sizeof(ProcessSerialNumber), &pdesc);
                AECreateAppleEvent('aevt', 'quit', &pdesc, kAutoGenerateReturnID,
                                   kAnyTransactionID, &ae);
                AESend(&ae, &answer, kAEQueueReply, kAENormalPriority,
                       kAEDefaultTimeout, NULL, NULL);
                AEDisposeDesc(&ae);
                AEDisposeDesc(&answer);
                return;
            }
        
        kill(p_pid, p_sig);
    }
    
    virtual void KillAll(void)
    {
        struct sigaction action;
        memset((char *)&action, 0, sizeof(action));
        action.sa_handler = (void (*)(int))SIG_IGN;
        sigaction(SIGCHLD, &action, NULL);
        while (MCnprocesses--)
        {
            MCValueRelease(MCprocesses[MCnprocesses] . name);
            MCprocesses[MCnprocesses] . name = nil;
            if (MCprocesses[MCnprocesses].pid != 0
		        && (MCprocesses[MCnprocesses].ihandle != NULL
		            || MCprocesses[MCnprocesses].ohandle != NULL))
            {
                kill(MCprocesses[MCnprocesses].pid, SIGKILL);
                waitpid(MCprocesses[MCnprocesses].pid, NULL, 0);
            }
        }
    }
    
    virtual Boolean Poll(real8 p_delay, int p_fd)
    {
        fd_set rmaskfd, wmaskfd, emaskfd;
        FD_ZERO(&rmaskfd);
        FD_ZERO(&wmaskfd);
        FD_ZERO(&emaskfd);
        int4 maxfd = 0;
        if (!MCnoui)
        {
            if (p_fd != 0)
                FD_SET(p_fd, &rmaskfd);
            maxfd = p_fd;
        }
        if (MCshellfd != -1)
        {
            FD_SET(MCshellfd, &rmaskfd);
            if (MCshellfd > maxfd)
                maxfd = MCshellfd;
        }
        
        struct timeval timeoutval;
        timeoutval.tv_sec = (long)p_delay;
        timeoutval.tv_usec = (long)((p_delay - floor(p_delay)) * 1000000.0);
        int n = 0;
        
        n = select(maxfd + 1, &rmaskfd, &wmaskfd, &emaskfd, &timeoutval);
        
        if (n <= 0)
            return False;
        
        if (MCshellfd != -1 && FD_ISSET(MCshellfd, &rmaskfd))
            return True;
        
        return True;
    }
    
    virtual Boolean IsInteractiveConsole(int p_fd)
    {
        return isatty(p_fd) != 0;
    }
    
    virtual int GetErrno()
    {
        return errno;
    }
    virtual void SetErrno(int p_errno)
    {
        errno = p_errno;
    }
    
    virtual void LaunchDocument(MCStringRef p_document)
    {
        int t_error = 0;
        
        FSRef t_document_ref;
        if (t_error == 0)
        {
            errno = MCS_mac_pathtoref(p_document, t_document_ref);
            if (errno != noErr)
            {
                // MW-2008-06-12: [[ Bug 6336 ]] No result set if file not found on OS X
                MCresult -> sets("can't open file");
                t_error = 1;
            }
        }
        
        if (t_error == 0)
        {
            errno = LSOpenFSRef(&t_document_ref, NULL);
            MCS_launch_set_result_from_lsstatus();
        }
    }
    
    virtual void LaunchUrl(MCStringRef p_document)
    {
        bool t_success;
        t_success = true;
        
        CFStringRef t_cf_document;
        t_cf_document = NULL;
        
        // SN-2014-07-30: [[ Bug 13024 ]] URLs for local files aren't URL-encoded, and the "file:"
        //  prefix is not to be part of the URL.
        MCAutoStringRef t_url;
        bool t_is_path;
        if (MCStringBeginsWithCString(p_document, (const char_t*)"file:", kMCStringOptionCompareCaseless))
        {
            MCStringCopySubstring(p_document, MCRangeMakeMinMax(5, MCStringGetLength(p_document)), &t_url);
            t_is_path = true;
        }
        else
        {
            t_url = p_document;
            t_is_path = false;
        }
        
        if (t_success)
        {
            if (!MCStringConvertToCFStringRef(*t_url, t_cf_document))
                t_success = false;
        }
        
        CFURLRef t_cf_url;
        t_cf_url = NULL;
        if (t_success)
        {
            // SN-2014-07-30: [[ Bug 13024 ]] Local file URLs are not built like standard URLs since they
            //  are not URL-encoded
            if (t_is_path)
                t_cf_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, t_cf_document, kCFURLPOSIXPathStyle, False);
            else
                t_cf_url = CFURLCreateWithString(kCFAllocatorDefault, t_cf_document, NULL);
            
            if (t_cf_url == NULL)
                t_success = false;
        }
        
        if (t_success)
        {
            errno = LSOpenCFURLRef(t_cf_url, NULL);
            MCS_launch_set_result_from_lsstatus();
        }
        
        if (t_cf_url != NULL)
            CFRelease(t_cf_url);
		
        if (t_cf_document != NULL)
            CFRelease(t_cf_document);
    }

#define APPLESCRIPT_SCRIPT \
	"local tTempFolder;" \
	"put the tempname into tTempFolder;" \
	"create folder tTempFolder;" \
	"local tStdout, tStderr, tScript;" \
	"put tTempFolder & \"/stdout.txt\" into tStdout;" \
	"put tTempFolder & \"/stderr.txt\" into tStderr;" \
	"put tTempFolder & \"/script.scpt\" into tScript;" \
	"put textEncode(param(1), \"utf8\") into url (\"binfile:\" & tScript);" \
	"get shell(format(\"arch -arm64 osascript %s 2>%s 1>%s\", tScript, tStderr, tStdout));" \
	"local tResult;" \
	"put the result into tResult;" \
	"delete file tScript;" \
	"local tValue;" \
	"if tResult is 0 then;" \
		"put url (\"binfile:\" & tStdout) into tValue;" \
		"if the last char of tValue is return then;" \
			"delete the last char of tValue;" \
		"end if;" \
	"else;" \
		"put url (\"binfile:\" & tStderr) into tValue;" \
		"if tValue contains \"script error\" then;" \
			"put \"compiler error\" into tValue;" \
		"else;" \
			"put \"execution error\" into tValue;" \
		"end if;" \
	"end if;" \
	"delete file tStdout;" \
	"delete file tStderr;" \
	"switch tValue;" \
	"case \"execution error\";" \
	"case empty;" \
		"return tValue;" \
	"default;" \
		"return \"{\" & tValue & \"}\";" \
	"end switch"

    virtual void DoAlternateLanguage(MCStringRef p_script, MCStringRef p_language)
    {
		if (MCmajorosversion >= MCOSVersionMake(10,16,0) &&
			MCStringIsEqualToCString(p_language, "AppleScript", kMCStringOptionCompareCaseless) &&
			GetProcessIsTranslated())
		{
			MCParameter *t_param = new (nothrow) MCParameter;
			t_param->setvalueref_argument(p_script);
			MCresult->clear();
			MCdefaultstackptr->domess(MCSTR(APPLESCRIPT_SCRIPT), t_param, true);
			delete t_param;
			return;
		}

        getosacomponents();
        OSAcomponent *posacomp = NULL;
        uint2 i;
        for (i = 0; i < osancomponents; i++)
        {
            if (MCStringIsEqualTo(p_language, osacomponents[i].compname, kMCStringOptionCompareCaseless))
            {
                posacomp = &osacomponents[i];
                break;
            }
        }
        if (posacomp == NULL)
        {
            MCresult->sets("alternate language not found");
            return;
        }

        // ARM: use NSAppleScript instead of Component Manager OSA pipeline
        MCAutoStringRefAsUTF8String t_script_utf8;
        if (!t_script_utf8.Lock(p_script))
        {
            MCresult->sets("compiler error");
            return;
        }
        NSString *t_ns_script = [NSString stringWithUTF8String: *t_script_utf8];
        NSAppleScript *t_as = [[NSAppleScript alloc] initWithSource: t_ns_script];
        NSDictionary *t_error_info = nil;
        NSAppleEventDescriptor *t_result = [t_as executeAndReturnError: &t_error_info];
        if (t_result == nil)
        {
            if (t_error_info != nil)
            {
                NSString *t_msg = [t_error_info objectForKey: NSAppleScriptErrorMessage];
                if (t_msg != nil)
                    MCresult->sets([[t_msg description] UTF8String]);
                else
                    MCresult->sets("execution error");
            }
            else
                MCresult->sets("compiler error");
        }
        else
        {
            NSString *t_str = [t_result stringValue];
            if (t_str != nil)
                /* UNCHECKED */ MCresult->sets([t_str UTF8String]);
            else
                MCresult->clear(False);
        }
        [t_as release];
    }
    
    virtual bool AlternateLanguages(MCListRef& r_list)
    {
        MCAutoListRef t_list;
        if (!MCListCreateMutable('\n', &t_list))
            return false;
        
        getosacomponents();
        for (uindex_t i = 0; i < osancomponents; i++)
            if (!MCListAppend(*t_list, osacomponents[i].compname))
                return false;
        
        return MCListCopy(*t_list, r_list);
    }
    
#define DNS_SCRIPT "repeat for each line l in url \"binfile:/etc/resolv.conf\";\
if word 1 of l is \"nameserver\" then put word 2 of l & cr after it; end repeat;\
delete last char of it; return it"
    virtual bool GetDNSservers(MCListRef& r_list)
    {
        MCAutoListRef t_list;
        
        MCresult->clear();
        MCdefaultstackptr->domess(MCSTR(DNS_SCRIPT));
        
        return MCListCreateMutable('\n', &t_list) &&
            MCListAppend(*t_list, MCresult->getvalueref()) &&
            MCListCopy(*t_list, r_list);
    }
    
    virtual void ShowMessageDialog(MCStringRef p_title,
                                   MCStringRef p_message)
    {
#ifndef _SERVER
        extern void MCMacPlatformShowMessageDialog(MCStringRef p_title,
                                                   MCStringRef p_message);
        MCMacPlatformShowMessageDialog(p_title,
                                       p_message);
#endif
    }
};

////////////////////////////////////////////////////////////////////////////////

MCSystemInterface *MCDesktopCreateMacSystem(void)
{
	return new MCMacDesktop;
}


/*****************************************************************************
 *  Apple events handler	 			 	             *
 *****************************************************************************/


// SN-2014-10-07: [[ Bug 13587 ]] Using a MCList allows us to preserve unicode chars
static bool fetch_ae_as_fsref_list(MCListRef &r_list)
{
	AEDescList docList; //get a list of alias records for the documents
    long count;
    // SN-2015-04-14: [[ Bug 15105 ]] We want to return at least an empty list
    //  in any case where we return true
    // SN-2014-10-07: [[ Bug 13587 ]] We store the paths in a list
    MCAutoListRef t_list;
    /* UNCHECKED */ MCListCreateMutable('\n', &t_list);
    
	if (AEGetParamDesc(aePtr, keyDirectObject,
					   typeAEList, &docList) == noErr
		&& AECountItems(&docList, &count) == noErr && count > 0)
	{
		AEKeyword rKeyword; //returned keyword
		DescType rType;    //returned type
		
		FSRef t_doc_fsref;
		
		Size rSize;      //returned size, atual size of the docName
		long item;
		// get a FSSpec record, starts from count==1
        
		for (item = 1; item <= count; item++)
		{
			if (AEGetNthPtr(&docList, item, typeFSRef, &rKeyword, &rType,
							&t_doc_fsref, sizeof(FSRef), &rSize) != noErr)
			{
				AEDisposeDesc(&docList);
				return false;
			}
            
            // SN-2014-10-07: [[ Bug 13587 ]] Append directly the string, instead of converting to a CString
            MCAutoStringRef t_fullpathname;
            if (MCS_mac_fsref_to_path(t_doc_fsref, &t_fullpathname))
                MCListAppend(*t_list, *t_fullpathname);
		}
		AEDisposeDesc(&docList);
	}
    return MCListCopy(*t_list, r_list);
}

///////////////////////////////////////////////////////////////////////////////
//**************************************************************************
// * Utility functions used by this module only
// **************************************************************************/

static OSStatus getDescFromAddress(MCStringRef address, AEDesc *retDesc)
{
	/* return an address descriptor based on the target address passed in
     * * There are 3 possible forms of target string: *
     * 1. ZONE:MACHINE:APP NAME
     * 2. MACHINE:APP NAME(sender & receiver are in the same zone but on different machine)
     * 3. APP NAME
     */
	errno = noErr;
	retDesc->dataHandle = NULL;  /* So caller can dispose always. */
    
    uindex_t t_index;
	/* UNCHECKED */ MCStringFirstIndexOfChar(address, ':', 0, kMCStringOptionCompareExact, t_index);
    
	if (t_index == 0)
	{ //address contains application name only. Form # 3
		errno = getDesc(0, NULL, NULL, address, retDesc);
	}
    
	/* CARBON doesn't support the seding apple events between systmes. Therefore no
	 need to do the complicated location/zone searching                       */
    
	return errno;
}

// MW-2006-08-05: Vetted for Endian issues
static OSStatus getDesc(short locKind, MCStringRef zone, MCStringRef machine,
                     MCStringRef app, AEDesc *retDesc)
{
    
    // ARM replacement: use NSRunningApplication to find process by name,
    // then create an AE address descriptor using the bundle ID or PID.
    MCAutoStringRefAsUTF8String t_app_utf8;
    if (!t_app_utf8.Lock(app))
        return ioErr;
    NSString *t_target_name = [NSString stringWithUTF8String: *t_app_utf8];

    NSArray *t_running = [NSRunningApplication
        runningApplicationsWithBundleIdentifier: t_target_name];
    NSRunningApplication *t_found_app = nil;

    if ([t_running count] == 0)
    {
        // Fall back: search by localizedName
        for (NSRunningApplication *t_app in
             [[NSWorkspace sharedWorkspace] runningApplications])
        {
            if ([[t_app localizedName] caseInsensitiveCompare: t_target_name]
                == NSOrderedSame)
            {
                t_found_app = t_app;
                break;
            }
        }
    }
    else
        t_found_app = [t_running objectAtIndex: 0];

    if (t_found_app == nil)
        return procNotFound;

    // Build AE address from PID
    pid_t t_pid = [t_found_app processIdentifier];
    return AECreateDesc(typeKernelProcessID, &t_pid, sizeof(pid_t), retDesc);
}

// MW-2006-08-05: Vetted for Endian issues
static OSStatus getAEAttributes(const AppleEvent *ae, AEKeyword key, MCStringRef &r_result)
{
	DescType rType;
	Size rSize;
	DescType dt;
	Size s;
    bool t_success = false;
	if ((errno = AESizeOfAttribute(ae, key, &dt, &s)) == noErr)
	{
		switch (dt)
		{
            case typeBoolean:
			{
				Boolean b;
				AEGetAttributePtr(ae, key, dt, &rType, &b, s, &rSize);
				r_result = MCValueRetain(b ? kMCTrueString : kMCFalseString);
                break;
			}
            case typeUTF8Text:
            {
                byte_t *result = new (nothrow) byte_t[s + 1];
                AEGetAttributePtr(ae, key, dt, &rType, result, s, &rSize);
                t_success = MCStringCreateWithBytes(result, s, kMCStringEncodingUTF8, false, r_result);
                delete[] result;
                break;
            }
            case typeChar:
            {
                char_t *result = new (nothrow) char_t[s + 1];
                AEGetAttributePtr(ae, key, dt, &rType, result, s, &rSize);
                t_success = MCStringCreateWithNativeChars(result, s, r_result);
                delete[] result;
                break;
            }
            case typeType:
            {
                FourCharCode t_type;
                AEGetAttributePtr(ae, key, dt, &rType, &t_type, s, &rSize);
                char *result;
                result = FourCharCodeToString(t_type);
                t_success = MCStringCreateWithNativeChars((char_t*)result, 4, r_result);
                delete[] result;
			}
                break;
            case typeSInt16:
			{
				int16_t i;
				AEGetAttributePtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRId16, i);
				break;
			}
            case typeSInt32:
			{
				int32_t i;
				AEGetAttributePtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRId32, i);
				break;
			}
            case typeSInt64:
            {
                int64_t i;
                AEGetAttributePtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRId64, i);
                break;
            }
            case typeIEEE32BitFloatingPoint:
			{
				float32_t f;
				AEGetAttributePtr(ae, key, dt, &rType, &f, s, &rSize);
                t_success = MCStringFormat(r_result, "%12.12g", f);
				break;
			}
            case typeIEEE64BitFloatingPoint:
			{
				float64_t f;
				AEGetAttributePtr(ae, key, dt, &rType, &f, s, &rSize);
                t_success = MCStringFormat(r_result, "%12.12g", f);
				break;
			}
            case typeUInt16:
            {
                uint16_t i;
                AEGetAttributePtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRIu16, i);
                break;
            }
            case typeUInt32:
			{
				uint32_t i;
				AEGetAttributePtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRIu32, i);
				break;
			}
            case typeUInt64:
            {
                uint64_t i;
                AEGetAttributePtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRIu64, i);
                break;
            }
            case typeNull:
                r_result = MCValueRetain(kMCEmptyString);
                break;
#ifndef __64_BIT__
            // FSSpecs don't exist in the 64-bit world
            case typeFSS:
			{
				FSSpec fs;
				errno = AEGetAttributePtr(ae, key, dt, &rType, &fs, s, &rSize);
				t_success = MCS_mac_FSSpec2path(&fs, r_result);
			}
                break;
#endif
            case typeFSRef:
			{
				FSRef t_fs_ref;
				errno = AEGetAttributePtr(ae, key, dt, &rType, &t_fs_ref, s, &rSize);
                t_success = MCS_mac_fsref_to_path(t_fs_ref, r_result);
			}
                break;
            default:
                t_success = MCStringFormat(r_result, "unknown type %4.4s", (char*)&dt);
                break;
		}
	}
    
    if (!t_success && errno == 0)
        errno = ioErr;
    
	return errno;
}

// MW-2006-08-05: Vetted for Endian issues
static OSStatus getAEParams(const AppleEvent *ae, AEKeyword key, MCStringRef &r_result)
{
	DescType rType;
	Size rSize;
	DescType dt;
	Size s;
    bool t_success = true;
	if ((errno = AESizeOfParam(ae, key, &dt, &s)) == noErr)
	{
		switch (dt)
		{
            case typeBoolean:
			{
				Boolean b;
				AEGetParamPtr(ae, key, dt, &rType, &b, s, &rSize);
				r_result = MCValueRetain(b ? kMCTrueString : kMCFalseString);
                break;
			}
            case typeUTF8Text:
            {
                byte_t *result = new (nothrow) byte_t[s + 1];
                AEGetParamPtr(ae, key, dt, &rType, result, s, &rSize);
                t_success = MCStringCreateWithBytesAndRelease(result, s, kMCStringEncodingUTF8, false, r_result);
                break;
            }
            case typeChar:
            {
                char_t *result = new (nothrow) char_t[s + 1];
                AEGetParamPtr(ae, key, dt, &rType, result, s, &rSize);
                t_success = MCStringCreateWithNativeChars(result, s, r_result);
                delete[] result;
                break;
            }
            case typeType:
            {
                FourCharCode t_type;
                AEGetParamPtr(ae, key, dt, &rType, &t_type, s, &rSize);
                char *result;
                result = FourCharCodeToString(t_type);
                t_success = MCStringCreateWithNativeChars((char_t*)result, 4, r_result);
                delete[] result;
			}
                break;
            case typeSInt16:
			{
				int16_t i;
				AEGetParamPtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRId16, i);
				break;
			}
            case typeSInt32:
			{
				int32_t i;
				AEGetParamPtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRId32, i);
				break;
			}
            case typeSInt64:
            {
                int64_t i;
                AEGetParamPtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRId64, i);
                break;
            }
            case typeIEEE32BitFloatingPoint:
			{
				float32_t f;
				AEGetParamPtr(ae, key, dt, &rType, &f, s, &rSize);
                t_success = MCStringFormat(r_result, "%12.12g", f);
				break;
			}
            case typeIEEE64BitFloatingPoint:
			{
				float64_t f;
				AEGetParamPtr(ae, key, dt, &rType, &f, s, &rSize);
                t_success = MCStringFormat(r_result, "%12.12g", f);
				break;
			}
            case typeUInt16:
            {
                uint16_t i;
                AEGetParamPtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRIu16, i);
                break;
            }
            case typeUInt32:
			{
				uint32_t i;
				AEGetParamPtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRIu32, i);
				break;
			}
            case typeUInt64:
            {
                uint64_t i;
                AEGetParamPtr(ae, key, dt, &rType, &i, s, &rSize);
                t_success = MCStringFormat(r_result, PRIu64, i);
                break;
            }
            case typeNull:
                r_result = MCValueRetain(kMCEmptyString);
                break;
#ifndef __64_BIT__
            case typeFSS:
			{
				FSSpec fs;
				errno = AEGetParamPtr(ae, key, dt, &rType, &fs, s, &rSize);
				t_success = MCS_mac_FSSpec2path(&fs, r_result);
			}
                break;
#endif
            case typeFSRef:
			{
				FSRef t_fs_ref;
				errno = AEGetParamPtr(ae, key, dt, &rType, &t_fs_ref, s, &rSize);
                t_success = MCS_mac_fsref_to_path(t_fs_ref, r_result);
			}
                break;
            default:
               t_success = MCStringFormat(r_result, "unknown type %4.4s", (char*)&dt);
                break;
		}
	}
    
    if (!t_success && errno == 0)
        errno = ioErr;
    
	return errno;
}

static OSStatus getAddressFromDesc(AEAddressDesc targetDesc, char *address)
{/* This function returns the zone, machine, and application name for the
  indicated target descriptor.  */
    
    
	address[0] = '\0';
	return noErr;
    
}

// ============================================================
// ARM Phase 2: OSA/Component Manager replacement with NSAppleScript
//
// Component Manager (OpenDefaultComponent, OSACompile, OSAExecute,
// CountComponents, FindNextComponent) is unavailable on arm64.
//
// We replace the OSA pipeline with NSAppleScript. Since Component
// Manager enumeration is gone, we report only "applescript" as the
// available alternate language - which is the only one that matters
// in practice.
// ============================================================

// Stub: ARM has no ComponentInstance - OSA calls route through NSAppleScript
static OSStatus osacompile(MCStringRef s, void* /*compinstance*/, OSAID &scriptid)
{
    // NSAppleScript compiles+executes in one step; use scriptid=1 as sentinel
    scriptid = 1;
    return noErr;
}

static OSStatus osaexecute(MCStringRef& r_string, void* /*compinstance*/, OSAID scriptid)
{
    (void)scriptid;
    // Actual execution is done in DoAlternateLanguage via NSAppleScript
    r_string = MCValueRetain(kMCEmptyString);
    return noErr;
}

static void getosacomponents()
{
    // ARM: Component Manager is gone. We register only "applescript"
    // which is handled natively via NSAppleScript in DoAlternateLanguage.
    if (osacomponents != NULL)
        return;
    osacomponents = new (nothrow) OSAcomponent[1];
    osacomponents[0].compname = MCSTR("applescript");
    osacomponents[0].compsubtype = 0;
    osacomponents[0].compinstance = NULL;
    osancomponents = 1;
}

struct  LangID2Charset
{
	Lang_charset charset;
	ScriptCode scriptcode;
};

static LangID2Charset scriptcodetocharsets[] = {
    { LCH_ENGLISH, smRoman },
    { LCH_ROMAN, smRoman },
    { LCH_JAPANESE, smJapanese },
    { LCH_CHINESE, smTradChinese },
    { LCH_RUSSIAN, smCyrillic },
    { LCH_TURKISH, smCyrillic },
    { LCH_BULGARIAN, smCyrillic },
    { LCH_UKRAINIAN, smCyrillic },
    { LCH_ARABIC, smArabic },
    { LCH_HEBREW, smHebrew },
    { LCH_GREEK, smGreek },
    { LCH_KOREAN, smKorean },
    { LCH_POLISH, smCentralEuroRoman },
    { LCH_VIETNAMESE, smVietnamese },
    { LCH_LITHUANIAN, smCentralEuroRoman },
    { LCH_THAI, smThai },
    { LCH_SIMPLE_CHINESE, smSimpChinese },
    { LCH_UNICODE, smUnicodeScript }
};

uint1 MCS_langidtocharset(uint2 scode)
{
	uint2 i;
	for (i = 0; i < ELEMENTS(scriptcodetocharsets); i++)
		if (scriptcodetocharsets[i].scriptcode == scode)
			return scriptcodetocharsets[i].charset;
	return 0;
}

uint2 MCS_charsettolangid(uint1 charset)
{
	uint2 i;
	for (i = 0; i < ELEMENTS(scriptcodetocharsets); i++)
		if (scriptcodetocharsets[i].charset == charset)
			return scriptcodetocharsets[i].scriptcode;
	return 0;
}

void MCS_unicodetomultibyte(const char *s, uint4 len, char *d,
                            uint4 destbufferlength, uint4 &destlen,
                            uint1 charset)
{
    // ARM replacement: use CFString for Unicode→multibyte conversion.
    if (!destbufferlength)
    {
        destlen = len; // conservative upper bound
        return;
    }

    // Map LiveCode charset tag to a CFStringEncoding
    CFStringEncoding t_encoding = kCFStringEncodingMacRoman;
    if (charset == LCH_JAPANESE)       t_encoding = kCFStringEncodingMacJapanese;
    else if (charset == LCH_CHINESE)   t_encoding = kCFStringEncodingMacChineseTrad;
    else if (charset == LCH_SIMPLE_CHINESE) t_encoding = kCFStringEncodingMacChineseSimp;
    else if (charset == LCH_KOREAN)    t_encoding = kCFStringEncodingMacKorean;
    else if (charset == LCH_ARABIC)    t_encoding = kCFStringEncodingMacArabic;
    else if (charset == LCH_HEBREW)    t_encoding = kCFStringEncodingMacHebrew;
    else if (charset == LCH_GREEK)     t_encoding = kCFStringEncodingMacGreek;
    else if (charset == LCH_RUSSIAN || charset == LCH_BULGARIAN ||
             charset == LCH_UKRAINIAN || charset == LCH_TURKISH)
                                        t_encoding = kCFStringEncodingMacCyrillic;
    else if (charset == LCH_THAI)      t_encoding = kCFStringEncodingMacThai;
    else if (charset == LCH_UNICODE)   t_encoding = kCFStringEncodingUTF16LE;

    CFStringRef t_str = CFStringCreateWithBytes(
        kCFAllocatorDefault, (const UInt8 *)s, (CFIndex)len,
        kCFStringEncodingUTF16LE, false);
    if (t_str == NULL) { destlen = 0; return; }

    CFIndex t_used = 0;
    CFStringGetBytes(t_str, CFRangeMake(0, CFStringGetLength(t_str)),
                     t_encoding, '?', false,
                     (UInt8 *)d, (CFIndex)destbufferlength, &t_used);
    destlen = (uint4)t_used;
    CFRelease(t_str);
}

void MCS_multibytetounicode(const char *s, uint4 len, char *d,
                            uint4 destbufferlength,
                            uint4 &destlen, uint1 charset)
{
    // ARM replacement: use CFString for multibyte→Unicode conversion.
    if (!destbufferlength)
    {
        destlen = len << 1; // conservative upper bound
        return;
    }

    CFStringEncoding t_encoding = kCFStringEncodingMacRoman;
    if (charset == LCH_JAPANESE)       t_encoding = kCFStringEncodingMacJapanese;
    else if (charset == LCH_CHINESE)   t_encoding = kCFStringEncodingMacChineseTrad;
    else if (charset == LCH_SIMPLE_CHINESE) t_encoding = kCFStringEncodingMacChineseSimp;
    else if (charset == LCH_KOREAN)    t_encoding = kCFStringEncodingMacKorean;
    else if (charset == LCH_ARABIC)    t_encoding = kCFStringEncodingMacArabic;
    else if (charset == LCH_HEBREW)    t_encoding = kCFStringEncodingMacHebrew;
    else if (charset == LCH_GREEK)     t_encoding = kCFStringEncodingMacGreek;
    else if (charset == LCH_RUSSIAN || charset == LCH_BULGARIAN ||
             charset == LCH_UKRAINIAN || charset == LCH_TURKISH)
                                        t_encoding = kCFStringEncodingMacCyrillic;
    else if (charset == LCH_THAI)      t_encoding = kCFStringEncodingMacThai;

    CFStringRef t_str = CFStringCreateWithBytes(
        kCFAllocatorDefault, (const UInt8 *)s, (CFIndex)len,
        t_encoding, false);
    if (t_str == NULL) { destlen = 0; return; }

    CFIndex t_chars = CFStringGetLength(t_str);
    CFIndex t_bytes = t_chars * sizeof(UniChar);
    if (t_bytes > (CFIndex)destbufferlength) t_bytes = (CFIndex)destbufferlength;
    CFStringGetCharacters(t_str, CFRangeMake(0, t_bytes / (CFIndex)sizeof(UniChar)),
                          (UniChar *)d);
    destlen = (uint4)t_bytes;
    CFRelease(t_str);
}

////////////////////////////////////////////////////////////////////////////////

static bool startprocess_create_argv(char *name, char *doc, uint32_t & r_argc, char**& r_argv)
{
	uint32_t argc;
	char **argv;
	argc = 0;
	argv = nil;
	if (doc == NULL || *doc == '\0')
	{
		char *sptr = name;
		while (*sptr)
		{
			while (isspace(*sptr))
				sptr++;
			MCU_realloc((char **)&argv, argc, argc + 2, sizeof(char *));
			if (*sptr == '"')
			{
				argv[argc++] = ++sptr;
				while (*sptr && *sptr != '"')
					sptr++;
			}
			else
			{
				argv[argc++] = sptr;
				while (*sptr && !isspace(*sptr))
					sptr++;
			}
			if (*sptr)
				*sptr++ = '\0';
		}
	}
	else
	{
		argv = new (nothrow) char *[3];
		argv[0] = name;
		argv[1] = doc;
		argc = 2;
	}
	
	argv[argc] = NULL;
	
	r_argc = argc;
	r_argv = argv;
	
	return true;
}

static bool startprocess_write_uint32_to_fd(int fd, uint32_t value)
{
	value = MCSwapInt32HostToNetwork(value);
	if (write(fd, &value, sizeof(uint32_t)) != sizeof(uint32_t))
		return false;
	return true;
}

static bool startprocess_write_cstring_to_fd(int fd, char *string)
{
	if (!startprocess_write_uint32_to_fd(fd, strlen(string) + 1))
		return false;
	if (write(fd, string, strlen(string) + 1) != strlen(string) + 1)
		return false;
	return true;
}

static bool startprocess_read_uint32_t_from_fd(int fd, uint32_t& r_value)
{
	uint32_t value;
	if (read(fd, &value, sizeof(uint32_t)) != sizeof(uint32_t))
		return false;
	r_value = MCSwapInt32NetworkToHost(value);
	return true;
}

static bool startprocess_read_cstring_from_fd(int fd, char*& r_cstring)
{
	uint32_t t_length;
	if (!startprocess_read_uint32_t_from_fd(fd, t_length))
		return false;
	
	char *t_string;
	t_string = (char *)malloc(t_length);
	if (read(fd, t_string, t_length) != t_length)
	{
		free(t_string);
		return false;
	}
	
	r_cstring = t_string;
	
	return true;
}

bool MCS_mac_elevation_bootstrap_main(int argc, char *argv[])
{
	// Read the argument count
	uint32_t t_arg_count;
	startprocess_read_uint32_t_from_fd(fileno(stdin), t_arg_count);
	
	// Read the arguments
	char **t_args;
	t_args = (char **)malloc(sizeof(char *) * (t_arg_count + 1));
	for(uint32_t i = 0; i < t_arg_count; i++)
		startprocess_read_cstring_from_fd(fileno(stdin), t_args[i]);
    
	t_args[t_arg_count] = nil;
	
	// Now send back our PID to the parent.
	startprocess_write_uint32_to_fd(fileno(stdout), getpid());
	
	// Make sure stderr is also sent to stdout
	close(2);
	dup(1);
	
	// And finally exec to the new process (this does not return if successful).
	execvp(t_args[0], t_args);
	
	// If we get this far then an error has occurred :o(
	return false;
}

static void MCS_startprocess_launch(MCNameRef name, MCStringRef docname, Open_mode mode)
{
	// ARM: LaunchParamBlockRec / AliasHandle removed (Carbon-only).
	// Process launch is done via LSOpenFromRefSpec / NSWorkspace below.
	AEDesc target;  // still used for PSN-based 'odoc' AE if process is known
	
	FSRef t_app_fsref;
	errno = MCS_mac_pathtoref(MCNameGetString(name), t_app_fsref);
	
	uint2 i;
    
	if (mode == OM_NEITHER)
	{
		if (errno != noErr)
		{
			MCresult->sets("no such program");
			return;
		}
        
		FSRef t_doc_fsref;
		
		LSLaunchFSRefSpec inLaunchSpec;
		FSRef launchedapp;
		inLaunchSpec.numDocs = 0;
		inLaunchSpec.itemRefs = NULL;
		if (!MCStringIsEmpty(docname))
		{
			if (MCS_mac_pathtoref(docname, t_doc_fsref) != noErr)
			{
				MCresult->sets("no such document");
				return;
			}
			inLaunchSpec.numDocs = 1;
			inLaunchSpec.itemRefs = &t_doc_fsref;
		}
		inLaunchSpec.appRef = &t_app_fsref;
		inLaunchSpec.passThruParams = NULL;
		inLaunchSpec.launchFlags = kLSLaunchDefaults;
		inLaunchSpec.asyncRefCon = NULL;
		errno = LSOpenFromRefSpec(&inLaunchSpec, &launchedapp);
		return;
	}
    
	// ARM replacement: LaunchApplication (Carbon) is gone.
	// We use NSWorkspace to open the app with a document, which handles
	// the 'odoc' Apple event internally.
	errno = connectionInvalid;
	if (!MCStringIsEmpty(docname))
	{
		// Try to open the app with the document via NSWorkspace
		MCAutoStringRefAsUTF8String t_app_utf8, t_doc_utf8;
		if (t_app_utf8.Lock(MCNameGetString(name)) && t_doc_utf8.Lock(docname))
		{
			NSString *t_app_path = [NSString stringWithUTF8String: *t_app_utf8];
			NSString *t_doc_path = [NSString stringWithUTF8String: *t_doc_utf8];
			NSURL *t_app_url = [NSURL fileURLWithPath: t_app_path];
			NSURL *t_doc_url = [NSURL fileURLWithPath: t_doc_path];
			NSWorkspaceOpenConfiguration *t_config = [NSWorkspaceOpenConfiguration configuration];
			[[NSWorkspace sharedWorkspace]
				openURLs: @[t_doc_url]
				withApplicationAtURL: t_app_url
				configuration: t_config
				completionHandler: nil];
			errno = noErr;
			MCresult->clear(False);
		}
		return;
	}

	// No doc: launch the app directly via LSOpenFromRefSpec (still valid on ARM)
	if (errno != noErr)
	{
		LSLaunchFSRefSpec t_spec;
		t_spec.appRef = &t_app_fsref;
		t_spec.numDocs = 0;
		t_spec.itemRefs = NULL;
		t_spec.passThruParams = NULL;
		t_spec.launchFlags = kLSLaunchDefaults;
		t_spec.asyncRefCon = NULL;
		FSRef t_launched;
		errno = LSOpenFromRefSpec(&t_spec, &t_launched);
		if (errno != noErr)
		{
			char buffer[7 + I2L];
			sprintf(buffer, "error %d", errno);
			MCresult->copysvalue(buffer);
		}
		else
		{
			MCresult->clear(False);
			// Track the launched process by PID using NSRunningApplication
			if (i == MCnprocesses)
			{
				MCU_realloc((char **)&MCprocesses, MCnprocesses, MCnprocesses + 1,
				            sizeof(Streamnode));
				MCprocesses[MCnprocesses].name = MCValueRetain(name);
				MCprocesses[MCnprocesses].mode = OM_NEITHER;
				MCprocesses[MCnprocesses].ihandle = NULL;
				MCprocesses[MCnprocesses].ohandle = NULL;
				MCprocesses[MCnprocesses].pid = ++curpid;
				memset(&MCprocesses[MCnprocesses].sn, 0, sizeof(MCMacProcessSerialNumber));
				MCnprocesses++;
			}
		}
	}
}

static void MCS_startprocess_unix(MCNameRef name, MCStringRef doc, Open_mode mode, Boolean elevated)
{
	Boolean noerror = True;
	Boolean reading = mode == OM_READ || mode == OM_UPDATE;
	Boolean writing = mode == OM_APPEND || mode == OM_WRITE || mode == OM_UPDATE;
	MCU_realloc((char **)&MCprocesses, MCnprocesses, MCnprocesses + 1, sizeof(Streamnode));
    
	// Store process information.
	uint2 index = MCnprocesses;
	MCprocesses[MCnprocesses].name = (MCNameRef)MCValueRetain(name);
	MCprocesses[MCnprocesses].mode = mode;
	MCprocesses[MCnprocesses].ihandle = NULL;
	MCprocesses[MCnprocesses].ohandle = NULL;
	MCprocesses[MCnprocesses].sn.highLongOfPSN = 0;
	MCprocesses[MCnprocesses].sn.lowLongOfPSN = 0;
	
    char *t_doc;
    /* UNCHECKED */ MCStringConvertToCString(doc, t_doc);
	if (!elevated)
	{
		int tochild[2]; // pipe to child
		int toparent[2]; // pipe to parent
		
		// If we are reading, create the pipe to parent.
		// Parent reads, child writes.
		if (reading)
			if (pipe(toparent) != 0)
				noerror = False;
		
		// If we are writing, create the pipe to child.
		// Parent writes, child reads.
		if (noerror && writing)
			if (pipe(tochild) != 0)
			{
				noerror = False;
				if (reading)
				{
					// error, get rid of these fds
					close(toparent[0]);
					close(toparent[1]);
				}
			}
        
		if (noerror)
		{
			// Fork
			if ((MCprocesses[MCnprocesses++].pid = fork()) == 0)
			{
                // [[ Bug 13622 ]] Make sure environ is appropriate (on Yosemite it can
                //    be borked).
                environ = fix_environ();

                char *t_utf8_string = nil;
                /* UNCHECKED */ MCStringConvertToUTF8String(MCNameGetString(name),
                                                            t_utf8_string);

				// The pid is 0, so here we are in the child process.
				// Construct the argument string to pass to the process..
				char **argv = NULL;
				uint32_t argc = 0;
				startprocess_create_argv(t_utf8_string, t_doc, argc, argv);
				
				// The parent is reading, so we (we are child) are writing.
				if (reading)
				{
					// Don't need to read
					close(toparent[0]);
					
					// Close the current stdout, and duplicate the out descriptor of toparent to stdout.
					close(1);
					dup(toparent[1]);
					
					// Redirect stderr of this child to toparent-out.
					close(2);
					dup(toparent[1]);
					
					// We no longer need this pipe, so close the output descriptor.
					close(toparent[1]);
				}
				else
				{
					// Not reading, so close stdout and stderr.
					close(1);
					close(2);
				}
				if (writing)
				{
					// Parent is writing, so child needs to read. Close tochild[1], we dont need it.
					close(tochild[1]);
					
					// Attach stdin to tochild[0].
					close(0);
					dup(tochild[0]);
					
					// Close, as we no longer need it.
					close(tochild[0]);
				}
				else // not writing, so close stdin
					close(0);
                
				// Execute a new process in a new process image.
				execvp(argv[0], argv);
				
				// If we get here, an error occurred
				_exit(-1);
			}
			
			// If we get here, we are in the parent process, as the child has exited.
			
			MCS_checkprocesses();
			
			if (reading)
			{
				close(toparent[1]);
				MCS_mac_nodelay(toparent[0]);
				// Store the in handle for the "process".
				MCprocesses[index].ihandle = MCsystem -> OpenFd(toparent[0], kMCOpenFileModeRead);
			}
			if (writing)
			{
				close(tochild[0]);
				// Store the out handle for the "process".
				MCprocesses[index].ohandle = MCsystem -> OpenFd(tochild[1], kMCOpenFileModeWrite);
			}
		}
	}
	else
	{
		OSStatus t_status;
		t_status = noErr;
		
		AuthorizationRef t_auth;
		t_auth = nil;
		if (t_status == noErr)
			t_status = AuthorizationCreate(nil, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &t_auth);
		
		if (t_status == noErr)
		{
			AuthorizationItem t_items =
			{
				kAuthorizationRightExecute, 0,
				NULL, 0
			};
			AuthorizationRights t_rights =
			{
				1, &t_items
			};
			AuthorizationFlags t_flags =
            kAuthorizationFlagDefaults |
            kAuthorizationFlagInteractionAllowed |
            kAuthorizationFlagPreAuthorize |
            kAuthorizationFlagExtendRights;
			t_status = AuthorizationCopyRights(t_auth, &t_rights, nil, t_flags, nil);
		}
		
		FILE *t_stream;
		t_stream = nil;
		if (t_status == noErr)
		{
			char *t_arguments[] =
			{
				"-elevated-slave",
				nil
			};
            
            MCAutoPointer<char> t_cmd;
            uindex_t t_ignored;
            
            /* UNCHECKED */ MCStringConvertToUTF8(MCcmd, &t_cmd, t_ignored);
            
			t_status = AuthorizationExecuteWithPrivileges(t_auth, *t_cmd, kAuthorizationFlagDefaults, t_arguments, &t_stream);
		}
		
		uint32_t t_pid;
		t_pid = 0;
		if (t_status == noErr)
		{
			// Split the arguments
			uint32_t t_argc;
			char **t_argv;
			MCAutoStringRefAsUTF8String t_document;
            MCAutoStringRefAsUTF8String t_name_dup;
            
            if(t_document.Lock(doc) && t_name_dup.Lock(MCNameGetString(name)))
            {
                startprocess_create_argv((char*)*t_name_dup,(char*)*t_document, t_argc, t_argv);
                startprocess_write_uint32_to_fd(fileno(t_stream), t_argc);
                for(uint32_t i = 0; i < t_argc; i++)
                    startprocess_write_cstring_to_fd(fileno(t_stream), t_argv[i]);
                if (!startprocess_read_uint32_t_from_fd(fileno(t_stream), t_pid))
                    t_status = errAuthorizationToolExecuteFailure;
                
                delete[] t_argv;
            }
            else
                t_status = errAuthorizationToolExecuteFailure;
		}
		
		if (t_status == noErr)
		{
			MCprocesses[MCnprocesses++].pid = t_pid;
			MCS_checkprocesses();
			
			if (reading)
			{
				int t_fd;
				t_fd = dup(fileno(t_stream));
				MCS_mac_nodelay(t_fd);
				MCprocesses[index].ihandle = MCsystem -> OpenFd(t_fd, kMCOpenFileModeRead);
			}
			if (writing)
				MCprocesses[index].ohandle = MCsystem -> OpenFd(dup(fileno(t_stream)), kMCOpenFileModeWrite);
			
			noerror = True;
		}
		else
			noerror = False;
		
		if (t_stream != nil)
			fclose(t_stream);
		
		if (t_auth != nil)
			AuthorizationFree(t_auth, kAuthorizationFlagDefaults);
	}
	
	if (!noerror || MCprocesses[index].pid == -1 || MCprocesses[index].pid == 0)
	{
		if (noerror)
			MCprocesses[index].pid = 0;
		MCresult->sets("not opened");
	}
	else
		MCresult->clear(False);

    
    delete t_doc;

}

////////////////////////////////////////////////////////////////////////////////

bool MCS_get_browsers(MCStringRef &r_browsers)
{
    bool t_success = true;
    
    MCAutoListRef t_browser_list;
    if (t_success)
        t_success = MCListCreateMutable('\n', &t_browser_list);
    
    CFURLRef t_url = nullptr;
    if (t_success)
    {
        t_url = CFURLCreateWithString(nullptr, CFSTR("http://localhost"), nullptr);
        t_success = t_url != nullptr;
    }
    
    CFArrayRef t_browsers = nullptr;
    if (t_success)
        t_browsers = LSCopyApplicationURLsForURL(t_url, kLSRolesAll);
    
    if (t_success && t_browsers != nullptr)
    {
        for (CFIndex i = 0; t_success && i < CFArrayGetCount(t_browsers); ++i)
        {
            CFURLRef t_browser_url = (CFURLRef)CFArrayGetValueAtIndex(t_browsers, i);
            
            CFStringRef t_browser_path = nullptr;
            if (t_success)
            {
                t_browser_path = CFURLCopyFileSystemPath(t_browser_url, kCFURLPOSIXPathStyle);
                t_success = t_browser_path != nullptr;
            }
            
            CFBundleRef t_browser_bundle = CFBundleCreate(nullptr, t_browser_url);
            if (t_success)
            {
                t_browser_bundle = CFBundleCreate(nullptr, t_browser_url);
                t_success = t_browser_bundle != nullptr;
            }
            
            CFStringRef t_browser_title = nullptr;
            if (t_success)
            {
                
                CFStringRef t_name = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(t_browser_bundle, kCFBundleNameKey);
                CFStringRef t_version = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(t_browser_bundle, kCFBundleVersionKey);
                t_browser_title = CFStringCreateWithFormat(nullptr, nullptr, CFSTR("%@ (%@),%@"), t_name, t_version, t_browser_path);
                t_success = t_browser_title != nullptr;
            }
            
            MCAutoStringRef t_browser_string;
            if (t_success)
                t_success = MCStringCreateWithCFStringRef(t_browser_title, &t_browser_string);
            
            if (t_success)
                t_success = MCListAppend(*t_browser_list, *t_browser_string);
            
            if (t_browser_path != nullptr)
                CFRelease(t_browser_path);
            if (t_browser_bundle != nullptr)
                CFRelease(t_browser_bundle);
            if (t_browser_title != nullptr)
                CFRelease(t_browser_title);
        }
    }
    
    if (t_success)
        t_success = MCListCopyAsString(*t_browser_list, r_browsers);
    
    if (t_browsers != nullptr)
        CFRelease(t_browsers);
    if (t_url != nullptr)
        CFRelease(t_url);
    
    return t_success;
}

////////////////////////////////////////////////////////////////////////////////
