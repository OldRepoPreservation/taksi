//
// CImageGDIP.h
//
#ifndef _INC_CImageGDIP_H
#define _INC_CImageGDIP_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CDll.h"
#include <gdiplus.h>

class LIBSPEC CImageGDIPInt : public CDllFile
{
	// Run time binding to the MsAcm32.DLL.
public:
	CImageGDIPInt();
	~CImageGDIPInt();

	bool AttachGDIPInt();
	void DetachGDIPInt();

	HRESULT GetEncoderClsid( const char* pszFormatExt, CLSID* pClsid );

public:
#define CIMAGEGDIPFUNC(a,b,c,d)	typedef b (WINGDIPAPI* PFN##a) c;
#include "CImageGDIPFunc.tbl"
#undef CIMAGEGDIPFUNC
	// DECLSPEC_IMPORT
#define CIMAGEGDIPFUNC(a,b,c,d)	PFN##a m_##a;	
#include "CImageGDIPFunc.tbl"
#undef CIMAGEGDIPFUNC

public:
	ULONG_PTR m_Token;
};

extern CImageGDIPInt g_gdiplus;

#endif	// _INC_CImageGDIP_H
