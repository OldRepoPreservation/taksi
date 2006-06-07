//
// TaksiDll.h
// Private to the DLL
//
#ifndef _INC_TAKSIDLL
#define _INC_TAKSIDLL
#if _MSC_VER > 1000
#pragma once
#endif

#include "../common/CThreadLockedLong.h"

extern CAVIFile g_AVIFile;

struct CTaksiFrameRate
{
	// watch the current frame rate.
	// frame rate calculator
public:
	CTaksiFrameRate()
		: m_dwLastOverhead(0)
		, m_dwFreqUnits(0)
		, m_tLastCount(0)
		, m_fLeftoverWeight(0.0)
		, m_dwPenalty(0)
	{
	}
	void InitFreqUnits();

	DWORD CheckFrameRate();

	void MeasureOverhead()
	{
		TIMEFAST_t tNow = GetPerformanceCounter();
		m_dwLastOverhead = (DWORD)( tNow - m_tLastCount );
	}

private:
	DWORD m_dwFreqUnits;		// the units/sec of the TIMEFAST_t timer.
	TIMEFAST_t m_tLastCount;	// when was CheckFrameRate() called?
	float m_fLeftoverWeight;
	DWORD m_dwLastOverhead;
	DWORD m_dwPenalty;
};
extern CTaksiFrameRate g_FrameRate;

struct CAVIFrame : public CVideoFrame
{
	// an en-queueed frame to be processed.
	CAVIFrame()
		: m_dwFrameDups(0)
	{
	}
	DWORD m_dwFrameDups;	// Dupe the current frame to catch up the frame rate. 0=unused
};

struct CAVIThread
{
	// A worker thread to compress frames and write AVI in the background
public:
	CAVIThread();
	~CAVIThread();

	HRESULT StartAVIThread();
	HRESULT StopAVIThread();

	void WaitForAllFrames();
	CAVIFrame* WaitForNextFrame();
	void SignalFrameStart( CAVIFrame* pFrame, DWORD dwFrameDups );	// ready to compress/write a raw frame

	int get_FrameBusyCount() const
	{
		return m_iFrameCount;	// how many busy frames are there? (not yet processed)
	}

private:
	DWORD ThreadRun();
	static DWORD __stdcall ThreadEntryProc( void* pThis );

private:
	CNTHandle m_hThread;
	DWORD m_nThreadId;
	bool m_bStop;

	CNTEvent m_EventDataStart;		// Data ready to work on. AVIThread waits on this
	CNTEvent m_EventDataDone;		// We are done compressing a single frame. foreground waits on this.

	// Make a pool of frames waiting to be compressed/written
	// ASSUME: dont need a critical section on these since i assume int x=y assignment is atomic.
#define AVI_FRAME_QTY 16	// make this variable ??? (full screen raw frames are HUGE!)
	CAVIFrame m_aFrames[AVI_FRAME_QTY];		// buffer to keep current video frame 
	int m_iFrameBusy;	// index to Frame ready to compress.
	int m_iFrameFree;	// index to Frame ready to fill.
	int m_iFrameCount;	// how many busy frames are there?
};
extern CAVIThread g_AVIThread;

//********************************************************

struct CTaksiProcess
{
	// Info about the Process this DLL is bound to.
	// The current application owning a graphics device.
	// What am i doing to the process?
public:
	CTaksiProcess()
		: m_dwConfigChangeCount(0)
		, m_pCustomConfig(NULL)
		, m_bIsProcessSpecial(false)
		, m_bStopGraphXMode(false)
	{
		m_Stats.InitProcStats();
		m_Stats.m_dwProcessId = ::GetCurrentProcessId();
		m_Stats.m_szLastError[0] = '\0';
	}

	bool IsProcPrime() const
	{
		return( m_Stats.m_dwProcessId == sg_ProcStats.m_dwProcessId );
	}

	bool CheckProcessMaster() const;
	bool CheckProcessSpecial() const;
	void CheckProcessCustom();

	void StopGraphXMode();
	void DetachGraphXMode();
	HRESULT AttachGraphXMode();

	bool OnDllProcessAttach();
	bool OnDllProcessDetach();

	int MakeFileName( TCHAR* pszFileName, const TCHAR* pszExt );
	void UpdateStat( TAKSI_PROCSTAT_TYPE eProp );

public:
	CTaksiProcStats m_Stats;	// For display in the Taksi.exe app.
	TCHAR m_szProcessTitleNoExt[_MAX_PATH]; // use as prefix for captured files.

	HINSTANCE m_hProc;		// handle to the process i'm attached to.
	HANDLE m_hHeap;			// the process heap to allocate on for me.

	CTaksiConfigCustom* m_pCustomConfig; // custom config for this app/process.
	DWORD m_dwConfigChangeCount;		// my last reconfig check id.

	// if set to true, then CBT should not take any action at all.
	bool m_bIsProcessSpecial;		// Is Master TAKSI.EXE or special app.
	bool m_bStopGraphXMode;			// I'm not the main app anymore.
};
extern CTaksiProcess g_Proc;

//*******************************************************

// Globals
extern HINSTANCE g_hInst;	// the DLL instance handle for the process.

// perf measure tool
#if 0 // def _DEBUG
#define CLOCK_START(v) TIMEFAST_t tClock_##v = ::GetPerformanceCounter();
#define CLOCK_STOP(v,pszMsg) LOG_MSG(( pszMsg, ::GetPerformanceCounter() - tClock_##v ));
#else
#define CLOCK_START(v)
#define CLOCK_STOP(v,pszMsg)
#endif

#endif // _INC_TAKSIDLL
