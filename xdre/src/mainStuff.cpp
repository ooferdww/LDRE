/* LDRE: Initialization
 * Authors: ooferdww */

#include "SDL.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_sdl2.h"

// opengl for windows, sdl2 for other platforms
#ifdef _WIN32
#include "SDL_opengl.h"
#include "../imgui/backends/imgui_impl_opengl3.h"
#else
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include "../imgui/backends/imgui_impl_sdlrenderer2.h"
#endif

#include "xdre.hpp"
#include "MainWindow.hpp"

#ifndef PACKAGE_STRING
#define PACKAGE_STRING "LDRE beta"
#endif

extern "C" {
    extern SDL_Window *sdl_window;
}

int main(int argc, char** argv) {

#ifdef _WIN32
    // anchor working dir to folder with ldre executable
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0) {
        char* lastSlash = strrchr(exePath, '\\');
        if (!lastSlash) lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            SetCurrentDirectoryA(exePath);
        }
    }
#else
    // anchor working dir to folder with ldre executable
    char exePath[PATH_MAX] = "";
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        char* lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            if (chdir(exePath) != 0) {}
        }
    }

    // force x11/xwayland for reliable window communication
    setenv("SDL_VIDEODRIVER", "x11", 0);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
#endif

    // init doom
    xdre::init(argc, argv);

