//
// TaksiDll.cpp
//
#include "../stdafx.h"
#include <stddef.h> // offsetof
#include "TaksiDll.h"
#include "graphx.h"
#include "HotKeys.h"

//**************************************************************************************
// Shared by all processes variables. 
// NOTE: Must be init with 0
// WARN: Constructors WILL BE CALLED for each DLL_PROCESS_ATTACH so we cant use Constructors.
//**************************************************************************************
#pragma data_seg(".SHARED") // ".HKT" ".SHARED"
CTaksiDll sg_Dll = {0};			// API to present to the EXE 
CTaksiConfigData sg_Config = {0};	// Read from the INI file. and set via CGuiConfig
CTaksiProcStats sg_ProcStats = {0};	// For display in the Taksi.exe app.
#pragma data_seg()
#pragma comment(linker, "/section:.SHARED,rws")

//**************************************************************************************
// End of Inter-Process shared section 
//**************************************************************************************

HINSTANCE g_hInst = NULL;		// Handle for the dll for the current process.
CTaksiProcess g_Proc;			// information about the process i am attached to.
CTaksiLogFile g_Log;			// Log file for each process. seperate

static CTaksiGraphX* const s_GraphxModes[ TAKSI_GRAPHX_QTY ] = 
{
	&g_OGL,	// TAKSI_GRAPHX_OGL
	&g_DX8,
	&g_DX9,
	&g_GDI,	// TAKSI_GRAPHX_GDI // Last
};

//**************************************************************************************

