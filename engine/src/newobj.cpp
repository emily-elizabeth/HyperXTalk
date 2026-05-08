#include "prefix.h"

#include "globdefs.h"
#include "parsedef.h"
#include "filedefs.h"

#include "cmds.h"
#include "visual.h"
#include "keywords.h"
#include "funcs.h"
#include "operator.h"
#include "newobj.h"
#include "answer.h"
#include "ask.h"
#include "internal.h"
#include "notification.h"
#include "share.h"

#include "mode.h"

MCStatement *MCN_new_statement(int2 which)
{
	switch (which)
	{
    case S_INTERNAL:
        return new MCInternal;
	case S_ACCEPT:
		return new MCAccept;
	case S_ADD:
		return new MCAdd;
	case S_ANSWER:
		return new MCAnswer;
	case S_ASK:
		return new MCAsk;
	// MW-2013-11-14: [[ AssertCmd ]] Constructor for assert command.
	case S_ASSERT:
		return new MCAssertCmd;
	case S_BEEP:
		return new MCBeep;
	case S_BRING_APPLICATION_TO_FRONT:
		return new MCBringApplicationToFront;
	case S_BREAK:
		return new MCBreak;
	case S_BREAKPOINT:
		return new MCBreakPoint;
	case S_CALL:
		return new MCCall;
	case S_CANCEL:
		return new MCCancel;
	case S_CANCEL_ALL_NOTIFICATIONS:
		return new MCCancelAllNotifications;
	case S_CANCEL_NOTIFICATION:
		return new MCCancelNotification;
	case S_CHOOSE:
		return new MCChoose;
	case S_CLICK:
		return new MCClickCmd;
	case S_CLONE:
		return new MCClone;
	case S_CLOSE:
		return new MCClose;
	case S_COMBINE:
		return new MCCombine;
	case S_COMPACT:
		return new MCCompact;
	case S_CONSTANT:
		return new MCLocalConstant;
	case S_CONVERT:
		return new MCConvert;
	case S_COPY:
		return new MCCopyCmd;
	case S_CREATE:
		return new MCCreate;
	case S_CROP:
		return new MCCrop;
	case S_CUT:
		return new MCCutCmd;
	case S_DEBUGDO:
		return new MCDebugDo;
	case S_DECRYPT:
		return new MCCipherDecrypt;
	case S_DEFINE:
		return new MCDefine;
	case S_DELETE:
		return new MCDelete;
    case S_DIFFERENCE:
        return new MCSetOp(MCSetOp::kOpDifference);
	case S_DISABLE:
		return new MCDisable;
	// MW-2008-11-05: [[ Dispatch Command ]] Create a dispatch statement object
	case S_DISPATCH:
		return new MCDispatchCmd;
	case S_DIVIDE:
		return new MCDivide;
	case S_DO:
		return new MCRegularDo;
	case S_DOMENU:
		return new MCDoMenu;
	case S_DRAG:
		return new MCDrag;
	case S_DRAWER:
		return new MCDrawer;
	case S_ECHO:
		return new MCEcho;
	case S_EDIT:
		return new MCEdit;
	case S_ENABLE:
		return new MCEnable;
	case S_ENCRYPT:
		return new MCCipherEncrypt;
	case S_EXPORT:
		return new MCExport;
	case S_EXIT:
		return new MCExit;
	case S_FILTER:
		return new MCFilter;
	case S_FIND:
		return new MCFind;
	case S_FLIP:
		return new MCFlip;
	case S_FOCUS:
		return new MCFocus;
	case S_GET:
		return new MCGet;
	case S_GLOBAL:
		return new MCGlobal;
	case S_GO:
		return new MCGo;
	case S_GRAB:
		return new MCGrab;
	case S_GROUP:
		return new MCMakeGroup;
	case S_HIDE:
		return new MCHide;
	case S_HILITE:
		return new MCHilite;
	case S_IF:
		return new MCIf;
	case S_INCLUDE:
		return new MCInclude(false);
	case S_IMPORT:
		return new MCImport;
	case S_INSERT:
		return new MCInsert;
	case S_INTERSECT:
		return new MCSetOp(MCSetOp::kOpIntersect);
	case S_KILL:
		return new MCKill;
	case S_LAUNCH:
		return new MCLaunch;
	case S_LIBRARY:
		return new MCLibrary;
	case S_LOAD:
		return new MCLoad;
	case S_LOCAL:
		return new MCLocalVariable;
	case S_LOCK:
		return new MCLock;
    case S_LOG:
        return new MCLogCmd;
    case S_MARK:
		return new MCMarkCommand;
	case S_MODAL:
		return new MCModal;
	case S_MODELESS:
		return new MCModeless;
	case S_MOVE:
		return new MCMove;
	case S_MULTIPLY:
		return new MCMultiply;
	case S_NEXT:
		return new MCNext;
	case S_OPEN:
		return new MCOpen;
	case S_OPTION:
		return new MCOption;
	case S_PALETTE:
		return new MCPalette;
	case S_PASS:
		return new MCPass;
	case S_PASTE:
		return new MCPasteCmd;
	case S_PLACE:
		return new MCPlace;
	case S_PLAY:
		return new MCPlay;
	case S_POP:
		return new MCPop;
	case S_POPUP:
		return new MCPopup;
	case S_POPOVER:
		return new MCPopover;
	case S_POST:
		return new MCPost;
	case S_PREPARE:
		return new MCPrepare;
	case S_PRINT:
		return new MCPrint;
	case S_PULLDOWN:
		return new MCPulldown;
	case S_PUSH:
		return new MCPush;
	case S_PUT:
		return new MCPut;
	case S_QUIT:
		return new MCQuit;
	case S_READ:
		return new MCRead;
	case S_RECORD:
		return new MCRecord;
	case S_REDO:
		return new MCRedo;
	case S_REGISTER_HOTKEY:
		return new MCRegisterHotkey;
	case S_RELAYER:
		return new MCRelayer;
	case S_REMOVE:
		return new MCRemove;
	case S_RENAME:
		return new MCRename;
	case S_REPEAT:
		return new MCRepeat;
	case S_REPLACE:
		return new MCReplace;
	case S_REPLY:
		return new MCReply;
	case S_REQUEST:
		return new MCRequest;
	case S_REQUEST_NOTIFICATION_PERMISSION:
		return new MCRequestNotificationPermission;
	case S_REQUIRE:
		return new MCInclude(true);
	case S_RESET:
		return new MCReset;
    case S_RESOLVE:
        return new MCResolveImage;
    case S_RETURN:
		return new MCReturn;
	case S_REVERT:
		return new MCRevert;
	case S_ROTATE:
		return new MCRotate;
	case S_SAVE:
		return new MCSave;
	case S_SCRIPT_ERROR:
		return new MCScriptError;
	// MM-2014-02-12: [[ SecureSocket ]] Create secure statement object
	case S_SECURE:
		return new MCSecure;
	case S_SEEK:
		return new MCSeek;
	case S_SELECT:
		return new MCSelect;
	case S_SEND:
		return new MCSend;
	case S_SHARE:
		return new MCShare;
	case S_SET:
		return new MCSet;
	case S_SHEET:
		return new MCSheet;
	case S_SHOW:
		return new MCShow;
	case S_SHOW_NOTIFICATION:
		return new MCShowNotification;
	case S_SORT:
		return new MCSort;
	case S_SPLIT:
		return new MCSplit;
	case S_START:
		return new MCStart;
	case S_STOP:
		return new MCStop;
	case S_SUBTRACT:
		return new MCSubtract;
	case S_SWITCH:
		return new MCSwitch;
    case S_SYMMETRIC:
        return new MCSetOp(MCSetOp::kOpSymmetricDifference);
	case S_THROW:
		return new MCThrowKeyword;
	case S_TOP_LEVEL:
		return new MCTopLevel;
	case S_TRY:
		return new MCTry;
	case S_TYPE:
		return new MCType;
	case S_UNDEFINE:
		return new MCUndefine;
	case S_UNDO:
		return new MCUndoCmd;
	case S_UNGROUP:
		return new MCUngroup;
	case S_UNHILITE:
		return new MCUnhilite;
	case S_UNION:
		return new MCSetOp(MCSetOp::kOpUnion);
	case S_UNLOAD:
		return new MCUnload;
	case S_UNLOCK:
		return new MCUnlock;
	case S_UNMARK:
		return new MCUnmark;
	case S_UNREGISTER_ALL_HOTKEYS:
		return new MCUnregisterAllHotkeys;
	case S_UNREGISTER_HOTKEY:
		return new MCUnregisterHotkey;
	case S_VALIDATE_FIELD:
		return new MCValidateField;
	case S_VISUAL:
		return new MCVisualEffect;
	case S_WAIT:
		return new MCWait;
	case S_WRITE:
		return new MCWrite;
	default:
		break;
	}

	MCStatement *t_new_statement;
	t_new_statement = MCModeNewCommand(which);
	if (t_new_statement != NULL)
		return t_new_statement;

	return new MCStatement;
}