#ifdef _WIN32

    // windows loop

    // set opengl attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    MainWindow mainWindow(argc, argv);

    int guiX = mainWindow.GetGuiPosX();
    int guiY = mainWindow.GetGuiPosY();

    // if out of bounds, center window
    if (guiX < 0 || guiY < 0 || guiX > 30000 || guiY > 30000) {
        guiX = SDL_WINDOWPOS_CENTERED;
        guiY = SDL_WINDOWPOS_CENTERED;
    }

    // build gui window
    SDL_Window* guiWindow = SDL_CreateWindow(
        PACKAGE_STRING,
        guiX,
        guiY,
        780,
        700,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_GLContext glContext = SDL_GL_CreateContext(guiWindow);
    SDL_GL_MakeCurrent(guiWindow, glContext);
    SDL_GL_SetSwapInterval(1);

    // init imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // match 35 fps ticker rate
    io.KeyRepeatDelay = 0.18f;
    io.KeyRepeatRate  = 1.0f / 35.0f;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;

    ImGui_ImplSDL2_InitForOpenGL(guiWindow, glContext);
    ImGui_ImplOpenGL3_Init("#version 130");
	
    // enable sdl text events
    SDL_StartTextInput();

    // restore dsda position
    if (sdl_window) {
        int const dX = mainWindow.GetDoomPosX();
        int const dY = mainWindow.GetDoomPosY();
        if (dX >= 0 && dY >= 0 && dX < 30000 && dY < 30000) {
            SDL_SetWindowPosition(sdl_window, dX, dY);
        }
    }

    // main loop
    while (mainWindow.IsRunning()) {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 16)) {
            do {
                ImGui_ImplSDL2_ProcessEvent(&event);

                if (event.type == SDL_QUIT) {
                    mainWindow.RequestExit();
                } else if (event.type == SDL_WINDOWEVENT) {
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        // check if guiwindow or sdl_close was triggered
                        Uint32 winId = event.window.windowID;
                        if (winId == SDL_GetWindowID(guiWindow) || (sdl_window && winId == SDL_GetWindowID(sdl_window))) {
                            mainWindow.RequestExit();
                        }
                    } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                               event.window.event == SDL_WINDOWEVENT_LEAVE) {
                        ImGui::GetIO().ClearInputKeys();
                        ImGui::GetIO().InputQueueCharacters.clear();
                    }
                }
            } while (SDL_PollEvent(&event));
        }

        // advance tick
        xdre::doSdlEvents();

        // render imgui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        mainWindow.Render();

        ImGui::Render();
        SDL_GL_MakeCurrent(guiWindow, glContext);
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // non-blocking swap
        SDL_GL_SwapWindow(guiWindow);
    }

    // save final window coordinates (space if minimized)
    if (guiWindow && !(SDL_GetWindowFlags(guiWindow) & SDL_WINDOW_MINIMIZED)) {
        int gx, gy;
        SDL_GetWindowPosition(guiWindow, &gx, &gy);
        if (gx >= 0 && gy >= 0) {
            mainWindow.SetGuiPos(gx, gy);
        }
    }

    if (sdl_window && !(SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_MINIMIZED)) {
        int dx, dy;
        SDL_GetWindowPosition(sdl_window, &dx, &dy);
        if (dx >= 0 && dy >= 0) {
            mainWindow.SetDoomPos(dx, dy);
        }
    }

    mainWindow.WriteConfig();

    // cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(guiWindow);

    return 0;
}
#else

    // other platforms loop

    MainWindow mainWindow(argc, argv);

    int guiX = mainWindow.GetGuiPosX();
    int guiY = mainWindow.GetGuiPosY();

    // if out of bounds, center window
    if (guiX < 0 || guiY < 0 || guiX > 30000 || guiY > 30000) {
        guiX = SDL_WINDOWPOS_CENTERED;
        guiY = SDL_WINDOWPOS_CENTERED;
    }

    // build gui window, software renderer
    SDL_Window* guiWindow = SDL_CreateWindow(
        PACKAGE_STRING,
        guiX,
        guiY,
        780,
        700,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_Renderer* guiRenderer = SDL_CreateRenderer(guiWindow, -1, SDL_RENDERER_SOFTWARE);

    // init imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // match 35 fps ticker rate
    io.KeyRepeatDelay = 0.18f;
    io.KeyRepeatRate  = 1.0f / 35.0f;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;

    ImGui_ImplSDL2_InitForSDLRenderer(guiWindow, guiRenderer);
    ImGui_ImplSDLRenderer2_Init(guiRenderer);

    // enable sdl text events
    SDL_StartTextInput();

    // restore dsda position
    if (sdl_window) {
        int const dX = mainWindow.GetDoomPosX();
        int const dY = mainWindow.GetDoomPosY();
        if (dX >= 0 && dY >= 0 && dX < 30000 && dY < 30000) {
            SDL_SetWindowPosition(sdl_window, dX, dY);
        }
    }

    // main loop
    while (mainWindow.IsRunning()) {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 16)) {
            do {
                ImGui_ImplSDL2_ProcessEvent(&event);

                if (event.type == SDL_QUIT) {
                    mainWindow.RequestExit();
                } else if (event.type == SDL_WINDOWEVENT) {
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        Uint32 winId = event.window.windowID;
                        if (winId == SDL_GetWindowID(guiWindow) || (sdl_window && winId == SDL_GetWindowID(sdl_window))) {
                            mainWindow.RequestExit();
                        }
                    } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                               event.window.event == SDL_WINDOWEVENT_LEAVE) {
                        ImGui::GetIO().ClearInputKeys();
                        ImGui::GetIO().InputQueueCharacters.clear();
                    }
                }
            } while (SDL_PollEvent(&event));
        }

        // advance tick
        xdre::doSdlEvents();

        // render imgui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        mainWindow.Render();

        ImGui::Render();
        SDL_SetRenderDrawColor(guiRenderer, 26, 26, 31, 255);
        SDL_RenderClear(guiRenderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), guiRenderer);

        SDL_RenderPresent(guiRenderer);
    }

    // save final window coordinates (space if minimized)
    if (guiWindow && !(SDL_GetWindowFlags(guiWindow) & SDL_WINDOW_MINIMIZED)) {
        int gx, gy;
        SDL_GetWindowPosition(guiWindow, &gx, &gy);
        if (gx >= 0 && gy >= 0) {
            mainWindow.SetGuiPos(gx, gy);
        }
    }

    if (sdl_window && !(SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_MINIMIZED)) {
        int dx, dy;
        SDL_GetWindowPosition(sdl_window, &dx, &dy);
        if (dx >= 0 && dy >= 0) {
            mainWindow.SetDoomPos(dx, dy);
        }
    }

    mainWindow.WriteConfig();

    // cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(guiRenderer);
    SDL_DestroyWindow(guiWindow);

    return 0;
}
#endif
