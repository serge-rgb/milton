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


#pragma once

#define GETPROCADDRESS(type, func) \
	tablet_state->wacom.func = (type)GetProcAddress(tablet_state->wintab_handle, #func); \
        if (!tablet_state->wacom.func) { \
                assert(!"Could not load function pointer from dll"); return false; }

#define wacom_func_check(f) if (!f) {\
    OutputDebugStringA("WARNING: Wacom API incomplete\n."); \
    tablet_state->wintab_handle = 0; \
    return false; \
}

#include "define_types.h"

func b32 win32_wacom_load_wintab(TabletState* tablet_state)
{
    tablet_state->wintab_handle = LoadLibraryA( "Wintab32.dll" );

    if ( !tablet_state->wintab_handle )
    {
        OutputDebugStringA("WARNING: Wacom DLL not found. Proceeding.");
        return false;
    }

    // Explicitly find the exported Wintab functions in which we are interested.
    // We are using the ASCII, not unicode versions (where applicable).
    GETPROCADDRESS( WTOPENA, WTOpenA );
    GETPROCADDRESS( WTINFOA, WTInfoA );
    GETPROCADDRESS( WTGETA, WTGetA );
    GETPROCADDRESS( WTSETA, WTSetA );
    GETPROCADDRESS( WTPACKET, WTPacket );
    GETPROCADDRESS( WTCLOSE, WTClose );
    GETPROCADDRESS( WTENABLE, WTEnable );
    GETPROCADDRESS( WTOVERLAP, WTOverlap );
    GETPROCADDRESS( WTSAVE, WTSave );
    GETPROCADDRESS( WTCONFIG, WTConfig );
    GETPROCADDRESS( WTRESTORE, WTRestore );
    GETPROCADDRESS( WTEXTSET, WTExtSet );
    GETPROCADDRESS( WTEXTGET, WTExtGet );
    GETPROCADDRESS( WTQUEUESIZESET, WTQueueSizeSet );
    GETPROCADDRESS( WTDATAPEEK, WTDataPeek );
    GETPROCADDRESS( WTPACKETSGET, WTPacketsGet );
    GETPROCADDRESS( WTMGROPEN, WTMgrOpen );
    GETPROCADDRESS( WTMGRCLOSE, WTMgrClose );
    GETPROCADDRESS( WTMGRDEFCONTEXT, WTMgrDefContext );
    GETPROCADDRESS( WTMGRDEFCONTEXTEX, WTMgrDefContextEx );

    wacom_func_check (tablet_state->wacom.WTInfoA);
    wacom_func_check (tablet_state->wacom.WTOpenA);
    wacom_func_check (tablet_state->wacom.WTGetA);
    wacom_func_check (tablet_state->wacom.WTSetA);
    wacom_func_check (tablet_state->wacom.WTClose);
    wacom_func_check (tablet_state->wacom.WTPacket);
    wacom_func_check (tablet_state->wacom.WTEnable);
    wacom_func_check (tablet_state->wacom.WTOverlap);
    wacom_func_check (tablet_state->wacom.WTSave);
    wacom_func_check (tablet_state->wacom.WTConfig);
    wacom_func_check (tablet_state->wacom.WTRestore);
    wacom_func_check (tablet_state->wacom.WTExtSet);
    wacom_func_check (tablet_state->wacom.WTExtGet);
    wacom_func_check (tablet_state->wacom.WTQueueSizeSet);
    wacom_func_check (tablet_state->wacom.WTDataPeek);
    wacom_func_check (tablet_state->wacom.WTPacketsGet);
    wacom_func_check (tablet_state->wacom.WTMgrOpen);
    wacom_func_check (tablet_state->wacom.WTMgrClose);
    wacom_func_check (tablet_state->wacom.WTMgrDefContext);
    wacom_func_check (tablet_state->wacom.WTMgrDefContextEx);

    return true;
}

