#ifndef _extcode_H
#define _extcode_H
/**

	© Copyright 1990-2006 by National Instruments Corp.
	All rights reserved.

	@file	extcode.h
	@brief	This document reflects what is published in the CIN manual.
		DO NOT CHANGE
*/

#define rcsid_extcode "$Id: //lvdist/branches/Europa/dev/plat/win/cintools/extcode.h#3 $"

/* define this to keep C++ isms from littering the code */
#ifdef __cplusplus
/* single instance extern C functions: */
	#define EXTERNC extern "C"
/* single instance extern C VARIABLES (needed because MSVC unnecessarily
   mangles global variable names): */
	#define EEXTERNC extern "C"
/* begin of extern c block: */
	#define BEGINEXTERNC extern "C" {
/* end of externc block */
	#define ENDEXTERNC }
/* default function argument value */
	#define DEFAULTARG(a)	= a
#else
	#define EXTERNC
	#define EEXTERNC extern
	#define BEGINEXTERNC
	#define ENDEXTERNC
	#define DEFAULTARG(a)
#endif

#include "platdefines.h"
#include "fundtypes.h"

#if !defined(_FUNCC)
	#if MSWin && (ProcessorType == kX86)
		#define _FUNCC __cdecl
	#else
		#define _FUNCC
	#endif
#endif

BEGINEXTERNC

#ifdef CIN_VERS
	#if MSWin && (ProcessorType == kX86)
		#pragma pack(1)
	#endif
#endif

/* Multi-threaded categorization macros */
#ifndef TH_NOTNEEDED
/* Function tags */
#define TH_REENTRANT  /* function is completely reentrant and calls only same */
#define TH_PROTECTED  /* function uses mutex calls protect itself */

#define TH_SINGLE_UI  /* function is known to be used only from UI thread */

#define TH_UNSAFE	  /* function is NOT thread safe and needs to be made so */

/* Variable tags */
#define TH_READONLY	  /* (global) variable is never written to after startup */
#endif


/* manager.h *=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*=-=*/
#if Mac
/*
// 2002.06.11: Because the gcc compiler and the Metrowerks compiler don't exactly
//		agree on the powerpc alignment rules (which are complicated when scalars
//		larger than 4 bytes are involved), we have decided to use "native" (or
//		"natural") alignment, which uses the same simple rules as are used on other 
//		platforms. Both gcc and Metrowerks seems to agree on these rules.
//		(You can read the rules in "MachORuntime.pdf" available at Apple's Developer
//		web site http://developer.apple.com/techpubs/. Search for "machoruntime".)
//
//		A simple struct that they disagree on is as follows:
//		(Tests done with gcc 3.1 and MWerks Pro 7)
//		typedef struct {
//			char charZero;
//			struct {
//				double doubleInStruct;	// gcc aligns this to 4, MWerks to 8
//				} embeddedStruct;
//			} FunnyStruct;
//		
//		Consequences:
//		-	A LabVIEW defined struct can be used as an alias for a system defined
//			struct only when no members are larger than 4 bytes. The LabVIEW source
//			code does not currently do this. Users writing CINs and DLLs should avoid
//			this as well, or at least be aware of the potential problems.
//		-	Neither of the compilers mentioned above have a command line or GUI option
//			for setting alignment to "native". So we set it via a pragma here.
//		-	Definitions of structs that may be affected by the difference in alignment
//			should never be made before this point in the include files.
//		-	Any other setting of the aligment options pragma must be temporary (should
//			always be matched by a #pragma options align = reset).
*/ 
		#if (Compiler == kGCC) || (Compiler == kMetroWerks)
			#pragma options align = natural
		#else
			#error "Unsupported compiler. Figure out how to set alignment to native/natural"
		#endif
/* these must be defined before we start including Mac headers */
	#ifndef ACCESSOR_CALLS_ARE_FUNCTIONS
		#define ACCESSOR_CALLS_ARE_FUNCTIONS 1
	#endif
	#ifndef OPAQUE_TOOLBOX_STRUCTS
		#define OPAQUE_TOOLBOX_STRUCTS 1
	#endif
	#include <MacTypes.h>
	typedef const uChar	ConstStr255[256];
#else
	/* All of these types are provided by Types.h on the Mac */
	typedef uChar		Str255[256], Str31[32], *StringPtr, **StringHandle;
	typedef const uChar	ConstStr255[256], *ConstStringPtr;
	typedef uInt32		ResType;
#endif

/*
Manager Error Codes are the error codes defined in labview-errors.txt
ranging from Error Code 1 to Error Code mgErrSentinel-1 (defined below). 
These codes are globally defined in the NI Error Codes Database. 
THE labview-errors.txt FILE IS AUTOMAGICALLY GENERATED FROM THE
DATABASE... DO NOT EDIT THEM DIRECTLY. When a new manager error
code is created there, this enum of values is updated to permit the
manager errors from being generated from DLLs and other external
blocks of code.
*/
enum {					/* Manager Error Codes */
	mgNoErr=0,
#if !Mac
	noErr = mgNoErr,	/* We need to move away from this, Mac OS headers already define it */
#endif
	mgArgErr=1,
	mFullErr,			/* Memory Mgr	2-3		*/
	mZoneErr,

	fEOF,				/* File Mgr		4-12	*/
	fIsOpen,
	fIOErr,
	fNotFound,
	fNoPerm,
	fDiskFull,
	fDupPath,
	fTMFOpen,
	fNotEnabled,

	rFNotFound,			/* Resource Mgr 13-15	*/
	rAddFailed,
	rNotFound,

	iNotFound,			/* Image Mgr	16-17	*/
	iMemoryErr,

	dPenNotExist,		/* Draw Mgr		18		*/

	cfgBadType,			/* Config Mgr	19-22	*/
	cfgTokenNotFound,
	cfgParseError,
	cfgAllocError,

	ecLVSBFormatError,	/* extCode Mgr	23		*/
	ecLVSBSubrError,	/* extCode Mgr	24		*/
	ecLVSBNoCodeError,	/* extCode Mgr	25		*/

	wNullWindow,		/* Window Mgr	26-27	*/
	wDestroyMixup,

	menuNullMenu,		/* Menu Mgr		28		*/

	pAbortJob,			/* Print Mgr	29-35	*/
	pBadPrintRecord,
	pDriverError,
	pWindowsError,
	pMemoryError,
	pDialogError,
	pMiscError,

