//
// TaksiVersion.h
// Put version info into a small file of its own 
// so as not to impact the version control system to much as this changes more than any other file.
#pragma once

// NOTE: Dont forget to change this in the MSI config as well!!
#define TAKSI_VERSION_N			0x0776
#ifdef _DEBUG
#define TAKSI_VERSION_S			"0.776 (Debug)"
#else
#define TAKSI_VERSION_S			"0.776"
#endif
// for VS_VERSION_INFO VERSIONINFO 
#define TAKSI_VERSION_RES_N		0,7,7,6
#define TAKSI_VERSION_RES_S		"0,7,7,6"

#define TAKSI_COPYRIGHT "Parts Copyright©2004 Anton Jouline (Juce) and Copyright©2006 Dennis Robinson (Menace)\0"
