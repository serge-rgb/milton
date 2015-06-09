#pragma once

#define GETPROCADDRESS(type, func) \
	win_state->wacom.func = (type)GetProcAddress(win_state->wintab_handle, #func); \
        if (!win_state->wacom.func) { \
                assert(!"Could not load function pointer from dll"); return false; }

#define wacom_func_check(f) if (!f) {\
    OutputDebugStringA("WARNING: Wacom API incomplete\n."); \
    win_state->wintab_handle = 0; \
    return false; \
}

static bool32 win32_wacom_load_wintab(Win32State* win_state)
{
    win_state->wintab_handle = LoadLibraryA( "Wintab32.dll" );

    if ( !win_state->wintab_handle )
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

    wacom_func_check (win_state->wacom.WTInfoA);
    wacom_func_check (win_state->wacom.WTOpenA);
    wacom_func_check (win_state->wacom.WTGetA);
    wacom_func_check (win_state->wacom.WTSetA);
    wacom_func_check (win_state->wacom.WTClose);
    wacom_func_check (win_state->wacom.WTPacket);
    wacom_func_check (win_state->wacom.WTEnable);
    wacom_func_check (win_state->wacom.WTOverlap);
    wacom_func_check (win_state->wacom.WTSave);
    wacom_func_check (win_state->wacom.WTConfig);
    wacom_func_check (win_state->wacom.WTRestore);
    wacom_func_check (win_state->wacom.WTExtSet);
    wacom_func_check (win_state->wacom.WTExtGet);
    wacom_func_check (win_state->wacom.WTQueueSizeSet);
    wacom_func_check (win_state->wacom.WTDataPeek);
    wacom_func_check (win_state->wacom.WTPacketsGet);
    wacom_func_check (win_state->wacom.WTMgrOpen);
    wacom_func_check (win_state->wacom.WTMgrClose);
    wacom_func_check (win_state->wacom.WTMgrDefContext);
    wacom_func_check (win_state->wacom.WTMgrDefContextEx);

    return true;
}

static void win32_wacom_get_context(Win32State* win_state)
{
    if ( !win_state->wintab_handle )
    {
        return;
    }

    WacomAPI* wacom = &win_state->wacom;

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

        res = wacom->WTInfoA(WTI_DEVICES + ctx_i, DVC_X, &range_y );
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
        log_context.lcOptions |= CXO_MESSAGES;
        log_context.lcOptions |= CXO_SYSTEM;
        log_context.lcPktMode = PACKETMODE;
        log_context.lcMoveMask = PACKETDATA;
        log_context.lcBtnUpMask = log_context.lcBtnDnMask;

#if 0
        log_context.lcInOrgX = 0;
        log_context.lcInOrgY = 0;
        log_context.lcInExtX = range_x.axMax;
        log_context.lcInExtY = range_y.axMax;

        // TODO: If/when we port to unix then we need the equivalent thing here...
        log_context.lcOutOrgX = GetSystemMetrics( SM_XVIRTUALSCREEN );
        log_context.lcOutOrgY = GetSystemMetrics( SM_YVIRTUALSCREEN );
        log_context.lcOutExtX = GetSystemMetrics( SM_CXVIRTUALSCREEN );
        log_context.lcOutExtY = -GetSystemMetrics( SM_CYVIRTUALSCREEN );
#endif

        HCTX ctx = wacom->WTOpenA(win_state->window, &log_context, TRUE);

        if ( ctx )
        {
            win_state->wacom_ctx = ctx;
            win_state->wacom_prs_max = pressure.axMax;
            OutputDebugStringA("GREAT SUCCESS: Wacom context locked and loaded!\n");

            // TODO: Samples does this. Is it necessary?
            if (!win_state->wacom.WTInfoA(0, 0, NULL))
            {
                win_state->wintab_handle = 0;
            }

            return;
        }
    }

    win_state->wintab_handle = 0;
}

#undef GETPROCADDRESS