	dvInvalidRefnum,	/* Device Mgr	36-41	*/
	dvDeviceNotFound,
	dvParamErr,
	dvUnitErr,
	dvOpenErr,
	dvAbortErr,

	bogusError,			/* generic error 42 */
	cancelError,		/* cancelled by user 43 */

	OMObjLowErr,		/* object message dispatcher errors 44-52 */
	OMObjHiErr,
	OMObjNotInHeapErr,
	OMOHeapNotKnownErr,
	OMBadDPIdErr,
	OMNoDPinTabErr,
	OMMsgOutOfRangeErr,
	OMMethodNullErr,
	OMUnknownMsgErr,

	mgNotSupported,

	ncBadAddressErr,		/* Net Connection errors 54-66 */
	ncInProgress,
	ncTimeOutErr,
	ncBusyErr,
	ncNotSupportedErr,
	ncNetErr,
	ncAddrInUseErr,
	ncSysOutOfMem,
	ncSysConnAbortedErr,	/* 62 */
	ncConnRefusedErr,
	ncNotConnectedErr,
	ncAlreadyConnectedErr,
	ncConnClosedErr,		/* 66 */

	amInitErr,				/* (Inter-)Application Message Manager 67- */

	occBadOccurrenceErr,	/* 68  Occurrence Mgr errors */
	occWaitOnUnBoundHdlrErr,
	occFunnyQOverFlowErr,

	fDataLogTypeConflict,	/* 71 */
	ecLVSBCannotBeCalledFromThread, /* ExtCode Mgr	72*/
	amUnrecognizedType,
	mCorruptErr,
	ecLVSBErrorMakingTempDLL,
	ecLVSBOldCIN,			/* ExtCode Mgr	76*/

	dragSktNotFound,		/* Drag Manager 77 - 80*/
	dropLoadErr,
	oleRegisterErr,
	oleReleaseErr,

	fmtTypeMismatch,		/* String processing (printf, scanf) errors */
	fmtUnknownConversion,
	fmtTooFew,
	fmtTooMany,
	fmtScanError,

	ecLVSBFutureCIN,		/* ExtCode Mgr	86*/

	lvOLEConvertErr,
	rtMenuErr,
	pwdTampered,			/* Password processing */
	LvVariantAttrNotFound,		/* LvVariant attribute not found 90-91*/
	LvVariantTypeMismatch,		/* Cannot convert to/from type */

	axEventDataNotAvailable,	/* Event Data Not Available 92-96*/
	axEventStoreNotPresent,		/* Event Store Not Present */
	axOccurrenceNotFound,		/* Occurence Not Found */
	axEventQueueNotCreated,		/* Event Queue not created */
	axEventInfoNotAvailable,	/* Event Info is not available */
		
	oleNullRefnumPassed,		/* Refnum Passed is Null */

	omidGetClassGUIDErr,		/* Error retrieving Class GUID from OMId 98-100*/
	omidGetCoClassGUIDErr,		/* Error retrieving CoClass GUID from OMId */
	omidGetTypeLibGUIDErr,		/* Error retrieving TypeLib GUID from OMId */
		
	appMagicBad,				/* bad built application magic bytes */

	iviInvalidDowncast,         /* IVI Invalid downcast*/
	iviInvalidClassSesn,		/* IVI No Class Session Opened */
		
	maxErr,						/* max manager 104-107 */
	maxConfigErr,				/* something not right with config objects */
	maxConfigLoadErr,			/* could not load configuration */
	maxGroupNotSupported,		

	ncSockNotMulticast,			/* net connection multicast specific errors 108-112 */
	ncSockNotSinglecast,
	ncBadMulticastAddr,
	ncMcastSockReadOnly,
	ncMcastSockWriteOnly,

	ncDatagramMsgSzErr,			/* net connection Datagram message size error 113 */

	bufferEmpty,				/* CircularLVDataBuffer (queues/notifiers) */
	bufferFull,					/* CircularLVDataBuffer (queues/notifiers) */
	dataCorruptErr,				/* error unflattening data */

	requireFullPathErr,			/* supplied folder path where full file path is required  */
	folderNotExistErr,			/* folder doesn't exist */

	ncBtInvalidModeErr,			/* invalid Bluetooth mode 119 */
	ncBtSetModeErr,				/* error setting Bluetooth mode 120 */

	mgBtInvalidGUIDStrErr,		/* The GUID string is invalid 121 */

	rVersInFuture,			/* Resource file is a future version 122 */

	mgErrSentinel,

	mgPrivErrBase = 500,	/* Start of Private Errors */
	mgPrivErrLast = 599,	/* Last allocated in Error DB */


	kAppErrorBase = 1000,	/* Start of application errors */
	kAppLicenseErr = 1380,	/* Failure to check out license error */
	kAppCharsetConvertErr =1396, /* could not convert text from charset to charset */
	kAppErrorLast = 1399	/* Last allocated in Error DB */
};


typedef int32	MgErr;

typedef enum {	iB=1, iW, iL, iQ, uB, uW, uL, uQ, fS, fD, fX, cS, cD, cX } NumType;

#define Private(T)	typedef struct T##_t { void *p; } *T
#define PrivateH(T)	 struct T##_t; typedef struct T##_t **T
#define PrivateP(T)	 struct T##_t; typedef struct T##_t *T
#define PrivateFwd(T)	typedef struct T##_t *T /* for forward declarations */

/** @struct Path 
Opaque type used by the path manager. See pathmgr.cpp
declared here for use in constant table */
PrivateH(Path);

/* forward definitions of LvVariant for DLLs and CINs */
#ifdef __cplusplus
class LvVariant;
#else
typedef struct LVVARIANT LvVariant;
#endif
typedef LvVariant* LvVariantPtr;

/* forward definitions of HWAVEFORM and HWAVES for DLLs and CINs */
typedef struct IWaveform IWaveform;
typedef struct IWaves IWaves;

typedef IWaveform* HWAVEFORM;
typedef IWaves* HWAVES;

/* forward definitions of ATime128 (time stamp) for DLLs and CINs */
#ifdef __cplusplus
class ATime128;
#else
typedef struct ATIME128 ATime128;
#endif
typedef ATime128* ATime128Ptr;


typedef struct {
	float32 re, im;
} cmplx64;

typedef struct {
	float64 re, im;
} cmplx128;

typedef struct {
	floatExt	re, im;
} cmplxExt;

typedef struct {
	int32	cnt;		/* number of bytes that follow */
	uChar	str[1];		/* cnt bytes */
} LStr, *LStrPtr, **LStrHandle;
typedef LStr const* ConstLStrP;
typedef LStr const*const* ConstLStrH;

