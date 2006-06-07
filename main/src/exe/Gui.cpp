//
// Gui.cpp
// main controls.
//
#include "../stdafx.h"
#include "Taksi.h"
#include "resource.h"
#include "guiConfig.h"

CGui g_GUI;

CGui::CGui()
{
}

int CGui::MakeWindowTitle( TCHAR* pszTitle, const TCHAR* pszHookApp ) // static
{
	TCHAR szName[ _MAX_PATH ];
	int iLen = LoadString( g_hInst, ID_APP, szName, sizeof(szName)-1 );
	if ( iLen <= 0 )
	{
		ASSERT(0);
		return iLen;
	}

	if ( pszHookApp == NULL || pszHookApp[0] == '\0' )
	{
		return _sntprintf( pszTitle, _MAX_PATH, _T("%s v") _T(TAKSI_VERSION_S), szName ); 
	}
	else
	{
		return _sntprintf( pszTitle, _MAX_PATH, _T("%s - %s"), szName, pszHookApp ); 
	}
}

bool CGui::UpdateWindowTitle()
{
	// Set the app title to reflect current hooked app.

	const TCHAR* pszHookApp = NULL;
	if ( sg_ProcStats.m_dwProcessId )
	{
		pszHookApp = ::GetFileTitlePtr( sg_ProcStats.m_szProcessFile );
	}

	TCHAR szTitle[ _MAX_PATH ];
	int iLen = MakeWindowTitle( szTitle, pszHookApp );
	if ( iLen <= 0 )
		return false;

	SetWindowText( m_hWnd, szTitle );
	return true;
}

bool CGui::IsButtonValid( TAKSI_HOTKEY_TYPE eKey ) const
{
	switch (eKey)
	{
	case TAKSI_HOTKEY_ConfigOpen:
		return true;
	case TAKSI_HOTKEY_HookModeToggle:
		return( ! sg_Dll.IsHookCBT());
	case TAKSI_HOTKEY_IndicatorToggle:
		if ( sg_Config.m_bShowIndicator )
			return true;
		return false;
	case TAKSI_HOTKEY_RecordStart:
	case TAKSI_HOTKEY_Screenshot:
	case TAKSI_HOTKEY_SmallScreenshot:
		if ( sg_ProcStats.m_dwProcessId == 0 )
			return false;
		if ( sg_ProcStats.m_eState == TAKSI_INDICATE_Recording )
			return false;
		return true;
	case TAKSI_HOTKEY_RecordStop:
		return( sg_ProcStats.m_eState == TAKSI_INDICATE_Recording );
	}
	return false;
}

LRESULT CGui::UpdateButton( TAKSI_HOTKEY_TYPE eKey )
{
	bool bGray = ! IsButtonValid(eKey);
	int i = eKey+((bGray)?BTN_QTY:0);
	return ::SendDlgItemMessage( m_hWnd, IDB_ConfigOpen + eKey,
		BM_SETIMAGE, IMAGE_BITMAP, 
		(LPARAM) m_Bitmap[i].get_HBitmap());
}

void CGui::UpdateButtonStates()
{
	// Gray record/stop depending on state.
	// IDB_RecordStop = TAKSI_HOTKEY_RecordStop
	for ( int i=0; i<TAKSI_HOTKEY_QTY; i++ )
	{
		UpdateButton( (TAKSI_HOTKEY_TYPE) i );
	}
	UpdateWindowTitle();
}

int CGui::GetVirtKeyName( TCHAR* pszName, int iLen, BYTE bVirtKey ) // static
{
	if ( bVirtKey == 0 )
		return 0;
	WORD bScanCode = ::MapVirtualKey( bVirtKey, 0);
	return ::GetKeyNameText( bScanCode << 16, pszName, iLen );
}

