//
// graphx.cpp
//
#include "../stdafx.h"
#include <tchar.h>
#include "TaksiDll.h"
#include "graphx.h"
#include "HotKeys.h"

#ifdef USE_GDIP	// use Gdiplus::Bitmap to save images as PNG (or JPEG)
#include "../common/CImageGDIP.h"

static HRESULT MakeScreenShotGDIP( TCHAR* pszFileName, CVideoFrame& frame )
{
	// use Gdiplus::Bitmap to save images as PNG (or JPEG)
	// Cant use this in Win2K!
	if ( ! g_gdiplus.AttachGDIPInt())
	{
		return S_FALSE;
	}
	if ( sg_Config.m_szImageFormatExt[0] == '\0' )
	{
		return S_FALSE;
	}

	int iLenStr = g_Proc.MakeFileName( pszFileName, sg_Config.m_szImageFormatExt );

	static CLSID encoderClsid = {0};	// we should just hard code this ? ImageFormatPNG
	if ( encoderClsid.Data1 == 0 )
	{	
		HRESULT hRes = g_gdiplus.GetEncoderClsid( sg_Config.m_szImageFormatExt, &encoderClsid);
		if ( FAILED(hRes))
		{
			LOG_MSG(( "CTaksiGraphXBase::MakeScreenShot GDI+ GetEncoderClsid FAILED 0x%x" LOG_CR, hRes ));
			return hRes;
		}
	}

	BITMAPINFO bmi;
	frame.SetupBITMAPINFOHEADER(bmi.bmiHeader);
	Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromBITMAPINFO(&bmi,frame.m_pPixels);
	if ( pBitmap == NULL )
	{
		LOG_MSG(( "CTaksiGraphXBase::MakeScreenShot GDI+ FromBITMAPINFO FAILED" LOG_CR ));
		return CONVERT10_E_STG_DIB_TO_BITMAP;
	}

	WCHAR wFileName[_MAX_PATH];
#ifdef UNICODE
	lstrcpyn( wFileName, pszFileName, COUNTOF(wFileName)-1 );
	int iLenW = lstrlen( wFileName );
#else
	int iLenW = ::MultiByteToWideChar( CP_UTF8, 0, pszFileName, iLenStr+1, wFileName, COUNTOF(wFileName));
#endif

	// EncoderParameters encoderParams;
	Gdiplus::Status status = pBitmap->Save( wFileName, &encoderClsid, NULL );  

	HRESULT hRes;
	if ( status != Gdiplus::Ok )
	{
		if ( status == Gdiplus::Win32Error )
		{
			DWORD dwLastError = ::GetLastError();
			hRes = HRESULT_FROM_WIN32(dwLastError);
		}
		else
		{
			hRes = E_FAIL;
		}
		LOG_MSG(( "CTaksiGraphXBase::MakeScreenShot GDI+ err=0x%x" LOG_CR, hRes ));
	}
	else
	{
		hRes = S_OK;
	}
	delete pBitmap;	//done with it.
	return hRes;
}

#endif

// D3DCOLOR format is high to low, Alpha, Blue, Green, Red
const DWORD CTaksiGraphXBase::sm_IndColors[TAKSI_INDICATE_QTY] = 
{
	// RGB() sort of
	0xff88fe00,	// TAKSI_INDICATE_Ready = green
	0xff4488fe,	// TAKSI_INDICATE_Hooked = Blue,
	0xfffe4400,	// TAKSI_INDICATE_Recording = red.
	0xff444444,	// TAKSI_INDICATE_RecordPaused = Gray
};

//**************************************************************** 

HRESULT CTaksiGraphXBase::MakeScreenShot( bool bHalfSize )
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
		m_bGotFrameInfo = false;	// must get it again.
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError),
			_T("MakeScreenShot: unable to get RGB-data. 0%x"), hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return hRes;
	}

	// save as bitmap
	TCHAR szFileName[_MAX_PATH];