// s_hxt_new_func_id thread_local removed: func_id now set post-construction.

MCExpression *MCN_new_function(int2 which)
{
	MCExpression *t_new_function = nullptr;
	switch (which)
	{
	case F_ABS:
		t_new_function = new MCAbsFunction; break;
	case F_ACOS:
		t_new_function = new MCAcos; break;
	case F_ALIAS_REFERENCE:
		t_new_function = new MCAliasReference; break;
	case F_ALTERNATE_LANGUAGES:
		t_new_function = new MCAlternateLanguages; break;
	case F_ANNUITY:
		t_new_function = new MCAnnuity; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'arithmeticMean' (was average)
	case F_ARI_MEAN:
		t_new_function = new MCArithmeticMean; break;
	case F_ARRAY_DECODE:
		t_new_function = new MCArrayDecode; break;
	case F_ARRAY_ENCODE:
		t_new_function = new MCArrayEncode; break;
	case F_ASIN:
		t_new_function = new MCAsin; break;
	case F_ATAN2:
		t_new_function = new MCAtan2; break;
	case F_ATAN:
		t_new_function = new MCAtan; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'averageDeviation'
	case F_AVG_DEV:
		t_new_function = new MCAvgDev; break;
	case F_BACK_SCRIPTS:
		t_new_function = new MCBackScripts; break;
	case F_BASE64_DECODE:
		t_new_function = new MCBase64Decode; break;
	case F_BASE64_ENCODE:
		t_new_function = new MCBase64Encode; break;
	case F_BASE_CONVERT:
		t_new_function = new MCBaseConvert; break;
	case F_BATTERY_LEVEL:
		t_new_function = new MCBatteryLevel; break;
    // AL-2014-10-17: [[ BiDi ]] Returns the result of applying the bi-directional algorithm to text
    case F_BIDI_DIRECTION:
        t_new_function = new MCBidiDirection; break;
	case F_BINARY_ENCODE:
		t_new_function = new MCBinaryEncode; break;
	case F_BINARY_DECODE:
		t_new_function = new MCBinaryDecode; break;
	case F_BUILD_NUMBER:
		t_new_function = new MCBuildNumber; break;
    case F_BYTE_OFFSET:
        t_new_function = new MCByteOffset; break;
	case F_CACHED_URLS:
		t_new_function = new MCCachedUrls; break;
	case F_CAPS_LOCK_KEY:
		t_new_function = new MCCapsLockKey; break;
	case F_CHAR_TO_NUM:
		t_new_function = new MCCharToNum; break;
	case F_BYTE_TO_NUM:
		t_new_function = new MCByteToNum; break;
	case F_CIPHER_NAMES:
		t_new_function = new MCCipherNames; break;
	case F_CLICK_CHAR:
		t_new_function = new MCClickChar; break;
	case F_CLICK_CHAR_CHUNK:
		t_new_function = new MCClickCharChunk; break;
	case F_CLICK_CHUNK:
		t_new_function = new MCClickChunk; break;
	case F_CLICK_FIELD:
		t_new_function = new MCClickField; break;
	case F_CLICK_H:
		t_new_function = new MCClickH; break;
	case F_CLICK_LINE:
		t_new_function = new MCClickLine; break;
	case F_CLICK_LOC:
		t_new_function = new MCClickLoc; break;
	case F_CLICK_STACK:
		t_new_function = new MCClickStack; break;
	case F_CLICK_TEXT:
		t_new_function = new MCClickText; break;
	case F_CLICK_V:
		t_new_function = new MCClickV; break;
	case F_CLIPBOARD:
		t_new_function = new MCClipboardFunc; break;
    case F_CODEPOINT_OFFSET:
        t_new_function = new MCCodepointOffset; break;
    case F_CODEUNIT_OFFSET:
        t_new_function = new MCCodeunitOffset; break;
	case F_COLOR_NAMES:
		t_new_function = new MCColorNames; break;
    case F_COMMAND_ARGUMENTS:
        t_new_function = new MCCommandArguments; break;
	case F_COMMAND_KEY:
		t_new_function = new MCCommandKey; break;
    case F_COMMAND_NAME:
        t_new_function = new MCCommandName; break;
	case F_COMMAND_NAMES:
		t_new_function = new MCCommandNames; break;
	case F_COMPOUND:
		t_new_function = new MCCompound; break;
	case F_COMPRESS:
		t_new_function = new MCCompress; break;
	case F_CONSTANT_NAMES:
		t_new_function = new MCConstantNames; break;
	case F_CONTROL_KEY:
		t_new_function = new MCControlKey; break;
	case F_COPY_RESOURCE:
		t_new_function = new MCCopyResource; break;
	case F_COS:
		t_new_function = new MCCos; break;
	case F_DATE:
		t_new_function = new MCDate; break;
	case F_DATE_FORMAT:
		t_new_function = new MCDateFormat; break;
	case F_DECOMPRESS:
		t_new_function = new MCDecompress; break;
	case F_DELETE_CREDENTIAL:
		t_new_function = new MCDeleteCredential; break;
	case F_DELETE_REGISTRY:
		t_new_function = new MCDeleteRegistry; break;
	case F_DELETE_RESOURCE:
		t_new_function = new MCDeleteResource; break;
	case F_DIRECTORIES:
		t_new_function = new MCFileItems(false); break;
	case F_DISK_SPACE:
		t_new_function = new MCDiskSpace; break;
	case F_DNS_SERVERS:
		t_new_function = new MCDNSServers; break;
	case F_DRAG_DESTINATION:
		t_new_function = new MCDragDestination; break;
	case F_DRAG_SOURCE:
		t_new_function = new MCDragSource; break;
	case F_DRIVER_NAMES:
		t_new_function = new MCDriverNames; break;
	case F_DRIVES:
		t_new_function = new MCDrives; break;
	case F_DROP_CHUNK:
		t_new_function = new MCDropChunk; break;
	case F_ENCRYPT:
		t_new_function = new MCEncrypt; break;
	case F_ENVIRONMENT:
		t_new_function = new MCEnvironment; break;
    case F_EVENT_CAPSLOCK_KEY:
        t_new_function = new MCEventCapsLockKey; break;
    case F_EVENT_COMMAND_KEY:
        t_new_function = new MCEventCommandKey; break;
    case F_EVENT_CONTROL_KEY:
        t_new_function = new MCEventControlKey; break;
    case F_EVENT_OPTION_KEY:
        t_new_function = new MCEventOptionKey; break;
    case F_EVENT_SHIFT_KEY:
        t_new_function = new MCEventShiftKey; break;
	case F_EXISTS:
		t_new_function = new MCExists; break;
	case F_EXP:
		t_new_function = new MCExp; break;
	case F_EXP1:
		t_new_function = new MCExp1; break;
	case F_EXP10:
		t_new_function = new MCExp10; break;
	case F_EXP2:
		t_new_function = new MCExp2; break;
	case F_EXTENTS:
		t_new_function = new MCExtents; break;
	case F_FILES:
		t_new_function = new MCFileItems(true); break;
	case F_FLUSH_EVENTS:
		t_new_function = new MCFlushEvents; break;
	case F_FOCUSED_OBJECT:
		t_new_function = new MCFocusedObject; break;
	case F_FONT_NAMES:
		t_new_function = new MCFontNames; break;
	case F_FONT_LANGUAGE:
		t_new_function = new MCFontLanguage; break;
	case F_FONT_SIZES:
		t_new_function = new MCFontSizes; break;
	case F_FONT_STYLES:
		t_new_function = new MCFontStyles; break;
	case F_FORMAT:
		t_new_function = new MCFormat; break;
	case F_FOUND_CHUNK:
		t_new_function = new MCFoundChunk; break;
	case F_FOUND_FIELD:
		t_new_function = new MCFoundField; break;
	case F_FOUND_LINE:
		t_new_function = new MCFoundLine; break;
	case F_FOUND_LOC:
		t_new_function = new MCFoundLoc; break;
	case F_FOUND_TEXT:
		t_new_function = new MCFoundText; break;
	case F_FRONT_SCRIPTS:
		t_new_function = new MCFrontScripts; break;
	case F_FUNCTION_NAMES:
		t_new_function = new MCFunctionNames; break;
	case F_GET_RESOURCE:
		t_new_function = new MCGetResource; break;
	case F_GET_RESOURCES:
		t_new_function = new MCGetResources; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'geometricMean'
	case F_GEO_MEAN:
		t_new_function = new MCGeometricMean; break;
	case F_GLOBAL_LOC:
		t_new_function = new MCGlobalLoc; break;
	case F_GLOBALS:
		t_new_function = new MCGlobals; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'harmonicMean'
	case F_HAR_MEAN:
		t_new_function = new MCHarmonicMean; break;
	case F_HAS_MEMORY:
		t_new_function = new MCHasMemory; break;
	case F_HEAP_SPACE:
		t_new_function = new MCHeapSpace; break;
	case F_HTTP_PROXY_FOR_URL:
		t_new_function = new MCHTTPProxyForURL; break;
	case F_ICON_DATA_FOR_EXTENSION:
		t_new_function = new MCIconDataForExtension; break;
	case F_ICON_DATA_FOR_FILE:
		t_new_function = new MCIconDataForFile; break;
	case F_IFF:
		t_new_function = new MCIff; break;
	case F_INTERRUPT:
		t_new_function = new MCInterrupt; break;
	case F_HA:
		t_new_function = new MCHostAddress; break;
	case F_HATON:
		t_new_function = new MCHostAtoN; break;
	case F_HN:
		t_new_function = new MCHostName; break;
	case F_HNTOA:
		t_new_function = new MCHostNtoA; break;
	case F_INTERSECT:
		t_new_function = new MCIntersect; break;
	case F_IS_NUMBER:
		t_new_function = new MCIsNumber; break;
	case F_ISO_TO_MAC:
		t_new_function = new MCIsoToMac; break;
	case F_ITEM_OFFSET:
		t_new_function = new MCItemOffset; break;
	case F_KEYS:
		t_new_function = new MCKeys; break;
	case F_KEYS_DOWN:
		t_new_function = new MCKeysDown; break;
	case F_LENGTH:
		t_new_function = new MCLength; break;
	case F_LICENSED:
		t_new_function = new MCLicensed; break;
	case F_LINE_OFFSET:
		t_new_function = new MCLineOffset; break;
	case F_LIST_REGISTRY:
		t_new_function = new MCListRegistry; break;
	case F_LN1:
		t_new_function = new MCLn1; break;
	case F_LN:
		t_new_function = new MCLn; break;
	case F_LOCAL_LOC:
		t_new_function = new MCLocalLoc; break;
	case F_LOCALS:
		t_new_function = new MCLocals; break;
	case F_LONG_FILE_PATH:
		t_new_function = new MCLongFilePath; break;
	case F_LOG10:
		t_new_function = new MCLog10; break;
	case F_LOG2:
		t_new_function = new MCLog2; break;
	case F_MACHINE:
		t_new_function = new MCMachine; break;
	case F_MAC_TO_ISO:
		t_new_function = new MCMacToIso; break;
	case F_MAIN_STACKS:
		t_new_function = new MCMainStacks; break;
	case F_MATCH_CHUNK:
		t_new_function = new MCMatchChunk; break;
	case F_MATCH_TEXT:
		t_new_function = new MCMatchText; break;
	case F_MATRIX_MULTIPLY:
		t_new_function = new MCMatrixMultiply; break;
	case F_MAX:
		t_new_function = new MCMaxFunction; break;
	case F_MCI_SEND_STRING:
		t_new_function = new MCMCISendString; break;
	case F_MD5_DIGEST:
		t_new_function = new MCMD5Digest; break;
	case F_MICROSECS:
		t_new_function = new MCMicrosecs; break;
	case F_ME:
		t_new_function = new MCMe; break;
	case F_MEDIAN:
		t_new_function = new MCMedian; break;
	case F_MENUS:
		t_new_function = new MCMenus; break;
	case F_MENU_OBJECT:
		t_new_function = new MCMenuObject; break;
	case F_MERGE:
		t_new_function = new MCMerge; break;
    case F_MESSAGE_DIGEST:
        t_new_function = new MCMessageDigestFunc; break;
	case F_MILLISECS:
		t_new_function = new MCMillisecs; break;
	case F_MIN:
		t_new_function = new MCMinFunction; break;
	case F_MONTH_NAMES:
		t_new_function = new MCMonthNames; break;
	case F_MOUSE:
		t_new_function = new MCMouse; break;
	case F_MOUSE_CHAR:
		t_new_function = new MCMouseChar; break;
	case F_MOUSE_CHAR_CHUNK:
		t_new_function = new MCMouseCharChunk; break;
	case F_MOUSE_CHUNK:
		t_new_function = new MCMouseChunk; break;
	case F_MOUSE_CLICK:
		t_new_function = new MCMouseClick; break;
	case F_MOUSE_COLOR:
		t_new_function = new MCMouseColor; break;
	case F_MOUSE_CONTROL:
		t_new_function = new MCMouseControl; break;
	case F_MOUSE_H:
		t_new_function = new MCMouseH; break;
	case F_MOUSE_LINE:
		t_new_function = new MCMouseLine; break;
	case F_MOUSE_LOC:
		t_new_function = new MCMouseLoc; break;
	case F_MOUSE_STACK:
		t_new_function = new MCMouseStack; break;
	case F_MOUSE_TEXT:
		t_new_function = new MCMouseText; break;
	case F_MOUSE_V:
		t_new_function = new MCMouseV; break;
	case F_MOVIE:
		t_new_function = new MCMovie; break;
	case F_MOVING_CONTROLS:
		t_new_function = new MCMovingControls; break;
    case F_NATIVE_CHAR_TO_NUM:
        t_new_function = new MCNativeCharToNum; break;
    case F_NATURAL_SCROLLING:
        t_new_function = new MCNaturalScrolling; break;
	case F_NUM_TO_CHAR:
		t_new_function = new MCNumToChar; break;
    case F_NUM_TO_NATIVE_CHAR:
        t_new_function = new MCNumToNativeChar; break;
    case F_NUM_TO_UNICODE_CHAR:
        t_new_function = new MCNumToUnicodeChar; break;
	case F_NUM_TO_BYTE:
		t_new_function = new MCNumToByte; break;
	case F_OFFSET:
		t_new_function = new MCOffset; break;
	case F_OPEN_FILES:
		t_new_function = new MCOpenFiles; break;
	case F_OPEN_PROCESSES:
		t_new_function = new MCOpenProcesses; break;
	case F_OPEN_PROCESS_IDS:
		t_new_function = new MCOpenProcessIds; break;
	case F_OPEN_SOCKETS:
		t_new_function = new MCOpenSockets; break;
	case F_OPEN_STACKS:
		t_new_function = new MCOpenStacks; break;
	case F_OPTION_KEY:
		t_new_function = new MCOptionKey; break;
	// MW-2008-11-05: [[ Owner Reference ]] Create a new MCOwner function object for syntax of
	//   the form 'the owner of ...'. 
	case F_OWNER:
		t_new_function = new MCOwner; break;
	case F_PA:
		t_new_function = new MCPeerAddress; break;
    case F_PARAGRAPH_OFFSET:
        t_new_function = new MCParagraphOffset; break;
	case F_PARAM:
		t_new_function = new MCParam; break;
	case F_PARAMS:
		t_new_function = new MCParams; break;
	case F_PARAM_COUNT:
		t_new_function = new MCParamCount; break;
	case F_NOTIFICATION_PERMISSION:
		t_new_function = new MCNotificationPermissionFunc; break;
	case F_PENDING_MESSAGES:
		t_new_function = new MCPendingMessages; break;
	case F_PLATFORM:
		t_new_function = new MCPlatform; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'populationStdDev'
	case F_POP_STD_DEV:
			t_new_function = new MCPopulationStdDev; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'populationVariance'
	case F_POP_VARIANCE:
		t_new_function = new MCPopulationVariance; break;
	case F_POWER_SOURCE:
		t_new_function = new MCPowerSource; break;
	case F_PROCESSOR:
		t_new_function = new MCProcessor; break;
	case F_PROCESS_ID:
		t_new_function = new MCPid; break;
	case F_PROPERTY_NAMES:
		t_new_function = new MCPropertyNames; break;
	case F_QT_EFFECTS:
		t_new_function = new MCQTEffects; break;
	case F_QT_VERSION:
		t_new_function = new MCQTVersion; break;
	case F_QUERY_REGISTRY:
		t_new_function = new MCQueryRegistry; break;
	case F_RANDOM:
		t_new_function = new MCRandom; break;
	case F_RECORD_COMPRESSION_TYPES:
		t_new_function = new MCRecordCompressionTypes; break;
    case F_RECORD_FORMATS:
        t_new_function = new MCRecordFormats; break;
	case F_RECORD_LOUDNESS:
		t_new_function = new MCRecordLoudness; break;
	case F_REPLACE_TEXT:
		t_new_function = new MCReplaceText; break;
	case F_RESULT:
		t_new_function = new MCTheResult; break;
	case F_RETRIEVE_CREDENTIAL:
		t_new_function = new MCRetrieveCredential; break;
	case F_ROUND:
		t_new_function = new MCRound; break;
	case F_SCREEN_COLORS:
		t_new_function = new MCScreenColors; break;
	case F_SCREEN_DEPTH:
		t_new_function = new MCScreenDepth; break;
	case F_SCREEN_LOC:
		t_new_function = new MCScreenLoc; break;
	case F_SCREEN_NAME:
		t_new_function = new MCScreenName; break;
	case F_SCREEN_RECT:
	case F_SCREEN_RECTS:
		t_new_function = new MCScreenRect(which == F_SCREEN_RECTS); break;
	case F_SCREEN_TYPE:
		t_new_function = new MCScreenType; break;
	case F_SCREEN_VENDOR:
		t_new_function = new MCScreenVendor; break;
	case F_SCRIPT_LIMITS:
		t_new_function = new MCScriptLimits; break;
	case F_SECONDS:
		t_new_function = new MCSeconds; break;
	case F_SELECTED_BUTTON:
		t_new_function = new MCSelectedButton; break;
	case F_SELECTED_CHUNK:
		t_new_function = new MCSelectedChunk; break;
	case F_SELECTED_FIELD:
		t_new_function = new MCSelectedField; break;
	case F_SELECTED_IMAGE:
		t_new_function = new MCSelectedImage; break;
	case F_SELECTED_LINE:
		t_new_function = new MCSelectedLine; break;
	case F_SELECTED_LOC:
		t_new_function = new MCSelectedLoc; break;
	case F_SELECTED_OBJECT:
		t_new_function = new MCSelectedObject; break;
	case F_SELECTED_TEXT:
		t_new_function = new MCSelectedText; break;
    case F_SENTENCE_OFFSET:
        t_new_function = new MCSentenceOffset; break;
	case F_SET_REGISTRY:
		t_new_function = new MCSetRegistry; break;
	case F_SET_RESOURCE:
		t_new_function = new MCSetResource; break;
	case F_SHA1_DIGEST:
		t_new_function = new MCSHA1Digest; break;
	case F_SHELL:
		t_new_function = new MCShell; break;
	case F_SHIFT_KEY:
		t_new_function = new MCShiftKey; break;
	case F_SHORT_FILE_PATH:
		t_new_function = new MCShortFilePath; break;
	case F_SIN:
		t_new_function = new MCSin; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'sampleStdDev' (was stdDev)
	case F_SMP_STD_DEV:
		t_new_function = new MCSampleStdDev; break;
	// JS-2013-06-19: [[ StatsFunctions ]] Constructor for 'sampleVariance'
	case F_SMP_VARIANCE:
		t_new_function = new MCSampleVariance; break;
	case F_SOUND:
		t_new_function = new MCSound; break;
	case F_SPECIAL_FOLDER_PATH:
		t_new_function = new MCSpecialFolderPath; break;
	case F_SQRT:
		t_new_function = new MCSqrt; break;
	case F_STACKS:
		t_new_function = new MCStacks; break;
	case F_STORE_CREDENTIAL:
		t_new_function = new MCStoreCredential; break;
	case F_STACK_SPACE:
		t_new_function = new MCStackSpace; break;
	case F_STAT_ROUND:
		t_new_function = new MCStatRound; break;
	case F_SUM:
		t_new_function = new MCSum; break;
	case F_SYS_ERROR:
		t_new_function = new MCSysError; break;
	case F_SYSTEM_VERSION:
		t_new_function = new MCSystemVersion; break;
	case F_TAN:
		t_new_function = new MCTan; break;
	case F_TARGET:
		t_new_function = new MCTarget; break;
	case F_TEMP_NAME:
		t_new_function = new MCTempName; break;
    case F_TEXT_DECODE:
        t_new_function = new MCTextDecode; break;
    case F_TEXT_ENCODE:
        t_new_function = new MCTextEncode; break;
	case F_TEXT_HEIGHT_SUM:
		t_new_function = new MCTextHeightSum; break;
	case F_TICKS:
		t_new_function = new MCTicks; break;
	case F_TIME:
		t_new_function = new MCTheTime; break;
    case F_TOKEN_OFFSET:
        t_new_function = new MCTokenOffset; break;
    case F_TOP_STACK:
		t_new_function = new MCTopStack; break;
	case F_TO_LOWER:
		t_new_function = new MCToLower; break;
	case F_TO_UPPER:
		t_new_function = new MCToUpper; break;
	case F_TRANSPOSE:
		t_new_function = new MCTranspose; break;
    case F_TRUEWORD_OFFSET:
        t_new_function = new MCTrueWordOffset; break;
	case F_TRUNC:
		t_new_function = new MCTrunc; break;
    case F_UNICODE_CHAR_TO_NUM:
        t_new_function = new MCUnicodeCharToNum; break;
	// MDW-2014-08-23 : [[ feature_floor ]]
	case F_FLOOR:
		t_new_function = new MCFloor; break;
	// MDW-2014-08-23 : [[ feature_floor ]]
	case F_CEIL:
		t_new_function = new MCCeil; break;
	case F_VALUE:
		t_new_function = new MCValue; break;
	case F_VARIABLES:
		t_new_function = new MCVariables; break;
    case F_VECTOR_DOT_PRODUCT:
        t_new_function = new MCVectorDotProduct; break;
	case F_VERSION:
		t_new_function = new MCVersion; break;
	case F_WAIT_DEPTH:
		t_new_function = new MCWaitDepth; break;
	case F_WEEK_DAY_NAMES:
		t_new_function = new MCWeekDayNames; break;
	case F_WITHIN:
		t_new_function = new MCWithin; break;
	case F_WORD_OFFSET:
		t_new_function = new MCWordOffset; break;
	case F_UNI_DECODE:
		t_new_function = new MCUniDecode; break;
	case F_UNI_ENCODE:
		t_new_function = new MCUniEncode; break;
	case F_URL_DECODE:
		t_new_function = new MCUrlDecode; break;
	case F_URL_ENCODE:
		t_new_function = new MCUrlEncode; break;
	case F_URL_STATUS:
		t_new_function = new MCUrlStatus; break;
	case F_RANDOM_BYTES:
		t_new_function = new MCRandomBytes; break;
	case F_CONTROL_AT_LOC:
		t_new_function = new MCControlAtLoc(false); break;
	case F_CONTROL_AT_SCREEN_LOC:
		t_new_function = new MCControlAtLoc(true); break;
	// MW-2013-05-08: [[ Uuid ]] Constructor for uuid function.
	case F_UUID:
		t_new_function = new MCUuidFunc; break;
    // MERG-2013-08-14: [[ MeasureText ]] Measure text relative to the effective font on an object
    case F_MEASURE_TEXT:
        t_new_function = new MCMeasureText(false); break;
    case F_MEASURE_UNICODE_TEXT:
        t_new_function = new MCMeasureText(true); break;
    case F_NORMALIZE_TEXT:
        t_new_function = new MCNormalizeText; break;
    case F_CODEPOINT_PROPERTY:
        t_new_function = new MCCodepointProperty; break;
    default:
		break;
	}


    // SN-2014-11-25: [[ Bug 14088 ]] A NULL pointer is returned if no function exists.
    //  (that avoids to get a MCFunction which does not implement eval_ctxt).
    // Fall back to mode-specific functions only if the main switch didn't match.
    if (t_new_function == nullptr)
        t_new_function = MCModeNewFunction(which);

    // Post-assign the func_id so hxt_serialize writes the correct Functions enum
    // value without per-subclass boilerplate.  All paths that produce a non-null
    // result (main switch + MCModeNewFunction) yield MCFunction subclasses.
    if (t_new_function != nullptr)
        static_cast<MCFunction*>(t_new_function)->m_hxt_func_id = which;

    return t_new_function;
}

