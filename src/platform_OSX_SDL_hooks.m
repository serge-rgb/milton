// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

// Notes:
//  This is a hack. It works but it ain't exactly pretty.
//  It won't work if SDL is linked dynamically.
//

#define MILTON_TABLET_EVT_QUEUE_SIZE 512


struct MiltonPressureQueue
{
    int count;
    float pressures[MILTON_TABLET_EVT_QUEUE_SIZE];
};

struct MiltonPressureQueue g_milton_tablet_pressures;

float* milton_osx_poll_pressures(int* out_num_pressures)
{
    *out_num_pressures = 0;
    float* pressures = NULL;
    if (g_milton_tablet_pressures.count != 0)
    {
        *out_num_pressures = g_milton_tablet_pressures.count;

        pressures = g_milton_tablet_pressures.pressures;
        g_milton_tablet_pressures.count = 0;
    }
    return pressures;
}

void milton_osx_tablet_hook(void* event_)
{
    NSEvent* event = (NSEvent*)event_;
    float pressure = [event pressure];
    if ( g_milton_tablet_pressures.count < MILTON_TABLET_EVT_QUEUE_SIZE );
    {
        g_milton_tablet_pressures.pressures[g_milton_tablet_pressures.count++] = pressure;
    }
}
