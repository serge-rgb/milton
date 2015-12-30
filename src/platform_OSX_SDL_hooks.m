/**
 *
 */

#include <stdio.h>

struct MiltonTabletEvent {
    int x;
    int y;
    float pressure;
};

struct MiltonTabletEventQueue {
    int head;
    int tail;
    struct MiltonTabletEvent events[MILTON_TABLET_EVT_QUEUE_SIZE];
};

void milton_osx_tablet_hook(NSEvent* event)
{
    exit(-1);
}