typedef uChar		*UPtr, **UHandle;
typedef uChar		*PStr, **PStrHandle, *CStr;
typedef const uChar	*ConstPStr, *ConstCStr, *ConstUPtr, **ConstPStrHandle;
/*
	The following function pointer types match AZ and DS handle allocation
	prototypes.
*/
typedef TH_REENTRANT UHandle	(_FUNCC *AllocProcPtr)(int32);
typedef TH_REENTRANT MgErr		(_FUNCC *DeallocProcPtr)(void *);
typedef TH_REENTRANT MgErr		(_FUNCC *ReallocProcPtr)(void *, int32);
typedef TH_REENTRANT int32		(_FUNCC *AllocSizeProcPtr)(const void *);

typedef struct {
	int32	cnt;		/* number of pascal strings that follow */
	uChar	str[1];		/* cnt bytes of concatenated pascal strings */
} CPStr, *CPStrPtr, **CPStrHandle;

/*** The Support Manager ***/

#define HiNibble(x)		(uInt8)(((x)>>4) & 0x0F)
#define LoNibble(x)		(uInt8)((x) & 0x0F)
#define HiByte(x)			((int8)((int16)(x)>>8))
#define LoByte(x)			((int8)(x))
#define Word(hi,lo)		(((int16)(hi)<<8) | ((int16)(uInt8)(lo)))
#define Hi16(x)			((int16)((int32)(x)>>16))
#define Lo16(x)			((int16)(int32)(x))
#define Long(hi,lo)		(((int32)(hi)<<16) | ((int32)(uInt16)(lo)))

#define Cat4Chrs(c1,c2,c3,c4)	(((int32)(uInt8)(c1)<<24)|((int32)(uInt8)(c2)<<16)|((int32)(uInt8)(c3)<<8)|((int32)(uInt8)(c4)))
#define Cat2Chrs(c1,c2)			(((int16)(uInt8)(c1)<<8)|((int16)(uInt8)(c2)))

#if BigEndian
#define RTToL(c1,c2,c3,c4)	Cat4Chrs(c1,c2,c3,c4)
#define RTToW(c1,c2)		Cat2Chrs(c1,c2)
#else
#define RTToL(c1,c2,c3,c4)	Cat4Chrs(c4,c3,c2,c1)
#define RTToW(c1,c2)		Cat2Chrs(c2,c1)
#endif

#define Offset(type, field)		((int32)&((type*)0)->field)
#if Compiler==kGCC && (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
// This is a GCC compiler extension when used on non-static fields to get the
// offset of the field in the class.  (Using the pointer cast of 0 generates a
// warnings when used on a C++ class that uses inheritance, so I introduced
// this new macro to get rid of the warning. CS 5/16/02
#define COffset(type, field)		((int32)&type::field)
#else
#define COffset(type, field)		((int32)&((type*)0)->field)
#endif

#if (ProcessorType==kX86) || (ProcessorType==kM68000)
	#define UseGetSetIntMacros	1
#else
	#define UseGetSetIntMacros	0
#endif

#if UseGetSetIntMacros
	#define GetAWord(p)			(*(int16*)(p))
	#define SetAWord(p,x)		(*(int16*)(p)= x)
	#define GetALong(p)			(*(int32*)(p))
	#define SetALong(p,x)		(*(int32*)(p)= x)
#else
TH_REENTRANT int32 _FUNCC	GetALong(const void *);
TH_REENTRANT int32 _FUNCC	SetALong(void *, int32);
TH_REENTRANT int16 _FUNCC	GetAWord(const void *);
TH_REENTRANT int16 _FUNCC	SetAWord(void *, int16);
#endif

#if !Palm
TH_REENTRANT int32 _FUNCC	Abs(int32);
TH_REENTRANT int32 _FUNCC	Min(int32, int32);
TH_REENTRANT int32 _FUNCC	Max(int32, int32);
TH_REENTRANT int32 _FUNCC	Pin(int32, int32, int32);
TH_REENTRANT Bool32 _FUNCC	IsDigit(uChar);
TH_REENTRANT Bool32 _FUNCC	IsAlpha(uChar);
TH_REENTRANT Bool32 _FUNCC	IsPunct(uChar);
TH_REENTRANT Bool32 _FUNCC	IsLower(uChar);
TH_REENTRANT Bool32 _FUNCC	IsUpper(uChar);
TH_REENTRANT int32 _FUNCC	ToUpper(int32);
TH_REENTRANT int32 _FUNCC	ToLower(int32);
TH_REENTRANT uChar _FUNCC	HexChar(int32);
TH_REENTRANT int32 _FUNCC	StrLen(const uChar *);
TH_REENTRANT int32 _FUNCC	StrCat(CStr, ConstCStr);
TH_REENTRANT CStr _FUNCC		StrCpy(CStr, ConstCStr);
TH_REENTRANT CStr _FUNCC	StrNCpy(CStr, ConstCStr, size_t);
TH_REENTRANT int32 _FUNCC	StrCmp(ConstCStr, ConstCStr);
TH_REENTRANT int32 _FUNCC	StrNCmp(ConstCStr, ConstCStr, size_t);
TH_REENTRANT int32 _FUNCC	StrNCaseCmp(ConstCStr, ConstCStr, size_t);
TH_REENTRANT int32 _FUNCC	StrIStr(ConstCStr s, ConstCStr r);
#endif

/* RandomGen is not truly reentrant but safe enough for our purposes */
TH_REENTRANT void _FUNCC		RandomGen(float64*);

/* FileNameCmp compares two PStr's with the same case-sensitivity as */
/* the filesystem. */
/* FileNameNCmp compares two CStr's (to length n) with the same */
/* case-sensitivity as the filesystem. */
/* FileNameIndCmp compares two PStr's (passing pointers to the string */
/* pointers) with the same case-sensitivity as the filesystem. */
#if Mac || MSWin
#define FileNameCmp PStrCaseCmp
#define FileNameNCmp StrNCaseCmp
#define FileNameIndCmp PPStrCaseCmp
#elif Unix
#define FileNameCmp PStrCmp
#define FileNameNCmp StrNCmp
#define FileNameIndCmp PPStrCmp
#endif

