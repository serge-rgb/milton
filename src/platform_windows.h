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


// The returns value mean different things, but other than that, we're ok
#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#define HEAP_BEGIN_ADDRESS NULL
#ifndef NDEBUG
#undef HEAP_BEGIN_ADDRESS
#define HEAP_BEGIN_ADDRESS (LPVOID)(1024LL * 1024 * 1024 * 1024)
#endif
#define allocate_big_chunk_of_memory(total_memory_size) VirtualAlloc(HEAP_BEGIN_ADDRESS, \
								     (total_memory_size),\
								     MEM_COMMIT | MEM_RESERVE, \
								     PAGE_READWRITE) \

void win32_log(char *format, ...);
#define milton_log win32_log

void win32_log(char *format, ...)
{
	char message[ 128 ];

	int num_bytes_written = 0;

	va_list args;

	assert ( format );

	va_start( args, format );

	num_bytes_written = _vsnprintf(message, sizeof( message ) - 1, format, args);

	if ( num_bytes_written > 0 )
	{
            OutputDebugStringA( message );
	}

	va_end( args );
}

int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
	milton_main();
}
