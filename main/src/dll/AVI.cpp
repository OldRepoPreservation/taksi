//
// AVI.cpp
// Background thread to perform the compression and writing.
//
#include "../stdafx.h"
#include "TaksiDll.h"

CTaksiFrameRate g_FrameRate;	// measure current video frame rate.
CAVIFile g_AVIFile;				// current video file we are recording.
CAVIThread g_AVIThread;			// put all compression on a back thread. Q raw data to be processed.

//**************************************************************************************

bool CTaksiFrameRate::InitFreqUnits()
{
	// record the freq of the timer.
	LARGE_INTEGER freq = {0, 0};
	if ( ::QueryPerformanceFrequency(&freq)) 
	{
		ASSERT( freq.HighPart == 0 );
		DEBUG_MSG(( "QueryPerformanceFrequency hi=%u, lo=%u" LOG_CR, freq.HighPart, freq.LowPart));
	}
	else
	{
		LOG_WARN(( "QueryPerformanceFrequency FAILED!" LOG_CR ));
		m_dwFreqUnits = 0;
		return false;	// this cant work!
	}
	ASSERT( freq.HighPart == 0 );
	m_dwFreqUnits = freq.LowPart;
	InitStart();
	return true;
}

double CTaksiFrameRate::CheckFrameWeight( __int64 iTimeDiff )
{
	double fTargetFrameRate;
	if ( g_Proc.m_pCustomConfig )
	{
		// using a custom frame rate/weight. ignore iTimeDiff
		double fFrameWeight = g_Proc.m_pCustomConfig->m_fFrameWeight;
		if (fFrameWeight>0)
		{
			return fFrameWeight;
		}
		fTargetFrameRate = g_Proc.m_pCustomConfig->m_fFrameRate;
	}
	else
	{
		fTargetFrameRate = sg_Config.m_fFrameRateTarget;
	}

	DEBUG_TRACE(( "iTimeDiff=%d" LOG_CR, (int) iTimeDiff ));

	if ( sg_Config.m_bUseOverheadCompensation )
	{
		if (m_dwLastOverhead > 0) 
		{
			m_dwOverheadPenalty = m_dwLastOverhead / 2;
			iTimeDiff -= (m_dwLastOverhead - m_dwOverheadPenalty);
			m_dwLastOverhead = 0;
		}
		else
		{
			m_dwOverheadPenalty = m_dwOverheadPenalty / 2;
			iTimeDiff += m_dwOverheadPenalty;
		}
		DEBUG_TRACE(( "adjusted iTimeDiff = %d, LastOverhead=%u" LOG_CR, (int) iTimeDiff, (int) m_dwLastOverhead ));
	}

	double fFrameRateCur = ((double)m_dwFreqUnits) / ((double)iTimeDiff);

	if ( g_Proc.m_Stats.m_fFrameRate != fFrameRateCur )
	{
		g_Proc.m_Stats.m_fFrameRate = fFrameRateCur;
		g_Proc.UpdateStat(TAKSI_PROCSTAT_FrameRate);
	}

	DEBUG_TRACE(( "currentFrameRate = %f" LOG_CR, fFrameRateCur ));

	if (fFrameRateCur <= 0)
	{
		return 0;
	}

	return fTargetFrameRate / fFrameRateCur;
}

DWORD CTaksiFrameRate::CheckFrameRate()
{
	// Called during frame present
	// determine whether this frame needs to be grabbed when recording.
	// RETURN:
	//  dwFrameDups = number of frames to record to match my desired frame rate.

	// How long since the last frame?
	TIMEFAST_t tNow = GetPerformanceCounter();
	__int64 iTimeDiff = (__int64)( tNow - m_tLastCount );
	if ( m_tLastCount == 0 || iTimeDiff <= 0 )	// first frame i've seen?
	{
		ASSERT(tNow);
		m_tLastCount = tNow;
		return 1;	// always take the first frame.
	}

	m_tLastCount = tNow;
	double fFrameWeight = CheckFrameWeight( iTimeDiff );

	fFrameWeight += m_fLeftoverWeight;
	DWORD dwFrameDups = (DWORD)(fFrameWeight);
	m_fLeftoverWeight = fFrameWeight - dwFrameDups;

	DEBUG_TRACE(( "dwFrameDups = %d + %f" LOG_CR, dwFrameDups, m_fLeftoverWeight ));

	return dwFrameDups;
}

