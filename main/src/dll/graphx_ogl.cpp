//
// graphx_ogl.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include "graphx.h"

#include <GL/gl.h>
#include "../common/glext.h"	// GL_MAX_TEXTURE_UNITS_ARB

CTaksiOGL g_OGL;

//**************************************************************************

static PFNGLACTIVETEXTUREARBPROC s_glActiveTextureARB = NULL;
static GLint s_iMaxTexUnits = 0;	// <= COUNTOF(s_TextureUnitNums)

static const GLenum s_TextureUnitNums[32] = 
{
	GL_TEXTURE0_ARB,  GL_TEXTURE1_ARB,  GL_TEXTURE2_ARB,  GL_TEXTURE3_ARB,
	GL_TEXTURE4_ARB,  GL_TEXTURE5_ARB,  GL_TEXTURE6_ARB,  GL_TEXTURE7_ARB,
	GL_TEXTURE8_ARB,  GL_TEXTURE9_ARB,  GL_TEXTURE10_ARB, GL_TEXTURE11_ARB,
	GL_TEXTURE12_ARB, GL_TEXTURE13_ARB, GL_TEXTURE14_ARB, GL_TEXTURE15_ARB,
	GL_TEXTURE16_ARB, GL_TEXTURE17_ARB, GL_TEXTURE18_ARB, GL_TEXTURE19_ARB,
	GL_TEXTURE20_ARB, GL_TEXTURE21_ARB, GL_TEXTURE22_ARB, GL_TEXTURE23_ARB,
	GL_TEXTURE24_ARB, GL_TEXTURE25_ARB, GL_TEXTURE26_ARB, GL_TEXTURE27_ARB,
	GL_TEXTURE28_ARB, GL_TEXTURE29_ARB, GL_TEXTURE30_ARB, GL_TEXTURE31_ARB,
};

//*****************************************************
// declare function pointer types for the OGL exported function.
// function pointers to allow explicit run-time linking.

#define GRAPHXOGLFUNC(a,b,c) \
	typedef WINGDIAPI b (APIENTRY * PFN##a) c;\
	static PFN##a s_##a = NULL;
#include "GraphX_OGL.TBL"
#undef GRAPHXOGLFUNC

//*****************************************************

bool CTaksiOGL::DrawIndicator( TAKSI_INDICATE_TYPE eIndicate )
{
	// Draws indicator in the top-left corner. 
	//DEBUG_TRACE(( "Drawing indicator..." LOG_CR));

	if ( ! m_bTestTextureCaps )
	{
		m_bTestTextureCaps = true;

		// Get OpenGL version and extensions info 
		char* pszVer = (char*)s_glGetString(GL_VERSION);
		if (pszVer == NULL)
		{
			ASSERT(0);
			return false;
		}
		DEBUG_TRACE(("OpenGL Version: %s" LOG_CR, pszVer));

		char* pszExt = (char*)s_glGetString(GL_EXTENSIONS);
		if (pszExt == NULL )
		{
			ASSERT(0);
			return false;
		}
		DEBUG_TRACE(("OpenGL Extensions: %s" LOG_CR, pszExt));
		if ( strstr(pszExt, "GL_ARB_multitexture"))
		{
			s_glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)s_wglGetProcAddress("glActiveTextureARB");
			DEBUG_TRACE(( "s_glActiveTextureARB = %08x" LOG_CR, (UINT_PTR)s_glActiveTextureARB ));

			s_glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &s_iMaxTexUnits);
			DEBUG_TRACE(( "Maximum texture units: %d" LOG_CR, s_iMaxTexUnits ));
		}
		else
		{
			s_glActiveTextureARB = NULL;
		}
	}

	s_glPushAttrib(GL_ALL_ATTRIB_BITS);

