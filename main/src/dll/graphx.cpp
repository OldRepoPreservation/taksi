//
// graphx.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include "graphx.h"
#include "HotKeys.h"

// D3DCOLOR format is high to low, Alpha, Blue, Green, Red
const DWORD CTaksiGraphX::sm_IndColors[TAKSI_INDICATE_QTY] = 
{
	0xff88fe00,	// TAKSI_INDICATE_Ready
	0xff4488fe,	// TAKSI_INDICATE_Hooked
	0xfffe4400,	// TAKSI_INDICATE_Recording
};

//**************************************************************** 

HRESULT CTaksiGraphX::MakeScreenShot( bool bHalfSize )
{
	// make custom screen shot
	CLOCK_START(a);
	int iDiv = bHalfSize ? 2 : 1;

	// allocate buffer for pixel data
	CVideoFrame frame;
	frame.AllocPadXY( g_Proc.m_Stats.m_SizeWnd.cx / iDiv, g_Proc.m_Stats.m_SizeWnd.cy / iDiv );

	// get pixels from the backbuffer into the new buffer
	HRESULT hRes = GetFrame( frame, bHalfSize );	// virtual
	if (FAILED(hRes))
	{
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError),
			_T("MakeScreenShot: unable to get RGB-data. 0%x"), hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return hRes;
	}

	// save as bitmap
	TCHAR szFileName[_MAX_PATH];
	g_Proc.MakeFileName( szFileName, "bmp" );

	hRes = frame.SaveAsBMP(szFileName);
	if (FAILED(hRes))
	{
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError),
			_T("MakeScreenShot(%d) SaveAsBMP '%s' failed. 0%x"),
			bHalfSize, szFileName, hRes	);
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return hRes;
	}

	CLOCK_STOP(a, "MakeScreenShot: clock=%d");

	g_Proc.m_Stats.m_dwDataRecorded = frame.get_SizeBytes();
	g_Proc.UpdateStat(TAKSI_PROCSTAT_DataRecorded);

	_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError),
		_T("MakeScreenShot(%d) '%s' OK"),
		bHalfSize, szFileName	);
	g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
	return S_OK;
}

//*****************************************

void CTaksiGraphX::RecordAVI_Reset()
{
	// There was a reset of the video device API
	// Could be the window is changing size etc.
	// Stop video record and start again later.

	// if in recording mode, close the AVI file,
	// and set the flag so that the next Present() will restart it.
	if ( g_AVIFile.IsOpen())
	{
		g_AVIThread.WaitForDataDone();
		g_AVIFile.CloseAVIFile();

		LOG_MSG(( "CTaksiGraphX::RecordAVI_Reset" LOG_CR));
		g_HotKeys.SetHotKey(TAKSI_HOTKEY_RecordStart);	// re-open it later.
	}

	g_VideoFrame.FreeFrame();	// re-alloc later.

	// is HWND still valid ? 
	if ( ! ::IsWindow( g_Proc.m_Stats.m_hWnd ))
	{
		g_Proc.m_Stats.m_hWnd = NULL;	// must re-query for this
		g_Proc.UpdateStat(TAKSI_PROCSTAT_Wnd);
	}
	m_bGotFrameInfo = false;	// must get it again.
}

HRESULT CTaksiGraphX::RecordAVI_Start()
{
	ASSERT( ! g_AVIFile.IsOpen());

	TCHAR szFileName[_MAX_PATH];
	g_Proc.MakeFileName( szFileName, "avi" );

	DEBUG_MSG(( "CTaksiGraphX::RecordAVI_Start (%s)." LOG_CR, szFileName ));

	if ( g_Proc.m_Stats.m_SizeWnd.cx <= 8 || g_Proc.m_Stats.m_SizeWnd.cy <= 8 )
	{
		// No point to making a video this small!
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			_T("Video Window Size TOO SMALL."));
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return E_FAIL;
	}

	int iDiv = (sg_Config.m_bVideoHalfSize) ? 2 : 1;

	// allocate buffer for pixel data
	g_VideoFrame.AllocPadXY( g_Proc.m_Stats.m_SizeWnd.cx/iDiv, g_Proc.m_Stats.m_SizeWnd.cy/iDiv );

	// What frame rate do we want for our movie?
	double fFrameRate = sg_Config.m_fFrameRateTarget;
	if (g_Proc.m_pCustomConfig)
	{
		fFrameRate = g_Proc.m_pCustomConfig->m_fFrameRate;
	}

	HRESULT hRes = g_AVIFile.OpenAVIFile( szFileName, g_VideoFrame, fFrameRate, sg_Config.m_VideoCodec );
	if ( FAILED(hRes))
	{
		// ? strerror()
		_snprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			"Cant open AVI stream. Error=0x%x.", hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		DEBUG_ERR(("g_AVIFile.OpenAVIFile FAIL %d." LOG_CR, hRes ));
		return hRes;
	}

	ASSERT( g_AVIFile.IsOpen());
	LOG_MSG(( "PresentFrameBegin Recording started." LOG_CR));

	hRes = g_AVIThread.StartAVIThread();
	if ( IS_ERROR(hRes))
	{
		_snprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			"Cant create AVI thread. Error=0x%x.", hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return hRes;
	}

	_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
		_T("AVI record started"));
	g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);

	sg_Dll.UpdateMaster();
	return S_OK;
}

