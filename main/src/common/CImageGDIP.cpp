//
// CImageGDIP.cpp
// Copyright 1992 - 2006 Dennis Robinson (www.menasoft.com)
//
#include "..\stdafx.h"
#include <gdiplus.h>
#include "CImageGDIP.h"

CImageGDIPInt g_gdiplus;

#ifdef _DEBUG
static void CImageGDIP_DebugPoint( const char* pszName )
{
	ASSERT(pszName);
	DEBUG_MSG(( "CImageGDIP call '%s'" LOG_CR, pszName ));
	ASSERT(g_gdiplus.m_Token);
}
#else
#define CImageGDIP_DebugPoint(pszName)
#endif

// Fix the weird obscured links.
namespace Gdiplus
{
extern "C"
{
#define CIMAGEGDIPFUNC(a,b,c,d)	b WINGDIPAPI a c { CImageGDIP_DebugPoint(#a); return g_gdiplus.m_##a d ; } 
#include "CImageGDIPFunc.tbl"
#undef CIMAGEGDIPFUNC
}
}

CImageGDIPInt::CImageGDIPInt() 
	: m_Token(0)
{
}

CImageGDIPInt::~CImageGDIPInt()
{
}

bool CImageGDIPInt::AttachGDIPInt()
{
	//  if we have already linked to the API's, then just succeed...
	if ( IsValidDll() || m_Token )
		return true;

	HRESULT hRes = LoadDll( _T("gdiplus.dll"));
	if ( IS_ERROR(hRes))
		return false;

#define CIMAGEGDIPFUNC(a,b,c,d)	\
	m_##a = (PFN##a) GetProcAddress(#a);\
	if ( m_##a == NULL )\
	{\
		DEBUG_ERR(( "Cant find GDIP '%s'" LOG_CR, #a ));\
		return false;\
	}
#include "CImageGDIPFunc.tbl"
#undef CIMAGEGDIPFUNC

	m_Token = 1;	// temporary just to get past the debug code.
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::Status status = Gdiplus::GdiplusStartup( &m_Token, &gdiplusStartupInput, NULL);
	if ( status != Gdiplus::Ok )
	{
		m_Token = 0;
		return false;
	}

	ASSERT(m_Token);
	return true;
}

void CImageGDIPInt::DetachGDIPInt()
{
	if ( ! IsValidDll())
		return;
	if ( m_Token )
	{
		Gdiplus::GdiplusShutdown(m_Token);
		m_Token = 0;
	}
	FreeDll();
}

HRESULT CImageGDIPInt::GetEncoderClsid( const char* pszFormatExt, CLSID* pClsid )
{
	// pszFormat = L"image/png"
	// "image/jpeg", 

	if ( !m_Token )
	{
		return E_FAIL;
	}

	ASSERT(pszFormatExt);
	ASSERT(pszFormatExt[0]);

	WCHAR wFormat[_MAX_PATH];
	wcscpy( wFormat, L"image/" );
	::MultiByteToWideChar( CP_UTF8, 0,
		pszFormatExt, strlen(pszFormatExt)+1, 
		wFormat+6, COUNTOF(wFormat)-6 );

	UINT num = 0;          // number of image encoders
	UINT size = 0;         // size of the image encoder array in bytes
	Gdiplus::GdipGetImageEncodersSize(&num, &size);
	if(size == 0)
	{
		return HRESULT_FROM_WIN32(ERROR_SXS_UNKNOWN_ENCODING_GROUP);  // Failure
	}

	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo == NULL)
	{
		return E_OUTOFMEMORY;  // Failure
	}
	GdipGetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if ( !wcscmp(pImageCodecInfo[j].MimeType, wFormat))
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}    
	}

	free(pImageCodecInfo);
	return HRESULT_FROM_WIN32(ERROR_SXS_UNKNOWN_ENCODING_GROUP);  // Failure
}