#ifdef USE_GDIP	// use Gdiplus::Bitmap to save images as PNG (or JPEG)
	hRes = MakeScreenShotGDIP( szFileName, frame );
	if ( hRes != S_OK )
#endif
	{
		// default to BMP format.
		g_Proc.MakeFileName( szFileName, _T("bmp"));
		hRes = frame.SaveAsBMP(szFileName);
	}
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

void CTaksiGraphXBase::RecordAVI_Reset()
{
	// There was a reset of the video device API
	// Could be the window is changing size etc.
	// Stop video record and start again later.

	// if in recording mode, close the AVI file,
	// and set the flag so that the next Present() will restart it.
	if ( g_AVIFile.IsOpen())
	{
		LOG_MSG(( "CTaksiGraphXBase::RecordAVI_Reset" LOG_CR));
		g_AVIThread.WaitForAllFrames();
		g_AVIFile.CloseAVI();
		if ( ! sg_Shared.m_bRecordPause )
		{
			g_HotKeys.ScheduleHotKey(TAKSI_HOTKEY_RecordBegin);	// re-open it later.
		}
	}

	// is HWND still valid ? 
	if ( ! ::IsWindow( g_Proc.m_Stats.m_hWndCap ))
	{
		g_Proc.m_Stats.m_hWndCap = NULL;	// must re-query for this
		g_Proc.UpdateStat(TAKSI_PROCSTAT_Wnd);
	}
	m_bGotFrameInfo = false;	// must get it again.
}

HRESULT CTaksiGraphXBase::RecordAVI_Start()
{
	// Start a ne wAVI file record. With a new name that has a time stamp.
	sg_Shared.m_bRecordPause = false;

	if ( g_AVIFile.IsOpen())
	{
		return S_OK;
	}

	g_Proc.m_Stats.ResetProcStats();
	g_Proc.UpdateStat(TAKSI_PROCSTAT_DataRecorded);
	g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);

	TCHAR szFileName[_MAX_PATH];
	g_Proc.MakeFileName( szFileName, _T("avi"));

	DEBUG_MSG(( "CTaksiGraphXBase::RecordAVI_Start (%s)." LOG_CR, szFileName ));

	if ( g_Proc.m_Stats.m_SizeWnd.cx <= 8 || g_Proc.m_Stats.m_SizeWnd.cy <= 8 )
	{
		// No point to making a video this small!
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			_T("Video Window Size TOO SMALL."));
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return DISP_E_BUFFERTOOSMALL;
	}

	int iDiv = (sg_Config.m_bVideoHalfSize) ? 2 : 1;

	g_FrameRate.InitStart();

	// allocate buffer for pixel data
	CVideoFrameForm FrameForm;
	FrameForm.InitPadded(g_Proc.m_Stats.m_SizeWnd.cx/iDiv, g_Proc.m_Stats.m_SizeWnd.cy/iDiv);

	// What frame rate do we want for our movie?
	double fFrameRate = sg_Config.m_fFrameRateTarget;
	if (g_Proc.m_pCustomConfig)
	{
		fFrameRate = g_Proc.m_pCustomConfig->m_fFrameRate;
	}

	HRESULT hRes = g_AVIFile.OpenAVICodec( FrameForm, fFrameRate, sg_Config.m_VideoCodec );
	if ( FAILED(hRes))
	{
		// ? strerror()
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			_T("Cant open AVI codec. Error=0x%x. Try a different video codec?"), hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		LOG_WARN(("g_AVIFile.OpenAVIFile FAILED 0x%x." LOG_CR, hRes ));
		return hRes;
	}
	
	hRes = g_AVIFile.OpenAVIFile( szFileName );
	if ( FAILED(hRes))
	{
		// ? strerror()
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			_T("Cant open AVI file. Error=0x%x."), hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		LOG_WARN(("g_AVIFile.OpenAVIFile FAILED 0x%x." LOG_CR, hRes ));
		return hRes;
	}

	ASSERT( g_AVIFile.IsOpen());
	LOG_MSG(( "RecordAVI_Start Recording started." LOG_CR));

	hRes = g_AVIThread.StartAVIThread();
	if ( IS_ERROR(hRes))
	{
		_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
			_T("Cant create AVI thread. Error=0x%x."), hRes );
		g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);
		return hRes;
	}

	_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
		_T("AVI record started"));
	g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);

	sg_Shared.UpdateMaster();
	return S_OK;
}