#if __cplusplus
ENDEXTERNC
inline uChar*       PStrBuf(const PStr b) { return &(b[1]); }
inline uChar&       PStrLen(const PStr b) { return b[0]; } // # of chars in string
inline const uChar* PStrBuf(const ConstPStr b) { return &(b[1]); }
inline uChar        PStrLen(const ConstPStr b) { return b[0]; } // # of chars in string
inline uInt16       PStrSize(const ConstPStr b) { return (uInt16)(b?PStrLen(b)+1:0); } // # of bytes including length; 
																				// The return type is uInt16, not uChar. It cannot be uChar since 
																				//	a) Maximum PStr length is 255
																				//	b) PStrSize is length plus 1
																				//	c) 255 + 1 = 256
																				//	d) 256 is greater than the maximum value of a uChar (255)
BEGINEXTERNC
#else
#define PStrBuf(b)		(&((PStr)(b))[1])
#define PStrLen(b)		(((PStr)(b))[0])				/* # of chars in string */
#define PStrSize(b)		(b?PStrLen(b)+1:0)				/* # of bytes including length */
#endif // __cplusplus

/* Helpers for LStr handles that allow for empty strings encoded as NULL handles.
Use LHStr macros instead of passing a *h to LStrBuf/Len/Size for right hand side refs. */
#define LHStrPtr(sh)	((sh)?*(sh):NULL)				/* Get LStr* (produces NULL ptr for NULL handle */
#define LHStrBuf(sh)	((sh)?(&(*(sh))->str[0]):NULL)	/* same as LStrBuf, but on string handle */
#define LHStrLen(sh)	((sh)?((*(sh))->cnt):0)			/* same as LStrLen, but on string handle */
#define LHStrSize(sh)	(LHStrLen(sh)+sizeof(int32))	/* same as LStrSize, but on string handle */

#define LStrBuf(sp)		(&((sp))->str[0])				/* pointer to first char of string */
#define LStrLen(sp)		(((sp))->cnt)					/* # of chars in string */
#define LStrSize(sp)	(LStrLen(sp)+sizeof(int32))		/* # of bytes including length */

#define CPStrLen		LStrLen			/* concatenated Pascal vs. LabVIEW strings */
#define CPStrBuf		LStrBuf			/* concatenated Pascal vs. LabVIEW strings */

TH_REENTRANT int32 _FUNCC	PStrCat(PStr, ConstPStr);
TH_REENTRANT PStr _FUNCC		PStrCpy(PStr, ConstPStr);
TH_REENTRANT PStr _FUNCC		PStrNCpy(PStr, ConstPStr, int32);
TH_REENTRANT int32 _FUNCC	PStrCmp(ConstPStr, ConstPStr);
TH_REENTRANT int32 _FUNCC	PPStrCmp(ConstPStr*, ConstPStr*);
TH_REENTRANT int32 _FUNCC	PStrCaseCmp(ConstPStr, ConstPStr);
TH_REENTRANT int32 _FUNCC	PPStrCaseCmp(ConstPStr*, ConstPStr*);
TH_REENTRANT int32 _FUNCC	LStrCmp(ConstLStrP, ConstLStrP);
TH_REENTRANT int32 _FUNCC	LStrCaseCmp(ConstLStrP, ConstLStrP);
TH_REENTRANT int32 _FUNCC	PtrLenStrCmp(uChar*, int32, uChar*, int32);
TH_REENTRANT int32 _FUNCC	PtrLenStrCaseCmp(uChar*, int32, uChar*, int32);
TH_REENTRANT int32 _FUNCC	LHStrCmp(ConstLStrH, ConstLStrH);
TH_REENTRANT int32 _FUNCC	LHStrCaseCmp(ConstLStrH, ConstLStrH);
TH_REENTRANT int32 _FUNCC	CPStrSize(CPStrPtr);
TH_REENTRANT int32 _FUNCC	CPStrCmp(CPStrPtr, CPStrPtr);
TH_REENTRANT MgErr _FUNCC	CPStrInsert(CPStrHandle, ConstPStr, int32);
TH_REENTRANT void _FUNCC		CPStrRemove(CPStrHandle, int32);
TH_REENTRANT PStr _FUNCC		CPStrIndex(CPStrHandle, int32);
TH_REENTRANT MgErr _FUNCC	CPStrReplace(CPStrHandle, ConstPStr, int32);
TH_REENTRANT int32 _FUNCC	BlockCmp(const void * p1, const void * p2, int32 n);

TH_REENTRANT int32 _FUNCC	CToPStr(ConstCStr, PStr);
TH_REENTRANT int32 _FUNCC	CToLStr(ConstCStr, LStrPtr);
TH_REENTRANT int32 _FUNCC	LToCStr(ConstLStrP, CStr);
TH_REENTRANT int32 _FUNCC	LToPStr(ConstLStrP, PStr);
TH_REENTRANT int32 _FUNCC	PToLStr(ConstPStr, LStrPtr);
TH_REENTRANT int32 _FUNCC	PToCStr(ConstPStr, CStr);
TH_REENTRANT PStr _FUNCC		PStrDup(ConstPStr buf);
TH_REENTRANT MgErr _FUNCC	PStrToDSLStr(PStr buf, LStrHandle *lStr);

TH_REENTRANT MgErr _FUNCC	DbgPrintf(const char *buf, ...);
TH_REENTRANT int32 _FUNCC	SPrintf(CStr, ConstCStr, ...);
TH_REENTRANT int32 _FUNCC	SPrintfp(CStr, ConstPStr, ...);
TH_REENTRANT int32 _FUNCC	PPrintf(PStr, ConstCStr, ...);
TH_REENTRANT int32 _FUNCC	PPrintfp(PStr, ConstPStr, ...);
TH_REENTRANT MgErr _FUNCC	LStrPrintf(LStrHandle t, CStr fmt, ...);
typedef		int32 (_FUNCC *CompareProcPtr)(const void*, const void*);
TH_REENTRANT void _FUNCC		QSort(void*, int32, int32, CompareProcPtr);
TH_REENTRANT int32 _FUNCC	BinSearch(const void*, int32, int32, const void*, CompareProcPtr);
TH_REENTRANT uInt32 _FUNCC	MilliSecs(void);
TH_REENTRANT uInt32 _FUNCC	TimeInSecs(void);

/*** The Memory Manager ***/

typedef struct {
	int32 totFreeSize, maxFreeSize, nFreeBlocks;
	int32 totAllocSize, maxAllocSize;
	int32 nPointers, nUnlockedHdls, nLockedHdls;
	int32 reserved[4];
} MemStatRec;

