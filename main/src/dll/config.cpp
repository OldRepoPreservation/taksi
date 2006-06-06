//
// config.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include <ctype.h>	// isspace()
#include "../common/CWaveDevice.h"

#ifndef __PLACEMENT_NEW_INLINE
#define __PLACEMENT_NEW_INLINE
inline void*__cdecl operator new(size_t, void*_P)
	{return (_P); }
#if     _MSC_VER >= 1200	// VS6
inline void __cdecl operator delete(void*, void*)
	{return; }
#endif
#endif

const char* CTaksiConfig::sm_Props[TAKSI_CFGPROP_QTY+1] = 
{
#define ConfigProp(a,b,c) #a,
#include "../ConfigProps.tbl"
#undef ConfigProp
	NULL,
};

static bool Str_GetQuoted( char* pszDest, const char* pszSrc, int iLenMax )
{
	char* pszStartQuote = strstr(pszSrc, "\"");
	if (pszStartQuote == NULL) 
		return false;
	char* pszEndQuote = strstr(pszStartQuote + 1, "\"");
	if (pszEndQuote == NULL)
		return false;
	int iLen = (int)( pszEndQuote - pszStartQuote ) - 1;
	if ( iLen > iLenMax-1 )
		iLen = iLenMax-1;
	strncpy( pszDest, pszStartQuote + 1, iLen );
	pszDest[iLen] = '\0';
	return true;
}

//*****************************************************

const char* CTaksiConfigCustom::sm_Props[TAKSI_CUSTOM_QTY+1] = // TAKSI_CUSTOM_TYPE
{
	"FrameRate",
	"FrameWeight",
	"Pattern",		// TAKSI_CUSTOM_Pattern
	NULL,
};

int CTaksiConfigCustom::PropGet( int eProp, char* pszValue, int iSizeMax ) const
{
	// TAKSI_CUSTOM_TYPE 
	switch ( eProp )
	{
	case TAKSI_CUSTOM_FrameRate:
		return _snprintf(pszValue, iSizeMax, "%g", m_fFrameRate);
	case TAKSI_CUSTOM_FrameWeight:
		return _snprintf(pszValue, iSizeMax, "%g", m_fFrameWeight);
	case TAKSI_CUSTOM_Pattern:
		return _snprintf(pszValue, iSizeMax, "\"%s\"", m_szPattern);
	}
	return -1;
}
bool CTaksiConfigCustom::PropSet( int eProp, const char* pszValue )
{
	// TAKSI_CUSTOM_TYPE 
	switch ( eProp )
	{
	case TAKSI_CUSTOM_FrameRate:
		m_fFrameRate = (float) atof(pszValue);
		break;
	case TAKSI_CUSTOM_FrameWeight:
		m_fFrameWeight = (float) atof(pszValue);
		break;
	case TAKSI_CUSTOM_Pattern:
		if (! Str_GetQuoted( m_szPattern, pszValue, sizeof(m_szPattern)))
			return false;
		break;
	default:
		return false;
	}
#ifdef _DEBUG
	char szTmp[_MAX_PATH*2];
	if ( PropGet(eProp,szTmp,sizeof(szTmp)) < 0 )
	{
		strcpy( szTmp, "<NA>" );
	}
	LOG_MSG(( "PropSet[%s] '%s'='%s'" LOG_CR, m_szAppId, sm_Props[eProp], szTmp ));
#endif
	return true;
}

//*****************************************************************************

void CTaksiConfigData::InitConfig()
{
	m_szCaptureDir[0] = '\0';
	m_bDebugLog = false;

	// Video Format
	m_fFrameRateTarget=20;
	m_VideoCodec.InitCodec();
	m_bVideoHalfSize = true;

	m_AudioCodec.InitFormatEmpty();
	m_iAudioDevice = WAVE_DEVICE_NONE;

	m_wHotKey[TAKSI_HOTKEY_ConfigOpen]=0x0;
	m_wHotKey[TAKSI_HOTKEY_HookModeToggle]=0x75;
	m_wHotKey[TAKSI_HOTKEY_IndicatorToggle]=0x74;
	m_wHotKey[TAKSI_HOTKEY_RecordStart]=0x7b;
	m_wHotKey[TAKSI_HOTKEY_RecordStop]=0x7b;
	m_wHotKey[TAKSI_HOTKEY_Screenshot]=0x77;
	m_wHotKey[TAKSI_HOTKEY_SmallScreenshot]=0x76;

	m_bShowIndicator = true;
	m_bUseDirectInput = true;

	m_bGDIUse = true;
	m_bGDIFrame = true;

	DEBUG_MSG(("CTaksiConfig::InitConfig" LOG_CR ));
}