//**************************************************************** 

CAVIThread::CAVIThread()
	: m_nThreadId(0)
	, m_bStop(false)
	, m_dwTotalFramesProcessed(0)
{
	InitFrameQ();
}
CAVIThread::~CAVIThread()
{
	// thread should already be killed!
	StopAVIThread();
}

void CAVIThread::InitFrameQ()
{
	// called by StartAVIThread() and constructor
	m_iFrameBusyIdx=0;	// index to Frame ready to compress.
	m_iFrameFreeIdx=0;	// index to empty frame. ready to fill
	m_iFrameCount.m_lValue = 0;
#ifdef USE_AUDIO
	m_AudioBufferSize = 0;
	m_AudioBufferHeadIdx=0;
	m_AudioBufferStart=0;
#endif
}

DWORD CAVIThread::ThreadRun()
{
	// A background thread to do the compression/writing.
	// m_hThread
	DEBUG_MSG(( "CAVIThread::ThreadRun() n=%d" LOG_CR, m_dwTotalFramesProcessed ));
	goto do_wait;

	while ( ! m_bStop && m_hThread.IsValidHandle())
	{
		int iFrameCountPrev = m_iFrameCount.m_lValue;
		ASSERT( m_iFrameCount.m_lValue > 0 );
		CAVIFrame* pFrame = &m_aFrames[ m_iFrameBusyIdx ];
		ASSERT(pFrame);
		ASSERT(pFrame->m_Video.IsValidFrame());
		ASSERT(pFrame->m_dwFrameDups);

		// compress and write video frame(s)
		HRESULT hRes = g_AVIFile.WriteVideoBlocks( pFrame->m_Video, pFrame->m_dwFrameDups ); 
		if ( SUCCEEDED(hRes))
		{
			if ( hRes )
			{
				g_Proc.m_Stats.m_dwDataRecorded += hRes;
				g_Proc.UpdateStat(TAKSI_PROCSTAT_DataRecorded);
			}
		}

#ifdef USE_AUDIO
		// Audio data?
		if ( m_AudioInput.get_Handle())	// g_AVIFile.HasAudioFormat()
		{
			// Write all the audio data available.

			// g_AVIFile.WriteAudioBlock( pFrame->m_Audio, pFrame->m_dwFrameDups );
		}
#endif

		// we are done.
		m_dwTotalFramesProcessed++;
		pFrame->m_dwFrameDups = 0;	// done.
		m_iFrameBusyIdx = ( m_iFrameBusyIdx + 1 ) % AVI_FRAME_QTY;
		ASSERT( m_iFrameCount.m_lValue > 0 );
		bool bMore = m_iFrameCount.Dec();
		m_EventDataDone.SetEvent();	// done at least one frame!
		if ( bMore )	// more to do ?
			continue;

do_wait:
		// and wait til i'm told to start
		DWORD dwRet = ::WaitForSingleObject( m_EventDataStart, INFINITE );
		if ( dwRet != WAIT_OBJECT_0 )
		{
			// failure?
			DEBUG_ERR(( "CAVIThread::WaitForSingleObject FAIL" LOG_CR ));
			break;
		}
	}

	m_iFrameCount.Exchange(0);
	m_nThreadId = 0;		// it actually is closed!
	DEBUG_MSG(( "CAVIThread::ThreadRun() END" LOG_CR ));
	return 0;
}

DWORD __stdcall CAVIThread::ThreadEntryProc( void* pThis ) // static
{
	ASSERT(pThis);
	return ((CAVIThread*)pThis)->ThreadRun();
}

//*****************************************************************

