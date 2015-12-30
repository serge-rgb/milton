/**
 * Hello there!
 * */

#define MILTON_TABLET_EVT_QUEUE_SIZE 512

// NOTE
//  Since we are using NSEvent to get tablet data, we are forcing OSX >= 10.4

extern void milton_osx_tablet_hook(NSEvent* event);
