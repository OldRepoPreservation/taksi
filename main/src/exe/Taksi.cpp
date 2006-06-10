//
// Taksi.cpp 
//
#include "../stdafx.h"
#include "Taksi.h"
#include "resource.h"
#include "guiConfig.h"

// Program Configuration
HINSTANCE g_hInst = NULL;
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
	MessageBox( hWnd, pszMsg, _T("Taksi"), MB_OK );
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

	g_Config.ReadIniFileFromDir(NULL);
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