void CTaksiConfigData::CopyConfig( const CTaksiConfigData& config )
{
	strncpy( m_szCaptureDir, config.m_szCaptureDir, sizeof(m_szCaptureDir)-1);

	m_fFrameRateTarget = config.m_fFrameRateTarget;
	m_bVideoHalfSize = config.m_bVideoHalfSize;
	m_VideoCodec.CopyCodec( config.m_VideoCodec );

	memcpy( m_wHotKey, config.m_wHotKey, sizeof(m_wHotKey));

	m_bUseDirectInput = config.m_bUseDirectInput;
	m_bDebugLog = config.m_bDebugLog;

	// CANT COPY m_pCustomList for interproces access ??
	DEBUG_MSG(("CTaksiConfig::CopyConfig '%s'" LOG_CR, m_szCaptureDir ));
}

bool CTaksiConfigData::SetHotKey( TAKSI_HOTKEY_TYPE eHotKey, WORD wHotKey )
{
	// Signal to unhook from IDirect3DDeviceN methods 
	// wHotKey = LOBYTE(virtual key code) + HIBYTE(HOTKEYF_ALT)
	if ( eHotKey < 0 || eHotKey >= TAKSI_HOTKEY_QTY )
		return false;
	if ( m_wHotKey[eHotKey] == wHotKey )
		return false;
	m_wHotKey[eHotKey] = wHotKey;
	return true;
}

WORD CTaksiConfigData::GetHotKey( TAKSI_HOTKEY_TYPE eHotKey ) const
{
	// wHotKey = LOBYTE(virtual key code) + HIBYTE(HOTKEYF_ALT)
	if ( eHotKey < 0 || eHotKey >= TAKSI_HOTKEY_QTY )
		return 0;
	return m_wHotKey[eHotKey];
}

void CTaksiConfigData::put_CaptureDir(const TCHAR* pszPath)
{
	ASSERT(pszPath);
	DEBUG_MSG(("CTaksiConfig::put_CaptureDir '%s' to '%s'" LOG_CR, m_szCaptureDir, pszPath ));
	strncpy(m_szCaptureDir, pszPath, sizeof(m_szCaptureDir)-1);
}

void CTaksiConfigData::GetCaptureDir(TCHAR* pszPath, int iSizeMax) const
{
	if (pszPath == NULL) 
		return;
	int iLen = lstrlen(m_szCaptureDir);
	strncpy( pszPath, m_szCaptureDir, iSizeMax-1 );
}

bool CTaksiConfigData::FixCaptureDir()
{
	int iLen = lstrlen(m_szCaptureDir);
	if (iLen==0)
	{
		// If capture directory is still unknown at this point
		// set it to the current APP directory then.
		::GetModuleFileName(NULL,m_szCaptureDir, sizeof(m_szCaptureDir)-1 );
		// strip off file name
		TCHAR* pTitle = GetFileTitlePtr(m_szCaptureDir);
		if(pTitle)
			*pTitle = '\0';
		return true;
	}

	// make sure it ends with backslash.
	if ( iLen <= 0 || m_szCaptureDir[iLen-1] != '\\') 
	{
		lstrcat(m_szCaptureDir, "\\");
		return true;
	}
	return false;
}

//************************************************************

CTaksiConfigCustom* CTaksiConfig::CustomConfig_Alloc() // static
{
	// NOTE: This should be in process shared memory ?
	CTaksiConfigCustom* pConfig = (CTaksiConfigCustom*) ::HeapAlloc( 
		::GetProcessHeap(), 
		HEAP_ZERO_MEMORY, sizeof(CTaksiConfigCustom));
	return new((void*) pConfig ) CTaksiConfigCustom();	// init the struct properly
}

void CTaksiConfig::CustomConfig_Delete( CTaksiConfigCustom* pConfig ) // static
{
	if ( pConfig == NULL )
		return;
	::HeapFree( ::GetProcessHeap(), 0, pConfig);
}

CTaksiConfigCustom* CTaksiConfig::CustomConfig_FindPattern( const TCHAR* pszProcessFile ) const
{
	for (CTaksiConfigCustom* p=m_pCustomList; p!=NULL; p=p->m_pNext )
	{
		// try to match the pattern with processfile
		if ( strstr(pszProcessFile, p->m_szPattern)) 
			return p;
	}
	return NULL;
}

CTaksiConfigCustom* CTaksiConfig::CustomConfig_FindAppId( const char* pszAppId ) const
{
	// Looks up a custom config in a list. If not found return NULL
	for (CTaksiConfigCustom* p=m_pCustomList; p!=NULL; p=p->m_pNext )
	{
		if ( ! strcmp(p->m_szAppId, pszAppId))
			return p;
	}
	return NULL;
}

