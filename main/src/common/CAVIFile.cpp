//
// CAVIFile.cpp
//
#include "../stdafx.h"
#include "CAVIFile.h"

// Header structure for an AVI file:
// with no audio stream, and one video stream, which uses bitmaps without palette.
// This is sort of cheating to mash it all together like this. might want to use mmio*() ??
// NOTE: Use RIFFpad at menasoft.com to check this format out.

#pragma pack(1)

struct AVI_FILE_HEADER
{
	FOURCC fccRiff;                 // "RIFF"
	DWORD  dwSizeRiff;          

	FOURCC fccForm;                 // "AVI "
	FOURCC fccList_0;				// "LIST"
	DWORD  dwSizeList_0;

	FOURCC fccHdrl;                 // "hdrl"
	FOURCC fccAvih;                 // "avih"
	DWORD  dwSizeAvih;			// sizeof(m_AviHeader)

	MainAVIHeader m_AviHeader;

	// Video Format.
	FOURCC fccList_V;                  // "LIST"
	DWORD  dwSizeList_V;

	FOURCC fccStrl;                 // "strl"
	FOURCC fccStrh;                 // "strh"
	DWORD  dwSizeStrh;
	AVIStreamHeader m_StreamHeader;	// sizeof() = 64 ?

	FOURCC fccStrf;                 // "strf"
	DWORD  dwSizeStrf;
	BITMAPINFOHEADER m_biHeader;

	FOURCC fccStrn;                 // "strn"
	DWORD  dwSizeStrn;
	char m_Strn[13];				// "Video Stream"
	char m_StrnPadEven;

	// Audio Format.
#if 0
	FOURCC fccList_A;				// "LIST"
	DWORD  dwSizeList_A;


#endif

#define AVI_MOVILIST_OFFSET 0x800

};

#pragma pack()

// end of AVI structures

void CVideoFrameForm::InitPadded( int cx, int cy, int iBPP, int iPad )
{
	// iPad = sizeof(DWORD)
	m_Size.cx = cx;
	m_Size.cy = cy;
	m_iBPP = iBPP;
	int iPitchUn = cx * iBPP;
	int iRem = iPitchUn % iPad;
	m_iPitch = (iRem == 0) ? iPitchUn : (iPitchUn + iPad - iRem);
}

//**************************************************************** 

void CVideoFrame::FreeFrame()
{
	if (m_pPixels == NULL) 
		return;
	::HeapFree( ::GetProcessHeap(), 0, m_pPixels);
	m_pPixels = NULL;
}

bool CVideoFrame::AllocForm( const CVideoFrameForm& FrameForm )
{
	if (m_pPixels)
	{
		if ( ! memcmp( &FrameForm, this, sizeof(FrameForm)))
			return true;
		FreeFrame();
	}
	((CVideoFrameForm&)*this) = FrameForm;
	m_pPixels = (BYTE*) ::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, get_SizeBytes());
	if (m_pPixels == NULL )
		return false;
	return true;
}

bool CVideoFrame::AllocPadXY( int cx, int cy, int iBPP, int iPad )
{
	// iPad = sizeof(DWORD)
	CVideoFrameForm FrameForm;
	FrameForm.InitPadded(cx,cy,iBPP,iPad);
	return AllocForm(FrameForm);
}

HRESULT CVideoFrame::SaveAsBMP( const TCHAR* pszFileName ) const
{
	// Writes a BMP file
	ASSERT( pszFileName );

	// save to file
	CNTHandle File( ::CreateFile( pszFileName,            // file to create 
		GENERIC_WRITE,                // open for writing 
		0,                            // do not share 
		NULL,                         // default security 
		OPEN_ALWAYS,                  // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,        // normal file 
		NULL ));                        // no attr. template 
	if ( ! File.IsValidHandle()) 
	{
		DWORD dwLastError = GetLastError();
		LOG_MSG(("CVideoFrame::SaveAsBMP: failed save to file. %d" LOG_CR, dwLastError ));
		return HRESULT_FROM_WIN32(dwLastError);	// 
	}

	// fill in the headers
	BITMAPFILEHEADER bmfh;
	bmfh.bfType = 0x4D42; // 'BM'
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + get_SizeBytes();
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	DWORD dwBytesWritten;
	::WriteFile(File, &bmfh, sizeof(bmfh), &dwBytesWritten, NULL);

	BITMAPINFOHEADER bmih;
	bmih.biSize = sizeof(BITMAPINFOHEADER);
	bmih.biWidth = m_Size.cx;
	bmih.biHeight = m_Size.cy;
	bmih.biPlanes = 1;
	bmih.biBitCount = m_iBPP*8;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
	::WriteFile(File, &bmih, sizeof(bmih), &dwBytesWritten, NULL);

	::WriteFile(File, m_pPixels, get_SizeBytes(), &dwBytesWritten, NULL);

	return S_OK;
}

