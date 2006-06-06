//
// Common.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "../stdafx.h"
#include <windowsx.h>
#include "Common.h"
#include "CWndGDI.h"

//**************************************************************************************

TIMEFAST_t GetPerformanceCounter()
{
	// Get high performance time counter. now
	LARGE_INTEGER count;
	::QueryPerformanceCounter(&count);
	return count.QuadPart;
}

TCHAR* GetFileTitlePtr( TCHAR* pszPath )
{
	// Given a full file path, get a pointer to its title.
	TCHAR* p = pszPath + lstrlen(pszPath); 
	while ((p > pszPath) && (*p != '\\')) 
		p--;
	if (*p == '\\')
		p++; 
	return p;
}

char* Str_SkipSpace( const char* pszNon )
{
	// NOTE: \n is a space.
	while ( *pszNon && isspace(*pszNon))
		pszNon++;
	return (char*) pszNon;
}

//**************************************************************************************

bool CHookJump::InstallHook( LPVOID pFunc, LPVOID pFuncNew )
{
	ASSERT(pFuncNew);
	if ( pFunc == NULL )
	{
		DEBUG_ERR(("InstallHook: NULL."));
		return false;
	}
	if ( ! memcmp(pFunc, m_Jump, sizeof(m_Jump)))
	{
		DEBUG_MSG(("InstallHook: already has JMP-implant." LOG_CR));
		return true;
	}

	// DEBUG_TRACE(("InstallHook: pFunc = %08x, pFuncNew = %08x" LOG_CR, (UINT_PTR)pFunc, (UINT_PTR)pFuncNew ));

	// unconditional JMP to relative address is 5 bytes.
	m_Jump[0] = 0xe9;
	DWORD dwAddr = (DWORD)((UINT_PTR)pFuncNew - (UINT_PTR)pFunc) - sizeof(m_Jump);
	DEBUG_TRACE(("JMP %08x" LOG_CR, dwAddr ));
	memcpy(m_Jump+1, &dwAddr, sizeof(dwAddr));

	memcpy(m_OldCode, pFunc, sizeof(m_OldCode));

	DWORD dwNewProtection = PAGE_EXECUTE_READWRITE;
	if ( ! ::VirtualProtect( pFunc, 8, dwNewProtection, &m_dwOldProtection))
	{
		ASSERT(0);
		return false;
	}

	memcpy(pFunc, m_Jump, sizeof(m_Jump));
	DEBUG_MSG(("InstallHook: JMP-hook planted." LOG_CR));
	return true;
}

void CHookJump::RemoveHook( LPVOID pFunc )
{
	if (pFunc == NULL)
		return;
	if ( m_OldCode[0] == 0)
		return;
	try 
	{
		memcpy(pFunc, m_OldCode, sizeof(m_OldCode));	// SwapOld(pFunc)
		DWORD dwOldProtection = 0;
		::VirtualProtect(pFunc, 8, m_dwOldProtection, &dwOldProtection );
	}
	catch (...)
	{
		DEBUG_ERR(("CHookJump::RemoveHook FAIL" LOG_CR));
	}
}

//***********************************************

int Mem_ConvertToString( char* pszDst, int iSizeDstMax, const BYTE* pSrc, int iLenSrcBytes )
{
	// Write bytes out in string format.
	// RETURN: the actual size of the string.
	iSizeDstMax -= 4;
	int iLenOut = 0;
	for ( int i=0; i<iLenSrcBytes; i++ )
	{
		if ( i )
		{
			pszDst[iLenOut++] = ',';
		}

		int iLenThis = _snprintf( pszDst+iLenOut, iSizeDstMax-iLenOut, "%d", pSrc[i] );
		if ( iLenThis <= 0 )
			break;
		iLenOut += iLenThis;
		if ( iLenOut >= iSizeDstMax )
			break;
	}
	return( iLenOut );
}

int Mem_ReadFromString( BYTE* pDst, int iLengthMax, const char* pszSrc )
{
	// Read bytes in from string format.
	// RETURN: the number of bytes read.
#define STR_GETNONWHITESPACE( pStr ) while ( isspace((pStr)[0] )) { (pStr)++; } // isspace()

	int i=0;
	for ( ; i<iLengthMax; i++ )
	{
		STR_GETNONWHITESPACE(pszSrc);
		if (pszSrc[0]=='\0')
			break;
		const char* pszSrcStart = pszSrc;
		char* pszEnd;
		pDst[i] = (BYTE) strtol(pszSrc,&pszEnd,0);
		if ( pszSrcStart == pszEnd )	// must be the field terminator? ")},;"
			break;
		pszSrc = pszEnd;
		STR_GETNONWHITESPACE(pszSrc);
		if ( pszSrc[0] != ',' )
			break;
		pszSrc++;
	}
	return i;
}

HWND FindWindowForProcessID( DWORD dwProcessID )
{
	// look through all the top level windows for the window that has this processid.
	// NOTE: there may be more than 1. just take the first one.

	if ( dwProcessID == 0 )
		return NULL;

	// Get the first window handle.
	HWND hWnd = ::FindWindow(NULL,NULL);
	ASSERT(hWnd);	// must be a desktop!

	HWND hWndBest=NULL;
	int iBestScore = 0;

	// Loop until we find the target or we run out
	// of windows.
    for ( ;hWnd!=NULL; hWnd = ::GetWindow(hWnd, GW_HWNDNEXT))
	{
		// See if this window has a parent. If not,
        // it is a top-level window.
		if ( ::GetParent(hWnd) != NULL )
			continue;
		// This is a top-level window. See if
        // it has the target instance handle.
		DWORD dwProcessIDTest = 0;
		DWORD dwThreadID = ::GetWindowThreadProcessId( hWnd, &dwProcessIDTest );
        if ( dwProcessIDTest != dwProcessID )
			continue;

		// WS_VISIBLE?
		DWORD dwStyle = GetWindowStyle(hWnd);
		if ( ! ( dwStyle & WS_VISIBLE ))
			continue;
		RECT rect;
		if ( ! ::GetWindowRect(hWnd,&rect))
			continue;
		int iScore = ( rect.right - rect.left ) * ( rect.bottom - rect.top );
		if ( iScore > iBestScore)
		{
			iBestScore = iScore;
			hWndBest = hWnd;
		}
	}
	return hWndBest;
}

HINSTANCE CHttpLink_GotoURL( const TCHAR* pszURL, int iShowCmd )
{
	// iShowCmd = SW_SHOWNORMAL = 1
	// pszURL = _T ("http://www.yoururl.com"), 
	// or _T ("mailto:you@hotmail.com"), 

    // First try ShellExecute()
	HINSTANCE hInstRet = ::ShellExecute(NULL, _T("open"), pszURL, NULL,NULL, iShowCmd);
    return hInstRet;
}

bool CWndDC::ReleaseDC()
{
	if ( m_hDC == NULL )
		return true;
	bool bRet;
	if ( m_hWndOwner != HWND_BROADCAST )
	{
		bRet = ::ReleaseDC(m_hWndOwner,m_hDC) ? true : false;
		m_hWndOwner = HWND_BROADCAST;
	}
	else
	{
		// We created this therefore we must delete it.
		bRet = ::DeleteDC(m_hDC) ? true : false;
	}
	m_hDC = NULL;
	return bRet;
}