func void win32_wacom_get_context(TabletState* tablet_state)
{
    if ( !tablet_state->wintab_handle )
    {
        return;
    }

    WacomAPI* wacom = &tablet_state->wacom;

    AXIS range_x           = { 0 };
    AXIS range_y           = { 0 };
    AXIS pressure          = { 0 };

    UINT res;

    for(UINT ctx_i = 0; ; ++ctx_i)
    {
        LOGCONTEXT log_context = { 0 };

        int found = wacom->WTInfoA(WTI_DDCTXS + ctx_i, 0, &log_context);
        if (!found)
        {
            break;
        }

        res = wacom->WTInfoA(WTI_DEVICES + ctx_i, DVC_X, &range_x );
        if (res != sizeof(AXIS))
        {
            continue;
        }

        res = wacom->WTInfoA(WTI_DEVICES + ctx_i, DVC_Y, &range_y );
        if (res != sizeof(AXIS))
        {
            continue;
        }
        res = wacom->WTInfoA( WTI_DEVICES + ctx_i, DVC_NPRESSURE, &pressure );

        if (res != sizeof(AXIS))
        {
            continue;
        }
        log_context.lcPktData = PACKETDATA;
        log_context.lcOptions |= CXO_SYSTEM;    // Move cursor
        log_context.lcOptions |= CXO_MESSAGES;
        log_context.lcPktMode = PACKETMODE;
        log_context.lcMoveMask = PACKETDATA;
        log_context.lcBtnUpMask = log_context.lcBtnDnMask;

        log_context.lcInOrgX = 0;
        log_context.lcInOrgY = 0;
        log_context.lcInExtX = range_x.axMax;
        log_context.lcInExtY = range_y.axMax;

        // Raymond Chen says that by definition, a primary monitor has its origin at (0,0)
        // We will configure the context to map the wacom to the primary monitor.

        // Since we are defaulting to the primary monitor, we don't need this,
        // but it's nice to know so that in the future we can introduce
        // different options
#if 0
        HMONITOR monitor_handle = MonitorFromPoint((POINT){0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO monitor_info;
        GetMonitorInfo(monitor_handle, &monitor_info);

        RECT monitor_dim = monitor_info.rcMonitor;
#endif

#if 1
        log_context.lcOutOrgX = 0;
        log_context.lcOutOrgY = 0;
        // Size of the primary display.
        log_context.lcOutExtX = GetSystemMetrics( SM_CXSCREEN );
        log_context.lcOutExtY = -GetSystemMetrics( SM_CYSCREEN );

        log_context.lcSysOrgX = 0;
        log_context.lcSysOrgY = 0;
        log_context.lcSysExtX = GetSystemMetrics( SM_CXSCREEN );
        log_context.lcSysExtY = GetSystemMetrics( SM_CYSCREEN );
        // From the sample code: The whole virtual screen
#else
        log_context.lcOutOrgX = GetSystemMetrics( SM_XVIRTUALSCREEN );
        log_context.lcOutOrgY = GetSystemMetrics( SM_YVIRTUALSCREEN );
        log_context.lcOutExtX = GetSystemMetrics( SM_CXVIRTUALSCREEN );
        log_context.lcOutExtY = -GetSystemMetrics( SM_CYVIRTUALSCREEN );
#endif

        HCTX ctx = wacom->WTOpenA(tablet_state->window, &log_context, TRUE);

        if ( ctx )
        {
            tablet_state->wacom_ctx = ctx;
            tablet_state->wacom_prs_max = pressure.axMax;
            milton_log("[DEBUG]: Wacom context locked and loaded!\n");

            // TODO: Samples does this. Is it necessary?
            if (!tablet_state->wacom.WTInfoA(0, 0, NULL))
            {
                tablet_state->wintab_handle = 0;
            }

            return;
        }
    }

    tablet_state->wintab_handle = 0;
}

#undef GETPROCADDRESS