void CTaksiGraphX::RecordAVI_Stop()
{
	if ( ! g_AVIFile.IsOpen())
		return;

	DEBUG_MSG(( "CTaksiGraphX:RecordAVI_Stop" LOG_CR));
	g_AVIThread.WaitForDataDone();
	g_AVIFile.CloseAVIFile();

	_snprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
		"AVI file recorded. Stopped." );
	g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);

	sg_Dll.UpdateMaster();
}

void CTaksiGraphX::RecordAVI_Frame()
{
	// We are actively recording the AVI. a frame is ready.
	ASSERT( g_AVIFile.IsOpen());

	// determine whether this frame needs to be grabbed when recording. or just skipped.
	DWORD dwFrameDups = g_FrameRate.CheckFrameRate();
	if ( dwFrameDups <= 0)	// i want this frame?
		return;

	g_AVIThread.WaitForDataDone();	// dont get new frame til i finish the last one.

	// NOTE: I cant safely use sg_Config.m_bVideoHalfSize in real time.
	bool bVideoHalfSize = ( g_VideoFrame.m_Size.cx < ( g_Proc.m_Stats.m_SizeWnd.cx - 4 ));

	CLOCK_START(b);
	// get pixels from the backbuffer into the new buffer
	GetFrame( g_VideoFrame, bVideoHalfSize );	// virtual
	CLOCK_STOP(b,"Clock GetFrame=%d");

	// Move compression work to another thread
	g_AVIThread.SignalDataStart(dwFrameDups);	// ready to compress/write

	g_FrameRate.MeasureOverhead();	// keep track of recording overhead
}

//********************************************************