CTaksiConfigCustom* CTaksiConfig::CustomConfig_Lookup( const TCHAR* pszAppId)
{
	//
	// Looks up a custom config in a list. If not found, creates one
	// and inserts at the beginning of the list.
	//
	CTaksiConfigCustom* p = CustomConfig_FindAppId(pszAppId);
	if (p)
		return p;

	// not found. Therefore, insert a new one
	p = CustomConfig_Alloc();
	if (p == NULL)
		return NULL;

	strncpy(p->m_szAppId, pszAppId, sizeof(p->m_szAppId)-1);
	p->m_pNext = m_pCustomList;
	m_pCustomList = p;
	return p;
}

void CTaksiConfig::CustomConfig_DeleteAppId(const TCHAR* pszAppId)
{
	// Deletes a custom config from the list and frees its memory. 
	// If not found, nothing is done.
	//
	CTaksiConfigCustom* pPrev = NULL;
	CTaksiConfigCustom* pCur = m_pCustomList;
	while (pCur != NULL)
	{
		CTaksiConfigCustom* pNext = pCur->m_pNext;
		if ( pszAppId == NULL || ! lstrcmp(pCur->m_szAppId, pszAppId)) 
		{
			if ( pPrev == NULL )
				m_pCustomList = pNext;
			else
				pPrev->m_pNext = pNext;
			CustomConfig_Delete(pCur);
		}
		else
		{
			pPrev = pCur;
		}
		pCur = pNext;
	}
}

//************************************************************

CTaksiConfig::CTaksiConfig()
	: m_pCustomList(NULL)
{
	PropsInit();
	InitConfig();
}

CTaksiConfig::~CTaksiConfig()
{
	CustomConfig_DeleteAppId(NULL);		// Free ALL memory taken by custom configs
	m_VideoCodec.DestroyCodec();	// cleanup VFW resources. 
}

int CTaksiConfig::PropGet( int eProp, char* pszValue, int iSizeMax ) const
{
	// TAKSI_CFGPROP_TYPE
	switch (eProp)
	{
	case TAKSI_CFGPROP_DebugLog:
		return _snprintf(pszValue, iSizeMax, "%d", m_bDebugLog);
	case TAKSI_CFGPROP_CaptureDir:
		return _snprintf(pszValue, iSizeMax, "\"%s\"", m_szCaptureDir);
	case TAKSI_CFGPROP_MovieFrameRateTarget:
		return _snprintf(pszValue, iSizeMax, "%g", m_fFrameRateTarget);
	case TAKSI_CFGPROP_VKey_ConfigOpen:
	case TAKSI_CFGPROP_VKey_HookModeToggle:
	case TAKSI_CFGPROP_VKey_IndicatorToggle:
	case TAKSI_CFGPROP_VKey_RecordStart:
	case TAKSI_CFGPROP_VKey_RecordStop:
	case TAKSI_CFGPROP_VKey_Screenshot:
	case TAKSI_CFGPROP_VKey_SmallScreenshot:
		{
		int iKey = TAKSI_HOTKEY_ConfigOpen + ( eProp - TAKSI_CFGPROP_VKey_ConfigOpen );
		return _snprintf(pszValue, iSizeMax, "%d", m_wHotKey[iKey] );
		}
	case TAKSI_CFGPROP_KeyboardUseDirectInput:
		return _snprintf(pszValue, iSizeMax, m_bUseDirectInput ? "1" : "0");
	case TAKSI_CFGPROP_VideoHalfSize:
		return _snprintf(pszValue, iSizeMax, m_bVideoHalfSize ? "1" : "0" );
	case TAKSI_CFGPROP_VideoCodecInfo:
		return m_VideoCodec.GetStr(pszValue, iSizeMax);
	case TAKSI_CFGPROP_AudioCodecInfo:
		return Mem_ConvertToString( pszValue, iSizeMax, (BYTE*) m_AudioCodec.get_WF(), m_AudioCodec.get_FormatSize());
	case TAKSI_CFGPROP_AudioDevice:
		return _snprintf(pszValue, iSizeMax, "%d", m_iAudioDevice );
	case TAKSI_CFGPROP_ShowIndicator:
		return _snprintf(pszValue, iSizeMax, m_bShowIndicator? "1" : "0" );
	}
	return -1;
}

