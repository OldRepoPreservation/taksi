//
// Taksi.cpp 
//
#include "../stdafx.h"
#include "Taksi.h"
#include "resource.h"
#include "guiConfig.h"

// Program Configuration
HINSTANCE g_hInst = NULL;
const TCHAR g_szAppTitle[] = _T("Taksi");
CTaksiConfig g_Config;

const TCHAR* CheckIntResource( const TCHAR* pszText, TCHAR* pszTmp )
{
	// Interpret MAKEINTRESOURCE()
	if ( ! ISINTRESOURCE(pszText))	// MAKEINTRESOURCE()
		return pszText;
	int iLen = LoadString( g_hInst, GETINTRESOURCE(pszText), pszTmp, _MAX_PATH-1 );
	if ( iLen <= 0 )
		return _T("");
	return pszTmp;
}

void DlgTODO( HWND hWnd, const TCHAR* pszMsg )
{
	// Explain to the user why this doesnt work yet.
	if ( pszMsg == NULL )
		pszMsg = _T("TODO");
	MessageBox( hWnd, pszMsg, g_szAppTitle, MB_OK );
}

void CheckVideoCodec( HWND hWnd )
{
	if ( g_Config.m_bVideoCodecMsg )
		return;
	g_Config.m_bVideoCodecMsg = true;
	switch ( g_Config.m_VideoCodec.m_v.fccHandler )
	{
	case MAKEFOURCC('D','I','B',' '): // no compress.
	case MAKEFOURCC('d','i','b',' '): // no compress.
		MessageBox( hWnd, 
			_T("The selected video codec uses a VERY large amount of disk space.")
			_T("You may want to check the configuration dialog and pick a better codec."),
			g_szAppTitle, MB_OK );
		break;
	case MAKEFOURCC('m','s','v','c'): // Video1 = CRAM
	case MAKEFOURCC('M','S','V','C'): // Video1 = CRAM
		// just warn that the codec isnt very good.
		MessageBox( hWnd, 
			_T("The selected video codec (MS-CRAM) is popular")
			_T("and has low CPU usage but doesnt have very good compression.")
			_T("You may want to check the configuration dialog and pick a different codec."),
			g_szAppTitle, MB_OK );
		break;
		// MPEG4 may slow frames per sec.
	}
}

int APIENTRY WinMain( HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPSTR     lpCmdLine,
					 int       nCmdShow)
{
	// NOTE: Dll has already been attached. DLL_PROCESS_ATTACH
	g_hInst = hInstance;

	HANDLE hMutex = ::CreateMutex( NULL, true, "TaksiMutex" );	// diff name than TAKSI_MASTER_CLASSNAME
	if ( ::GetLastError() == ERROR_ALREADY_EXISTS )
	{
		// Set focus to the previous window.
		HWND hWnd = FindWindow( TAKSI_MASTER_CLASSNAME, NULL );
		if ( hWnd )
		{
			::SetActiveWindow( hWnd );
		}
		return -1;
	}

	if ( ! g_Config.ReadIniFileFromDir(NULL))
	{
		// this is ok since we can just take all defaults.
	}

	// Test my Save Dir
	HRESULT hRes = g_Config.FixCaptureDir();
	if ( FAILED(hRes))
	{
		MessageBox( NULL, 
			_T("Cant create save file directory. Please check the configuration dialog"),
			g_szAppTitle, MB_OK );
	}

	// Test my codec.
	hRes = g_Config.FixVideoCodec();
	if ( FAILED(hRes))
	{
		MessageBox( NULL, 
			_T("The selected video codec doesnt work on this system. Please check the configuration dialog"),
			g_szAppTitle, MB_OK );
	}
	else 
	{
		// may want to select a better codec?
		CheckVideoCodec( NULL );
	}

	sg_Config.CopyConfig( g_Config );

	LoadString( g_hInst, IDS_SELECT_APP_HOOK, 
		sg_ProcStats.m_szLastError, COUNTOF(sg_ProcStats.m_szLastError));

	InitCommonControls();
#ifdef _DEBUG
	CTaksiDll* pDll = &sg_Dll;
#endif
	if ( ! g_GUI.CreateGuiWindow(nCmdShow))
	{
		return -1;
	}

#ifdef USE_DX
	Test_DirectX8(g_GUI.m_hWnd);		// Get method offsets of IDirect3DDevice8 vtable
	Test_DirectX9(g_GUI.m_hWnd);		// Get method offsets of IDirect3DDevice9 vtable
#endif

	// Set HWND for reference by the DLL
	if ( ! sg_Dll.InitMasterWnd(g_GUI.m_hWnd))
	{
		return -1;
	}

	for(;;)
	{
		MSG msg; 
		if ( ! GetMessage(&msg,NULL,0,0))
			break;
		if (!IsDialogMessage(g_GUIConfig.m_hWnd, &msg)) // need to call this to make WS_TABSTOP work
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// Free memory taken by custom configs etc.
	sg_Dll.DestroyDll();
	return 0;
}
