/* LDRE: Initialization
 * Authors: ooferdww */

#include "SDL.h"
#include "SDL_opengl.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_sdl2.h"
#include "../imgui/backends/imgui_impl_opengl3.h"

#include "xdre.hpp"
#include "MainWindow.hpp"

extern "C" {
    extern SDL_Window *sdl_window;
}

int main(int argc, char** argv) {
    // init doom
    xdre::init(argc, argv);

    // set opengl attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    MainWindow mainWindow;

    int guiX = mainWindow.GetGuiPosX();
    int guiY = mainWindow.GetGuiPosY();

    // if out of bounds, center window
    if (guiX < 0 || guiY < 0 || guiX > 30000 || guiY > 30000) {
        guiX = SDL_WINDOWPOS_CENTERED;
        guiY = SDL_WINDOWPOS_CENTERED;
    }

    // build gui window
    SDL_Window* guiWindow = SDL_CreateWindow(
        "LDRE v0.3.1 beta",
        guiX,
        guiY,
        780,
        700,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_GLContext glContext = SDL_GL_CreateContext(guiWindow);
    SDL_GL_MakeCurrent(guiWindow, glContext);
    SDL_GL_SetSwapInterval(0);

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
        while (SDL_PollEvent(&event)) {
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
                }
                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || 
                         event.window.event == SDL_WINDOWEVENT_LEAVE) {
                    ImGui::GetIO().ClearInputKeys();
                    ImGui::GetIO().InputQueueCharacters.clear();
                }
            }
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

    return 0;
}