int CGui::GetHotKeyName( TCHAR* pszName, int iLen, TAKSI_HOTKEY_TYPE eHotKey ) // static
{
	// Get the text to describe the hotkey.
	// Control, Alt, Shift

	static const BYTE sm_vKeysExt[3] = 
	{
		VK_SHIFT,		// HOTKEYF_SHIFT
		VK_CONTROL,		// HOTKEYF_CONTROL
		VK_MENU,		// HOTKEYF_ALT
	};

	WORD wHotKey = g_Config.GetHotKey( eHotKey );
	if ( wHotKey == 0 )
		return 0;

	int i=0;
	BYTE bExt = HIBYTE(wHotKey);
	for ( int j=0; bExt && j<COUNTOF(sm_vKeysExt); j++, bExt >>= 1 )
	{
		if ( bExt & 1 )
		{
			i += GetVirtKeyName( pszName+i, iLen-i, sm_vKeysExt[j] );
			pszName[i++] = '+';
		}
	}
	return GetVirtKeyName( pszName+i, iLen-i, LOBYTE(wHotKey));
}

void CGui::UpdateButtonToolTips()
{
	// reflect state in tooltip ???
	for ( int i=0; i<BTN_QTY; i++ )
	{
		TCHAR szHelp[ _MAX_PATH ];
		LoadString( g_hInst, i + IDB_ConfigOpen, szHelp, sizeof(szHelp)-1 );

		TCHAR* pszText;
		TCHAR szHotKey[ _MAX_PATH ];
		int iLen = GetHotKeyName( szHotKey, COUNTOF(szHotKey), (TAKSI_HOTKEY_TYPE) i ); 
		if ( iLen > 0 )
		{
			TCHAR szText[ _MAX_PATH ];
			_sntprintf( szText, COUNTOF(szText), _T("(%s) %s"), szHotKey, szHelp );
			pszText = szText;
		}
		else
		{
			pszText = szHelp;
		}

		HWND hWndCtrl = GetDlgItem(i + IDB_ConfigOpen);
		ASSERT(hWndCtrl);
		if ( ! m_ToolTips.AddTool( hWndCtrl, pszText ))
		{
			DEBUG_ERR(("No tooltips" LOG_CR));
		}
	}
}

void CGui::OnCommandHelpURL() // static
{
	// IDM_SC_HELP_URL
	// NOTE: use the proper URL here? 
	// _T("http://www.menasoft.com/taksi")
	// _T("http://taksi.sourceforge.net/")
	CHttpLink_GotoURL( _T("http://taksi.sourceforge.net/"), SW_SHOWNORMAL );
}

BOOL CALLBACK AboutDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    switch (message) 
    { 
	case WM_INITDIALOG:
		{
			TCHAR szTmp[ _MAX_PATH ];
			int iLen = g_GUI.MakeWindowTitle( szTmp, NULL );
			iLen += _sntprintf( szTmp+iLen, COUNTOF(szTmp)-iLen, ", (built:" __DATE__ ")" );
			SetDlgItemText( hwndDlg, IDC_ABOUT_1, szTmp );
		}
		break;
	case WM_COMMAND: 
		switch (LOWORD(wParam)) 
        { 
		case IDOK: 
        case IDCANCEL: 
			EndDialog(hwndDlg, wParam); 
            return true; 
		} 
    } 
    return false; 
} 

int CGui::OnCommandHelpAbout( HWND hWndParent )
{
	// IDM_SC_HELP_ABOUT
	return DialogBox( g_hInst, MAKEINTRESOURCE(IDD_About), hWndParent, AboutDlgProc );
}

