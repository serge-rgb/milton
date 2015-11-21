// ImGui GLFW binding with OpenGL3 + shaders
// You can copy and use unmodified imgui_impl_* files in your project.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// See main.cpp for an example of using this.
// https://github.com/ocornut/imgui

struct GLFWwindow;

IMGUI_API bool        ImGui_ImplSDLGL3_Init();
IMGUI_API void        ImGui_ImplSDLGL3_Shutdown();
void ImGui_ImplSDLGL3_NewFrame(int w, int h,  // Window size
                               int display_w, int display_h  // Framebuffer size
                               );

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void        ImGui_ImplSDLGL3_InvalidateDeviceObjects();
IMGUI_API bool        ImGui_ImplSDLGL3_CreateDeviceObjects();

/* // GLFW callbacks (installed by default if you enable 'install_callbacks' during initialization) */
/* // Provided here if you want to chain callbacks. */
/* // You can also handle inputs yourself and use those as a reference. */
/* IMGUI_API void        ImGui_ImplSDLGL3_MouseButtonCallback(int button, int action, int mods); */
/* IMGUI_API void        ImGui_ImplSDLGL3_ScrollCallback(double xoffset, double yoffset); */
/* IMGUI_API void        ImGui_ImplSDLGL3_KeyCallback(int key, int scancode, int action, int mods); */
/* IMGUI_API void        ImGui_ImplSDLGL3_CharCallback(unsigned int c); */
