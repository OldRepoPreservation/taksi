//
// graphx_gdi.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include "graphx.h"
#include "../common/CWndGDI.h"

CTaksiGDI g_GDI;

#define IDT_TIMER 0x1234

//*****************************************

bool CTaksiGDI::DrawIndicator( TAKSI_INDICATE_TYPE eIndicate )
{
	// Draw my indicator on the window. Hooked WM_PAINT, or WM_NCPAINT

	m_dwTimeLastDrawIndicator = GetTickCount();

	if ( m_hWnd == NULL )
		return false;
	if ( eIndicate < 0 || eIndicate >= TAKSI_INDICATE_QTY )
		return false;
	CWndDC dc;
	// NOTE: I cant tell if the window is being overlapped?
	if ( ! dc.GetDCEx( m_hWnd, DCX_WINDOW|DCX_PARENTCLIP )) // DCX_CLIPSIBLINGS
		return false;
	// eIndicate = color.
	const BYTE* pColorDX = (const BYTE*) &sm_IndColors[eIndicate];
	COLORREF color = RGB( pColorDX[2], pColorDX[1], pColorDX[0] );
	CWndGDI brush;
	if ( brush.CreateSolidBrush(color) == NULL )
	{
		return false;
	}

	RECT rect = { INDICATOR_X, INDICATOR_Y, INDICATOR_X+INDICATOR_Width, INDICATOR_Y+INDICATOR_Height };

#if 0
	RECT rectW = rect;
	LPPOINT pPoint = (LPPOINT)&rectW;
	ClientToScreen( m_hWnd, pPoint+0 );
	ClientToScreen( m_hWnd, pPoint+1 );
	::InvalidateRect( NULL, &rectW, false );
#endif

	if ( ! ::FillRect( dc, &rect, brush.get_HBrush()))
	{
		return false;
	}
#if 0
	::ValidateRect( NULL, &rectW );
#endif
	return true;
}

HWND CTaksiGDI::GetFrameInfo( SIZE& rSize ) // virtual
{
	// Get the window and its size.
	// Find the primary window for the process.
	// Whole screen ??
	if ( m_hWnd == NULL )
	{
		m_hWnd = FindWindowForProcessID( ::GetCurrentProcessId());
		if ( m_hWnd == NULL )
			return NULL;
	}

	if ( sg_Config.m_bGDIFrame )
	{
		// Record the whole window include non client area.
		if ( ! ::GetWindowRect(m_hWnd, &m_WndRect))
			return NULL;
	}
	else
	{
		// Record just the client area.
		if ( ! ::GetClientRect(m_hWnd, &m_WndRect))
			return NULL;
		::ClientToScreen(m_hWnd,((LPPOINT)&m_WndRect)+0);
		::ClientToScreen(m_hWnd,((LPPOINT)&m_WndRect)+1);
	}
	rSize.cx = m_WndRect.right - m_WndRect.left;
	rSize.cy = m_WndRect.bottom - m_WndRect.top;
	return m_hWnd;
}

void CTaksiGDI::DrawMouse( HDC hMemDC )
{
	// Draw the cursor on the sampled bitmap.
	POINT xPoint; 
	::GetCursorPos( &xPoint ); 
	xPoint.x -= m_WndRect.left;
	xPoint.y -= m_WndRect.top;

	HCURSOR hCursor = ::GetCursor();

	ICONINFO iconinfo;	
	BOOL bRet = ::GetIconInfo( hCursor,  &iconinfo );
	if (bRet) 
	{
		xPoint.x -= iconinfo.xHotspot;
		xPoint.y -= iconinfo.yHotspot;

		//need to delete the hbmMask and hbmColor bitmaps
		//otherwise the program will crash after a while after running out of resource
		if (iconinfo.hbmMask) 
			::DeleteObject(iconinfo.hbmMask);
		if (iconinfo.hbmColor) 
			::DeleteObject(iconinfo.hbmColor);
	}		
		
	::DrawIcon(hMemDC,  xPoint.x,  xPoint.y, hCursor);
}

HRESULT CTaksiGDI::GetFrame( CVideoFrame& frame, bool bHalfSize )
{
	// Grab the whole window (or just the client area)

	// Create hBitmap for temporary use.
	CWndDC ScreenDC;
	if ( ! ScreenDC.GetDC( NULL ))
		return E_FAIL;
	CWndDC MemDC;
	if ( ! MemDC.CreateCompatibleDC( ScreenDC ))
		return E_FAIL;

	CWndGDI Bitmap( ScreenDC.CreateCompatibleBitmap( g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy ));
	if ( Bitmap.m_hObject == NULL )
	{
		return E_FAIL; 
	}
	ASSERT( Bitmap.get_HBitmap());

	// Put the pixels i want into Bitmap.
	{
	CWndGDISelect BitmapOld( MemDC, Bitmap );	 
	BOOL bBltRet = BitBlt( MemDC, 0, 0, g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy, ScreenDC, 
		m_WndRect.left, m_WndRect.top, SRCCOPY );
	if ( ! bBltRet)
		return E_FAIL;
	DrawMouse(MemDC);	// Draw Mouse cursor if they want that.
	}
#ifdef _DEBUG
	BITMAP BitmapInfo;
	Bitmap.GetObject( sizeof(BITMAP),&BitmapInfo );
#endif

	// Now move Bitmap pixels to frame
	BITMAPINFOHEADER bmih;
	ZeroMemory( &bmih, sizeof(bmih));
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = frame.m_Size.cx;
	bmih.biHeight = frame.m_Size.cy;
	bmih.biPlanes = 1;
	bmih.biBitCount = 24;	// 24_BPP
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = frame.get_SizeBytes();

	//
	// Get the bits from the bitmap and stuff them after the LPBI
	ASSERT(frame.m_pPixels);
	int iRet = ::GetDIBits( MemDC, Bitmap.get_HBitmap(),
		0,bmih.biHeight,frame.m_pPixels,
		(LPBITMAPINFO)&bmih, DIB_RGB_COLORS );
	if ( iRet<=0 )
	{
		DWORD dwLastError = ::GetLastError();
		DEBUG_ERR(( "GetDIBits FAILED (%d)" LOG_CR, dwLastError ));
		return HRESULT_FROM_WIN32(dwLastError);
	}

	// ??? bHalfSize not used !!

	return S_OK;
}