bool CGui::OnCommand( int id, int iNotify, HWND hControl )
{
	switch (id)
	{
	case TAKSI_HOTKEY_ConfigOpen:
	case IDB_ConfigOpen:
		if ( g_GUIConfig.m_hWnd == NULL )
		{
			g_GUIConfig.m_hWnd = CreateDialog( g_hInst, 
				MAKEINTRESOURCE(IDD_GuiConfig),
				NULL, CGuiConfig::DialogProc );
			if (g_GUIConfig.m_hWnd == NULL) 
			{
				return false;
			}
		}
		else
		{
			::ShowWindow(g_GUIConfig.m_hWnd,SW_SHOWNORMAL);
			::SetFocus(g_GUIConfig.m_hWnd);
		}
		return true;
	case TAKSI_HOTKEY_HookModeToggle:
	case IDB_HookModeToggle:
		if ( sg_Dll.IsHookCBT())
		{
			sg_Dll.HookCBT_Uninstall();
		}
		else
		{
			sg_Dll.HookCBT_Install();
		}
		UpdateButtonStates();
		return true;
	case TAKSI_HOTKEY_IndicatorToggle:
	case IDB_IndicatorToggle:
		g_Config.m_bShowIndicator = !g_Config.m_bShowIndicator;
		sg_Config.m_bShowIndicator = g_Config.m_bShowIndicator;
		UpdateButtonStates();
		return true;
	case TAKSI_HOTKEY_RecordStart:
	case TAKSI_HOTKEY_RecordStop:
	case IDB_RecordStart:
	case IDB_RecordStop:
	case IDB_Screenshot:
	case IDB_SmallScreenshot:
		{
		TAKSI_HOTKEY_TYPE eKey = (TAKSI_HOTKEY_TYPE)( TAKSI_HOTKEY_ConfigOpen + (id - IDB_ConfigOpen));
		if ( ! IsButtonValid(eKey))
			return false;
		sg_Dll.m_dwHotKeyMask |= (1<<eKey);
		}
		UpdateButtonStates();
		return true;
	case IDM_SC_HELP_URL:
		OnCommandHelpURL();
		return true;
	case IDM_SC_HELP_ABOUT:
		OnCommandHelpAbout(m_hWnd);
		return true;
	}
	return false;
}

LRESULT CALLBACK CGui::WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) // static
{
	switch (uMsg)
	{
	case WM_CREATE:
		DEBUG_MSG(( "CGui::WM_CREATE" LOG_CR ));
		break;
	case WM_MOVE:
		{
		ASSERT(hWnd);
		RECT rect;
		::GetWindowRect(hWnd,&rect);
		g_Config.m_ptMasterWindow.x = rect.left;
		g_Config.m_ptMasterWindow.y = rect.top;
		sg_Config.m_ptMasterWindow = g_Config.m_ptMasterWindow;
		}
		break;
	case WM_DESTROY: // destroy the app with the window.
		// Exit the application when the window closes
		DEBUG_MSG(( "CGui::WM_DESTROY" LOG_CR ));
		g_GUI.m_ToolTips.DestroyWindow();
		sg_Dll.m_hMasterWnd = NULL;
		::PostQuitMessage(1);
		break;
	case WM_SYSCOMMAND:	// off the sys menu.
		// wParam = SC_CLOSE
		// IDM_SC_HELP_URL 0xEEE0
		if ( g_GUI.OnCommand( wParam, 0, NULL ))
			return 0;
		break;
	case WM_COMMAND:
		if ( g_GUI.OnCommand( LOWORD(wParam), HIWORD(wParam), (HWND)lParam ))
			return 0;
		break;
	case WM_APP_UPDATE:
		// Use m_iMasterUpdateCount to throttle these ?
		g_GUI.UpdateButtonToolTips();
		g_GUI.UpdateButtonStates();
		return 1;
	case WM_APP_REHOOKCBT:
		// Re-install system-wide hook
		sg_Dll.HookCBT_Install();
		g_GUI.UpdateButtonStates();
		return 1;
	}
	return ::DefWindowProc(hWnd,uMsg,wParam,lParam);
}

static BOOL AppendMenuStr( HMENU hMenu, int id )
{
	// IDM_SC_HELP_URL
	TCHAR szTmp[ _MAX_PATH ];
	int iLen = LoadString( g_hInst, id, szTmp, sizeof(szTmp)-1 );
	return AppendMenu( hMenu, MF_INSERT|MF_STRING|MF_BYCOMMAND, 
		id, szTmp );
}

