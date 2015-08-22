//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.



// A lot of this is taken from the Wacom samples.

#pragma once


#include <wintab.h>

#define PACKETDATA PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE
#define PACKETMODE PK_BUTTONS

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