MCExpression *MCN_new_operator(int2 which)
{
	switch (which)
	{
	case O_AND:
		return new MCAnd;
	case O_AND_BITS:
		return new MCAndBits;
	case O_CONCAT:
		return new MCConcat;
	case O_CONCAT_SPACE:
		return new MCConcatSpace;
	case O_CONTAINS:
		return new MCContains;
	case O_DIV:
		return new MCDiv;
	case O_EQ:
		return new MCEqual;
	case O_GE:
		return new MCGreaterThanEqual;
	case O_GROUPING:
		return new MCGrouping;
	case O_GT:
		return new MCGreaterThan;
	case O_IS:
		return new MCIs;
	case O_LE:
		return new MCLessThanEqual;
	case O_LT:
		return new MCLessThan;
	case O_MINUS:
		return new MCMinus;
	case O_MOD:
		return new MCMod;
	case O_WRAP:
		return new MCWrap;
	case O_NE:
		return new MCNotEqual;
	case O_NOT:
		return new MCNot;
	case O_NOT_BITS:
		return new MCNotBits;
	case O_OR:
		return new MCOr;
	case O_OR_BITS:
		return new MCOrBits;
	case O_OVER:
		return new MCOver;
	case O_PLUS:
		return new MCPlus;
	case O_POW:
		return new MCPow;
	case O_THERE:
		return new MCThere;
	case O_TIMES:
		return new MCTimes;
	case O_XOR_BITS:
		return new MCXorBits;
	case O_BEGINS_WITH:
		return new MCBeginsWith;
	case O_ENDS_WITH:
		return new MCEndsWith;
	case O_MATCHES:
		return new MCMatches;
	default:
		return new MCExpression;
	}
}
