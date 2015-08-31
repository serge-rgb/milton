
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


#ifdef __linux__
#define _GNU_SOURCE  // To get MAP_ANONYMOUS on linux
#define __USE_MISC 1  // MAP_ANONYMOUS and MAP_NORESERVE dont' get defined without this
#include <sys/mman.h>
#undef __USE_MISC

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#if 0
#include <X11/extensions/XIproto.h>
#include <X11/keysym.h>
#endif

#elif defined(__MACH__)
#include <sys/mman.h>
#else
#error "This is not the Unix you're looking for"
#endif

#define HEAP_BEGIN_ADDRESS NULL
#if 0
#ifndef NDEBUG
#undef HEAP_BEGIN_ADDRESS
#define HEAP_BEGIN_ADDRESS (void*)(1024LL * 1024 * 1024 * 1024)
#endif
#endif

#ifndef MAP_ANONYMOUS

#ifndef MAP_ANON
#error "MAP_ANONYMOUS flag not found!"
#else
#define MAP_ANONYMOUS MAP_ANON
#endif

#endif // MAP_ANONYMOUS

#define platform_allocate(size) unix_allocate((size))

#define platform_deallocate(pointer) unix_deallocate((pointer)); {(pointer) = NULL;}

func void milton_fatal(char* message);
#define milton_log printf

#ifdef __linux__
#define platform_load_gl_func_pointers()
#elif __MACH__

#define platform_load_gl_func_pointers()
#endif

#ifdef __linux__
struct TabletState_s {
    // Handle to the X11 window
    Display* display;

    // List of all input devices for window.
    XDeviceInfoPtr input_devices;

    // Wacom X11 Device
    XDevice*        wacom_device;
    XDeviceInfoPtr  wacom_device_info;

    u32             motion_type;
    XEventClass     event_classes[1024];
    u32             num_event_classes;

    i32 min_pressure;
    i32 max_pressure;

};
#elif __MACH__
struct TabletState_s
{
    // TODO: implement OSX
    int foo;
};
#endif

typedef struct UnixMemoryHeader_s
{
    size_t size;
} UnixMemoryHeader;

