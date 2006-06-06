//
// stdafx.h
// included in both EXE and DLL
//
#if _MSC_VER > 1000
#pragma once
#endif
#define USE_DX	// remove this to compile if u dont have the DirectX SDK
#define USE_LOGFILE

// System includes always go first.
#include <windows.h>
#include <windef.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#ifdef TAKSIDLL_EXPORTS
#define LIBSPEC __declspec(dllexport)
#else
#define LIBSPEC __declspec(dllimport)
#endif // TAKSIDLL_EXPORTS

#include "../common/Common.h"
#include "../common/IRefPtr.h"
#include "../common/CLogBase.h"
#include "../common/CAVIFile.h"	// CAVIFile
#include "../common/CWaveACM.h"

#include "TaksiVersion.h"

enum TAKSI_HOTKEY_TYPE
{
	// Monitor these hot keys.
#define IDC_HOTKEY_FIRST 200	// WM_COMMAND message = IDB_ConfigOpen + hotkey
	TAKSI_HOTKEY_ConfigOpen,
	TAKSI_HOTKEY_HookModeToggle,
	TAKSI_HOTKEY_IndicatorToggle,
	TAKSI_HOTKEY_RecordStart,
	TAKSI_HOTKEY_RecordStop,
	TAKSI_HOTKEY_Screenshot,
	TAKSI_HOTKEY_SmallScreenshot,
	TAKSI_HOTKEY_QTY,
};

struct LIBSPEC CTaksiLogFile : public CLogFiltered
{
public:
	CTaksiLogFile() 
	{
		g_pLog = this;
	}
	~CTaksiLogFile()
	{
		CloseLogFile();
	}
	bool OpenLogFile( const TCHAR* pszFileName );
	void CloseLogFile();
	virtual int EventStr( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszMsg );
public:
	CNTHandle m_File;
};
extern LIBSPEC CTaksiLogFile g_Log;

#define WM_APP_UPDATE		(WM_APP + 0) // WM_USER ?
#define WM_APP_REHOOKCBT	(WM_APP + 1) // WM_USER ?

enum TAKSI_CUSTOM_TYPE
{
	TAKSI_CUSTOM_FrameRate,
	TAKSI_CUSTOM_FrameWeight,
	TAKSI_CUSTOM_Pattern,
	TAKSI_CUSTOM_QTY,
};

struct LIBSPEC CTaksiConfigCustom : public CIniObject
{
	// Custom frame rate/weight settings for a specific app.
	// If the DLL binds to this app. then do something special?
#define TAKSI_CUSTOM_SECTION "TAKSI_CUSTOM"

public:
	virtual const char** get_Props() const
	{
		return sm_Props;
	}
	virtual int get_PropQty() const
	{
		return TAKSI_CUSTOM_QTY;
	}
	virtual bool PropSet( int eProp, const char* pszValue );
	virtual int PropGet( int eProp, char* pszValue, int iSizeMax ) const;

public:
	TCHAR  m_szAppId[_MAX_PATH];	// [friendly name] in the INI file.
	TCHAR  m_szPattern[_MAX_PATH];	// id for the process.	TAKSI_CUSTOM_Pattern
	float  m_fFrameRate;	// video frame rate. TAKSI_CUSTOM_FrameRate
	float  m_fFrameWeight;	// this assumes frames come in at a set rate and we dont have to measure them!

	static const char* sm_Props[TAKSI_CUSTOM_QTY+1];

public:
	// Work data.
	struct CTaksiConfigCustom* m_pNext;
};

enum TAKSI_CFGPROP_TYPE
{
	// enum the tags that can go in the config file.
#define ConfigProp(a,b) TAKSI_CFGPROP_##a,
#include "ConfigProps.tbl"
#undef ConfigProp
	TAKSI_CFGPROP_QTY,
};

struct LIBSPEC CTaksiConfigData
{
	// Interprocess shared data block.
	// NOTE: cant have a constructor for this! since we put it in CTaksiDll SHARED
	// Some params can be set from CGuiConfig.

	void InitConfig();
	void CopyConfig( const CTaksiConfigData& config );

	bool SetHotKey(TAKSI_HOTKEY_TYPE eHotKey, WORD vKey);
	WORD GetHotKey(TAKSI_HOTKEY_TYPE eHotKey) const;