/*
For parameters to the memory manager functions below
p means pointer
h means handle
ph means pointer to handle
*/
#if !Palm
TH_REENTRANT MgErr _FUNCC	AZCheckHandle(const void *h);
TH_REENTRANT MgErr _FUNCC	AZCheckPtr(void *p);
TH_REENTRANT MgErr _FUNCC	AZDisposeHandle(void *h);
TH_REENTRANT MgErr _FUNCC	AZDisposePtr(void *p);
TH_REENTRANT size_t _FUNCC	AZGetHandleSize(const void *h);
TH_REENTRANT MgErr _FUNCC	AZHLock(void *h);
TH_REENTRANT MgErr _FUNCC	AZHUnlock(void *h);
TH_REENTRANT void _FUNCC		AZHPurge(void *h);
TH_REENTRANT void _FUNCC		AZHNoPurge(void *h);
TH_REENTRANT UHandle _FUNCC	AZNewHandle(size_t);
TH_REENTRANT UHandle _FUNCC	AZNewHClr(size_t);
TH_REENTRANT UPtr _FUNCC		AZNewPtr(size_t);
TH_REENTRANT UPtr _FUNCC		AZNewPClr(size_t);
TH_REENTRANT UHandle _FUNCC	AZRecoverHandle(void *p);
TH_REENTRANT MgErr _FUNCC	AZSetHandleSize(void *h, size_t);
TH_REENTRANT MgErr _FUNCC	AZSetHSzClr(void *h, size_t);
TH_REENTRANT int32 _FUNCC	AZHeapCheck(Bool32);
TH_REENTRANT size_t _FUNCC	AZMaxMem(void);
TH_REENTRANT MgErr _FUNCC	AZMemStats(MemStatRec *msrp);
TH_REENTRANT MgErr _FUNCC	AZCopyHandle(void *ph, const void *hsrc);
TH_REENTRANT MgErr _FUNCC	AZSetHandleFromPtr(void *ph, const void *psrc, size_t n);

TH_REENTRANT MgErr _FUNCC	DSCheckHandle(const void *h);
TH_REENTRANT MgErr _FUNCC	DSCheckPtr(void *p);
TH_REENTRANT MgErr _FUNCC	DSDisposeHandle(void *h);
TH_REENTRANT MgErr _FUNCC	DSDisposePtr(void *p);
TH_REENTRANT size_t _FUNCC	DSGetHandleSize(const void *h);
TH_REENTRANT UHandle _FUNCC DSNewHandle(size_t);
TH_REENTRANT UHandle _FUNCC	DSNewHClr(size_t);
TH_REENTRANT UPtr _FUNCC		DSNewPtr(size_t);
TH_REENTRANT UPtr _FUNCC		DSNewPClr(size_t);
TH_REENTRANT UHandle _FUNCC	DSRecoverHandle(void *p);
TH_REENTRANT MgErr _FUNCC	DSSetHandleSize(void *h, size_t);
TH_REENTRANT MgErr _FUNCC	DSSetHSzClr(void *h, size_t);
TH_REENTRANT MgErr _FUNCC	DSCopyHandle(void *ph, const void *hsrc);
TH_REENTRANT MgErr _FUNCC	DSSetHandleFromPtr(void *ph, const void *psrc, size_t n);
// DSSetHandleFromPtrNULLMeansEmpty differs from DSSetHandleFromPtr in that 
// if n is zero, the handle will be deallocated and NULL'd out. The regular 
// DSSetHandleFromPtr will allocate a handle for zero bytes. 
TH_REENTRANT MgErr _FUNCC	DSSetHandleFromPtrNULLMeansEmpty(void *ph, const void *psrc, size_t n);
TH_REENTRANT MgErr _FUNCC	DSHeapCheck(Bool32);
TH_REENTRANT size_t _FUNCC	DSMaxMem(void);
TH_REENTRANT MgErr _FUNCC	DSMemStats(MemStatRec *msrp);
TH_REENTRANT void _FUNCC		ClearMem(void*, size_t);
TH_REENTRANT void _FUNCC		MoveBlock(const void *src, void *dest, size_t); // accepts zero bytes without problem
TH_REENTRANT void _FUNCC		SwapBlock(void *src, void *dest, size_t);
#else
TH_REENTRANT UHandle _FUNCC	DSNewHandle(size_t);
TH_REENTRANT UHandle _FUNCC	DSNewHClr(size_t);
TH_REENTRANT UPtr _FUNCC		DSNewPtr(size_t);
TH_REENTRANT MgErr _FUNCC	DSDisposeHandle(void *h);
TH_REENTRANT MgErr _FUNCC	DSDisposePtr(void *p);
TH_REENTRANT MgErr _FUNCC	DSCopyHandle(void *ph, const void *hsrc);
TH_REENTRANT MgErr _FUNCC	DSSetHandleSize(void *h, size_t);
TH_REENTRANT MgErr _FUNCC	DSSetHSzClr(void *h, size_t);
TH_REENTRANT UPtr _FUNCC		DSNewPClr(size_t);
#endif
/*** The Magic Cookie Manager ***/

/** @struct MagicCookie 
Opaque type used by the cookie manager. See cookie.cpp */
Private(MagicCookie);
#define kNotAMagicCookie ((MagicCookie)0L)	/* canonical invalid magic cookie */

/*** The File Manager ***/

/** open modes */
enum { openReadWrite, openReadOnly, openWriteOnly, openWriteOnlyTruncate }; 
/** deny modes */
enum { denyReadWrite, denyWriteOnly, denyNeither };		
/** seek modes */
enum { fStart=1, fEnd, fCurrent };											
/** For access, see fDefaultAccess */

/** Path type codes */
enum {	fAbsPath,
	fRelPath,
	fNotAPath,
	fUNCPath,								/**< uncfilename */
	nPathTypes };							

/** @struct File 
Opaque type used by the file manager. See filemgr.cpp */
Private(File);

/** Used for FGetInfo */
typedef struct {			/**< file/directory information record */
	int32	type;			/**< system specific file type-- 0 for directories */
	int32	creator;		/**< system specific file creator-- 0 for directories */
	int32	permissions;	/**< system specific file access rights */
	int32	size;			/**< file size in bytes (data fork on Mac) or entries in folder */
	int32	rfSize;			/**< resource fork size (on Mac only) */
	uInt32	cdate;			/**< creation date */
	uInt32	mdate;			/**< last modification date */
	Bool32	folder;			/**< indicates whether path refers to a folder */
	Bool32	isInvisible; /**< indicates whether the file is visible in File Dialog */
	struct {
		int16 v, h;
	} location;			/**< system specific geographical location */
	Str255	owner;			/**< owner (in pascal string form) of file or folder */
	Str255	group;			/**< group (in pascal string form) of file or folder */
} FInfoRec, *FInfoPtr;