#define INDIC_SIZE 32
	s_glViewport(0, g_Proc.m_Stats.m_SizeWnd.cy-INDIC_SIZE, INDIC_SIZE, INDIC_SIZE);
	s_glMatrixMode(GL_PROJECTION);
	s_glPushMatrix();
	s_glLoadIdentity();
	s_glOrtho(0.0, INDIC_SIZE, 0.0, INDIC_SIZE, -1.0, 1.0);
	s_glMatrixMode(GL_MODELVIEW);
	s_glPushMatrix();
	s_glLoadIdentity();

	s_glDisable(GL_LIGHTING);
	s_glDisable(GL_TEXTURE_1D);
	if (s_glActiveTextureARB)
	{
		for (int i=0; i<s_iMaxTexUnits; i++)
		{
			//DEBUG_TRACE(("disabling GL_TEXTURE%d_ARB..." LOG_CR, i));
			s_glActiveTextureARB( s_TextureUnitNums[i]);
			s_glDisable(GL_TEXTURE_2D);
		}
	}
	else
	{
		s_glDisable(GL_TEXTURE_2D);
	}
	s_glDisable(GL_DEPTH_TEST);
	s_glShadeModel(GL_FLAT);

	// set appropriate color for the indicator
	// CTaksiGraphX::sm_IndColors
	switch (eIndicate)
	{
	case TAKSI_INDICATE_Recording:
		s_glColor3f(1.0f, 0.26f, 0.0f); // RED: recording mode
		break;
	case TAKSI_INDICATE_Hooked:
		s_glColor3f(0.26f, 0.53f, 1.0f); // BLUE: system wide hooking mode
		break;
	default:
		s_glColor3f(0.53f, 1.0f, 0.0f); // GREEN: normal mode
		break;
	}

	s_glBegin(GL_POLYGON);
		s_glVertex2i(INDICATOR_X, INDICATOR_Y);
		s_glVertex2i(INDICATOR_X, INDICATOR_Y + INDICATOR_Height);
		s_glVertex2i(INDICATOR_X + INDICATOR_Width, INDICATOR_Y + INDICATOR_Height);
		s_glVertex2i(INDICATOR_X + INDICATOR_Width, INDICATOR_Y);
	s_glEnd();

	// black outline
	s_glColor3f(0.0, 0.0, 0.0);
	s_glBegin(GL_LINE_LOOP);
		s_glVertex2i(INDICATOR_X, INDICATOR_Y);
		s_glVertex2i(INDICATOR_X, INDICATOR_Y + INDICATOR_Height);
		s_glVertex2i(INDICATOR_X + INDICATOR_Width, INDICATOR_Y + INDICATOR_Height);
		s_glVertex2i(INDICATOR_X + INDICATOR_Width, INDICATOR_Y);
	s_glEnd();

	s_glMatrixMode(GL_PROJECTION);
	s_glPopMatrix();
	s_glMatrixMode(GL_MODELVIEW);
	s_glPopMatrix();

	s_glPopAttrib();
	return true;
}

HWND CTaksiOGL::GetFrameInfo( SIZE& rSize ) // virtual
{
	HDC hDC = s_wglGetCurrentDC();
	HWND hWnd = ::WindowFromDC(hDC);
	if ( hWnd == NULL )
		return NULL;

	RECT rect;
	if ( ! ::GetClientRect(hWnd, &rect))
		return NULL;

	rSize.cx = rect.right;
	rSize.cy = rect.bottom;
	return hWnd;
}

static void SwapColorComponents(BYTE* rgbBuf, int cx, int cy )
{
	// Swap 'R' and 'B' components for each pixel. 
	// NOTE: image width (in pixels) must be a multiple of 4. 
	DEBUG_TRACE(( "SwapColorComponents: CALLED." LOG_CR ));
	for (int i=0; i<cy; i++)
	{
		for (int j=0; j<cx; j++)
		{
			int k = i*cx*3 + j*3;
			SwapColorsRGB( &rgbBuf[k] );
		}
	}
}

static void GetHalfSizeImage( const BYTE* srcBuf, int width, int height, BYTE* destBuf)
{
	// Makes a minimized image (1/2 size in each direction).
	const int iBPP = 3; // source bytes-per-pixel

	int dWidth = width/2;
	int dHeight = height/2;

	int iSrcPitch = width * iBPP;
	int destPitch = dWidth * iBPP;

	int k=0;
	for (int i=0; i<dHeight, k<height-1; i++, k+=2)
	{
		int m=0;
		for (int j=0; j<dWidth, m<width-1; j++, m+=2)
		{
			/*
			// fetch 1 pixel: nearest neigbour
			destBuf[i*destPitch + j*3] = srcBuf[k*iSrcPitch + m*iBPP];
			destBuf[i*destPitch + j*3 + 1] = srcBuf[k*iSrcPitch + m*iBPP + 1];
			destBuf[i*destPitch + j*3 + 2] = srcBuf[k*iSrcPitch + m*iBPP + 2];
			*/

			// fetch 4 pixels: bilinear interpolation
			BYTE temp = (srcBuf[k*iSrcPitch + m*iBPP] + 
					srcBuf[k*iSrcPitch + (m+1)*iBPP] + 
					srcBuf[(k+1)*iSrcPitch + m*iBPP] +
					srcBuf[(k+1)*iSrcPitch + (m+1)*iBPP]) >> 2;
			destBuf[i*destPitch + j*3] = (BYTE)temp;
			temp = (srcBuf[k*iSrcPitch + m*iBPP + 1] + 
					srcBuf[k*iSrcPitch + (m+1)*iBPP + 1] + 
					srcBuf[(k+1)*iSrcPitch + m*iBPP + 1] +
					srcBuf[(k+1)*iSrcPitch + (m+1)*iBPP + 1]) >> 2;
			destBuf[i*destPitch + j*3 + 1] = (BYTE)temp;
			temp = (srcBuf[k*iSrcPitch + m*iBPP + 2] + 
					srcBuf[k*iSrcPitch + (m+1)*iBPP + 2] + 
					srcBuf[(k+1)*iSrcPitch + m*iBPP + 2] +
					srcBuf[(k+1)*iSrcPitch + (m+1)*iBPP + 2]) >> 2;
			destBuf[i*destPitch + j*3 + 2] = (BYTE)temp;
		}
	}
}

