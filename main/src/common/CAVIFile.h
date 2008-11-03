//
// CAVIFile.h
// Video Compressor and Video File.
//
#ifndef _INC_CAVIFile_H
#define _INC_CAVIFile_H
#if _MSC_VER >= 1000
#pragma once
#endif

#include "CNTHandle.h"
#include "CWaveFormat.h"
#include "CWaveACM.h"

#include <vfw.h>	// COMPVARS

struct TAKSI_LINK CVideoFrameForm
{
	// The format of a single video frame.
	CVideoFrameForm()
		: m_iPitch(0)
		, m_iBPP(3)
	{
		m_Size.cx = 0;
		m_Size.cy = 0;
	}
	bool IsFrameFormInit() const
	{
		if ( m_Size.cx <= 0 )
			return false;
		if ( m_Size.cy <= 0 )
			return false;
		if ( m_iPitch <= 0 )
			return false;
		if ( m_iBPP <= 0 )
			return false;
		return true;
	}
	int get_SizeBytes() const
	{
		return m_iPitch * m_Size.cy;
	}
	void InitPadded( int cx, int cy, int iBPP=3, int iPad=sizeof(DWORD));

public:
	SIZE m_Size;		// Frame size in pixels. (check m_iPitch for padding)
	int m_iPitch;		// Bytes for a row with padding.
	int m_iBPP;			// Bytes per pixel.
};

struct TAKSI_LINK CVideoFrame : public CVideoFrameForm
{
	// buffer to keep a single video frame 
public:
	CVideoFrame()
		: m_pPixels(NULL)
	{
	}
	~CVideoFrame()
	{
		FreeFrame();
	}

	bool IsValidFrame() const
	{
		return m_pPixels != NULL;
	}

	void FreeFrame();
	bool AllocForm( const CVideoFrameForm& form );
	bool AllocPadXY( int cx, int cy, int iBPP=3, int iPad=sizeof(DWORD));	// padded to 4 bytes

	void SetupBITMAPINFOHEADER( BITMAPINFOHEADER& bmih ) const;
	HRESULT SaveAsBMP( const TCHAR* pszFilename ) const;

public:
	BYTE* m_pPixels;	// Pixel data for the frame buffer.
};

struct TAKSI_LINK CVideoCodec
{
	// Wrap the COMPVARS video codec.
	// For compress, decompress or render.
	// NOTE: cant have a constructor for this!

public:
	void InitCodec();
	void DestroyCodec();
	void CopyCodec( const CVideoCodec& codec );
	static HRESULT GetHError( LRESULT lRes );

	// serializers
	int GetStr( char* pszValue, int iSizeMax) const;
	bool put_Str( const char* pszValue );

    HRESULT OpenCodec( WORD wMode = ICMODE_FASTCOMPRESS );
	void CloseCodec();

	bool CompChooseDlg( HWND hWnd, char* lpszTitle );
	bool CompSupportsConfigure() const;
	LRESULT CompConfigureDlg( HWND hWndApp );

	bool GetCodecInfo( ICINFO& icInfo ) const;
	LRESULT GetCompFormat( const BITMAPINFO* lpbiIn, BITMAPINFO* lpbiOut=NULL ) const;

	HRESULT CompStart( BITMAPINFO* lpbi = NULL );
	HRESULT CompFrame( const CVideoFrame& frame, const void*& rpCompRet, LONG& nSizeRet, BOOL& bIsKey );
	void CompEnd();

#ifdef _DEBUG
	void DumpSettings();
#endif

public:
	COMPVARS m_v;			// m_v.hic from <vfw.h>
	bool m_bCompressing;	// CompStart() has been called.
};

struct TAKSI_LINK CAVIIndexBlock
{
	// Build an index of frames to append to the end of the file.
#define AVIINDEX_QTY 1024
	AVIINDEXENTRY* m_pEntry;	// block of AVIINDEX_QTY entries
	struct CAVIIndexBlock* m_pNext;
};

struct TAKSI_LINK CAVIIndex
{
	// Create an index for an AVI file.
	// NOTE: necessary to index all frames ???
public:
	CAVIIndex();
	~CAVIIndex();

	void AddFrame( const AVIINDEXENTRY& indexentry );
	void FlushIndexChunk( HANDLE hFile = INVALID_HANDLE_VALUE );

	DWORD get_ChunkSize() const
	{
		// NOT including the chunk overhead.
		return( m_dwFramesTotal * sizeof(AVIINDEXENTRY));
	}

private:
	CAVIIndexBlock* CreateIndexBlock();
	void DeleteIndexBlock( CAVIIndexBlock* pIndex );

private:
	DWORD m_dwFramesTotal;		// total number of frames indexed.

	DWORD m_dwIndexBlocks;		// count of total index blocks created.
	CAVIIndexBlock* m_pIndexFirst;	// list of blocks of indexes to frames.
	CAVIIndexBlock* m_pIndexCur;

	DWORD m_dwIndexCurFrames;	// Count up to AVIINDEX_QTY in m_pIndexCur.
};

struct AVI_FILE_HEADER;
struct AVI_VIDEO_HEADER;
struct AVI_AUDIO_HEADER;

struct TAKSI_LINK CAVIFile
{
	// Stream to create a AVI file
	// handles audio, video and possibly other data in the time stream.

public:
	CAVIFile();
	~CAVIFile();

	bool IsOpen() const
	{
		return m_File.IsValidHandle();
	}
	bool HasVideo() const
	{
		return( true );
	}
	bool HasAudioFormat() const
	{
		return( m_AudioFormat.IsValidFormat());
	}

	HRESULT OpenAVICodec( CVideoFrameForm& FrameForm, double fFrameRate, const CVideoCodec& CodecInfo, const CWaveFormat* pAudioFormat=NULL );
	HRESULT OpenAVIFile( const TCHAR* pszFileName );
	//HRESULT OpenAVI( const TCHAR* pszFileName, CVideoFrameForm& FrameForm, double fFrameRate, const CVideoCodec& VideoCodec, const CWaveFormat* pAudioFormat );

	// compress the raw data and store it.
	HRESULT WriteAudioBlock( const BYTE* pWaveData, DWORD dwLength );
	HRESULT WriteVideoBlocks( CVideoFrame& frame, int iTimesDupe );

	HRESULT CloseAVI();

#ifdef _DEBUG
	static bool _stdcall UnitTest();
#endif

private:
	void InitBitmapIn( BITMAPINFO& biIn ) const;
	int InitFileHeader( AVI_FILE_HEADER& afh, AVI_VIDEO_HEADER* pVideo, AVI_AUDIO_HEADER* pAudio );

public:
	CVideoFrameForm m_FrameForm;		// pixel format of each video frame.
	const char* m_pJunkData; // _snprintf( sdf, sizeof(sdf), "TAKSI v" TAKSI_VERSION_S " built:" __DATE__ " AVI recorded: XXX" );

private:
	CNTHandle m_File;	// the open AVI file.

	DWORD m_dwMoviChunkSize;	// total amount of data created. in 'movi' chunk
	DWORD m_dwTotalFrames;		// total frames compressed/written.

	CAVIIndex m_Index;			// build an index as we go.

	// Params set at OpenAVIFile().
	double m_fFrameRate;		// What video framerate are we trying to use? (x/second)
	CVideoCodec m_VideoCodec;	// What video compression params did we choose?
	BITMAPINFO m_biIn;			// m_VideoCodec points at this. needs to persist.

	CWaveFormat m_AudioFormat;
};

TAKSI_LINK HRESULT HRes_GetLastErrorDef( HRESULT hDefault = E_FAIL );

#endif // _INC_CAVIFile_H