//******************************************************************************

void CVideoCodec::InitCodec()
{
	m_bCompressing = false;
	ZeroMemory(&m_v,sizeof(m_v));
	// Use a default codec
	m_v.cbSize = sizeof(m_v); // 0x40;
	m_v.dwFlags = ICMF_COMPVARS_VALID;
	m_v.fccType = mmioFOURCC('v','i','d','c');	
	m_v.fccHandler = mmioFOURCC('D','I','B',' '); // no compress.
	// mmioFOURCC('iyuv') = intel yuv format works.
	m_v.lKey = 0x01;
	m_v.lDataRate = 0x12c;
	//m_v.lpState = 0xsdfsdf;
	//m_v.cbState = 0x38;
}

void CVideoCodec::DestroyCodec()
{
	if ( m_v.cbSize != 0) 
	{
		// Must do this after ICCompressorChoose, ICSeqCompressFrameStart, ICSeqCompressFrame, and ICSeqCompressFrameEnd functions
		::ICCompressorFree(&m_v);
		m_v.cbSize = 0;
	}
	CloseCodec();
}

int CVideoCodec::GetStr( char* pszValue, int iSizeMax) const
{
	// serialize to a string format.
	// RETURN:
	//  length of the string.
	if ( m_v.cbSize <= 0 || iSizeMax <= 0 )
		return 0;
	CVideoCodec codec;
	codec.CopyCodec( *this );
	return Mem_ConvertToString( pszValue, iSizeMax, (BYTE*) &codec.m_v, sizeof(codec.m_v));
}

bool CVideoCodec::put_Str( const char* pszValue )
{
	// serialize from a string format.
	DestroyCodec();
	ZeroMemory( &m_v, sizeof(m_v));
	int iSize = Mem_ReadFromString((BYTE*) &m_v, sizeof(m_v), pszValue );
	if ( iSize != sizeof(m_v))
	{
	}
	return true;
}

void CVideoCodec::CopyCodec( const CVideoCodec& codec )
{
	// cant copy the m_v.hic, etc.
	// DONT allow pointers to get copied! this will leak CloseCodec()!
	memcpy( &m_v, &codec.m_v, sizeof(m_v));
	m_v.hic = NULL;
	m_v.lpbiIn = NULL;
	m_v.lpbiOut = NULL;
	m_v.lpBitsPrev = NULL;
	m_v.lpState = NULL;
}

bool CVideoCodec::CompChooseDlg( HWND hWnd, LPSTR lpszTitle )
{
	// MUST call CloseCodec() after this.
	// dwFlags |= ICMF_COMPVARS_VALID ?
	m_v.cbSize = sizeof(m_v);	// Must use ICCompressorFree later.
	if ( ! ::ICCompressorChoose( hWnd, 
		ICMF_CHOOSE_ALLCOMPRESSORS, // ICMF_CHOOSE_PREVIEW|ICMF_CHOOSE_KEYFRAME|ICMF_CHOOSE_DATARATE
		NULL, NULL,
		&m_v, (LPSTR) lpszTitle ))
	{
		return false;
	}
	// ICMF_COMPVARS_VALID
	return true; 
}

#ifdef _DEBUG
void CVideoCodec::DumpSettings()
{
	// Prints out COMPVARS 
	DEBUG_TRACE(("CVideoCodec::DumpSettings: cbSize = %d" LOG_CR, (DWORD)m_v.cbSize));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: dwFlags = %08x" LOG_CR, (DWORD)m_v.dwFlags));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: hic = %08x" LOG_CR, (UINT_PTR)m_v.hic));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: fccType = %08x" LOG_CR, (DWORD)m_v.fccType));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: fccHandler = %08x" LOG_CR, (DWORD)m_v.fccHandler));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lpbiOut = %08x" LOG_CR, (UINT_PTR)m_v.lpbiOut));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lKey = %d" LOG_CR, m_v.lKey));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lDataRate = %d" LOG_CR, m_v.lDataRate));
	DEBUG_TRACE(("CVideoCodec::DumpSettings: lQ = %d" LOG_CR, m_v.lQ));
}
#endif

bool CVideoCodec::OpenCodec( WORD wMode )
{
	// Open a compressor or decompressor codec.
	// wMode = ICMODE_FASTCOMPRESS, fccType = 
	if ( m_v.hic )
	{
		// May not be the mode i want. so close and re-open
		CloseCodec();	
	}

	m_v.hic = ::ICOpen( m_v.fccType, m_v.fccHandler, wMode );
	if ( m_v.hic == NULL)
	{
		DWORD dwLastError = ::GetLastError();
		DEBUG_WARN(("CVideoCodec::Open: ICOpen(0%x,0%x) RET NULL (%d)" LOG_CR,
			m_v.fccType, m_v.fccHandler, dwLastError ));
		return false;
	}

	DEBUG_MSG(("CVideoCodec::OpenCodec:ICOpen(0%x,0%x) OK hic=%08x" LOG_CR, 
		m_v.fccType, m_v.fccHandler, (UINT_PTR)m_v.hic ));
	return true;
}