void CAVIThread::WaitForAllFrames()
{
	// Just wait for all outstanding frames to complete.
	// 
	if ( m_nThreadId == 0 )
		return;
	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!
	while ( m_iFrameCount.m_lValue ) 
	{
		DWORD dwRet = ::WaitForSingleObject( m_EventDataDone, INFINITE );
		if ( dwRet != WAIT_OBJECT_0 )
		{
			DEBUG_ERR(( "CAVIThread::WaitForNextFrame FAILED" LOG_CR ));
			return;	// failed.
		}
		m_EventDataDone.ResetEvent();	// wait again til we get them all.
	}
	// Free all my frames?
}

CAVIFrame* CAVIThread::WaitForNextFrame()
{
	// Get a new video frame to put raw data.
	// If none available. Wait for the thread to finish what it was doing.
	// bFlush = wait for them all to be done.

	if ( m_nThreadId == 0 )
		return NULL;
	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!

	// just wait for a free frame. all busy.
	if ( m_iFrameCount.m_lValue >= AVI_FRAME_QTY )
	{
		DWORD dwRet = ::WaitForSingleObject( m_EventDataDone, INFINITE );
		if ( dwRet != WAIT_OBJECT_0 )
		{
			DEBUG_ERR(( "CAVIThread::WaitForNextFrame FAILED" LOG_CR ));
			return NULL;	// failed.
		}
	}

	ASSERT( m_iFrameCount.m_lValue < AVI_FRAME_QTY );
	CAVIFrame* pFrame = &m_aFrames[ m_iFrameFreeIdx ];
	ASSERT( pFrame );
	ASSERT( pFrame->m_dwFrameDups == 0 ); 
	ASSERT( g_AVIFile.m_FrameForm.get_SizeBytes());
	if ( ! pFrame->m_Video.AllocForm( g_AVIFile.m_FrameForm ))
	{
		ASSERT(0);
		return NULL;
	}

	return pFrame;	// ready
}

void CAVIThread::SignalFrameAdd( CAVIFrame* pFrame, DWORD dwFrameDups )	// ready to compress/write
{
	// New data is ready so wake up the thread.
	// ARGS:
	//  pFrame = m_aFrames[ m_iFrameFreeIdx ]
	//  dwFrameDups = Multiple time frames have gone by.
	// ASSUME: 
	//  WaitForNextFrame() was just called.

	if ( m_nThreadId == 0 )
		return;

	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!
	ASSERT(dwFrameDups>0);
	ASSERT(pFrame);
	ASSERT( pFrame->m_Video.IsValidFrame());
	ASSERT( pFrame->m_dwFrameDups == 0 ); 
	ASSERT( pFrame == &m_aFrames[ m_iFrameFreeIdx ] );

	pFrame->m_dwFrameDups = dwFrameDups;	// indicate this pFrame is ready to go.
	m_iFrameFreeIdx = ( m_iFrameFreeIdx + 1 ) % AVI_FRAME_QTY;	// its full and ready to process.
	m_EventDataDone.ResetEvent();	// manual reset.

	ASSERT( m_iFrameCount.m_lValue < AVI_FRAME_QTY );
	if ( m_iFrameCount.Inc() == 1 )
	{
		m_EventDataStart.SetEvent();	// wake the thread if it needs it.
	}
}

//*****************************************************************

#ifdef USE_AUDIO

void CALLBACK CAVIThread::AudioInProc( HWAVEIN hWaveIn, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 ) // static
{
	// waveInProc() = data is ready
	// uMsg = MM_WIM_DATA etc.
	// dwInstance = CAVIThread
	// NOTE: this is called on an unknown thread!

	CAVIThread* pThread = (CAVIThread*)dwInstance;
	ASSERT(pThread);

	if ( uMsg == MM_WIM_DATA )
	{
		// waveInAddBuffer is complete. recycle and/or move to next
	}
}