void CTaksiGraphXBase::RecordAVI_Stop()
{
	if ( ! g_AVIFile.IsOpen())
		return;

	DEBUG_MSG(( "CTaksiGraphXBase:RecordAVI_Stop" LOG_CR));
	g_AVIThread.WaitForAllFrames();
	g_AVIFile.CloseAVI();

	_sntprintf( g_Proc.m_Stats.m_szLastError, sizeof(g_Proc.m_Stats.m_szLastError), 
		_T("AVI file recorded. Stopped.") );
	g_Proc.UpdateStat(TAKSI_PROCSTAT_LastError);

	sg_Shared.UpdateMaster();
}

bool CTaksiGraphXBase::RecordAVI_Frame()
{
	// We are actively recording the AVI. a frame is ready if we want it.
	ASSERT( g_AVIFile.IsOpen());
	if ( sg_Shared.m_bRecordPause )	// just skip.
		return true;

	// determine whether this frame needs to be grabbed when recording. or just skipped.
	// dwFrameDups = multiple time frame shave gone by. compensate for this.
	DWORD dwFrameDups = g_FrameRate.CheckFrameRate();
	if ( dwFrameDups <= 0)	// i want this frame? or too soon to record a new frame?
		return true;

	CAVIFrame* pFrame = g_AVIThread.WaitForNextFrame();	// dont get new frame til i finish the last one.
	if (pFrame==NULL)
	{
		LOG_WARN(("CTaksiGraphXBase::RecordAVI_Frame() FAILED" LOG_CR ));
		return false;
	}

	// NOTE: I cant safely use sg_Config.m_bVideoHalfSize in real time.
	bool bVideoHalfSize = ( pFrame->m_Size.cx < ( g_Proc.m_Stats.m_SizeWnd.cx - 4 ));

	CLOCK_START(b);
	// get pixels from the backbuffer into the new buffer
	HRESULT hRes = GetFrame( *pFrame, bVideoHalfSize );	// virtual
	if ( IS_ERROR(hRes))
	{
		m_bGotFrameInfo = false;	// must get it again.
	}
	CLOCK_STOP(b,"Clock GetFrame=%d");

	// Move compression work to another thread
	g_AVIThread.SignalFrameAdd(pFrame,dwFrameDups);	// ready to compress/write

	g_FrameRate.EndOfOverheadTime();	// keep track of recording overhead
	return true;
}

//********************************************************

void CTaksiGraphXBase::ProcessHotKey( TAKSI_HOTKEY_TYPE eHotKey )
{
	// Open AVI file
	switch (eHotKey)
	{
	case TAKSI_HOTKEY_ConfigOpen:	// Open the config dialog window.
		sg_Shared.HotKey_ConfigOpen();
		return;
	case TAKSI_HOTKEY_HookModeToggle:	// don't switch during video capture?
		sg_Shared.HotKey_HookModeToggle();
		return;
	case TAKSI_HOTKEY_IndicatorToggle:
		sg_Shared.HotKey_IndicatorToggle();
		return;

	case TAKSI_HOTKEY_RecordBegin:
		// toggle video capturing mode
		if ( g_AVIFile.IsOpen())
		{
			RecordAVI_Stop();
		}
		else
		{
			RecordAVI_Start();
		}
		break;
	case TAKSI_HOTKEY_RecordStop: // Close AVI file.
		RecordAVI_Stop();
		break;
	case TAKSI_HOTKEY_RecordPause:	// toggle pause.
		sg_Shared.m_bRecordPause = ! sg_Shared.m_bRecordPause;
		if ( ! g_AVIFile.IsOpen() && ! sg_Shared.m_bRecordPause )
		{
			RecordAVI_Start();
		}
		g_FrameRate.InitStart();
		break;
	case TAKSI_HOTKEY_Screenshot:	// make custom screen shot
		MakeScreenShot(false);
		break;
	case TAKSI_HOTKEY_SmallScreenshot: // make small screen shot
		MakeScreenShot(true);
		break;
	}
}

