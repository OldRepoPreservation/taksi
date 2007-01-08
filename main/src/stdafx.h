//
// stdafx.h
// included in both EXE and DLL
//
#if _MSC_VER > 1000
#pragma once
#endif
#define USE_DX	// remove this to compile if u dont have the DirectX SDK
#define USE_LOGFILE

// NOTE: This causes a hang bug when closing Taksi before closing the app!
// #define USE_GDIP	// use Gdiplus::Bitmap to save images as PNG (or JPEG)

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
#include "Config.h"

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
	HRESULT OpenLogFile( const TCHAR* pszFileName );
	void CloseLogFile();
	virtual int EventStr( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszMsg );
private:
	CNTHandle m_File;
};
extern LIBSPEC CTaksiLogFile g_Log;

// Messages to the Master Window.
#define WM_APP_UPDATE		(WM_APP + 0) // WM_USER ?
#define WM_APP_REHOOKCBT	(WM_APP + 1) // WM_USER ?
#define WM_APP_TRAY_NOTIFICATION (WM_APP+2)

#define IDC_HOTKEY_FIRST 200	// WM_COMMAND message = IDC_HOTKEY_FIRST + TAKSI_HOTKEY_TYPE (same as IDB_ConfigOpen)

#define TAKSI_MASTER_CLASSNAME _T("TaksiMaster")

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
	TAKSI_INDICATE_RecordPaused,	
	//TAKSI_INDICATE_Error,		// sit in error state til cleared ??
	TAKSI_INDICATE_QTY,
};

enum TAKSI_PROCSTAT_TYPE
{
#define ProcStatProp(a,b,c,d) TAKSI_PROCSTAT_##a,
#include "ProcStatProps.tbl"
#undef ProcStatProp
	TAKSI_PROCSTAT_QTY,
};

#pragma pack(4)	// try to be explicit about this since its shared.
struct LIBSPEC CTaksiProcStats
{
	// Record relevant stats on the current foreground app/process.
	// Display stats in the dialog in real time. SHARED

public:
	void InitProcStats();
	void CopyProcStats( const CTaksiProcStats& stats );
	void UpdateProcStat( TAKSI_PROCSTAT_TYPE eProp )
	{
		ASSERT( eProp >= 0 && eProp < TAKSI_PROCSTAT_QTY );
		m_dwPropChangedMask |= (1<<eProp);
	}
	void CopyProcStat( const CTaksiProcStats& stats, TAKSI_PROCSTAT_TYPE eProp )
	{
		// Move just a single property.
		ASSERT( eProp >= 0 && eProp < TAKSI_PROCSTAT_QTY );
		const WORD wOffset = sm_Props[eProp][0];
		ASSERT( wOffset >= 0 && wOffset < sizeof(CTaksiProcStats));
		const WORD wSize = sm_Props[eProp][1];
		ASSERT( wSize > 0 && wSize < sizeof(CTaksiProcStats));
		const WORD wEnd = wSize+wOffset;
		ASSERT( wEnd > 0 && wEnd <= sizeof(CTaksiProcStats));
		memcpy(((BYTE*)this) + wOffset, ((const BYTE*)&stats) + wOffset, wSize );
		UpdateProcStat(eProp);
	}

	float get_DataRecMeg() const
	{
		return ((float) m_dwDataRecorded ) / (1024*1024);
	}

public:
	DWORD m_dwProcessId;		// What process is this?
	TCHAR m_szProcessFile[ _MAX_PATH ];	// What is the file name/path for the current process.

	// dynamic info.
	TCHAR m_szLastError[ _MAX_PATH ];	// last error message (if action failed)

	HWND m_hWndCap;	// the window the graphics is in
	SIZE m_SizeWnd;	// the window/backbuffer size. (pixels)
	TAKSI_API_TYPE m_eGraphXAPI;

	TAKSI_INDICATE_TYPE m_eState;	// What are we doing with the process?
	float m_fFrameRate;			// measured frame rate. recording or not.
	DWORD m_dwDataRecorded;		// How much video data recorded in current stream (if any)

public:
	static const WORD sm_Props[ TAKSI_PROCSTAT_QTY ][2]; // offset,size
	DWORD m_dwPropChangedMask;		// TAKSI_PROCSTAT_TYPE mask
};
extern LIBSPEC CTaksiProcStats sg_ProcStats;	// For display in the Taksi.exe app.
#pragma pack()	// try to be explicit about this since its shared.

#pragma pack(4)	// try to be explicit about this since its shared.
struct LIBSPEC CTaksiShared
{
	// This structure is interprocess SHARED!
	// NOTE: So it CANT have a constructor or contain any data types that do!!
public:

	bool InitShared();
	void DestroyShared();

	HRESULT LogMessage( const TCHAR* pszPrefix );	// LOG_NAME_DLL
	void SendReHookMessage();

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

	void HotKey_ConfigOpen();
	void HotKey_IndicatorToggle();
	void HotKey_HookModeToggle();

public:
	DWORD	m_dwVersionStamp;	// TAKSI_VERSION_N
	HWND	m_hMasterWnd;		// The master server EXE's window.
	bool	m_bMasterExiting;	// The master server is exiting so all attached DLL's should close down.
	HHOOK	m_hHookCBT;			// SetWindowsHookEx( WH_CBT ) for hooking new processes
	int		m_iProcessCount;	// how many processes attached?

	TCHAR m_szIniDir[_MAX_PATH];		// what dir to find the INI file and put the log files.

#define LOG_NAME_DLL	_T("TaksiDll.log")	// common log shared by all processes.

	DWORD m_dwConfigChangeCount;	// changed when the Custom stuff in m_Config changes.
	DWORD m_dwHotKeyMask;			// TAKSI_HOTKEY_TYPE mask from App. Schedule these in FrameBegin()
	bool m_bRecordPause;			// paused video record by command.
	int m_iMasterUpdateCount;		// how many WM_APP_UPDATE messages unprocessed.

#ifdef USE_DX
	// keep Direct3D-related pointers, tested just once for all.
	// DX8
	UINT_PTR m_nDX8_Present; // offset from start of DLL to the interface element i want.
	UINT_PTR m_nDX8_Reset;
	// DX9
	UINT_PTR m_nDX9_Present;
	UINT_PTR m_nDX9_Reset;
#endif
};
extern LIBSPEC CTaksiShared sg_Shared;
#pragma pack()