HRESULT CAVIThread::OpenAudioInputDevice( WAVE_DEVICEID_TYPE iWaveDeviceId, CWaveFormat& WaveFormat )
{
	// Open the audio input device. Codec is built in!
	if ( iWaveDeviceId == WAVE_DEVICE_NONE )	// want no audio.
	{
		return S_FALSE;
	}
	if ( ! WaveFormat.IsValidFormat())
	{
		return S_FALSE;
	}

	MMRESULT mmRes = m_AudioInput.put_DeviceID(iWaveDeviceId);
	if ( mmRes != 0 )
	{
		return E_FAIL;
	}

	// NOTE: this engages the ACM for us under the hood.
	mmRes = m_AudioInput.Open( WaveFormat, (UINT_PTR) AudioInProc, (UINT_PTR) this, CALLBACK_FUNCTION|WAVE_MAPPED );
	if ( mmRes != 0 )
	{
		// No codec for this? just do PCM and do the ACM manually? waveInOpen()
		if ( mmRes == WAVERR_BADFORMAT )
		{
		}
		// Device is busy?
		return E_FAIL;
	}

	ASSERT( m_AudioInput.get_Handle());

	// Queue up Max of N seconds of audio.
	// Compute a buffer size. approximate max per frame. honor blockAlign min
	DWORD dwBufferSize = 0;
	for ( int j=0; j<COUNTOF(m_AudioBuffers); j++ )
	{
		m_AudioBuffers[j].AttachData(NULL,dwBufferSize);
	}

	return S_OK;
}

#endif // USE_AUDIO

//*****************************************************************

HRESULT CAVIThread::StopAVIThread()
{
	// Destroy the thread. might be better to just let it sit ?
	if ( ! m_hThread.IsValidHandle())
	{
		return S_FALSE;
	}

	DEBUG_TRACE(( "CAVIThread::StopAVIThread" LOG_CR ));

	m_bStop	= true;
	m_EventDataStart.SetEvent();	// wake up to exit.

	// Wait for it !
	HRESULT hRes = S_OK;
	if (m_nThreadId)	// has not yet closed!
	{
		hRes = ::WaitForSingleObject( m_hThread, 15*1000 );
		if ( hRes == WAIT_OBJECT_0 )
		{
			//ASSERT(m_nThreadId==0);
			hRes = S_OK;
		}
		else 
		{
			DEBUG_ERR(( "CAVIThread::StopAVIThread TIMEOUT!" LOG_CR ));
			ASSERT( hRes == WAIT_TIMEOUT );
		}
	}

#ifdef USE_AUDIO
	// Close the audio last. 
	if ( m_AudioInput.get_Handle())
	{
		m_AudioInput.Close();
		for ( int j=0; j<COUNTOF(m_AudioBuffers); j++ )
		{
			m_AudioBuffers[j].DetachData();
		}
	}
#endif

	m_hThread.CloseHandle();
	return hRes;
}

HRESULT CAVIThread::StartAVIThread()
{
	// Create my resources.	
	if ( m_nThreadId != NULL )	// already running
	{
		return S_FALSE;
	}

	DEBUG_TRACE(( "CAVIThread::StartAVIThread" LOG_CR));
	m_bStop	= false;
	InitFrameQ();

	if ( ! m_EventDataStart.CreateEvent(NULL,false,false))
	{
		goto do_erroret;
	}
	if ( ! m_EventDataDone.CreateEvent(NULL,true,true))	// manual set/reset.
	{
		goto do_erroret;
	}

	m_hThread.AttachHandle( ::CreateThread(
		NULL, // LPSECURITY_ATTRIBUTES lpThreadAttributes,
		0, // SIZE_T dwStackSize,
		ThreadEntryProc, // LPTHREAD_START_ROUTINE lpStartAddress,
		this, // LPVOID lpParameter,
		0, // dwCreationFlags
		&m_nThreadId ));
	if ( ! m_hThread.IsValidHandle())
	{
		m_nThreadId = 0;
do_erroret:
		HRESULT hRes = Check_GetLastError( HRESULT_FROM_WIN32(ERROR_TOO_MANY_TCBS));
		LOG_WARN(( "CAVIThread: FAILED to create new thread 0x%x" LOG_CR, hRes ));
		return hRes;
	}

#ifdef USE_AUDIO
	if ( m_AudioInput.get_Handle())
	{
		// HasAudio
		m_AudioInput.Start();	// start active recording.
	}
#endif
	return S_OK;
}
