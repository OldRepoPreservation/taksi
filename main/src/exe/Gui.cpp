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

bool CGui::UpdateWindowTitle()
{
	// Set the app title to reflect current hooked app.
	TCHAR szName[ _MAX_PATH ];
	if ( ! LoadString( g_hInst, ID_APP, szName, sizeof(szName)-1 ))
	{
		ASSERT(0);
		return false;
	}
	const TCHAR* pszHookApp = NULL;
	if ( sg_ProcStats.m_dwProcessId )
	{
		pszHookApp = ::GetFileTitlePtr( sg_ProcStats.m_szProcessFile );
	}

	TCHAR szTitle[ _MAX_PATH ];
	if ( pszHookApp == NULL || pszHookApp[0] == '\0' )
	{
		_sntprintf( szTitle, COUNTOF(szTitle), _T("%s v") _T(TAKSI_VERSION_S), szName ); 
	}
	else
	{
		_sntprintf( szTitle, COUNTOF(szTitle), _T("%s - %s"), szName, pszHookApp ); 
	}

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
			return false;
		return true;
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
	UpdateButton( TAKSI_HOTKEY_HookModeToggle );
	UpdateButton( TAKSI_HOTKEY_RecordStart );
	UpdateButton( TAKSI_HOTKEY_RecordStop );
	UpdateButton( TAKSI_HOTKEY_Screenshot );
	UpdateButton( TAKSI_HOTKEY_SmallScreenshot );

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
	BYTE bExt = wHotKey >> 8 ;
	for ( int j=0; bExt && j<COUNTOF(sm_vKeysExt); j++, bExt >>= 1 )
	{
		if ( bExt & 1 )
		{
			i += GetVirtKeyName( pszName+i, iLen-i, sm_vKeysExt[j] );
			pszName[i++] = '+';
		}
	}
	return GetVirtKeyName( pszName+i, iLen-i, (BYTE) wHotKey );
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
	// NOTE: use the proper URL here? 
	// _T("http://www.menasoft.com/taksi")
	// _T("http://taksi.sourceforge.net/")
	CHttpLink_GotoURL( _T("http://taksi.sourceforge.net/"), SW_SHOWNORMAL );
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
	case IDB_HookModeToggle:
	case TAKSI_HOTKEY_HookModeToggle:
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
	case IDB_IndicatorToggle:
	case TAKSI_HOTKEY_IndicatorToggle:
		g_Config.m_bShowIndicator = !g_Config.m_bShowIndicator;
		sg_Config.m_bShowIndicator = g_Config.m_bShowIndicator;
		UpdateButtonStates();
		return true;
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
		CW_USEDEFAULT,  CW_USEDEFAULT,  // initial x, y coordinate
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
		// IDM_SC_HELP_URL
		TCHAR szTmp[ _MAX_PATH ];
		LoadString( g_hInst, IDM_SC_HELP_URL, szTmp, sizeof(szTmp)-1 );
		AppendMenu( hMenu, MF_INSERT|MF_STRING|MF_BYCOMMAND, 
			IDM_SC_HELP_URL, szTmp );
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
