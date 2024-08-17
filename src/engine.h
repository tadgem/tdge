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

    template<typename _Ty>
    static wgpu::Buffer create_buffer(_Ty* data, size_t data_count, wgpu::BufferUsage usage)
    {
        wgpu::BufferDescriptor buffer_desc;
        buffer_desc.mappedAtCreation = true;
        buffer_desc.size = data_count * sizeof(_Ty);
        buffer_desc.usage = usage;
        wgpu::Buffer buf = tdg::engine::gpu.device.CreateBuffer(&buffer_desc);
        std::memcpy(buf.GetMappedRange(), data, buffer_desc.size);
        buf.Unmap();
        return buf;
    }
};
}