void CVideoCodec::CloseCodec()
{
	CompEnd();
	if ( m_v.hic != NULL )
	{
		::ICClose(m_v.hic);
		m_v.hic = NULL;
		LOG_MSG(("CVideoCodec:CloseCodec ICClose" LOG_CR));
	}
}

bool CVideoCodec::CompSupportsConfigure() const
{
	return ICQueryConfigure(m_v.hic);
}
LRESULT CVideoCodec::CompConfigureDlg( HWND hWndApp )
{
	// a config dialog specific to the chosen codec.
	return ICConfigure(m_v.hic,hWndApp);
}

bool CVideoCodec::GetCodecInfo( ICINFO& icInfo ) const
{
	// Get name etc info on the m_v.hic codec.
	if ( m_v.hic == NULL )
	{
		// open a temporary version of this.
		CVideoCodec codec;
		codec.CopyCodec( *this );
		if ( ! codec.OpenCodec(ICMODE_QUERY))
			return false;
		ASSERT( codec.m_v.hic );
		bool bRet = codec.GetCodecInfo(icInfo);
		codec.DestroyCodec();
		return bRet;
	}

	ZeroMemory(&icInfo, sizeof(ICINFO));
	icInfo.dwSize = sizeof(ICINFO);
	if ( ! ::ICGetInfo(m_v.hic, &icInfo, sizeof(ICINFO)))
	{
		DEBUG_ERR(("CVideoCodec::GetCodecInfo: ICGetInfo FAILED." LOG_CR));
		return false;
	}

	DEBUG_TRACE(("CVideoCodec::GetCodecInfo: fccType: {%08x}" LOG_CR, icInfo.fccType));
	DEBUG_TRACE(("CVideoCodec::GetCodecInfo: fccHandler: {%08x}" LOG_CR, icInfo.fccHandler));
	DEBUG_TRACE(("CVideoCodec::GetCodecInfo: Name: {%S}" LOG_CR, icInfo.szName));
	DEBUG_TRACE(("CVideoCodec::GetCodecInfo: Desc: {%S}" LOG_CR, icInfo.szDescription));
	DEBUG_TRACE(("CVideoCodec::GetCodecInfo: Driver: {%S}" LOG_CR, icInfo.szDriver));
	return true;
}

LRESULT CVideoCodec::GetCompFormat( const BITMAPINFO* lpbiIn, BITMAPINFO* lpbiOut ) const
{
	// Query compressor to determine its output format structure size.
	// NOTE: lpbiOut = NULL, returns the size required for the output format.
	// RETURN: 
	//  -2 = ICERR_BADFORMAT;

	ASSERT(lpbiIn);
	LRESULT lRes = ::ICCompressGetFormat( m_v.hic, lpbiIn, lpbiOut );
	if (lRes != ICERR_OK)
	{
		if ( lpbiOut == NULL )	// just ret the size.
		{
			if ( lRes <= 0 || lRes >= 0x10000 )
			{
				DEBUG_ERR(( "CVideoCodec::ICCompressGetFormat BAD RET=%d" LOG_CR, lRes ));
			}
			return lRes;
		}
		DEBUG_ERR(("CVideoCodec:ICCompressGetFormat FAILED %d." LOG_CR, lRes ));
		return lRes;
	}

	if ( lpbiOut == NULL )	// size = 0. this is bad.
		return ICERR_BADFORMAT;

	DEBUG_TRACE(("CVideoCodec:GetCompFormat: lpbiOut.bmiHeader.biWidth = %d" LOG_CR, 
		lpbiOut->bmiHeader.biWidth));
	DEBUG_TRACE(("CVideoCodec:GetCompFormat: lpbiOut.bmiHeader.biHeight = %d" LOG_CR, 
		lpbiOut->bmiHeader.biHeight));
	DEBUG_TRACE(("CVideoCodec:GetCompFormat: lpbiOut.bmiHeader.biCompression = %08x" LOG_CR, 
		lpbiOut->bmiHeader.biCompression));

	return ICERR_OK;
}

