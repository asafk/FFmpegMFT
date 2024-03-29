// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers


// Windows Header Files
#include <windows.h>


// reference additional headers your program requires here

#include <atlbase.h>
#include <atlstr.h>

#include <mfapi.h>
#include <mftransform.h>
#include <mfidl.h>
#include <mferror.h>

#include <shlwapi.h>

#include <strsafe.h>

#include <initguid.h>

#include "Logger.h"

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define BREAK_ON_FAIL(value)            if(FAILED(value)) { Logger::getInstance().LogError("Error 0x%x, %s:%d", value, __FILENAME__, __LINE__); break; }
#define BREAK_ON_NULL(value, newHr)     if(value == NULL) { hr = newHr; break; }
#define SAFE_DELETE(a)					if( (a) != NULL ) { delete (a); (a) = NULL; }



extern ULONG g_dllLockCount;


// {FBB85B4F-F21F-4016-BCF1-2A234162F1C5}
DEFINE_GUID(CLSID_FFmpegMFT, 0xfbb85b4f, 0xf21f, 0x4016, 0xbc, 0xf1, 0x2a, 0x23, 0x41, 0x62, 0xf1, 0xc5);
DEFINE_MEDIATYPE_GUID( MFVideoFormat_V264,      FCC('V264') ); //propriety codec
DEFINE_MEDIATYPE_GUID( MFVideoFormat_V265,      FCC('V265') ); //propriety codec


#define FFMPEG_MFT_CLSID_STR	L"Software\\Classes\\CLSID\\{FBB85B4F-F21F-4016-BCF1-2A234162F1C5}"
#define FFMPEG_MFT_STR			L"FFmpeg Decoder MFT"