bool CGui::CreateGuiWindow( UINT nCmdShow )
{
	// CreateDialog - IDD_Gui
	// cbSize - the size of the structure.

	WNDCLASSEX wcx;
	ZeroMemory(&wcx,sizeof(wcx));
	wcx.cbSize = sizeof(WNDCLASSEX);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = CGui::WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = g_hInst;
	wcx.hIcon = LoadIcon(g_hInst,MAKEINTRESOURCE(ID_APP));
	wcx.hCursor = LoadCursor(NULL,IDC_ARROW);
	wcx.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = TAKSI_MASTER_CLASSNAME;
	wcx.hIconSm = NULL;

	// Register the class with Windows
	if ( ! ::RegisterClassEx(&wcx))
	{
		return false;
	}

	for ( int i=0; i<COUNTOF(m_Bitmap); i++ )
	{
		m_Bitmap[i].Attach( ::LoadBitmap( g_hInst, MAKEINTRESOURCE(i+IDB_ConfigOpen)));
		// ASSERT(m_Bitmap[i].get_HBitmap());
	}

	BITMAP bmi;
    int iRet = m_Bitmap[0].GetObject( sizeof(bmi), &bmi );
	if ( iRet != sizeof(bmi))
	{
		return false;
	}

	const DWORD c_dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	const DWORD c_dwExStyle = WS_EX_APPWINDOW ; // WS_EX_STATICEDGE

	RECT rect = { 0, 0, bmi.bmWidth*BTN_QTY, bmi.bmHeight };
	::AdjustWindowRectEx( &rect, c_dwStyle, false, c_dwExStyle );

	m_hWnd = CreateWindowEx( c_dwExStyle,
		TAKSI_MASTER_CLASSNAME,      // class name
		"", // title for our window (appears in the titlebar)
		c_dwStyle,
		g_Config.m_ptMasterWindow.x, g_Config.m_ptMasterWindow.y,  // initial x, y coordinate
		1+(rect.right-rect.left), 1+(rect.bottom-rect.top),   // width and height of the window
		HWND_DESKTOP,           // no parent window.
		NULL,           // no menu
		g_hInst,           // no creator
		NULL );          // no extra data
	if ( m_hWnd == NULL )
	{
		return false;
	}

	// SysMenu
	HMENU hMenu = ::GetSystemMenu(m_hWnd,false);
	if ( hMenu )
	{
		AppendMenuStr(hMenu,IDM_SC_HELP_URL);
		AppendMenuStr(hMenu,IDM_SC_HELP_ABOUT);
	}

	// build GUI controls
	// NOTE: Bitmap text font is : Small Fonts 7 ?
	if ( ! m_ToolTips.Create( m_hWnd, WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON ))
	{
		DEBUG_ERR(("No tooltips" LOG_CR));
	}

	//m_ToolTips.SetDelayTime(TTDT_INITIAL,1);

	int x=0;
	for ( int i=0; i<BTN_QTY; i++ )
	{
		HWND hWndCtrl = CreateWindow( _T("BUTTON"), _T(""), 
			WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_BITMAP,
			bmi.bmWidth*i, 0, 
			bmi.bmWidth, bmi.bmHeight,
			m_hWnd,
			(HMENU)(INT_PTR)( i + IDB_ConfigOpen ),
			g_hInst,
			NULL );
		ASSERT(hWndCtrl);

		// set the proper bitmap for the button.
		ASSERT(m_Bitmap[i].get_HBitmap());
		::SendMessage( hWndCtrl, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM) m_Bitmap[i].get_HBitmap());
	}

	UpdateButtonToolTips();
	UpdateButtonStates();

	// Show the window
	::ShowWindow(m_hWnd, nCmdShow);
	m_ToolTips.Start();
	return true;
}