/** Used for FGetInfo, 64-bit version */
typedef uInt32	FGetInfoWhich;
enum {
	kFGetInfoType			= 1L << 0,
	kFGetInfoCreator		= 1L << 1,
	kFGetInfoPermissions	= 1L << 2,
	kFGetInfoSize			= 1L << 3,
	kFGetInfoRFSize			= 1L << 4,
	kFGetInfoCDate			= 1L << 5,
	kFGetInfoMDate			= 1L << 6,
	kFGetInfoFolder			= 1L << 7,
	kFGetInfoIsInvisible	= 1L << 8,
	kFGetInfoLocation		= 1L << 9,
	kFGetInfoOwner			= 1L << 10,
	kFGetInfoGroup			= 1L << 11,
	kFGetInfoAll			= 0xEFFFFFFFL
};
typedef struct {			/**< file/directory information record */
	int32	type;			/**< system specific file type-- 0 for directories */
	int32	creator;		/**< system specific file creator-- 0 for directories */
	int32	permissions;	/**< system specific file access rights */
	int64	size;			/**< file size in bytes (data fork on Mac) or entries in folder */
	int64	rfSize;			/**< resource fork size (on Mac only) */
	uInt32	cdate;			/**< creation date */
	uInt32	mdate;			/**< last modification date */
	Bool32	folder;			/**< indicates whether path refers to a folder */
	Bool32	isInvisible; /**< indicates whether the file is visible in File Dialog */
	struct {
		int16 v, h;
	} location;			/**< system specific geographical location */
	Str255	owner;			/**< owner (in pascal string form) of file or folder */
	Str255	group;			/**< group (in pascal string form) of file or folder */
} FInfoRec64, *FInfo64Ptr;

/** Used for FGetVolInfo */
typedef struct {
	uInt32	size;			/**< size in bytes of a volume */
	uInt32	used;			/**< number of bytes used on volume */
	uInt32	free;			/**< number of bytes available for use on volume */
} VInfoRec;

/** Used with FListDir2 */
typedef struct {
	int32 flags, type;
} FMFileType;

#if !Mac
/* For backward compatability with old CINs
-- There was a name collision with Navigation Services on MacOS */
#define FileType	FMFileType
#endif /* !Mac */

/** Type Flags used with FMFileType */
#define kIsFile				0x01
#define kRecognizedType		0x02
#define kIsLink				0x04
#define kFIsInvisible		0x08
#define kIsTopLevelVI		0x10	/**< Used only for VIs in archives */
#define kErrGettingType		0x20	/**< error occurred getting type info */
#if Mac
#define kFIsStationery		0x40
#endif

/** Used for converting from NICOM to different flavors of LV-WDT */
enum {
	kWDTUniform =0L,	/*< Uniform Flt64 WDT */
	kArrayWDTUniform	/*< Array of uniform flt64 WDT */
};

/** Used with FExists */
enum {
	kFNotExist = 0L,
	kFIsFile,
	kFIsFolder
};

TH_REENTRANT MgErr _FUNCC	FCreate(File *fdp, Path path, int32 access, int32 openMode, int32 denyMode, PStr group);
TH_REENTRANT MgErr _FUNCC	FCreateAlways(File *fdp, Path path, int32 access, int32 openMode, int32 denyMode, PStr group);
TH_REENTRANT MgErr _FUNCC	FMOpen(File *fdp, Path path, int32 openMode, int32 denyMode);
TH_REENTRANT MgErr _FUNCC	FMClose(File fd);
TH_REENTRANT MgErr _FUNCC	FMSeek(File fd, int32 ofst, int32 mode);
TH_REENTRANT MgErr _FUNCC	FMSeek64(File fd, int64 ofst, int32 mode);
TH_REENTRANT MgErr _FUNCC	FMTell(File fd, int32 *ofstp);
TH_REENTRANT MgErr _FUNCC	FMTell64(File fd, int64 *ofstp);
TH_REENTRANT MgErr _FUNCC	FGetEOF(File fd, int32 *sizep);
TH_REENTRANT MgErr _FUNCC	FGetEOF64(File fd, int64 *sizep);
TH_REENTRANT MgErr _FUNCC	FSetEOF(File fd, int32 size);
TH_REENTRANT MgErr _FUNCC	FSetEOF64(File fd, int64 size);
TH_REENTRANT MgErr _FUNCC	FMRead(File fd, int32 inCount, int32 *outCountp, UPtr buffer);
TH_REENTRANT MgErr _FUNCC	FMWrite(File fd, int32 inCount, int32 *outCountp, UPtr buffer);
TH_REENTRANT MgErr _FUNCC	FFlush(File fd);
TH_REENTRANT MgErr _FUNCC	FGetInfo(Path path, FInfoPtr infop);
TH_REENTRANT MgErr _FUNCC	FGetInfo64(Path path, FInfo64Ptr infop, FGetInfoWhich which DEFAULTARG(kFGetInfoAll));
TH_REENTRANT int32 _FUNCC	FExists(Path path);
TH_REENTRANT MgErr _FUNCC	FGetAccessRights(Path path, PStr owner, PStr group, int32 *permPtr);
TH_REENTRANT MgErr _FUNCC	FSetInfo(Path path, FInfoPtr infop);
TH_REENTRANT MgErr _FUNCC	FSetInfo64(Path path, FInfo64Ptr infop);
TH_REENTRANT MgErr _FUNCC	FSetAccessRights(Path path, PStr owner, PStr group, int32 *permPtr);
TH_REENTRANT MgErr _FUNCC	FMove(Path oldPath, Path newPath);
TH_REENTRANT MgErr _FUNCC	FCopy(Path oldPath, Path newPath);
TH_REENTRANT MgErr _FUNCC	FRemove(Path path);
TH_REENTRANT MgErr _FUNCC	FRemoveToRecycle(Path path);
TH_REENTRANT MgErr _FUNCC	FNewDir(Path path, int32 permissions);

typedef CPStr FDirEntRec, *FDirEntPtr, **FDirEntHandle; /**< directory list record */

