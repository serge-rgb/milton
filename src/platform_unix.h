
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

struct TabletState_s {
    // List of all input devices for window.
    XDeviceInfoPtr input_devices;

    // Pointer to wacom device.
    XDeviceInfoPtr wacom_device;
};

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

// References:
//  - GDK gdkinput-x11.c
//  - Wine winex11.drv/wintab.c
void unix_find_wacom(TabletState* tablet_state, Display* disp)
{
    assert (disp);

    milton_log("Scanning for wacom device\n");
    i32 count;
    XDeviceInfoPtr devices = (XDeviceInfoPtr) XListInputDevices(disp, &count);
    if (devices)
    {
        tablet_state->input_devices = devices;
        for (int i = 0; i < count; ++i)
        {
            milton_log("Device[%d] -- %s\n", i, devices[i].name);
            if (strstr(devices[i].name, "stylus"))
            {
                milton_log("[FOUND] ^--This one seems to be the one.\n");
                tablet_state->wacom_device = &devices[i];
            }
        }
        if (tablet_state->wacom_device)
        {
            XAnyClassPtr class_ptr = tablet_state->wacom_device->inputclassinfo;
            for (int i = 0; i < tablet_state->wacom_device->num_classes; ++i)
            {
                switch(class_ptr->class)
                {
                case ValuatorClass:
                    {
                        XValuatorInfo *xvi = (XValuatorInfo *)class_ptr;
                        break;
                    }
                }
                class_ptr = (XAnyClassPtr) ((u8*)class_ptr + class_ptr->length);
            }
        }
    }
}

void platform_wacom_init(TabletState* tablet_state, SDL_Window* window)
{
    {
        SDL_SysWMinfo sysinfo;
        SDL_GetVersion(&sysinfo.version);
        if (SDL_GetWindowWMInfo(window, &sysinfo))
        {
            Display* disp = sysinfo.info.x11.display;
            unix_find_wacom(tablet_state, disp);
        }
    }
}

void platform_wacom_deinit(TabletState* tablet_state)
{
    if (tablet_state->input_devices)
    {
        XFreeDeviceList(tablet_state->input_devices);
    }
}


int main(int argc, char** argv)
{
    milton_main();
}