bool CTaksiConfig::PropSet( int eProp, const char* pszValue )
{
	// TAKSI_CFGPROP_TYPE
	switch (eProp)
	{
	case TAKSI_CFGPROP_DebugLog:
		m_bDebugLog = atoi(pszValue);
		break;
	case TAKSI_CFGPROP_CaptureDir:
		if (! Str_GetQuoted( m_szCaptureDir, pszValue, sizeof(m_szCaptureDir)))
			return false;
		break;
	case TAKSI_CFGPROP_MovieFrameRateTarget:
		m_fFrameRateTarget = (float) atof(pszValue);
		break;
		
	case TAKSI_CFGPROP_VKey_ConfigOpen:
	case TAKSI_CFGPROP_VKey_HookModeToggle:
	case TAKSI_CFGPROP_VKey_IndicatorToggle:
	case TAKSI_CFGPROP_VKey_RecordStart:
	case TAKSI_CFGPROP_VKey_RecordStop:
	case TAKSI_CFGPROP_VKey_Screenshot:
	case TAKSI_CFGPROP_VKey_SmallScreenshot:
		{
		int iKey = TAKSI_HOTKEY_ConfigOpen + ( eProp - TAKSI_CFGPROP_VKey_ConfigOpen );
		ASSERT( iKey >= 0 && iKey < TAKSI_HOTKEY_QTY );
		char* pszEnd;
		m_wHotKey[iKey] = (WORD) strtol(pszValue,&pszEnd,0);
		}
		break;
	case TAKSI_CFGPROP_KeyboardUseDirectInput:
		m_bUseDirectInput = atoi(pszValue) ? true : false;
		break;
	case TAKSI_CFGPROP_VideoHalfSize:
		m_bVideoHalfSize = atoi(pszValue) ? true : false;
		break;
	case TAKSI_CFGPROP_VideoCodecInfo:
		if ( ! m_VideoCodec.put_Str(pszValue))
			return false;
		break;
	case TAKSI_CFGPROP_AudioCodecInfo:
		{
		BYTE bTmp[1024];
		ZeroMemory( &bTmp, sizeof(bTmp));
		int iSize = Mem_ReadFromString( bTmp, sizeof(bTmp), pszValue );
		if ( iSize <= sizeof(PCMWAVEFORMAT))
			return false;
		m_AudioCodec.ReAllocFormatSize(iSize);
		memcpy( m_AudioCodec.get_WF(), bTmp, iSize );
		}
		break;
	case TAKSI_CFGPROP_AudioDevice:
		m_iAudioDevice = atoi(pszValue);
		break;
	case TAKSI_CFGPROP_ShowIndicator:
		m_bShowIndicator = atoi(pszValue) ? true : false;
		break;
	default:
		return false;
	}
#ifdef _DEBUG
	char szTmp[_MAX_PATH*2];
	if ( PropGet(eProp,szTmp,sizeof(szTmp)) < 0 )
	{
		strcpy( szTmp, "<NA>" );
	}
	LOG_MSG(( "PropSet '%s'='%s'" LOG_CR, sm_Props[eProp], szTmp));
#endif
	return true;
}

//***************************************************************************

bool CTaksiConfig::ReadIniFileFromDir(const TCHAR* pszDir)
{
	// Read an INI file in standard INI file format.
	// Returns true if successful.

	char szConfigFileName[_MAX_PATH];
	if (pszDir != NULL) 
		lstrcpy(szConfigFileName, pszDir);
	else
		szConfigFileName[0] = '\0';
	lstrcat(szConfigFileName, TAKSI_INI_FILE);

	FILE* pFile = fopen(szConfigFileName, "rt");
	if (pFile == NULL) 
		return false;

	CIniObject* pObj = NULL;
	while (true)
	{
		char szBuffer[_MAX_PATH*2];
		fgets(szBuffer, sizeof(szBuffer)-1, pFile);
		if (feof(pFile))
			break;

		// skip/trim comments
		char* pszComment = strstr(szBuffer, ";");
		if (pszComment != NULL) 
			pszComment[0] = '\0';

		// parse the line
		char* pszName = Str_SkipSpace(szBuffer);	// skip leading spaces.
		if ( *pszName == '\0' )
			continue;

		if ( *pszName == '[' )
		{
			if ( ! strnicmp( pszName, "[" TAKSI_SECTION "]", 7 ))
			{
				pObj = this;
			}
			else if ( ! strnicmp( pszName, "[" TAKSI_CUSTOM_SECTION " ", sizeof(TAKSI_CUSTOM_SECTION)+1 ))
			{
				char szSection[ _MAX_PATH ];
				strncpy( szSection, pszName+sizeof(TAKSI_CUSTOM_SECTION)+1, sizeof(szSection));
				char* pszEnd = strstr(szSection, "]");
				if (pszEnd)
					*pszEnd = '\0';
				pObj = CustomConfig_Lookup(szSection);
			}
			else
			{
				pObj = NULL;
			}
			continue;
		}

		if ( pObj == NULL )	// skip
			continue;

		char* pszEq = strstr(pszName, "=");
		if (pszEq == NULL) 
			continue;
		pszEq[0] = '\0';

		char* pszValue = Str_SkipSpace( pszEq + 1 );	// skip leading spaces.

		pObj->PropSetName( pszName, pszValue );
	}

	fclose(pFile);
	FixCaptureDir();
	return true;
}