void CTaksiGraphXBase::PresentFrameBegin( bool bChange )
{
	// We have hooked the present function of the graphics mode.
	// Called for each frame as it is about to be drawn.
	// NOTE: a hook is not truly complete til PresentFrameBegin() is called.

	ASSERT( m_bHookedFunctions );

	// Switching to hook some other app?
	if ( sg_Shared.IsHookCBT())
	{
		// Success!! we hooked a process! make this the prime process.
		if ( ! g_Proc.IsProcPrime())
		{
			LOG_MSG(( "PresentFrameBegin: New Prime Focus PID=%d." LOG_CR, g_Proc.m_Stats.m_dwProcessId ));
			sg_ProcStats.CopyProcStats( g_Proc.m_Stats );
			sg_Shared.HookCBT_Uninstall(); // thread safe?
		}
	}
	else if ( ! g_Proc.IsProcPrime())
	{
		// Dispose of myself! a new process has taken over.
		g_Proc.m_bStopGraphXAPI = true;
		return;
	}

	if ( sg_Shared.m_bMasterExiting )
		return;

	// Declare we have a working GraphXAPI hook.
	if ( ! g_Proc.StartGraphXAPI( get_GraphXAPI()))
	{
		// mode not allowed!
		return;	
	}

	// if the frame format has changed i want to know about that now
	if (!m_bGotFrameInfo)
	{
		// Determine HWND for our app window, and window/device dimensions. 
		DEBUG_TRACE(( "PresentFrameBegin.GetFrameInfo" LOG_CR ));
		g_Proc.m_Stats.m_hWndCap = GetFrameInfo( g_Proc.m_Stats.m_SizeWnd );	// virtual
		if ( ! g_Proc.m_Stats.m_hWndCap )	
		{
			// This is fatal i can not proceed!
			LOG_WARN(( "PresentFrameBegin.GetFrameInfo FAILED!" LOG_CR));
			return;	// i can do nothing else here!
		}
		g_Proc.UpdateStat(TAKSI_PROCSTAT_Wnd);
		g_Proc.UpdateStat(TAKSI_PROCSTAT_SizeWnd);
		m_bGotFrameInfo = true;
	}

	// Force hotkeys on the prime process.
	if (!g_HotKeys.m_bAttachedHotKeys)
	{
		// determine how we are going to handle keyboard hot-keys:
		g_HotKeys.AttachHotKeysToApp();
	}
#ifdef USE_DIRECTX
	// Process DirectInput input. polled.
	if ( g_UserDI.m_bSetup)
	{
		g_UserDI.ProcessDirectInput(); // will call ScheduleHotKey()
	}
#endif

	DWORD dwHotKeyMask = g_HotKeys.m_dwHotKeyMask;
	g_HotKeys.m_dwHotKeyMask = 0;
	if ( sg_Shared.m_dwHotKeyMask && g_Proc.IsProcPrime())
	{
		dwHotKeyMask |= sg_Shared.m_dwHotKeyMask;
		sg_Shared.m_dwHotKeyMask = 0;
	}
	for ( int i=0; dwHotKeyMask; i++, dwHotKeyMask>>=1 )
	{
		if ( dwHotKeyMask & 1 )
		{
			ProcessHotKey( (TAKSI_HOTKEY_TYPE) i );
		}
	}

	// write AVI-file, if in recording mode. 
	if ( g_AVIFile.IsOpen())
	{
		if ( ! RecordAVI_Frame())
		{
			// The record failed!
		}
	}

	// draw the indicator. (after video and screen cap are done)
	if ( bChange && ( sg_Config.m_bShowIndicator || g_AVIFile.IsOpen()))
	{
		TAKSI_INDICATE_TYPE eIndicate;
		if ( sg_Shared.m_bRecordPause )
			eIndicate = TAKSI_INDICATE_RecordPaused;
		else if ( g_AVIFile.IsOpen())
			eIndicate = TAKSI_INDICATE_Recording;
		else if ( sg_Shared.IsHookCBT()) 
			eIndicate = TAKSI_INDICATE_Hooked;
		else
			eIndicate = TAKSI_INDICATE_Ready;
		if ( g_Proc.m_Stats.m_eState != eIndicate )
		{
			g_Proc.m_Stats.m_eState = eIndicate;
			g_Proc.UpdateStat(TAKSI_PROCSTAT_State);
			sg_Shared.UpdateMaster();
		}
		HRESULT hRes = DrawIndicator( eIndicate );
		if ( IS_ERROR(hRes))	//virtual
		{
			LOG_WARN(( "DrawIndicator FAILED 0x%x!" LOG_CR, hRes )); 
			sg_Config.m_bShowIndicator = false;
		}
	}
}

