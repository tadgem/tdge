#pragma once
#include <SDL.h>
#include "sdl2webgpu.h"
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#endif

#include <webgpu/webgpu_cpp.h>


namespace tdg {
class engine {
public:

    struct wgpu_state
    {
        wgpu::Instance  instance;
        wgpu::Adapter   adapter;
        wgpu::Device    device;
        wgpu::Queue     queue;

        wgpu::Surface   present_surface;

        SDL_Window*     window;
    };

    inline static wgpu_state gpu;

    static constexpr int starting_window_width = 1280;
    static constexpr int starting_window_height = 720;

    static void init();
};
}