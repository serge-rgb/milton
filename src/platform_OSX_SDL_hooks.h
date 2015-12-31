/**
 * Hello there!
 * */

#pragma once


// NOTE
//  Since we are using NSEvent to get tablet data, we are forcing OSX >= 10.4

#ifdef __cplusplus
extern "C" {
#endif

void     milton_osx_tablet_hook(/* NSEvent* */void* event);
float*   milton_osx_poll_pressures(int* out_num_pressures);

#ifdef __cplusplus
}
#endif
