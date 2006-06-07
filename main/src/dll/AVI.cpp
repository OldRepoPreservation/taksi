//
// AVI.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"

CTaksiFrameRate g_FrameRate;	// measure current video frame rate.
CAVIFile g_AVIFile;				// current video file we are recording.
CAVIThread g_AVIThread;			// put all compression on a back thread. Q raw data to be processed.

//CWaveACMInt g_ACM;			// compress audio stream.
//CWaveRecorder g_AudioInput;	// Raw PCM audio input. (loopback from output)

//**************************************************************************************

void CTaksiFrameRate::InitFreqUnits()
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
		DEBUG_ERR(( "QueryPerformanceFrequency FAILED!" LOG_CR ));
	}
	m_dwFreqUnits = freq.LowPart;
	m_tLastCount = 0;
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
	double fTargetFrameRate;

	if ( g_Proc.m_pCustomConfig )
	{
		// using a custom frame rate/weight
		double fFrameWeight = g_Proc.m_pCustomConfig->m_fFrameWeight;
		if (fFrameWeight>0)
		{
			fFrameWeight += m_fLeftoverWeight;
			DWORD dwFrameDups = (DWORD)(fFrameWeight);
			m_fLeftoverWeight = fFrameWeight - dwFrameDups;
			DEBUG_TRACE(( "dwFrameDups = %d" LOG_CR, dwFrameDups ));
			//DEBUG_TRACE(( "g_extraWeight = %f" LOG_CR, m_fLeftoverWeight ));
			return dwFrameDups;
		}
		fTargetFrameRate = g_Proc.m_pCustomConfig->m_fFrameRate;
	}
	else
	{
		fTargetFrameRate = sg_Config.m_fFrameRateTarget;
	}

	DEBUG_TRACE(( "tNow=%u, iTimeDiff=%d, LastOverhead=%u" LOG_CR, (int) tNow, (int) iTimeDiff, (int) m_dwLastOverhead ));

	if (m_dwLastOverhead > 0) 
	{
		m_dwPenalty = m_dwLastOverhead / 2;
		iTimeDiff -= (m_dwLastOverhead - m_dwPenalty);
		m_dwLastOverhead = 0;
	}
	else
	{
		m_dwPenalty = m_dwPenalty / 2;
		iTimeDiff += m_dwPenalty;
	}

	DEBUG_TRACE(( "adjusted iTimeDiff = %d" LOG_CR, (int) iTimeDiff ));

	double fFrameRateCur = (double)m_dwFreqUnits / (double)iTimeDiff;

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

	double fFrameWeight = fTargetFrameRate / fFrameRateCur + m_fLeftoverWeight;
	DWORD dwFrameDups = (DWORD)(fFrameWeight);
	m_fLeftoverWeight = fFrameWeight - dwFrameDups;

	DEBUG_TRACE(( "dwFrameDups = %d" LOG_CR, dwFrameDups ));
	//DEBUG_TRACE(( "g_extraWeight = %f" LOG_CR, m_fLeftoverWeight));

	return dwFrameDups;
}

//**************************************************************** 

CAVIThread::CAVIThread()
	: m_nThreadId(0)
	, m_bStop(false)
	, m_iFrameBusy(0)	// index to Frame ready to compress.
	, m_iFrameFree(0)	// index to empty frame. ready to fill
	, m_iFrameCount(0)
{
}
CAVIThread::~CAVIThread()
{
	// thread should already be killed!
	StopAVIThread();
}