TH_REENTRANT MgErr _FUNCC		FListDir(Path path, FDirEntHandle list, FMFileType **);
TH_REENTRANT MgErr _FUNCC		FAddPath(Path basePath, Path relPath, Path newPath);
TH_REENTRANT MgErr _FUNCC		FAppendName(Path path, ConstPStr name);
TH_REENTRANT MgErr _FUNCC		FRelPath(Path start, Path end, Path relPath);
TH_REENTRANT MgErr _FUNCC		FName(Path path, StringHandle name);
TH_REENTRANT MgErr _FUNCC		FNamePtr(Path path, PStr name);
TH_REENTRANT ConstPStr _FUNCC	FNameNoCopy(Path path); // Same as FNamePtr, but does not duplicate name into new PStr
TH_REENTRANT MgErr _FUNCC		FDirName(Path path, Path dir);
TH_REENTRANT MgErr _FUNCC		FVolName(Path path, Path vol);
TH_REENTRANT Path _FUNCC			FMakePath(Path path, int32 type, ...);
TH_REENTRANT Path _FUNCC			FEmptyPath(Path);
TH_REENTRANT Path _FUNCC			FNotAPath(Path);
TH_REENTRANT MgErr _FUNCC		FPathToPath(Path *p);
TH_REENTRANT MgErr _FUNCC		FPathCpy(Path dst, Path src);
TH_REENTRANT void _FUNCC			FDestroyPath(Path *pp); // FDestroyPath releases memory AND sets pointer to NULL.
TH_REENTRANT MgErr _FUNCC		FDisposePath(Path p);   // Use of FDestroyPath recommended over FDisposePath.
TH_REENTRANT int32 _FUNCC		FUnFlattenPath(UPtr fp, Path *pPtr);
TH_REENTRANT int32 _FUNCC		FFlattenPath(Path p, UPtr fp);
TH_REENTRANT int32 _FUNCC		FDepth(Path path);
TH_REENTRANT LStrHandle _FUNCC FGetDefGroup(LStrHandle);
TH_REENTRANT Bool32 _FUNCC		FStrFitsPat(const uChar*, const uChar*, int32, int32);
TH_REENTRANT int32 _FUNCC		FPathCmp(Path, Path);
TH_REENTRANT int32 _FUNCC		FPathCmpLexical(Path, Path);
TH_REENTRANT UHandle _FUNCC		PathToCString(Path );
TH_REENTRANT MgErr _FUNCC		FPathToDSString(Path, LStrHandle*);
TH_REENTRANT MgErr _FUNCC		FStringToPath(ConstLStrH, Path*);
TH_REENTRANT MgErr _FUNCC      FPathToFileSystemDSString(Path p, LStrHandle *txt);
TH_REENTRANT MgErr _FUNCC      FFileSystemCStrToPath(ConstCStr text, Path *p);
TH_REENTRANT MgErr _FUNCC		FTextToPath(ConstUPtr, int32, Path*);
TH_REENTRANT MgErr _FUNCC		FLockOrUnlockRange(File, int32, int32, int32, Bool32);
TH_REENTRANT MgErr _FUNCC		FGetVolInfo(Path, VInfoRec*);
TH_REENTRANT MgErr _FUNCC		FMGetVolInfo(Path, float64*, float64*);
TH_REENTRANT MgErr _FUNCC		FMGetVolPath(Path, Path*);
TH_REENTRANT MgErr _FUNCC		FSetPathType(Path, int32);
TH_REENTRANT MgErr _FUNCC		FGetPathType(Path, int32*);
TH_REENTRANT Bool32 _FUNCC		FIsAPath(Path);
TH_REENTRANT Bool32 _FUNCC		FIsEmptyPath(Path);
TH_REENTRANT Bool32 _FUNCC		FIsAPathOrNotAPath(Path);
TH_REENTRANT Bool32 _FUNCC		FIsAPathOfType(Path, int32);
TH_REENTRANT Bool32 _FUNCC		FIsAbsPath(Path);
TH_REENTRANT MgErr _FUNCC		FAppPath(Path);

#define LVRefNum MagicCookie
typedef MagicCookie LVUserEventRef;
typedef MagicCookie Occurrence;

TH_REENTRANT MgErr _FUNCC	PostLVUserEvent(LVUserEventRef ref, void *data);
TH_PROTECTED MgErr _FUNCC	Occur(Occurrence o);
TH_REENTRANT MgErr _FUNCC	FNewRefNum(Path, File, LVRefNum*);
TH_REENTRANT Bool32 _FUNCC	FIsARefNum(LVRefNum);
TH_REENTRANT MgErr _FUNCC	FDisposeRefNum(LVRefNum);
TH_REENTRANT MgErr _FUNCC	FRefNumToFD(LVRefNum, File*);
TH_REENTRANT MgErr _FUNCC	FRefNumToPath(LVRefNum, Path);
TH_REENTRANT MgErr _FUNCC	FArrToPath(UHandle, Bool32, Path);
TH_REENTRANT MgErr _FUNCC	FPathToArr(Path, Bool32*, UHandle);
TH_REENTRANT int32 _FUNCC	FPrintf(File, CStr, ...);  /* moved from support manager area */
TH_REENTRANT MgErr _FUNCC	FPrintfWErr(File fd, CStr fmt, ...);
TH_UNSAFE CStr _FUNCC			DateCString(uInt32, int32);
TH_UNSAFE CStr _FUNCC			TimeCString(uInt32, int32);
TH_UNSAFE CStr _FUNCC			ASCIITime(uInt32);

typedef struct {	/* overlays ANSI definition for unix, watcom, think, mpw */
	int32	sec;	/* 0:59 */
	int32	min;	/* 0:59 */
	int32	hour;	/* 0:23 */
	int32	mday;	/* day of the month, 1:31 */
	int32	mon;	/* month of the year, 1:12 */
	int32	year;	/* year, 1904:2040 */
	int32	wday;	/* day of the week, 1:7 for Sun:Sat */
	int32	yday;	/* day of year (julian date), 1:366 */
	int32	isdst;	/* 1 if daylight savings time */
} DateRec;

TH_REENTRANT void _FUNCC SecsToDate(uInt32, DateRec*);
TH_REENTRANT uInt32 _FUNCC DateToSecs(DateRec*);

// User Defined Refnum callback functions
TH_SINGLE_UI EXTERNC int32 _FUNCC OMEventFired(int32 session, LStrHandle typeName, LStrHandle className,
										int32 eventCode, void **data);
TH_SINGLE_UI EXTERNC int32 _FUNCC OMEventFired2(int32 session, uInt32 refnumKind, LStrHandle typeName, LStrHandle className,
										 int32 eventCode, void **data);
