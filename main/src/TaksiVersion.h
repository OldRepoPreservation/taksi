//
// TaksiVersion.h
// Put version info into a small file of its own 
// so as not to impact the version control system to much as this changes more than any other file.
// Shared with DLL and EXE
//
#ifndef _INC_TaksiVersion_H
#define _INC_TaksiVersion_H
#if _MSC_VER > 1000
#pragma once
#endif

// NOTE: Dont forget to change this in the MSI config as well!!
#define TAKSI_VERSION_N			0x0778
#ifdef _DEBUG
#define TAKSI_VERSION_S			"0.778 (Debug)"
#else
#define TAKSI_VERSION_S			"0.778"
#endif
// for VS_VERSION_INFO VERSIONINFO 
#define TAKSI_VERSION_RES_N		0,7,7,8
#define TAKSI_VERSION_RES_S		"0,7,7,8"

#define TAKSI_COPYRIGHT "Parts Copyright©2004 Anton Jouline (Juce) and Copyright©2008 Dennis Robinson (Menace)\0"
#endif // _INC_TaksiVersion_H