bool CVideoCodec::CompStart( BITMAPINFO* lpbiIn )
{
	// open the compression stream.
	// MUST call CompEnd() and CloseCodec() after this.

	ASSERT(lpbiIn);
	if ( m_bCompressing )	// already started?!
	{
		return true;
	}
	DEBUG_MSG(( "CVideoCodec::CompStart" LOG_CR ));

	if ( ! ::ICSeqCompressFrameStart( &m_v, lpbiIn ))
	{
		DWORD dwLastError = ::GetLastError();
		DEBUG_ERR(("CVideoCodec::CompStart: ICSeqCompressFrameStart (%d x %d) FAILED (%d)." LOG_CR, 
			lpbiIn->bmiHeader.biWidth, lpbiIn->bmiHeader.biHeight, dwLastError ));
		return false;
	}

	m_bCompressing = true;
	DEBUG_MSG(("CVideoCodec:CompStart: ICSeqCompressFrameStart success." LOG_CR));
	return true;
}

void CVideoCodec::CompEnd()
{
	// End the compression stream.
	// ASSUME: CompStart() was called.
	if ( ! m_bCompressing )
		return;
	DEBUG_MSG(( "CVideoCodec:CompEnd" LOG_CR ));
	m_bCompressing = false;
	::ICSeqCompressFrameEnd(&m_v);
}

bool CVideoCodec::CompFrame( CVideoFrame& frame, void*& rpCompRet, LONG& nSize, BOOL& bIsKey )
{
	// Compress the frame in the stream.
	// ASSUME: CompStart() was called.
	// ASSUME frame == m_FrameForm
	// RETURN:
	//  bIsKey = this was used as the key frame?

	if ( ! m_bCompressing )
	{
		// DEBUG_TRACE(("CVideoCodec::CompFrame: NOT ACTIVE!." LOG_CR));
		rpCompRet = frame.m_pPixels;
		nSize = frame.get_SizeBytes();
		return false;
	}
	void* pCompRet = ::ICSeqCompressFrame( &m_v, 0, frame.m_pPixels, &bIsKey, &nSize );
	if (pCompRet == NULL)
	{
		// Just use the raw frame.
		DEBUG_ERR(("CVideoCodec::CompFrame: ICSeqCompressFrame FAILED."));
		rpCompRet = frame.m_pPixels;
		nSize = frame.get_SizeBytes();
		return false;
	}
	rpCompRet = pCompRet;
	return true;
}

//******************************************************************************

CAVIIndex::CAVIIndex()
	: m_dwFramesTotal(0)
	, m_dwIndexBlocks(0)
	, m_pIndexFirst(NULL)
	, m_pIndexCur(NULL)
	, m_dwIndexCurFrames(0)
{
}

CAVIIndex::~CAVIIndex()
{
}

CAVIIndexBlock* CAVIIndex::CreateIndexBlock()
{
	HANDLE hHeap = ::GetProcessHeap();
	ASSERT(hHeap);
	CAVIIndexBlock* pIndex = (CAVIIndexBlock*) HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(CAVIIndexBlock));
	ASSERT(pIndex);
	pIndex->m_pEntry = (AVIINDEXENTRY*) HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(AVIINDEXENTRY)*AVIINDEX_QTY );
	ASSERT(pIndex->m_pEntry);
	pIndex->m_pNext = NULL;
	m_dwIndexBlocks++;
	return pIndex;
}

void CAVIIndex::DeleteIndexBlock( CAVIIndexBlock* pIndex )
{
	HANDLE hHeap = ::GetProcessHeap();
	::HeapFree(hHeap, 0, pIndex->m_pEntry); // release index
	::HeapFree(hHeap, 0, pIndex); // release index
}

void CAVIIndex::AddFrame( const AVIINDEXENTRY& indexentry )
{
	// Add a single index to the array.
	// update index
	if ( m_pIndexCur == NULL )
	{
		// First frame.
		ASSERT( m_dwIndexCurFrames == 0 );
		m_pIndexFirst = CreateIndexBlock();
		m_pIndexCur = m_pIndexFirst;	// initialize index pointer
	}
	if (m_dwIndexCurFrames >= AVIINDEX_QTY)
	{
		// allocate new index, if this one is full.
		CAVIIndexBlock* pIndexNew = CreateIndexBlock();
		ASSERT(pIndexNew);
		m_pIndexCur->m_pNext = pIndexNew;
		m_pIndexCur = pIndexNew;
		m_dwIndexCurFrames = 0;
	}

	m_pIndexCur->m_pEntry[m_dwIndexCurFrames] = indexentry;
	m_dwIndexCurFrames++;
	m_dwFramesTotal++;
}

