//
// AVI.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"

CTaksiFrameRate g_FrameRate;	// measure current video frame rate.
CAVIFile g_AVIFile;				// current video file we are recording.
CAVIThread g_AVIThread;			// put all compression on a back thread.
CVideoFrame g_VideoFrame;		// buffer to keep current video frame 

//CWaveACMInt g_ACM;			// compress audio stream.
//CWaveRecorder g_AudioInput;	// Raw PCM audio input. (loopback from output)

//**************************************************************************************

void CTaksiFrameRate::InitFreqUnits()
{
	// record the freq of the timer.
	LARGE_INTEGER freq = {0, 0};
	if ( ::QueryPerformanceFrequency(&freq)) 
	{
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

	while ( ! m_bStop && m_hThread.IsValidHandle())
	{
		// and wait til i'm told to start
		DWORD dwRet = ::WaitForSingleObject( m_hEventDataStart, INFINITE );
		if ( dwRet != WAIT_OBJECT_0 )
		{
			// failure?
			DEBUG_ERR(( "CAVIThread::WaitForSingleObject FAIL" LOG_CR ));
			break;
		}

		if ( m_bStop )
			break;

		// compress and write
		HRESULT hRes = g_AVIFile.WriteVideoFrame( g_VideoFrame, m_dwFrameDups ); 
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

		// Set my event when done. 
		m_hEventDataDone.SetEvent();	// done
	}

	DEBUG_MSG(( "CAVIThread::ThreadRun() END" LOG_CR ));
	m_nThreadId = 0;		// it actually is closed!
	return 0;
}

DWORD __stdcall CAVIThread::ThreadEntryProc( void* pThis ) // static
{
	ASSERT(pThis);
	return ((CAVIThread*)pThis)->ThreadRun();
}

DWORD CAVIThread::WaitForDataDone()
{
	// Wait for the thread to finish what it was doing.
	if ( m_nThreadId == 0 )
		return 0;
	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!
	return ::WaitForSingleObject( m_hEventDataDone, INFINITE );
}

void CAVIThread::SignalDataStart( DWORD dwFrameDups )	// ready to compress/write
{
	// New data is ready so wake up the thread.
	// ASSUME: WaitForDataDone() was just called.
	if ( m_nThreadId == 0 )
		return;
	ASSERT( GetCurrentThreadId() != m_nThreadId );	// never call on myself!
	m_dwFrameDups = dwFrameDups;
	m_hEventDataDone.ResetEvent();	// manual reset.
	m_hEventDataStart.SetEvent();
}

HRESULT CAVIThread::StopAVIThread()
{
	// Destroy the thread. might be better to just let it sit ?
	if ( ! m_hThread.IsValidHandle())
		return S_FALSE;

	DEBUG_TRACE(( "CAVIThread::StopAVIThread" LOG_CR ));
	m_bStop	= true;
	m_hEventDataStart.SetEvent();	// wake up to exit.

	// Wait for it !
	HRESULT hRes = S_OK;
	if (m_nThreadId)	// has not yet closed!
	{
		hRes = ::WaitForSingleObject( m_hThread, 15*1000 );
		if ( hRes == WAIT_OBJECT_0 )
		{
			ASSERT(m_nThreadId==0);
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

	if ( ! m_hEventDataStart.CreateEvent(NULL,false,false))
	{
		goto do_erroret;
	}
	if ( ! m_hEventDataDone.CreateEvent(NULL,true,true))	// manual reset.
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