	void put_CaptureDir(const TCHAR* pszPath);
	void GetCaptureDir(TCHAR* pszPath, int iSizeMax) const;
	bool FixCaptureDir();

public:
	// CAN be set from CGuiConfig
	TCHAR  m_szCaptureDir[_MAX_PATH];	// files go here!

	float  m_fFrameRateTarget;			// What do we want our movies frame rate to be? (f/sec)
	CVideoCodec m_VideoCodec;			// Video Compression scheme selected
	bool   m_bVideoHalfSize;			// full vs half sized video frames.

	int m_iAudioDevice;	// audio input device. WAVE_MAPPER = -1, WAVE_DEVICE_NONE = -2. CWaveRecorder
	CWaveFormat m_AudioCodec;

	WORD   m_wHotKey[TAKSI_HOTKEY_QTY];	// Virtual keys + HOTKEYF_ALT for the HotKeys

	// CAN NOT be set from CGuiConfig (yet)
	bool   m_bShowIndicator;
	bool   m_bDebugLog;			// keep log files or not?
	bool   m_bUseDirectInput;	// use direct input for key presses. else just keyboard hook

	// CONSTANTS! (for now)
	bool   m_bUseGDI;		// hook GDI mode at all?
	bool   m_bRecordNonClient;
};
extern LIBSPEC CTaksiConfigData sg_Config;	// Read from the INI file. and set via CGuiConfig

struct LIBSPEC CTaksiConfig : public CIniObject, public CTaksiConfigData
{
	// Params read from the INI file.
#define TAKSI_INI_FILE _T("taksi.ini")
#define TAKSI_SECTION "TAKSI"
public:
	CTaksiConfig();
	~CTaksiConfig();

	bool ReadIniFileFromDir(const TCHAR* pszDir);
	bool WriteIniFile();

	CTaksiConfigCustom* CustomConfig_FindAppId( const TCHAR* pszAppId ) const;
	CTaksiConfigCustom* CustomConfig_Lookup( const TCHAR* pszAppId);
	void CustomConfig_DeleteAppId( const TCHAR* pszAppId);

	CTaksiConfigCustom* CustomConfig_FindPattern( const TCHAR* pszProcessfile ) const;

	static CTaksiConfigCustom* CustomConfig_Alloc();
	static void CustomConfig_Delete( CTaksiConfigCustom* pConfig );

	virtual const char** get_Props() const
	{
		return sm_Props;
	}
	virtual int get_PropQty() const
	{
		return TAKSI_CFGPROP_QTY;
	}
	virtual bool PropSet( int eProp, const char* pValue );
	virtual int PropGet( int eProp, char* pszValue, int iSizeMax ) const;

public:
	static const char* sm_Props[TAKSI_CFGPROP_QTY+1];

	// NOTE: NOT interprocess shared memory. so each dll instance must re-read from INI ??
	CTaksiConfigCustom* m_pCustomList;	// linked list. 
};

#ifdef USE_DX
enum TAKSI_INTF_TYPE
{
	// DX interface entry offsets.
	TAKSI_INTF_QueryInterface = 0,	// always 0
	TAKSI_INTF_AddRef = 1,
	TAKSI_INTF_Release = 2,

	TAKSI_INTF_DX8_Reset = 14,
	TAKSI_INTF_DX8_Present = 15,

	TAKSI_INTF_DX9_Reset = 16,
	TAKSI_INTF_DX9_Present = 17,
};
#endif

enum TAKSI_INDICATE_TYPE
{
	// Indicate my current state to the viewer
	// Color a square to indicate my mode.
	TAKSI_INDICATE_Ready = 0,
	TAKSI_INDICATE_Hooked,	// CBT is hooked. (looking for new app to load)
	TAKSI_INDICATE_Recording,	// actively recording right now
	//TAKSI_INDICATE_Error,	// sit in error state til cleared.
	//TAKSI_INDICATE_Paused,
	TAKSI_INDICATE_QTY,
};

enum TAKSI_PROCSTAT_TYPE
{
#define ProcStatProp(a,b,c,d) TAKSI_PROCSTAT_##a,
#include "ProcStatProps.tbl"
#undef ProcStatProp
	TAKSI_PROCSTAT_QTY,
};