void CAVIIndex::FlushIndexChunk( HANDLE hFile )
{
	// release memory that index took
	// m_pIndexCur = Last;

	DWORD dwBytesWritten;
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		// write index
		DWORD dwTags[2];
		dwTags[0] = ckidAVINEWINDEX;	// mmioFOURCC('i', 'd', 'x', '1')
		dwTags[1] = get_ChunkSize();
		::WriteFile(hFile, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
	}

	int iCount = 0;
	CAVIIndexBlock* p = m_pIndexFirst;
	if (p)
	{
		DWORD dwTotal = m_dwFramesTotal;
		while (p != NULL)
		{
			DWORD dwFrames = ( p == m_pIndexCur ) ? 
				( m_dwFramesTotal % AVIINDEX_QTY ) : AVIINDEX_QTY;
			dwTotal -= dwFrames;
			if ( hFile != INVALID_HANDLE_VALUE )
			{
				::WriteFile( hFile, p->m_pEntry, dwFrames*sizeof(AVIINDEXENTRY),
					&dwBytesWritten, NULL);
			}
			CAVIIndexBlock* q = p->m_pNext;
			DeleteIndexBlock(p);
			p = q;
			iCount++;
		}
		DEBUG_MSG(( "CAVIIndex::FlushIndex() %d" LOG_CR, iCount ));
		ASSERT( dwTotal == 0 );
	}
	ASSERT( iCount == m_dwIndexBlocks );

	m_dwFramesTotal = 0;
	m_pIndexFirst = NULL;
	m_pIndexCur = NULL;
	m_dwIndexCurFrames = 0;
	m_dwIndexBlocks = 0;
}

//******************************************************************************

CAVIFile::CAVIFile() 
	: m_dwMoviChunkSize( sizeof(DWORD))
	, m_dwTotalFrames(0)
{
	m_VideoCodec.InitCodec();
}

CAVIFile::~CAVIFile() 
{
	CloseAVIFile();
	m_Index.FlushIndexChunk(m_File);
	// m_VideoCodec.DestroyCodec();
}

int CAVIFile::InitFileHeader( AVI_FILE_HEADER& afh )
{
	// build the AVI file header structure
	ASSERT(m_VideoCodec.m_v.lpbiOut);
	LOG_MSG(("CAVIFile:InitFileHeader framerate=%u" LOG_CR, (DWORD)m_fFrameRate ));
	ZeroMemory( &afh, sizeof(afh));

	afh.fccRiff = FOURCC_RIFF; // "RIFF"
	afh.dwSizeRiff = sizeof(afh);	// re-calc later.

	afh.fccForm = formtypeAVI; // "AVI "
	afh.fccList_0  = FOURCC_LIST; // "LIST"
	afh.dwSizeList_0 = 0;	// re-calc later.

	afh.fccHdrl = listtypeAVIHEADER; // "hdrl"
	afh.fccAvih = ckidAVIMAINHDR; // "avih"
	afh.dwSizeAvih = sizeof(afh.m_AviHeader);

	// Video Format
	afh.fccList_V  = FOURCC_LIST; // "LIST"
	afh.dwSizeList_V = 0;	// recalc later.

	afh.fccStrl = listtypeSTREAMHEADER; // "strl"
	afh.fccStrh = ckidSTREAMHEADER; // "strh"
	afh.dwSizeStrh = sizeof(afh.m_StreamHeader);

	afh.fccStrf = ckidSTREAMFORMAT; // "strf"
	afh.dwSizeStrf = sizeof(afh.m_biHeader);

	afh.fccStrn = ckidSTREAMNAME; // "strn"
	afh.dwSizeStrn = sizeof(afh.m_Strn);
	ASSERT( afh.dwSizeStrn == 13 );
	strcpy( afh.m_Strn, "Video Stream" );

	afh.dwSizeList_V = sizeof(FOURCC) 
		+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(afh.m_StreamHeader)
		+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(afh.m_biHeader)
		+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(afh.m_Strn);
	if ( afh.dwSizeList_V & 1 )	// always even size.
		afh.dwSizeList_V ++;

	afh.dwSizeList_0 = sizeof(FOURCC) 
		+ sizeof(FOURCC) + sizeof(DWORD) + sizeof(afh.m_AviHeader) 
		+ sizeof(FOURCC) + sizeof(DWORD) + afh.dwSizeList_V;
	if ( afh.dwSizeList_0 & 1 )	// always even size.
		afh.dwSizeList_0 ++;

	// update RIFF chunk size
	int iPadFile = ( m_dwMoviChunkSize ) & 1; // make even
	afh.dwSizeRiff = AVI_MOVILIST_OFFSET + 8 + m_dwMoviChunkSize +
		iPadFile + m_Index.get_ChunkSize();

	// fill-in MainAVIHeader
	afh.m_AviHeader.dwMicroSecPerFrame = (DWORD)( 1000000.0/m_fFrameRate );
	afh.m_AviHeader.dwMaxBytesPerSec = (DWORD)( m_FrameForm.get_SizeBytes() * m_fFrameRate );
	afh.m_AviHeader.dwTotalFrames = m_dwTotalFrames;
	afh.m_AviHeader.dwStreams = 1;
	afh.m_AviHeader.dwFlags = 0x10; // uses index
	afh.m_AviHeader.dwSuggestedBufferSize = m_FrameForm.get_SizeBytes();
	afh.m_AviHeader.dwWidth = m_FrameForm.m_Size.cx;
	afh.m_AviHeader.dwHeight = m_FrameForm.m_Size.cy;	// ?? was /2 ?

	// fill-in AVIStreamHeader
	afh.m_StreamHeader.fccType = streamtypeVIDEO; // 'vids' - NOT m_VideoCodec.m_v.fccType; = 'vidc'
	afh.m_StreamHeader.fccHandler = m_VideoCodec.m_v.fccHandler;

	afh.m_StreamHeader.dwScale = 1;
	afh.m_StreamHeader.dwRate = (DWORD) m_fFrameRate;
	afh.m_StreamHeader.dwLength = m_dwTotalFrames;
	afh.m_StreamHeader.dwQuality = m_VideoCodec.m_v.lQ;
	afh.m_StreamHeader.dwSuggestedBufferSize = afh.m_AviHeader.dwSuggestedBufferSize;
	afh.m_StreamHeader.rcFrame.right = m_FrameForm.m_Size.cx;
	afh.m_StreamHeader.rcFrame.bottom = m_FrameForm.m_Size.cy;

	// fill in bitmap header
	memcpy(&afh.m_biHeader, &m_VideoCodec.m_v.lpbiOut->bmiHeader, sizeof(afh.m_biHeader));
	return iPadFile;
}