void CTaksiGraphXBase::PresentFrameEnd()
{
	// check unhook flag
	if ( sg_Shared.m_bMasterExiting || g_Proc.m_bStopGraphXAPI )
	{
		DEBUG_TRACE(( "PresentFrameEnd:StopGraphXAPI." LOG_CR ));
		UnhookFunctions();					// we're ordered to unhook methods.
		g_Proc.StopGraphXAPIs();
		// the dll should now unload?
		return;
	}

	// check if we need to reconfigure
	if ( sg_Shared.m_dwConfigChangeCount != g_Proc.m_dwConfigChangeCount && 
		! g_AVIFile.IsOpen())
	{
		LOG_MSG(( "PresentFrameEnd:m_dwConfigChangeCount (%d)..." LOG_CR, g_Proc.m_dwConfigChangeCount ));
		g_Proc.CheckProcessCustom();
	}
}

HRESULT CTaksiGraphXBase::AttachGraphXAPI()
{
	// DLL_PROCESS_ATTACH
	// Check the DLL that would support this graphics mode.
	// NOTE: Dont force graphics DLL to load. just look for its presense.
	// ONLY CALLED FROM: AttachGraphXAPIs()
	// Not fully attached til PresentFrameBegin() is called.

	if ( ! sg_Config.m_abUseAPI[ get_GraphXAPI() ] )	// not allowed.
	{
		return HRESULT_FROM_WIN32(ERROR_DLL_NOT_FOUND);
	}

	const TCHAR* pszName = get_DLLName();	// virtual
	if ( !FindDll(pszName)) // already loaded?
	{
		DEBUG_TRACE(( "AttachGraphXAPI NO '%s'" LOG_CR, pszName ));
		return HRESULT_FROM_WIN32(ERROR_DLL_NOT_FOUND);
	}

	ASSERT( IsValidDll());
	HRESULT hRes = HookFunctions();	// virtual
	if ( FAILED(hRes))
	{
		m_bHookedFunctions = false;	
		return HRESULT_FROM_WIN32(ERROR_INVALID_HOOK_HANDLE);
	}
	if ( hRes == S_FALSE )
	{
		ASSERT(m_bHookedFunctions);
		return S_OK;
	}

	m_bHookedFunctions = true;	
	LOG_MSG(( "AttachGraphXAPI '%s'" LOG_CR, pszName ));
	// NOTE: a hook is not truly complete til PresentFrameBegin() is called.
	m_bGotFrameInfo = false;	// must update this ASAP.
	return S_OK;
}