struct LIBSPEC CTaksiProcStats
{
	// Record relevant stats on the current foreground app/process.
	// Display stats in the dialog in real time. SHARED

public:
	void InitProcStats();
	void CopyProcStats( const CTaksiProcStats& stats );
	void UpdateProcStat( TAKSI_PROCSTAT_TYPE eProp )
	{
		m_dwPropChangedMask |= (1<<eProp);
	}
	void CopyProcStat( const CTaksiProcStats& stats, TAKSI_PROCSTAT_TYPE eProp )
	{
		// Move just a single property.
		ASSERT( eProp >= 0 && eProp < TAKSI_PROCSTAT_QTY );
		const WORD wOffset = sm_Props[eProp][0];
		memcpy(((BYTE*)this) + wOffset, ((const BYTE*)&stats) + wOffset, sm_Props[eProp][1] );
		UpdateProcStat(eProp);
	}

public:
	DWORD m_dwProcessId;		// What process is this?
	TCHAR m_szProcessFile[ _MAX_PATH ];	// What is the file name/path for the current process.

	// dynamic info.
	TCHAR m_szLastError[ _MAX_PATH ];	// last error message (if action failed)

	HWND m_hWnd;	// the window the graphics is in
	SIZE m_SizeWnd;	// the window/backbuffer size. (pixels)

	TAKSI_INDICATE_TYPE m_eState;	// What are we doing with the process?
	float m_fFrameRate;			// measured frame rate. recording or not.
	DWORD m_dwDataRecorded;		// How much video data recorded in current stream (if any)

public:
	static const WORD sm_Props[ TAKSI_PROCSTAT_QTY ][2]; // offset,size
	DWORD m_dwPropChangedMask;		// TAKSI_PROCSTAT_TYPE mask
};
extern LIBSPEC CTaksiProcStats sg_ProcStats;	// For display in the Taksi.exe app.

struct LIBSPEC CTaksiDll
{
	// This structure is interprocess SHARED!
	// NOTE: So it CANT have a constructor or contain any data types that do!!
public:

	bool InitDll();
	void DestroyDll();

	void OnDetachProcess();
	void LogMessage( const TCHAR* pszPrefix );	// LOG_NAME_DLL

	bool IsHookCBT() const
	{
		return m_hHookCBT != NULL;
	}
	static LRESULT CALLBACK HookCBTProc(int nCode, WPARAM wParam, LPARAM lParam);
	bool HookCBT_Install(void);
	bool HookCBT_Uninstall(void);

	bool InitMasterWnd(HWND hWnd);
	void UpdateMaster();
	void UpdateConfigCustom();

public:
	DWORD	m_dwVersionStamp;	// TAKSI_VERSION_N
	HWND	m_hMasterWnd;		// The master server EXE's window.
	bool	m_bMasterExiting;	// The master server is exiting so all attached DLL's should close down.
	HHOOK	m_hHookCBT;			// SetWindowsHookEx( WH_CBT ) for hooking new processes
	int		m_iProcessCount;	// how many processes attached?

#ifdef USE_DX
	// keep Direct3D-related pointers just once for all.
	// DX8
	UINT_PTR m_nDX8_Present; // offset from start of DLL to the interface element i want.
	UINT_PTR m_nDX8_Reset;
	// DX9
	UINT_PTR m_nDX9_Present;
	UINT_PTR m_nDX9_Reset;
#endif

public:
	TCHAR m_szDllPathName[_MAX_PATH];	// DLL file. GetModuleFileName()
	TCHAR m_szDllDir[_MAX_PATH];

#define LOG_NAME_DLL	_T("TaksiDll.log")	// common log shared by all processes.
	TCHAR m_szLogCentral[_MAX_PATH];	// DLL common, NOT for each process. LOG_NAME_DLL

	DWORD m_dwConfigChangeCount;	// changed when the Custom stuff in m_Config changes.
	DWORD m_dwHotKeyMask;	// TAKSI_HOTKEY_TYPE mask from App.
	int m_iMasterUpdateCount;	// how many WM_APP_UPDATE messages unprocessed.
};
extern LIBSPEC CTaksiDll sg_Dll;

#define TAKSI_MASTER_CLASSNAME _T("TaksiMaster")