bool CTaksiConfig::WriteIniFile()
{
	// RETURN: true = success
	//  false = cant save!
	//
	char* pFileOld = NULL;
	DWORD nSizeOld = 0;

	// first read all lines
	CNTHandle FileOld( ::CreateFile( TAKSI_INI_FILE, 
		GENERIC_READ, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ));
	if ( FileOld.IsValidHandle())
	{
		nSizeOld = ::GetFileSize(FileOld, NULL);
		pFileOld = (char*) ::HeapAlloc( g_Proc.m_hHeap, HEAP_ZERO_MEMORY, nSizeOld);
		if (pFileOld == NULL) 
			return false;

		DWORD dwBytesRead = 0;
		::ReadFile(FileOld, pFileOld, nSizeOld, &dwBytesRead, NULL);
		if (dwBytesRead != nSizeOld) 
		{
			::HeapFree( g_Proc.m_hHeap, 0, pFileOld );
			return false;
		}
		FileOld.CloseHandle();
	}

	// create new file
	FILE* pFile = fopen(TAKSI_INI_FILE, "wt");
	if ( pFile == NULL )
		return false;

	// loop over every line from the old file, and overwrite it in the new file
	// if necessary. Otherwise - copy the old line.
	
	CIniObject* pObj = NULL;
	char* pszLine = pFileOld; 

	if (pFileOld)
	while (true)
	{
		if ( pszLine >= pFileOld + nSizeOld )
			break;

		if ( *pszLine == '[' )
		{
			if ( pObj ) // finish previous section.
			{
				pObj->PropsWrite(pFile);
			}
			if ( ! strnicmp( pszLine, "[" TAKSI_SECTION "]", sizeof(TAKSI_SECTION)+1 ))
			{
				pObj = this;
			}
			else if ( ! strnicmp( pszLine, "[" TAKSI_CUSTOM_SECTION " ", sizeof(TAKSI_CUSTOM_SECTION)+1 ))
			{
				char szSection[ _MAX_PATH ];
				strncpy( szSection, pszLine+14, sizeof(szSection));
				char* pszEnd = strstr(szSection, "]");
				if (pszEnd)
					*pszEnd = '\0';
				pObj = CustomConfig_FindAppId(szSection);
			}
			else
			{
				pObj = NULL;
			}
		}

		char* pszEndLine = strchr(pszLine, '\n' ); // INI_CR
		if (pszEndLine) 
		{
			// covers \n or \r\n
			char* pszTmp = pszEndLine;
			for ( ; pszTmp >= pszLine && Str_IsSpace(*pszTmp); pszTmp-- )
				pszTmp[0] = '\0'; 
			pszEndLine++;
		}

		// it's a custom setting.
		bool bReplaced;
		if (pObj)
		{
			bReplaced = pObj->PropWriteName( pFile, pszLine );
		}
		else
		{
			bReplaced = false;
		}
		if (!bReplaced)
		{
			// take the old line as it was, might be blank or comment.
			fprintf(pFile, "%s" INI_CR, pszLine);
		}
		if (pszEndLine == NULL)
			break;
		pszLine = pszEndLine;
	}

	// release buffer
	if(pFileOld)
	{
		::HeapFree( g_Proc.m_hHeap, 0, pFileOld );
	}

	if ( pObj ) // finish previous section.
	{
		pObj->PropsWrite(pFile);
	}

	// if wasn't written, make sure we write it.
	if  (!m_dwWrittenMask)
	{
		fprintf( pFile, "[" TAKSI_SECTION "]" INI_CR );
		PropsWrite(pFile);
	}
	PropsInit();

	// same goes for NEW custom configs
	CTaksiConfigCustom* p = m_pCustomList;
	while (p != NULL)
	{
		if ( ! p->m_dwWrittenMask )
		{
			fprintf( pFile, "[" TAKSI_CUSTOM_SECTION " %s]" INI_CR, p->m_szAppId );
			p->PropsWrite(pFile);
		}
		p->PropsInit();
		p = p->m_pNext;
	}

	fclose(pFile);
	return true;
}
