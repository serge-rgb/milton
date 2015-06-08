// win32_wacom_defines.h
// (c) Copyright 2015 by Sergio Gonzalez

// A lot of this is taken from the Wacom samples.

#pragma once


#include <wintab.h>
#define PACKETDATA	(PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE)
#define PACKETMODE	PK_BUTTONS
#include <pktdef.h>

// Function pointers to Wintab functions exported from wintab32.dll.
typedef UINT ( API * WTINFOA ) ( UINT, UINT, LPVOID );
typedef HCTX ( API * WTOPENA )( HWND, LPLOGCONTEXTA, BOOL );
typedef BOOL ( API * WTGETA ) ( HCTX, LPLOGCONTEXT );
typedef BOOL ( API * WTSETA ) ( HCTX, LPLOGCONTEXT );
typedef BOOL ( API * WTCLOSE ) ( HCTX );
typedef BOOL ( API * WTENABLE ) ( HCTX, BOOL );
typedef BOOL ( API * WTPACKET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( API * WTOVERLAP ) ( HCTX, BOOL );
typedef BOOL ( API * WTSAVE ) ( HCTX, LPVOID );
typedef BOOL ( API * WTCONFIG ) ( HCTX, HWND );
typedef HCTX ( API * WTRESTORE ) ( HWND, LPVOID, BOOL );
typedef BOOL ( API * WTEXTSET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( API * WTEXTGET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( API * WTQUEUESIZESET ) ( HCTX, int );
typedef int  ( API * WTDATAPEEK ) ( HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  ( API * WTPACKETSGET ) (HCTX, int, LPVOID);
typedef HMGR ( API * WTMGROPEN ) ( HWND, UINT );
typedef BOOL ( API * WTMGRCLOSE ) ( HMGR );
typedef HCTX ( API * WTMGRDEFCONTEXT ) ( HMGR, BOOL );
typedef HCTX ( API * WTMGRDEFCONTEXTEX ) ( HMGR, UINT, BOOL );

// Loaded WinAPI functions
typedef struct WacomAPI_s
{
    WTINFOA             WTInfoA;
    WTOPENA             WTOpenA;
    WTGETA              WTGetA;
    WTSETA              WTSetA;
    WTCLOSE             WTClose;
    WTPACKET            WTPacket;
    WTENABLE            WTEnable;
    WTOVERLAP           WTOverlap;
    WTSAVE              WTSave;
    WTCONFIG            WTConfig;
    WTRESTORE           WTRestore;
    WTEXTSET            WTExtSet;
    WTEXTGET            WTExtGet;
    WTQUEUESIZESET      WTQueueSizeSet;
    WTDATAPEEK          WTDataPeek;
    WTPACKETSGET        WTPacketsGet;
    WTMGROPEN           WTMgrOpen;
    WTMGRCLOSE          WTMgrClose;
    WTMGRDEFCONTEXT     WTMgrDefContext;
    WTMGRDEFCONTEXTEX   WTMgrDefContextEx;
} WacomAPI;

