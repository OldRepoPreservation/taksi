#ifndef _JUCE_SHAREDMEM
#define _JUCE_SHAREDMEM

#if _MSC_VER > 1000
	#pragma once
#endif

#define LIBSPEC __declspec(dllexport)

#define TAKSI_EXE "taksi.exe"

#include <vfw.h>
#include "config.h"

EXTERN_C LIBSPEC void  SetUnhookFlag(BOOL);
EXTERN_C LIBSPEC BOOL  GetUnhookFlag(void);
EXTERN_C LIBSPEC void  SetTargetFrameRate(DWORD);
EXTERN_C LIBSPEC DWORD GetTargetFrameRate(void);
EXTERN_C LIBSPEC void  SetIgnoreOverhead(BOOL);
EXTERN_C LIBSPEC BOOL  GetIgnoreOverhead(void);
EXTERN_C LIBSPEC void  SetDebug(DWORD);
EXTERN_C LIBSPEC DWORD GetDebug(void);
EXTERN_C LIBSPEC void  SetKey(int, int);
EXTERN_C LIBSPEC int   GetKey(int);
EXTERN_C LIBSPEC void  SetUseDirectInput(bool);
EXTERN_C LIBSPEC bool  GetUseDirectInput(void);
EXTERN_C LIBSPEC void  SetStartupModeSystemWide(bool);
EXTERN_C LIBSPEC bool  GetStartupModeSystemWide(void);
EXTERN_C LIBSPEC void  SetFullSizeVideo(bool);
EXTERN_C LIBSPEC bool  GetFullSizeVideo(void);
EXTERN_C LIBSPEC void  SetExiting(BOOL);
EXTERN_C LIBSPEC BOOL  GetExiting(void);
EXTERN_C LIBSPEC void  SetServerWnd(HWND);
EXTERN_C LIBSPEC HWND  GetServerWnd(void);
EXTERN_C LIBSPEC void  SetCaptureDir(char* path);
EXTERN_C LIBSPEC void  GetCaptureDir(char* path);
EXTERN_C LIBSPEC void  SetReconfCounter(DWORD);
EXTERN_C LIBSPEC DWORD GetReconfCounter(void);
EXTERN_C LIBSPEC void  SetPresent8(DWORD);
EXTERN_C LIBSPEC DWORD GetPresent8(void);
EXTERN_C LIBSPEC void  SetReset8(DWORD);
EXTERN_C LIBSPEC DWORD GetReset8(void);
EXTERN_C LIBSPEC void  SetPresent9(DWORD);
EXTERN_C LIBSPEC DWORD GetPresent9(void);
EXTERN_C LIBSPEC void  SetReset9(DWORD);
EXTERN_C LIBSPEC DWORD GetReset9(void);
EXTERN_C LIBSPEC TAXI_CONFIG* GetConfig(void);
EXTERN_C LIBSPEC COMPVARS* GetCompVars(void);

#endif /* _JUCE_SHAREDMEM */