void CTaksiGraphX::PresentFrameBegin( bool bChange )
{
	// We have hooked the present function of the graphics mode.
	// Called for each frame as it is about to be drawn.
	// NOTE: a hook is not truly complete til PresentFrameBegin() is called.

	// Switching to hook some other app?
	if ( sg_Dll.IsHookCBT())
	{
		// Success!! we hooked a process! make this the prime process.
		if ( ! g_Proc.IsProcPrime())
		{
			LOG_MSG(( "PresentFrameBegin: New Prime Focus." LOG_CR));
			sg_ProcStats.CopyProcStats( g_Proc.m_Stats );
			sg_Dll.HookCBT_Uninstall();
		}
	}
	else if ( ! g_Proc.IsProcPrime())
	{
		// Dispose of myself! a new process has taken over.
		g_Proc.m_bStopGraphXMode = true;
		return;
	}

	if ( sg_Dll.m_bMasterExiting )
		return;

	if ( g_Proc.IsProcPrime() && sg_Dll.m_dwHotKeyMask )
	{
		g_HotKeys.m_dwHotKeyMask |= sg_Dll.m_dwHotKeyMask;
		sg_Dll.m_dwHotKeyMask = 0;
	}

	// if the frame format has changed i want to know about that now
	if (!m_bGotFrameInfo)
	{
		// Determine HWND for our app window, and window/device dimensions. 
		DEBUG_TRACE(( "PresentFrameBegin.GetFrameInfo" LOG_CR ));
		g_Proc.m_Stats.m_hWnd = GetFrameInfo( g_Proc.m_Stats.m_SizeWnd );	// virtual
		if ( ! g_Proc.m_Stats.m_hWnd )	
		{
			// This is fatal i can not proceed!
			DEBUG_ERR(( "PresentFrameBegin.GetFrameInfo FAIL!" LOG_CR));
			return;	// i can do nothing else here!
		}
		g_Proc.UpdateStat(TAKSI_PROCSTAT_Wnd);
		g_Proc.UpdateStat(TAKSI_PROCSTAT_SizeWnd);
		m_bGotFrameInfo = true;
	}

	if (!g_HotKeys.m_bAttachedHotKeys)
	{
		// determine how we are going to handle keyboard hot-keys:
		g_HotKeys.AttachHotKeysToApp();
	}

#ifdef USE_DX
	// Process DirectInput input. polled.
	if ( g_UserDI.m_bSetup)
	{
		g_UserDI.ProcessDirectInput();
	}
#endif

	// Open AVI file
	if ( g_HotKeys.IsHotKey(TAKSI_HOTKEY_RecordStart)) 
	{
		RecordAVI_Start();
		g_HotKeys.ClearHotKey(TAKSI_HOTKEY_RecordStart);
	}

	// Close AVI file.
	else if ( g_HotKeys.IsHotKey(TAKSI_HOTKEY_RecordStop))
	{
		RecordAVI_Stop();
		g_HotKeys.ClearHotKey(TAKSI_HOTKEY_RecordStop);
	}

	// make custom screen shot
	if ( g_HotKeys.IsHotKey(TAKSI_HOTKEY_Screenshot))
	{
		MakeScreenShot(false);
		g_HotKeys.ClearHotKey(TAKSI_HOTKEY_Screenshot);
	}

	// make small screen shot
	else if ( g_HotKeys.IsHotKey(TAKSI_HOTKEY_SmallScreenshot))
	{
		MakeScreenShot(true);
		g_HotKeys.ClearHotKey(TAKSI_HOTKEY_SmallScreenshot);
	}

	// write AVI-file, if in recording mode. 
	if ( g_AVIFile.IsOpen())
	{
		RecordAVI_Frame();
	}

	// draw the indicator. (after video and screen cap are done)
	if ( bChange && ( sg_Config.m_bShowIndicator || g_AVIFile.IsOpen()))
	{
		TAKSI_INDICATE_TYPE eIndicate;
		if ( g_AVIFile.IsOpen())
			eIndicate = TAKSI_INDICATE_Recording;
		else if ( sg_Dll.IsHookCBT()) 
			eIndicate = TAKSI_INDICATE_Hooked;
		else 
			eIndicate = TAKSI_INDICATE_Ready;
		if ( g_Proc.m_Stats.m_eState != eIndicate )
		{
			g_Proc.m_Stats.m_eState = eIndicate;
			g_Proc.UpdateStat(TAKSI_PROCSTAT_State);
			sg_Dll.UpdateMaster();
		}
		if ( ! DrawIndicator( eIndicate ))	//virtual
		{
			DEBUG_ERR(( "DrawIndicator FAIL!" LOG_CR )); 
			sg_Config.m_bShowIndicator = false;
		}
	}
}

void CTaksiGraphX::PresentFrameEnd()
{
	// check unhook flag
	if ( sg_Dll.m_bMasterExiting || g_Proc.m_bStopGraphXMode )
	{
		DEBUG_TRACE(( "PresentFrameEnd:StopGraphXMode." LOG_CR ));
		UnhookFunctions();					// we're ordered to unhook methods.
		g_Proc.StopGraphXMode();
		// the dll should now unload?
		return;
	}

	// check if we need to reconfigure
	if ( sg_Dll.m_dwConfigChangeCount != g_Proc.m_dwConfigChangeCount && 
		! g_AVIFile.IsOpen())
	{
		LOG_MSG(( "PresentFrameEnd:m_dwConfigChangeCount (%d)..." LOG_CR, g_Proc.m_dwConfigChangeCount ));
		g_Proc.CheckProcessCustom();
	}
}

HRESULT CTaksiGraphX::AttachGraphXMode()
{
	// DLL_PROCESS_ATTACH
	// Check the DLL that would support this graphics mode.
	// NOTE: Dont force graphics DLL to load. just look for its presense.

	if (m_bHookedFunctions) 
		return S_FALSE;

	const TCHAR* pszName = get_DLLName();	// virtual
	if ( !FindDll(pszName)) // already loaded?
	{
		DEBUG_TRACE(( "AttachGraphXMode NO '%s'" LOG_CR, pszName ));
		return E_FAIL;
	}

	LOG_MSG(( "AttachGraphXMode '%s'" LOG_CR, pszName ));

	m_bHookedFunctions = HookFunctions();	// virtual
	if ( ! m_bHookedFunctions )
	{
		return E_FAIL;
	}

	// NOTE: a hook is not truly complete til PresentFrameBegin() is called.
	m_bGotFrameInfo = false;	// must update this ASAP.
	return S_OK;
}