bool CTaksiLogFile::OpenLogFile( const TCHAR* pszFileName )
{
	CloseLogFile();
	m_File.AttachHandle( ::CreateFile( pszFileName,            // file to create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL ));                        // no attr. template 
	return m_File.IsValidHandle();
}

void CTaksiLogFile::CloseLogFile()
{
	if ( ! m_File.IsValidHandle())
		return;
	Debug_Info("Closing log." LOG_CR);
	m_File.CloseHandle();
}

int CTaksiLogFile::EventStr( LOG_GROUP_MASK dwGroupMask, LOGL_TYPE eLogLevel, const LOGCHAR* pszMsg )
{
	// Write to the m_File. 
	LOGCHAR szTmp[_MAX_PATH];
	int iLen = _snprintf( szTmp, sizeof(szTmp)-1,
		"Taksi:%s:%s", g_Proc.m_szProcessTitleNoExt, pszMsg );

	if ( m_File.IsValidHandle())
	{
	}

	return __super::EventStr(dwGroupMask,eLogLevel,szTmp);
}

//**************************************************************************************

const WORD CTaksiProcStats::sm_Props[ TAKSI_PROCSTAT_QTY ][2] = // offset,size
{
#define ProcStatProp(a,b,c,d) { ( offsetof(CTaksiProcStats,b)), sizeof(((CTaksiProcStats*)0)->b) },
#include "../ProcStatProps.tbl"
#undef ProcStatProp
};

void CTaksiProcStats::InitProcStats()
{
	m_dwProcessId = 0;
	m_szProcessFile[0] = '\0';
	m_szLastError[0] = '\0';
	m_hWnd = NULL;
	m_eState = TAKSI_INDICATE_QTY;	
	m_dwPropChangedMask = 0xFFFFFFFF;	// all new
}

void CTaksiProcStats::CopyProcStats( const CTaksiProcStats& stats )
{
	memcpy( this, &stats, sizeof(stats));
	m_dwPropChangedMask = 0xFFFFFFFF;	// all new
}

//**************************************************************************************

void CTaksiDll::UpdateMaster()
{
	if ( m_hMasterWnd == NULL )
		return;
	if ( m_bMasterExiting )
		return;
	if ( ::PostMessage( m_hMasterWnd, WM_APP_UPDATE, 0, 0 ))
	{
		m_iMasterUpdateCount ++;	// count unprocessed WM_APP_UPDATE messages
	}
}

LRESULT CALLBACK CTaksiDll::HookCBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	// WH_CBT = computer-based training - hook procedure
	// NOTE: This is how we inject this DLL into other processes.
	// NOTE: There is a race condition here where sg_Dll.m_hHookCBT can be NULL. 
	// ASSERT(sg_Dll.IsHookCBT());
	switch (nCode)
	{
	// HCBT_CREATEWND
	case HCBT_SETFOCUS:
		// Set the DLL implants on whoever gets the focus!
		// ASSUME DLL_PROCESS_ATTACH was called.
		// TAKSI_INDICATE_Hooked
		if ( g_Proc.m_bIsProcessSpecial )
			break;
		LOG_MSG(( "HookCBTProc: nCode=%08x" LOG_CR, (DWORD)nCode ));
		if ( g_Proc.AttachGraphXMode() == S_OK )
		{
			g_Proc.CheckProcessCustom();	// determine frame capturing algorithm 
		}
		break;
	}

	// We must pass the all messages on to CallNextHookEx.
	return ::CallNextHookEx(sg_Dll.m_hHookCBT, nCode, wParam, lParam);
}

bool CTaksiDll::HookCBT_Install(void)
{
	// Installer for calling of HookCBTProc 
	// This causes my dll to be loaded into another process space.
	if ( IsHookCBT())
	{
		LOG_MSG(( "HookCBT_Install: already installed. (0%x)" LOG_CR, m_hHookCBT ));
		return false;
	}

	m_hHookCBT = ::SetWindowsHookEx( WH_CBT, HookCBTProc, g_hInst, 0);

	LOG_MSG(( "HookCBT_Install: hHookCBT=0%x,ProcID=%d" LOG_CR,
		m_hHookCBT, ::GetCurrentProcessId() ));

	UpdateMaster();
	return( IsHookCBT());
}

bool CTaksiDll::HookCBT_Uninstall(void)
{
	// Uninstaller for WH_CBT
	// NOTE: This may not be the process that started the hook
	// We may have successfully hooked an API. so stop looking.
	if (!IsHookCBT())
	{
		LOG_MSG(( "HookCBT_Uninstall: already uninstalled." LOG_CR));
		return false;
	}

	LOG_MSG(( "CTaksiDll::HookCBT_Uninstall: hHookCBT=0%x,ProcID=%d" LOG_CR,
		m_hHookCBT, ::GetCurrentProcessId() ));

	::UnhookWindowsHookEx( m_hHookCBT );
	m_hHookCBT = NULL;
	UpdateMaster();
	return true;
}

void CTaksiDll::LogMessage( const TCHAR* pszPrefix )
{
	// Log a common shared message for the dll, not the process.
	// LOG_NAME_DLL
	if ( sg_Config.m_dwDebug <= 0)
		return;

	TCHAR szMsg[ _MAX_PATH + 64 ];
	int iLenMsg = _sntprintf( szMsg, COUNTOF(szMsg)-1, _T("%s:%s"), 
		pszPrefix, g_Proc.m_Stats.m_szProcessFile );

	CNTHandle File( ::CreateFile( m_szLogCentral,       // file to open/create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL ));                        // no attr. template 
	if ( !File.IsValidHandle()) 
	{
		return;
	}

	DWORD dwBytesWritten;
	::SetFilePointer(File, 0, NULL, FILE_END);
	::WriteFile(File, (LPVOID)szMsg, iLenMsg, &dwBytesWritten, NULL);
	::WriteFile(File, (LPVOID)"\r\n", 2, &dwBytesWritten, NULL);
}

bool CTaksiDll::InitMasterWnd(HWND hWnd)
{
	// The TAKSI.EXE App just started.
	ASSERT(hWnd);
	m_hMasterWnd = hWnd;
	m_iMasterUpdateCount = 0;

	// check for "silent" mode
	if ( sg_Config.m_dwDebug == 0 )
	{
		::DeleteFile(LOG_NAME_DLL);
	}

	// Install the hook
	return HookCBT_Install();
}

void CTaksiDll::UpdateConfigCustom()
{
	DEBUG_MSG(( "CTaksiDll::UpdateConfigCustom" LOG_CR ));
	// Increase the reconf-counter thus telling all the mapped DLLs that
	// they need to reconfigure themselves. (and check m_Config)
	m_dwConfigChangeCount++;
}

void CTaksiDll::OnDetachProcess()
{
	HookCBT_Uninstall();

	// If i was the current hook. then we probably weant to re-hook some other app.
	// if .exe is still running, tell it to re-install the CBT hook
	if ( !m_bMasterExiting && m_hMasterWnd )
	{
		::PostMessage(m_hMasterWnd, WM_APP_REHOOKCBT, 0, 0 );
		LOG_MSG(( "Post message for Master to re-hook CBT." LOG_CR));
	}
}

void CTaksiDll::DestroyDll()
{
	// Master App Taksi.EXE is exiting. so close up shop.
	// Set flag, so that TaksiDll knows to unhook device methods
	DEBUG_TRACE(( "CTaksiDll::DestroyDll" LOG_CR ));
	// Signal to unhook from IDirect3DDeviceN methods
	m_bMasterExiting = true;
	// sg_Config.DestroyConfig();

	// Uninstall the hook
	HookCBT_Uninstall();
}

bool CTaksiDll::InitDll()
{
	// Call this only once the first time.
	// determine my file name 
	ASSERT(g_hInst);
	DEBUG_MSG(( "CTaksiDll::InitDll" LOG_CR ));

	m_dwVersionStamp = TAKSI_VERSION_N;
	m_hMasterWnd = NULL;
	m_iMasterUpdateCount = 0;
	m_bMasterExiting = false;
	m_hHookCBT = NULL;
	m_iProcessCount = 0;	// how many processes attached?
	m_dwHotKeyMask = 0;

#ifdef USE_DX
	m_nDX8_Present = 0; // offset from start of DLL to the interface element i want.
	m_nDX8_Reset = 0;
	m_nDX9_Present = 0;
	m_nDX9_Reset = 0;
#endif

	if ( ! ::GetModuleFileName( g_hInst, m_szDllPathName, sizeof(m_szDllPathName)-1 ))
	{
		ASSERT(0);
		m_szDllPathName[0] = '\0';
	}

	// determine my directory 
	lstrcpy( m_szDllDir, m_szDllPathName);
	TCHAR* pszTitle = GetFileTitlePtr(m_szDllDir);
	ASSERT(pszTitle);
	*pszTitle = '\0';

	// determine logfile full path
	lstrcpy( m_szLogCentral, m_szDllPathName );
	TCHAR* pszShortLogFile = GetFileTitlePtr(m_szLogCentral);
	ASSERT(pszShortLogFile);
	lstrcpy( pszShortLogFile, LOG_NAME_DLL);

	// read optional configuration file. global init.
	m_dwConfigChangeCount = 0;	// changed when the Custom stuff in m_Config changes.

	// ASSUME InitMasterWnd will be called shortly.
	return true;
}

//**************************************************************************************

void CTaksiProcess::UpdateStat( TAKSI_PROCSTAT_TYPE eProp )
{
	// We just updated a m_Stats.X
	m_Stats.UpdateProcStat(eProp);
	if ( IsProcPrime())
	{
		sg_ProcStats.CopyProcStat( m_Stats, eProp );
	}
	if ( eProp == TAKSI_PROCSTAT_LastError )
	{
		LOG_MSG(( "%s" LOG_CR, m_Stats.m_szLastError ));
	}
}

int CTaksiProcess::MakeFileName( TCHAR* pszFileName, const TCHAR* pszExt )
{
	// pszExt = "avi" or "bmp"
	// ASSUME sizeof(pszFileName) = _MAX_PATH

	SYSTEMTIME time;
	::GetLocalTime(&time);

	int iLen = _sntprintf( pszFileName, _MAX_PATH-1,
		_T("%s%s-%d%02d%02d-%02d%02d%02d%03d.%s"), 
		sg_Config.m_szCaptureDir, m_szProcessTitleNoExt, 
		time.wYear, time.wMonth, time.wDay,
		time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
		pszExt );

	return iLen;
}

bool CTaksiProcess::CheckProcessMaster() const
{
	// the master process ?
	// TAKSI.EXE has special status!
	return( ! lstrcmpi( m_szProcessTitleNoExt, _T("taksi")));
}

bool CTaksiProcess::CheckProcessSpecial() const
{
	// This functions should be called when DLL is mapped into an application
	// to check if this is one of the special apps, that we shouldn't do any
	// graphics API hooking.
	// sets m_bIsProcessSpecial

	if ( CheckProcessMaster())
		return true;

	static const TCHAR* sm_SpecialNames[] = 
	{
		_T("devenv"),	// debugger!
		_T("dwwin"),	// debugger!
		_T("gaim"),
		_T("js7jit"),	// debugger!
		_T("monitor"),	// debugger!
		_T("taskmgr"),	// debugger!
	};

	for ( int i=0; i<COUNTOF(sm_SpecialNames); i++ )
	{
		if (!lstrcmpi( m_szProcessTitleNoExt, sm_SpecialNames[i] ))
			return true;
	}

	// Check if it's Windows Explorer. We don't want to hook it either.
	TCHAR szExplorer[_MAX_PATH];
	::GetWindowsDirectory( szExplorer, sizeof(szExplorer));
	lstrcat(szExplorer, _T("\\Explorer.EXE"));
	if (!lstrcmpi( m_Stats.m_szProcessFile, szExplorer))
		return true;

	if ( ! ::IsWindow( sg_Dll.m_hMasterWnd ))
	{
		// The master app is gone! this is bad!
		DEBUG_ERR(( "Taksi.EXE App is NOT Loaded! (%d)" LOG_CR, sg_Dll.m_hMasterWnd ));
#if 1
		sg_Dll.m_hMasterWnd = NULL;
		sg_Dll.m_bMasterExiting = true;
		return true;
#endif
	}

	return false;
}

void CTaksiProcess::CheckProcessCustom()
{
	// We have found a new process.
	// determine frame capturing algorithm special for the process.

	m_dwConfigChangeCount = sg_Dll.m_dwConfigChangeCount;

	// free the old custom config, if one existed
	if (m_pCustomConfig)
	{
		CTaksiConfig::CustomConfig_Delete(m_pCustomConfig);
		m_pCustomConfig = NULL;
	}

	m_bIsProcessSpecial = CheckProcessSpecial();
	if ( m_bIsProcessSpecial )
	{
		return;
	}

	// Re-read the configuration file and try to match any custom config with the
	// specified pszProcessFile. If successfull, return true.
	// Parameter: [in out] ppCfg  - gets assigned to the pointer to new custom config.
	// NOTE: this function must be called from within the DLL - not the main Taksi app.
	//

	CTaksiConfig config;
	if ( config.ReadIniFileFromDir(sg_Dll.m_szDllDir)) 
	{
		CTaksiConfigCustom* pCfgMatch = config.CustomConfig_FindPattern(m_Stats.m_szProcessFile);
		if ( pCfgMatch )
		{
			LOG_MSG(( "Using custom FrameRate=%g FPS, FrameWeight=%0.4f" LOG_CR, 
				pCfgMatch->m_fFrameRate, pCfgMatch->m_fFrameWeight ));

			// ignore this app?
			m_bIsProcessSpecial = ( pCfgMatch->m_fFrameRate <= 0 || pCfgMatch->m_fFrameWeight <= 0 );
			if ( m_bIsProcessSpecial )
			{
				LOG_MSG(( "FindCustomConfig: 0 framerate" LOG_CR));
				return;
			}

			// make a copy of custom config object
			m_pCustomConfig = config.CustomConfig_Alloc();
			if (!m_pCustomConfig)
			{
				LOG_MSG(( "FindCustomConfig: Unable to allocate new custom config" LOG_CR));
				return;
			}
			*m_pCustomConfig = *pCfgMatch;
			m_pCustomConfig->m_pNext = NULL;
			return;
		}
	}

	LOG_MSG(( "CheckProcessCustom: No custom config match." LOG_CR ));
}

HRESULT CTaksiProcess::AttachGraphXMode()
{
	// see if any of supported graphics API DLLs are already loaded. (and can be hooked)
	// NOTE: They are not truly attached til PresentFrameBegin() is called.

	if (m_bIsProcessSpecial)	// Dont hook special apps like My EXE or Explorer.
		return S_FALSE;

	// Checks whether an application uses any of supported APIs (D3D8, D3D9, OpenGL).
	// If so, their corresponding buffer-swapping/Present routines are hooked. 

	HRESULT hRes;
	for ( int i=0; i<COUNTOF(s_GraphxModes); i++ )
	{
		hRes = s_GraphxModes[i]->AttachGraphXMode();
		if ( SUCCEEDED(hRes))
			break;
	}
	return hRes;
}

void CTaksiProcess::StopGraphXMode()
{
	// this can be called in the PresentFrameEnd
	g_AVIThread.StopAVIThread();	// kill my thread, i'm done
	g_AVIFile.CloseAVIFile();	// close AVI file, if we were in recording mode 
	g_VideoFrame.FreeFrame();	// release buffer
	g_HotKeys.DetachHotKeys();
}

void CTaksiProcess::DetachGraphXMode()
{
	// the DLL is unloading or some other app now has the main focus/hook.
	StopGraphXMode();

	// give graphics module a chance to clean up.
	for ( int i=0; i<COUNTOF(s_GraphxModes); i++ )
	{
		s_GraphxModes[i]->FreeDll();
	}
}

bool CTaksiProcess::OnDllProcessAttach()
{
	// DLL_PROCESS_ATTACH
	// We have attached to a new process. via the CBT most likely.

	// Get Name of the process the DLL is attaching to
	if ( ! ::GetModuleFileName(NULL, m_Stats.m_szProcessFile, sizeof(m_Stats.m_szProcessFile)))
	{
		m_Stats.m_szProcessFile[0] = '\0';
	}

	// determine process full path 
	const TCHAR* pszTitle = GetFileTitlePtr(m_Stats.m_szProcessFile);
	ASSERT(pszTitle);

	// save short filename without ".exe" extension.
	const TCHAR* pExt = pszTitle + lstrlen(pszTitle) - 4;
	if ( !lstrcmpi(pExt, _T(".exe"))) 
	{
		int iLen = pExt - pszTitle;
		lstrcpyn( m_szProcessTitleNoExt, pszTitle, iLen+1 ); 
		m_szProcessTitleNoExt[iLen] = '\0';
	}
	else 
	{
		lstrcpy( m_szProcessTitleNoExt, pszTitle );
	}

	bool bProcMaster = CheckProcessMaster();
	if ( bProcMaster )
	{
		// First time here!
		if ( ! sg_Dll.InitDll())
		{
			DEBUG_ERR(( "InitDll FAILED!" LOG_CR ));
			return false;
		}
		sg_ProcStats.InitProcStats();
		sg_Config.InitConfig();
	}
	else
	{
		if ( sg_Dll.m_dwVersionStamp != TAKSI_VERSION_N )	// this is weird!
		{
			DEBUG_ERR(( "InitDll BAD VERSION 0%x != 0%x" LOG_CR, sg_Dll.m_dwVersionStamp, TAKSI_VERSION_N ));
			return false;
		}
	}

	sg_Dll.m_iProcessCount ++;
	LOG_MSG(( "DLL_PROCESS_ATTACH '%s' v" TAKSI_VERSION_S " (num=%d)" LOG_CR, pszTitle, sg_Dll.m_iProcessCount ));

	// determine process handle that is loading this DLL. 
	m_Stats.m_dwProcessId = ::GetCurrentProcessId();
	m_hProc = ::GetModuleHandle(NULL);

	// save handle to process' heap
	m_hHeap = ::GetProcessHeap();

	// do not hook on selected applications. set m_bIsProcessSpecial
	// (such as: myself, Explorer.EXE)
	CheckProcessCustom();	// determine frame capturing algorithm 

	if ( m_bIsProcessSpecial && ! bProcMaster )
	{
		LOG_MSG(( "Special process ignored." LOG_CR ));
		return true;
	}

	// log information on which process mapped the DLL
	sg_Dll.LogMessage( _T("mapped: "));

#ifdef USE_LOGFILE
	// open log file, specific for this process
	TCHAR szLogName[ _MAX_PATH ];
	lstrcpy(szLogName, sg_Dll.m_szDllDir );
	lstrcat(szLogName, _T("Taksi_") ); 
	lstrcat(szLogName, m_szProcessTitleNoExt); 
	lstrcat(szLogName, _T(".log"));

	if ( ! g_Log.OpenLogFile(szLogName))
	{
		LOG_MSG(( "Log start FAIL." LOG_CR));
	}
	else
	{
		DEBUG_TRACE(( "Log started." LOG_CR));
	}
#endif

	DEBUG_TRACE(( "m_Config.m_dwDebug=%d" LOG_CR, sg_Config.m_dwDebug));
	DEBUG_TRACE(( "m_Config.m_bUseDirectInput=%d" LOG_CR, sg_Config.m_bUseDirectInput));
	DEBUG_TRACE(( "sg_Dll.m_hHookCBT=%d" LOG_CR, (UINT_PTR)sg_Dll.m_hHookCBT));
	DEBUG_TRACE(( "sg_Dll.m_bMasterExiting=%d" LOG_CR, sg_Dll.m_bMasterExiting));

	g_FrameRate.InitFreqUnits();

	// see if any of supported API DLLs are already loaded.
	// If not, we'll just try again later.
	if ( AttachGraphXMode())
	{
	}

	return true;
}

bool CTaksiProcess::OnDllProcessDetach()
{
	// DLL_PROCESS_DETACH
	LOG_MSG(( "DLL_PROCESS_DETACH (num=%d)" LOG_CR, sg_Dll.m_iProcessCount ));

	DetachGraphXMode();

	// uninstall keyboard hook. was just for this process anyhow.
	g_UserKeyboard.UninstallHookKeys();

	// uninstall system-wide hook. then re-install it later if i want.
	sg_Dll.OnDetachProcess();

	if (m_pCustomConfig)
	{
		CTaksiConfig::CustomConfig_Delete(m_pCustomConfig);
		m_pCustomConfig = NULL;
	}

	// close specific log file 
	g_Log.CloseLogFile();

	// log information on which process unmapped the DLL 
	sg_Dll.LogMessage( _T("unmapped: "));
	sg_Dll.m_iProcessCount --;
	return true;
}

//**************************************************************************************

BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved )
{
	// DLL Entry Point 
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hInst = hInstance;
		ZeroMemory(&g_Proc, sizeof(g_Proc));
		return g_Proc.OnDllProcessAttach();
	case DLL_PROCESS_DETACH:
		return g_Proc.OnDllProcessDetach();
	}
	return true;    // ok
}