HRESULT CAVIFile::OpenAVIFile( const TCHAR* pszFileName, CVideoFrameForm& FrameForm, double fFrameRate, const CVideoCodec& VideoCodec, const CWaveFormat* pAudioCodec )
{
	// Opens a new file, and writes the AVI header 
	// prepare filename
	// we could use mmioOpen() ??
	// NOTE:
	//  FrameForm = could be modified to match a format i can use.

	ASSERT(pszFileName);
	LOG_MSG(("OpenAVIFile: szFileName: %s" LOG_CR, pszFileName ));

	// Store my params i'm going to use.
	m_FrameForm = FrameForm;
	m_fFrameRate = fFrameRate;
	// prepare compressor. 
	m_VideoCodec.CopyCodec( VideoCodec );

	// reset frames counter and chunk size
	m_dwMoviChunkSize = sizeof(DWORD);
	m_dwTotalFrames = 0;
	m_Index.FlushIndexChunk();	// kill any previous index.

	// open compressor
	if ( ! m_VideoCodec.OpenCodec(ICMODE_FASTCOMPRESS))
	{
		DEBUG_ERR(( "CAVIFile:OpenCodec FAILED" LOG_CR ));
		return E_FAIL;
	}

	if ( pAudioCodec )
	{
		m_AudioCodec.SetFormat( *pAudioCodec );
	}
	else
	{
		m_AudioCodec.SetFormat( NULL );
	}

	// prepare BITMAPINFO for compressor
	static BITMAPINFO biIn;
	ZeroMemory( &biIn, sizeof(biIn));
	biIn.bmiHeader.biSize = sizeof(biIn.bmiHeader);
	biIn.bmiHeader.biWidth = m_FrameForm.m_Size.cx;
	biIn.bmiHeader.biHeight = m_FrameForm.m_Size.cy;
	biIn.bmiHeader.biPlanes = 1;
	biIn.bmiHeader.biBitCount = m_FrameForm.m_iBPP*8;	// always write 24-bit AVIs.
	biIn.bmiHeader.biCompression = BI_RGB;

#if 0 //def _DEBUG
	// print compressor settings
	// m_VideoCodec.DumpSettings();

	LRESULT dwFormatSize = m_VideoCodec.GetCompFormat(&biIn,NULL);
	if ( dwFormatSize <= 0 )
	{
		// dwFormatSize = -2 = ICERR_BADFORMAT 
		DEBUG_ERR(("CAVIFile:GetCompFormat=%d BAD FORMAT?!" LOG_CR, dwFormatSize));
		//ASSERT(0);
		return E_FAIL;
	}

	DEBUG_MSG(("CAVIFile:GetCompFormatSize=%d" LOG_CR, dwFormatSize));

	BITMAPINFO biOut;
	ZeroMemory(&biOut, sizeof(biOut));
	LRESULT lRes = m_VideoCodec.GetCompFormat(&biIn,&biOut);
	if ( lRes != 0 )
	{	
		// lRes = -2 = ICERR_BADFORMAT, should be 0x28 = sizeof(biIn.bmiHeader)
		DEBUG_ERR(( "CAVIFile:GetCompFormat FAILED %d" LOG_CR, lRes ));
		//ASSERT(0);
		return E_FAIL;
	}

	// The codec doesnt like odd sizes !!
	if ( biIn.bmiHeader.biHeight != biOut.bmiHeader.biHeight 
		|| biIn.bmiHeader.biWidth != biOut.bmiHeader.biWidth )
	{
		DEBUG_MSG(( "Codec changes the size! from (%d x %d) to (%d x %d)" LOG_CR,
			biIn.bmiHeader.biHeight, biIn.bmiHeader.biWidth,
			biOut.bmiHeader.biWidth, biOut.bmiHeader.biHeight ));
	}
#endif

	// initialize compressor
do_retry:
	if ( ! m_VideoCodec.CompStart(&biIn))
	{
		// re-try the aligned size.
		if (( FrameForm.m_Size.cx & 3 ) || ( FrameForm.m_Size.cy & 3 ))
		{
			DEBUG_WARN(( "CAVIFile:CompStart retry at (%d x %d)" LOG_CR, FrameForm.m_Size.cx, FrameForm.m_Size.cy ));
			FrameForm.m_Size.cx &= ~3;
			FrameForm.m_Size.cy &= ~3;
			biIn.bmiHeader.biWidth = FrameForm.m_Size.cx;
			biIn.bmiHeader.biHeight = FrameForm.m_Size.cy;
			m_FrameForm = FrameForm;
			goto do_retry;
		}

		DEBUG_ERR(( "CAVIFile:CompStart FAILED (%d x %d)" LOG_CR, FrameForm.m_Size.cx, FrameForm.m_Size.cy ));
		return E_FAIL;
	}

	//***************************************************
	m_File.AttachHandle( ::CreateFile( pszFileName, // file to create 
		GENERIC_READ | GENERIC_WRITE,  // open for reading and writing 
		0,                             // do not share 
		NULL,                          // default security 
		CREATE_ALWAYS,                 // overwrite existing 
		FILE_ATTRIBUTE_NORMAL,         // normal file 
		NULL));                         // no attr. template 
	if ( ! m_File.IsValidHandle()) 
	{
		DWORD dwLastError = ::GetLastError();
		DEBUG_ERR(( "CAVIFile:OpenAVIFile CreateFile FAILED %d" LOG_CR, dwLastError ));
		return HRESULT_FROM_WIN32(dwLastError);
	}

	AVI_FILE_HEADER afh;
	InitFileHeader(afh); // needs m_VideoCodec.m_v.lpbiOut

	DWORD dwBytesWritten = 0;
	::WriteFile(m_File, &afh, sizeof(afh), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(afh))
	{
		DWORD dwLastError = ::GetLastError();
		DEBUG_ERR(( "CAVIFile:OpenAVIFile WriteFile FAILED %d" LOG_CR, dwLastError ));
		return HRESULT_FROM_WIN32(dwLastError);
	}

	int iJunkChunkSize = AVI_MOVILIST_OFFSET - sizeof(afh);
	ASSERT(iJunkChunkSize>0);

	{
	// add "JUNK" chunk to get the 2K-alignment
	HANDLE hHeap = ::GetProcessHeap();
	DWORD* pChunkJunk = (DWORD*) ::HeapAlloc( hHeap, HEAP_ZERO_MEMORY, iJunkChunkSize);
	ASSERT(pChunkJunk);
	pChunkJunk[0] = ckidAVIPADDING; // "JUNK"
	pChunkJunk[1] = iJunkChunkSize - 8;

	::WriteFile(m_File, pChunkJunk, iJunkChunkSize, &dwBytesWritten, NULL);
	::HeapFree(hHeap,0,pChunkJunk);
	}
	if ( dwBytesWritten != iJunkChunkSize )
	{
		DWORD dwLastError = ::GetLastError();
		return HRESULT_FROM_WIN32(dwLastError);
	}

	// ASSERT( SetFilePointer(m_hFile, 0, NULL, FILE_CURRENT) == AVI_MOVILIST_OFFSET );

	DWORD dwTags[3];
	dwTags[0] = FOURCC_LIST;	// "LIST"
	dwTags[1] = m_dwMoviChunkSize;	// length to be filled later.
	dwTags[2] = listtypeAVIMOVIE;	// "movi"
	::WriteFile(m_File, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
	if ( dwBytesWritten != sizeof(dwTags))
	{
		DWORD dwLastError = ::GetLastError();
		return HRESULT_FROM_WIN32(dwLastError);
	}

	return S_OK;
}

void CAVIFile::CloseAVIFile()
{
	// finish sequence compression
	if ( ! m_File.IsValidHandle())
		return;

	// flush the buffers
	::FlushFileBuffers(m_File);

	// read the avi-header. So i can make my changes to it.
	AVI_FILE_HEADER afh;
	int iPadFile = InitFileHeader(afh); // needs m_VideoCodec.m_v.lpbiOut

	// save update header back with my changes.
	DWORD dwBytesWritten = 0;
	::SetFilePointer(m_File, 0, NULL, FILE_BEGIN);
	::WriteFile(m_File, &afh, sizeof(afh), &dwBytesWritten, NULL);

	// update movi chunk size
	::SetFilePointer(m_File, AVI_MOVILIST_OFFSET + 4, NULL, FILE_BEGIN);
	::WriteFile(m_File, &m_dwMoviChunkSize, sizeof(m_dwMoviChunkSize), &dwBytesWritten, NULL);

	// write some padding (if necessary) so that index always align at 16 bytes boundary
	::SetFilePointer(m_File, 0, NULL, FILE_END); 
	if (iPadFile > 0) 
	{ 
		BYTE zero = 0; 
		::WriteFile(m_File, &zero, 1, &dwBytesWritten, NULL);
	}

	m_Index.FlushIndexChunk(m_File);

	m_VideoCodec.DestroyCodec();	// leave m_v.lpbiOut->bmiHeader.biCompression til now

	// close file
	m_File.CloseHandle();
}

HRESULT CAVIFile::WriteVideoFrame( CVideoFrame& frame, int nTimes )
{
	// ARGS:
	//  nTimes = FrameDups = duplicate this frame a few times.
	// RETURN:
	//  bytes written
	// ASSUME: 
	//  This frame is at the correct frame rate.
	// NOTE: 
	//  This can be pretty slow. we may want to put this on another thread??

	if ( ! m_File.IsValidHandle()) 
		return 0;
	if ( nTimes <= 0 ) 
		return 0;
	ASSERT( ! memcmp( &frame, &m_FrameForm, sizeof(m_FrameForm)));
	DEBUG_TRACE(("CAVIFile:WriteVideoFrame: called. writing frame %d time(s)" LOG_CR, nTimes ));

	bool bCompressed;
	void* pCompBuf;
	LONG nSizeComp;
	BOOL bIsKey = false;

	DWORD dwBytesWritten = 0;
	for (int i=0; i<nTimes; i++)
	{
		// compress the frame, using chosen compressor.
		// NOTE: we need to recompress the frame even tho it may be the same as last time!
		if ( i < 2 )
		{
			bCompressed = m_VideoCodec.CompFrame( frame, pCompBuf, nSizeComp, bIsKey );
		}
		else
		{
			// no change.
		}

		DEBUG_TRACE(("CAVIFile:WriteVideoFrame: size=%d, bIsKey=%d" LOG_CR, nSizeComp, bIsKey ));

		DWORD dwTags[2];
		dwTags[0] = bCompressed ? mmioFOURCC('0', '0', 'd', 'c') : mmioFOURCC('0', '0', 'd', 'b');
		dwTags[1] = nSizeComp;

		// NOTE: do we really need to index each frame ?
		//  we could do it less often to save space???
		AVIINDEXENTRY indexentry;
		indexentry.ckid = dwTags[0]; 
		indexentry.dwFlags = (bIsKey) ? AVIIF_KEYFRAME : 0;
		indexentry.dwChunkOffset = AVI_MOVILIST_OFFSET + 8 + m_dwMoviChunkSize;
		indexentry.dwChunkLength = dwTags[1];
		m_Index.AddFrame( indexentry );

		// write video frame
		::WriteFile(m_File, dwTags, sizeof(dwTags), &dwBytesWritten, NULL);
		if ( dwBytesWritten != sizeof(dwTags))
		{
			DWORD dwLastError = ::GetLastError();
			DEBUG_ERR(("CAVIFile:WriteVideoFrame:WriteFile FAIL=%d" LOG_CR, dwLastError ));
			return HRESULT_FROM_WIN32(dwLastError);
		}
		::WriteFile(m_File, pCompBuf, (DWORD) nSizeComp, &dwBytesWritten, NULL);

		if ( nSizeComp & 1 ) // pad to even size.
		{
			BYTE bPad;
			::WriteFile(m_File, &bPad, 1, &dwBytesWritten, NULL);
			m_dwMoviChunkSize++;
		}

		m_dwTotalFrames ++;
		m_dwMoviChunkSize += sizeof(dwTags) + nSizeComp;
	}

	return dwBytesWritten;	// ASSUME not negative -> error
}

HRESULT CAVIFile::WriteAudioFrame( const BYTE* pWaveData )
{
	// TODO ???
	return 0;
}

#ifdef _DEBUG
bool CAVIFile::UnitTest()
{
	// a unit test for the AVIFile.
	return true;
}
#endif
