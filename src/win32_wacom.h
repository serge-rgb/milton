#pragma once

#define GETPROCADDRESS(type, func) \
	win_state->wacom.func = (type)GetProcAddress(win_state->wintab_handle, #func); \
        if (!win_state->wacom.func) { \
                assert(!"Could not load function pointer from dll"); return false; }

static bool32 load_wintab(Win32State* win_state)
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

    return true;
}
#undef GETPROCADDRESS
