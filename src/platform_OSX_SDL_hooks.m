// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.

// Notes:
//  This is a hack. It works but it ain't exactly pretty.
//  It won't work if SDL is linked dynamically.
//

#define MILTON_TABLET_EVT_QUEUE_SIZE 512


struct MiltonPressureQueue {
    int count;
    float pressures[MILTON_TABLET_EVT_QUEUE_SIZE];
};

struct MiltonPressureQueue g_milton_tablet_pressures;

float* milton_osx_poll_pressures(int* out_num_pressures)
{
    *out_num_pressures = 0;
    float* pressures = NULL;
    if (g_milton_tablet_pressures.count != 0) {
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
    g_milton_tablet_pressures.pressures[g_milton_tablet_pressures.count++] = pressure;
    assert ( g_milton_tablet_pressures.count < MILTON_TABLET_EVT_QUEUE_SIZE );
}
