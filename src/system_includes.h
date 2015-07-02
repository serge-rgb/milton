#ifdef _WIN32
#pragma warning(push, 0)
#endif

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#endif

// Platform independent includes:
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// Local includes
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/wglew.h>
#include <SDL.h>

#ifdef _WIN32
#pragma warning(pop)
#endif