TH_SINGLE_UI EXTERNC int32 _FUNCC OMFlushPendingEvents(int32 session, uInt32 refnumKind, LStrHandle typeName, LStrHandle className);
TH_SINGLE_UI EXTERNC MgErr _FUNCC	OMGetClassPropListIds(LStrHandle typeName, LStrHandle className, int32 ***propIDs);

TH_PROTECTED EXTERNC MgErr _FUNCC UDCookieToSesn(void* refObjIn, uInt32 *sesn);
TH_PROTECTED MgErr _FUNCC UDCookieToName(LStrHandle* name, void* refObj);
TH_PROTECTED EXTERNC MgErr _FUNCC UDRegisterSesn(LStrHandle name, uInt32 sesn, LStrHandle typeName, 
										  LStrHandle className, void* refObj, int32 cleanup,
										  LStrHandle libName, int32 calling, LStrHandle openFunc, 
										  LStrHandle closeFunc);
TH_PROTECTED EXTERNC MgErr _FUNCC UDRegisterSesnKind(LStrHandle name, uInt32 sesn, LStrHandle typeName,
											  LStrHandle className, uInt32 refnumKind, void* refObj,
											  int32 cleanup, LStrHandle libName, int32 calling,
											  LStrHandle openFunc, LStrHandle closeFunc);
TH_PROTECTED EXTERNC MgErr _FUNCC UDRegisterSesnFlatten(LStrHandle name, uInt32 sesn, LStrHandle typeName,
														LStrHandle className, uInt32 refnumKind, void* refObjIn,
														int32 cleanup, LStrHandle libName, int32 calling,
														LStrHandle openFunc, LStrHandle closeFunc, LStrHandle flattenFunc,
														LStrHandle unflattenFunc);
TH_PROTECTED EXTERNC MgErr _FUNCC UDRegisterSesnFlatten2(LStrHandle name, uInt32 sesn, LStrHandle typeName,
														LStrHandle className, uInt32 refnumKind, void* refObjIn,
														int32 cleanup, LStrHandle libName, int32 calling,
														LStrHandle openFunc, LStrHandle closeFunc, LStrHandle flatten2Func,
														LStrHandle unflattenFunc);
TH_PROTECTED EXTERNC MgErr _FUNCC UDUnregisterSesn(void* refObjIn);
TH_PROTECTED EXTERNC MgErr _FUNCC UDRemoveSesn(uInt32 sesn, LStrHandle typeName, LStrHandle className,
										Bool32 removeProc);
TH_PROTECTED EXTERNC MgErr _FUNCC UDRemoveSesnKind(uInt32 sesn, LStrHandle typeName, LStrHandle className,
											uInt32 refnumKind, Bool32 removeProc);

/*** The Resource Manager ***/

/** @struct RsrcFile 
The opaque type used by the resource manager. See resource.cpp */
Private(RsrcFile);

/*	Debugging ON section Begin	*/
#ifndef DBG
#define DBG 1
#endif
/*	Debugging ON section End	*/

/*	Debugging OFF section Begin
#undef DBG
#define DBG 0
	Debugging OFF section End	*/

/** Refer to SPrintfv() */
TH_PROTECTED int32 DBPrintf(const char *fmt, ...);

/* LabVIEW Bool32 representation and values */
typedef uInt16 LVBooleanU16;
#define LVBooleanU16True	((LVBooleanU16)0x8000)
#define LVBooleanU16False	((LVBooleanU16)0x0000)
typedef uInt8 LVBoolean;
#define LVBooleanTrue		((LVBoolean)1)
#define LVBooleanFalse	((LVBoolean)0)
#define LVTRUE			LVBooleanTrue			/* for CIN users */
#define LVFALSE			LVBooleanFalse

typedef double floatNum;
#if Mac		// we want to make sure that any C code file we generate have their functions properly externed christina.dellas 9.25.02 
	#define CIN EXTERNC
#else
	#define CIN
#endif
#define ENTERLVSB
#define LEAVELVSB

#if MSWin
double log1p(double x);
#endif

TH_REENTRANT MgErr _FUNCC NumericArrayResize(int32, int32, UHandle*, int32);
TH_REENTRANT MgErr _FUNCC CallChain(UHandle);

/** NIGetOneErrorCode translates an error code from any NI product into 
its text description. Returns LVBooleanFalse if error code is not found 
in the error code text files; returns LVBooleanTrue if the code was found. */
TH_REENTRANT LVBoolean _FUNCC NIGetOneErrorCode(int32 errCode, LStrHandle *errText);

/* CIN-specific prototypes */
int32 _FUNCC GetDSStorage(void);
int32 _FUNCC SetDSStorage(int32 newVal);	/* returns old value */
int16* _FUNCC GetTDPtr(void);
UPtr _FUNCC  GetLVInternals(void);
MgErr _FUNCC SetCINArraySize(UHandle, int32, int32);
	   

CIN MgErr _FUNCC CINInit(void);
CIN MgErr _FUNCC CINDispose(void);
CIN MgErr _FUNCC CINAbort(void);
CIN MgErr _FUNCC CINLoad(RsrcFile reserved);
CIN MgErr _FUNCC CINUnload(void);
CIN MgErr _FUNCC CINSave(RsrcFile reserved);
CIN MgErr _FUNCC CINProperties(int32 selector, void *arg);

/* selectors for CINProperties */
enum { kCINIsReentrant };

/* CINInit -- Called after the VI is loaded or recompiled. */
#define UseDefaultCINInit CIN MgErr _FUNCC CINInit() { return noErr; }

/* CINDispose -- Called before the VI is unloaded or recompiled.*/
#define UseDefaultCINDispose CIN MgErr _FUNCC CINDispose() \
		{ return noErr; }

/* CINAbort-- Called when the VI is aborted. */
#define UseDefaultCINAbort CIN MgErr _FUNCC CINAbort() { return noErr; }

/* CINLoad -- Called when the VI is loaded. */
#define UseDefaultCINLoad CIN MgErr _FUNCC CINLoad(RsrcFile reserved) \
		{ Unused(reserved); return noErr; }

/* CINUnload -- Called when the VI is unloaded. */
#define UseDefaultCINUnload CIN MgErr _FUNCC CINUnload() \
		{ return noErr; }

/* CINSave -- Called when the VI is saved. */
#define UseDefaultCINSave CIN MgErr _FUNCC CINSave(RsrcFile reserved) \
		{ Unused(reserved); return noErr; }

#if defined(CIN_VERS) && MSWin && (ProcessorType == kX86)
	#pragma pack()
#endif

ENDEXTERNC

#endif /* _extcode_H */
