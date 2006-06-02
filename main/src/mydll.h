#ifndef _DEFINED_MYDLL
#define _DEFINED_MYDLL

#if _MSC_VER > 1000
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _COMPILING_MYDLL
#define LIBSPEC __declspec(dllexport)
#else
#define LIBSPEC __declspec(dllimport)
#endif /* _COMPILING_MYDLL */

#define BUFLEN 4096  /* 4K buffer length */
#define MAX_FILEPATH BUFLEN

#define WM_APP_REHOOK WM_APP + 1
#define WM_APP_KEYDEF WM_APP + 2

#define VKEY_INDICATOR_TOGGLE 0
#define VKEY_HOOK_MODE_TOGGLE 1
#define VKEY_SMALL_SCREENSHOT 2
#define VKEY_SCREENSHOT       3
#define VKEY_VIDEO_CAPTURE    4

LIBSPEC LRESULT CALLBACK CBTProc(int code, WPARAM wParam, LPARAM lParam);
LIBSPEC LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
LIBSPEC LRESULT CALLBACK DummyKeyboardProc(int code, WPARAM wParam, LPARAM lParam);

LIBSPEC BOOL InstallTheHook(void);
LIBSPEC BOOL UninstallTheHook(void);

DWORD QPC();
DWORD GetFrameDups();
void InstallKeyboardHook(DWORD tid);
void InstallDummyKeyboardHook(DWORD tid);
void UninstallKeyboardHook();

typedef enum { CUSTOM, ADAPTIVE } ALGORITHM_TYPE;

// Process information pack
typedef struct _MYINFO {
	char file[MAX_FILEPATH];
	char dir[MAX_FILEPATH];
	char logfile[MAX_FILEPATH];
	char *shortLogfile;
	char processfile[MAX_FILEPATH];
	char *shortProcessfile;
	char shortProcessfileNoExt[MAX_FILEPATH];

} MYINFO;

// Taksi state information pack
typedef struct _MYSTATE {
	BOOL bIndicate;
	BOOL bKeyboardInitDone;
	BOOL bMakeScreenShot;
	BOOL bMakeSmallScreenShot;
	BOOL bStartRecording;
	BOOL bNowRecording;
	BOOL bSystemWideMode;
	BOOL bStopRecording;
	BOOL bUnhookGetMsgProc;

} MYSTATE;

// Structure to keep Direct3D-related pointers
typedef struct _struct_DXPOINTERS {
	DWORD present8;
	DWORD reset8;
	DWORD present9;
	DWORD reset9;

} DXPOINTERS;

/* ... more declarations as needed */
#undef LIBSPEC

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _DEFINED_MYDLL */