DWORD CAVIThread::ThreadRun()
{
	// m_hThread
	DEBUG_MSG(( "CAVIThread::ThreadRun()" LOG_CR));

	goto do_wait;

	for (;;)
	{
		ASSERT( m_iFrameBusy != m_iFrameFree );
		CAVIFrame* pFrame = &m_aFrames[ m_iFrameBusy ];
		ASSERT(pFrame);
		ASSERT(pFrame->IsValidFrame());
		ASSERT(pFrame->m_dwFrameDups);

		// compress and write
		HRESULT hRes = g_AVIFile.WriteVideoFrame( *pFrame, pFrame->m_dwFrameDups ); 
		if ( SUCCEEDED(hRes))
		{
			if ( hRes )
			{
				g_Proc.m_Stats.m_dwDataRecorded += hRes;
				g_Proc.UpdateStat(TAKSI_PROCSTAT_DataRecorded);
			}
		}

		// Audio data ?
		// g_AVIFile.WriteAudioFrame(xxx);

		// we are done.
		pFrame->m_dwFrameDups = 0;	// done.
		m_iFrameBusy = ( m_iFrameBusy + 1 ) % AVI_FRAME_QTY;
		m_iFrameCount--;
		m_EventDataDone.SetEvent();	// done at least one frame!
		if ( m_iFrameBusy != m_iFrameFree )	// more to do ?
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

		if ( m_bStop || ! m_hThread.IsValidHandle())
			break;
	}

	m_iFrameCount = 0;
	m_nThreadId = 0;		// it actually is closed!
	DEBUG_MSG(( "CAVIThread::ThreadRun() END" LOG_CR ));
	return 0;
}

DWORD __stdcall CAVIThread::ThreadEntryProc( void* pThis ) // static
{
	ASSERT(pThis);
	return ((CAVIThread*)pThis)->ThreadRun();
}

void CAVIThread::WaitForAllFrames()
{
	// Just wait for all outstanding frames to complete.
	// 
	if ( m_nThreadId == 0 )
		return;
	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!
	while ( m_iFrameCount ) 
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
	if ( m_iFrameFree == m_iFrameBusy && m_iFrameCount >= AVI_FRAME_QTY )
	{
		DWORD dwRet = ::WaitForSingleObject( m_EventDataDone, INFINITE );
		if ( dwRet != WAIT_OBJECT_0 )
		{
			DEBUG_ERR(( "CAVIThread::WaitForNextFrame FAILED" LOG_CR ));
			return NULL;	// failed.
		}
	}

	CAVIFrame* pFrame = &m_aFrames[ m_iFrameFree ];
	ASSERT( pFrame );
	ASSERT( pFrame->m_dwFrameDups == 0 );
	if ( ! pFrame->AllocForm( g_AVIFile.m_FrameForm ))
		return NULL;
	return pFrame;	// ready
}

void CAVIThread::SignalFrameStart( CAVIFrame* pFrame, DWORD dwFrameDups )	// ready to compress/write
{
	// New data is ready so wake up the thread.
	// ASSUME: WaitForNextFrame() was just called.
	if ( m_nThreadId == 0 )
		return;
	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!
	ASSERT(dwFrameDups);
	ASSERT(pFrame);
	ASSERT( pFrame->m_dwFrameDups == 0 );
	pFrame->m_dwFrameDups = dwFrameDups;
	m_iFrameFree = ( m_iFrameFree + 1 ) % AVI_FRAME_QTY;	// its ready.
	m_iFrameCount++;
	m_EventDataDone.ResetEvent();	// manual reset.
	m_EventDataStart.SetEvent();	// wake the thread if it needs it.
}

HRESULT CAVIThread::StopAVIThread()
{
	// Destroy the thread. might be better to just let it sit ?
	if ( ! m_hThread.IsValidHandle())
		return S_FALSE;

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

	m_hThread.CloseHandle();
	return hRes;
}

HRESULT CAVIThread::StartAVIThread()
{
	// Create my resources.	
	if ( m_nThreadId )	// already running
		return S_FALSE;

	DEBUG_TRACE(( "CAVIThread::StartAVIThread" LOG_CR));
	m_bStop	= false;

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
		DWORD dwLastError = ::GetLastError();
		if ( dwLastError == 0 )
			dwLastError = E_FAIL;
		DEBUG_ERR(( "CAVIThread:Unable to create new thread %d" LOG_CR, dwLastError ));
		return HRESULT_FROM_WIN32(dwLastError);
	}
	return S_OK;
}