func void* unix_allocate(size_t size)
{
    void* ptr = mmap(HEAP_BEGIN_ADDRESS, size + sizeof(UnixMemoryHeader),
                     PROT_WRITE | PROT_READ,
                     MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (ptr)
    {
        *((UnixMemoryHeader*)ptr) = (UnixMemoryHeader)
        {
            .size = size,
        };
        ptr += sizeof(UnixMemoryHeader);
    }
    return ptr;
}

func void unix_deallocate(void* ptr)
{
    assert(ptr);
    void* begin = ptr - sizeof(UnixMemoryHeader);
    size_t size = *((size_t*)begin);
    munmap(ptr, size);
}

func NativeEventResult platform_native_event_poll(TabletState* tablet_state, SDL_SysWMEvent event,
                                                  i32 width, i32 height,
                                                  v2i* out_point,
                                                  f32* out_pressure)
{
    NativeEventResult caught_event = Caught_NONE;
#ifdef __linux__
    if (event.type == SDL_SYSWMEVENT)
    {
        if (event.msg)
        {
            SDL_SysWMmsg msg = *event.msg;
            if (msg.subsystem == SDL_SYSWM_X11)
            {
                XEvent xevent = msg.msg.x11.event;

                if (xevent.type == tablet_state->motion_type)
                {
                    XDeviceMotionEvent* dme = (XDeviceMotionEvent*)(&xevent);
#if 0
                    milton_log("========\n");
                    milton_log("Axes_count %d\n", dme->axes_count);
                    milton_log ("xevent type %d\n", xevent.type);
                    for (u8 i = 0; i < dme->axes_count; ++i)
                    {
                        milton_log ("xevent axis[%d] %d\n", i, dme->axis_data[i]);
                    }
#endif
                    *out_pressure = (f32)dme->axis_data[2] /
                            (f32)(tablet_state->max_pressure - tablet_state->min_pressure);
                    caught_event |= Caught_PRESSURE;
                }
            }
        }
    }
// TODO: IMPLEMENT OSX
#endif // __linux__
    return caught_event;
}
// References:
//  - GDK gdkinput-x11.c
//  - Wine winex11.drv/wintab.c
//  - http://www.x.org/archive/X11R7.5/doc/man/man3/XOpenDevice.3.html
func void platform_wacom_init(TabletState* tablet_state, SDL_Window* window)
{
#ifdef __linux__
    // Tell SDL we want system events, to get the pressure
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

    SDL_SysWMinfo sysinfo;
    SDL_GetVersion(&sysinfo.version);

    Display* disp = NULL;

    if (SDL_GetWindowWMInfo(window, &sysinfo))
    {
        disp = sysinfo.info.x11.display;
    }
    else
    {
        return;
    }

    tablet_state->display = disp;

    milton_log("Scanning for wacom device...\n");
    i32 count;
    XDeviceInfoPtr devices = (XDeviceInfoPtr) XListInputDevices(disp, &count);
    if (devices)
    {
        tablet_state->input_devices = devices;
        for (int i = 0; i < count; ++i)
        {
            milton_log("Device[%d] -- %s\n", i, devices[i].name);
            if (  //==== Not a single codebase does anything smarter.
                strstr(devices[i].name, "stylus")
                // || strstr(devices[i].name, "eraser")
                )

            {
                milton_log("[FOUND] ^--This one seems to be the one.\n");
                tablet_state->wacom_device_info = &devices[i];
                if (tablet_state->wacom_device_info)
                {
                    tablet_state->wacom_device =
                            XOpenDevice(disp, tablet_state->wacom_device_info->id);

                    // This reeeealy shouldn't fail at this point...
                    assert (tablet_state->wacom_device);

                    XAnyClassPtr class_ptr = tablet_state->wacom_device_info->inputclassinfo;
                    for (int i = 0; i < tablet_state->wacom_device_info->num_classes; ++i)
                    {
                        switch(class_ptr->class)
                        {
                        case ValuatorClass:
                            {
                                XValuatorInfo *xvi = (XValuatorInfo *)class_ptr;
                                milton_log("Valuator. Num axes: %d\n", xvi->num_axes);
                                for (int axis = 0; axis < xvi->num_axes; axis++)
                                {
                                    milton_log ("axis %d, min: %d, max: %d\n",
                                                axis,
                                                xvi->axes[axis].min_value,
                                                xvi->axes[axis].max_value);
                                    // 2 is pressure. Seems to be the convention... *cross fingers*
                                    if (axis == 2)
                                    {
                                        tablet_state->min_pressure = xvi->axes[i].min_value;
                                        tablet_state->max_pressure = xvi->axes[i].max_value;
                                        break;
                                    }
                                }
                                milton_log("[DEBUG] Wacom pressure range: [%d, %d]\n",
                                           tablet_state->min_pressure,
                                           tablet_state->max_pressure);
                                XEventClass cls;
                                DeviceMotionNotify(tablet_state->wacom_device,
                                                   tablet_state->motion_type,
                                                   cls);
                                if (cls)
                                {
                                    tablet_state->event_classes[tablet_state->num_event_classes++] = cls;
                                }

 // Other stuff I might be interested in.
#if 0
                                DeviceButton1Motion  (tablet_state->wacom_device, 0, cls);
                                if (cls)
                                {
                                    tablet_state->event_classes[tablet_state->num_event_classes++] = cls;
                                }
                                DeviceButton1Motion  (tablet_state->wacom_device, 0, cls);
                                if (cls)
                                {
                                    tablet_state->event_classes[tablet_state->num_event_classes++] = cls;
                                }
                                DeviceButton1Motion  (tablet_state->wacom_device, 0, cls);
                                if (cls)
                                {
                                    tablet_state->event_classes[tablet_state->num_event_classes++] = cls;
                                }
                                DeviceButtonMotion  (tablet_state->wacom_device, 0, cls);
                                if (cls)
                                {
                                    tablet_state->event_classes[tablet_state->num_event_classes++] = cls;
                                }
#endif

                                milton_log("[DEBUG] Registered motion type %d\n",
                                           tablet_state->motion_type);
                                break;
                            }
                        }
                        class_ptr = (XAnyClassPtr) ((u8*)class_ptr + class_ptr->length);
                    }

                    XSelectExtensionEvent(tablet_state->display,
                                          DefaultRootWindow(tablet_state->display),
                                          tablet_state->event_classes,
                                          tablet_state->num_event_classes);
                }
            }
        }
    }
// TODO: implement OSX
#endif // __linux__
}

void platform_wacom_deinit(TabletState* tablet_state)
{
#ifdef __linux__
    if (tablet_state->input_devices)
    {
        XFreeDeviceList(tablet_state->input_devices);
    }
#endif
// TODO: implement osx
}


int main(int argc, char** argv)
{
    milton_main();
}