LRESULT CALLBACK CTaksiGDI::WndProcHook( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) // static
{
	ASSERT(g_GDI.m_WndProcOld);
	ASSERT(g_GDI.m_hWnd);

	switch ( uMsg )
	{
	case WM_DESTROY:
		// we need to unhook.
		{
		g_GDI.m_iReentrant++;
		WNDPROC WndProcOld = g_GDI.m_WndProcOld;
		g_GDI.UnhookFunctions();
		LRESULT lRes = ::CallWindowProc( WndProcOld, hWnd, uMsg, wParam, lParam );
		g_GDI.m_hWnd = NULL;
		g_GDI.m_bGotFrameInfo = false;
		g_GDI.m_iReentrant--;
		DEBUG_MSG((  "CTaksiGDI::WM_DESTROY" LOG_CR ));
		return lRes;
		}
	case WM_SIZE:
	case WM_MOVE:
		g_GDI.m_bGotFrameInfo = false;	// must update this.
		break;
#if 1
	case WM_PAINT:
		// painting the client area.
		if (g_GDI.m_iReentrant)
		{
			DEBUG_ERR(( "CTaksiGDI::WM_PAINT reentrant!" LOG_CR ));
		}
		else
		{
		g_GDI.m_iReentrant++;
		LRESULT lRes = ::CallWindowProc( g_GDI.m_WndProcOld, hWnd, uMsg, wParam, lParam );
		g_GDI.PresentFrameBegin(true);	// must be after the real WM_PAINT.
		g_GDI.PresentFrameEnd();
		g_GDI.m_iReentrant--;
		return lRes;
		}
#else
	case WM_NCPAINT:
		// painting the non-client area.		
		if (g_GDI.m_iReentrant)
		{
			DEBUG_ERR(( "CTaksiGDI::WM_PAINT reentrant!" LOG_CR ));
		}
		else
		{
		g_GDI.m_iReentrant++;
		g_GDI.PresentFrameBegin(true);	// must be after the real WM_PAINT.
		g_GDI.PresentFrameEnd();
		LRESULT lRes = ::CallWindowProc( g_GDI.m_WndProcOld, hWnd, uMsg, wParam, lParam );
		g_GDI.m_iReentrant--;
		return lRes;
		}
		break;
#endif
	case WM_TIMER: 
		if ( wParam != IDT_TIMER )	// the app owns this timer.
			break;
		if (g_GDI.m_iReentrant)
		{
			DEBUG_ERR(( "CTaksiGDI::WM_TIMER reentrant!" LOG_CR ));
			break;
		}
		g_GDI.m_iReentrant++;
		g_GDI.PresentFrameBegin( GetTickCount() - g_GDI.m_dwTimeLastDrawIndicator > 1000 );
		g_GDI.PresentFrameEnd();
		g_GDI.m_iReentrant--;
		return 0;	// this is my timer. don't pass to the app!
	}
	
	return ::CallWindowProc( g_GDI.m_WndProcOld, hWnd, uMsg, wParam, lParam );
}

bool CTaksiGDI::HookFunctions()
{
	// we should capture WM_PAINT + periodic
	if (!IsValidDll())
	{
		return false;
	}
	if ( ! sg_Config.m_bGDIUse )	// not allowed.
	{
		return false;
	}
	if ( m_hWnd == NULL )	// must find the window first.
	{
		m_hWnd = GetFrameInfo( g_Proc.m_Stats.m_SizeWnd );
		if ( m_hWnd == NULL )
			return false;
	}

	// SubClass the window.
	if ( m_WndProcOld )	// already hooked!
		return true;
	m_WndProcOld = (WNDPROC) GetWindowLongPtr( m_hWnd, GWL_WNDPROC );
	if ( m_WndProcOld == NULL )
		return false;
	SetWindowLongPtr( m_hWnd, GWL_WNDPROC, (LONG_PTR) WndProcHook );

	// Set up a timer to give me a frame rate.
	DWORD dwMSec = (DWORD)( 1000.0 / sg_Config.m_fFrameRateTarget );
	if ( dwMSec <= 0 )
		dwMSec = 1;
	UINT_PTR idTimer = ::SetTimer( m_hWnd, IDT_TIMER, dwMSec, NULL );
	if ( idTimer == 0 )
	{
		return false;
	}
	return true;
}

void CTaksiGDI::UnhookFunctions()
{
	if ( m_WndProcOld == NULL )	// not hooked!
		return;
	ASSERT(m_hWnd);
	// Release the timer.
	::KillTimer( m_hWnd, IDT_TIMER );
	SetWindowLongPtr( m_hWnd, GWL_WNDPROC, (LONG_PTR) m_WndProcOld );
	m_WndProcOld = NULL;

	// repaint just the indicator part ???
	RECT rect = { INDICATOR_X, INDICATOR_Y, INDICATOR_X+INDICATOR_Width, INDICATOR_Y+INDICATOR_Height };
	::InvalidateRect(m_hWnd,NULL,false);	// repaint just to be safe.

	__super::UnhookFunctions();
}

void CTaksiGDI::FreeDll()
{
	if ( ! IsValidDll())
		return;
	UnhookFunctions();
	__super::FreeDll();
}