void CTaksiOGL::GetFrameHalfSize(CVideoFrame& frame)
{
	// Gets current frame to be stored in video file.
	// NOTE: Size MUST be 4 byte aligned!!
	if ( m_SurfTemp.m_pPixels == NULL)
	{
		// allocate big buffer for RAW pixel data
		m_SurfTemp.AllocPadXY( g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy );
	}

	// select back buffer
	s_glReadBuffer(GL_BACK);

	// read the pixels data
	s_glReadPixels(0, 0, g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy, GL_RGB, GL_UNSIGNED_BYTE, m_SurfTemp.m_pPixels );

	// minimize the image
	GetHalfSizeImage( m_SurfTemp.m_pPixels, g_Proc.m_Stats.m_SizeWnd.cx, g_Proc.m_Stats.m_SizeWnd.cy, frame.m_pPixels);

	// Adjust pixel encodings: RGB -> BGR
	SwapColorComponents(frame.m_pPixels, g_Proc.m_Stats.m_SizeWnd.cx/2, g_Proc.m_Stats.m_SizeWnd.cy/2);
}

void CTaksiOGL::GetFrameFullSize(CVideoFrame& frame)
{
	// Gets current frame to be stored in video file.
	// select back buffer
	s_glReadBuffer(GL_BACK);

	// read the pixels data
	s_glReadPixels(0, 0, frame.m_Size.cx, frame.m_Size.cy, GL_RGB, GL_UNSIGNED_BYTE, frame.m_pPixels);

	// Adjust pixel encodings: RGB -> BGR
	SwapColorComponents(frame.m_pPixels, frame.m_Size.cx, frame.m_Size.cy);
}

HRESULT CTaksiOGL::GetFrame( CVideoFrame& frame, bool bHalfSize )
{
	if (bHalfSize) 
		GetFrameHalfSize( frame );
	else
		GetFrameFullSize( frame );
	return S_OK;
}

//*******************************************************************

EXTERN_C __declspec(dllexport) BOOL APIENTRY OGL_WglSwapBuffers(HDC hdc)
{
	// New wglSwapBuffers function 
	DEBUG_TRACE(( "--------------------------------" LOG_CR ));
	DEBUG_TRACE(( "OGL_WglSwapBuffers: called." LOG_CR ));

	// put back saved code fragment
	g_OGL.m_Hook.SwapOld( s_wglSwapBuffers);

	// determine how we are going to handle keyboard hot-keys:
	// Use DirectInput, if configured so, 
	// Otherwise use thread-specific keyboard hook.
	g_OGL.PresentFrameBegin(true);

	// call original function
	BOOL bRes = s_wglSwapBuffers(hdc);

	g_OGL.PresentFrameEnd();

	// put JMP instruction again
	g_OGL.m_Hook.SwapReset(s_wglSwapBuffers);
	return bRes;
}

//*******************************************************************

bool CTaksiOGL::HookFunctions()
{
	// Hooks multiple functions 
	// hook wglSwapBuffers, using code modifications at run-time.
	// ALGORITHM: we overwrite the beginning of real wglSwapBuffers
	// with a JMP instruction to our routine (OGL_WglSwapBuffers).
	// When our routine gets called, first thing that it does - it restores 
	// the original bytes of wglSwapBuffers, then performs its pre-call tasks, 
	// then calls wglSwapBuffers, then does post-call tasks, then writes
	// the JMP instruction back into the beginning of wglSwapBuffers, and
	// returns.
	
	if (!IsValidDll())
	{
		return false;
	}
  	
	// initialize function pointers
#define GRAPHXOGLFUNC(a,b,c) s_##a = (PFN##a) GetProcAddress(#a);\
	if ( s_##a == NULL ) \
	{\
		DEBUG_ERR(( "CTaksiOGL::HookFunctions GetProcAddress FAIL" LOG_CR ));\
		return false;\
	}
#include "GraphX_OGL.TBL"
#undef GRAPHXOGLFUNC

	DEBUG_MSG(( "CTaksiOGL::HookFunctions: checking JMP-implant..." LOG_CR));
	if ( ! m_Hook.InstallHook(s_wglSwapBuffers,OGL_WglSwapBuffers))
	{
		return false;
	}

	DEBUG_MSG(( "CTaksiOGL::HookFunctions: done." LOG_CR ));
	return __super::HookFunctions();
}

void CTaksiOGL::UnhookFunctions()
{
	__super::UnhookFunctions();
}

void CTaksiOGL::FreeDll()
{
	// This function is called by the main DLL thread, when
	// the DLL is about to detach. All the memory clean-ups should be done here.
	if ( ! IsValidDll())
		return;

	// restore wglSwapBuffers
	m_Hook.RemoveHook(s_wglSwapBuffers);

	// release pixel buffers
	m_SurfTemp.FreeFrame();

	DEBUG_MSG(( "CTaksiOGL::FreeDll: done." LOG_CR));
	__super::FreeDll();
}
