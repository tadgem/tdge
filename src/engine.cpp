#include "engine.h"
#include <iostream>
/// GPU Adapter request callbacks
#ifndef __EMSCRIPTEN__
// TODO: move to another file
wgpu::Adapter request_adapter(wgpu::Instance &instance,
                              const wgpu::RequestAdapterOptions &options)
{
    struct Result {
        WGPUAdapter adapter = nullptr;
        bool success = false;
    };

    Result result;
    instance.RequestAdapter(
        &options,
        [](WGPURequestAdapterStatus status,
           WGPUAdapter adapter,
           const char *msg,
           void *user_data) {
            Result *res = reinterpret_cast<Result *>(user_data);
            if (status == WGPURequestAdapterStatus_Success) {
                res->adapter = adapter;
                res->success = true;
            } else {
                std::cerr << "Failed to get WebGPU adapter: " << msg << std::endl;
            }
        },
        &result);

    if (!result.success) {
        throw std::runtime_error("Failed to get WebGPU adapter");
    }

    return wgpu::Adapter::Acquire(result.adapter);
}

wgpu::Device request_device(wgpu::Adapter &adapter, const wgpu::DeviceDescriptor &options)
{
    struct Result {
        WGPUDevice device = nullptr;
        bool success = false;
    };

    Result result;
    adapter.RequestDevice(
        &options,
        [](WGPURequestDeviceStatus status,
           WGPUDevice device,
           const char *msg,
           void *user_data) {
            Result *res = reinterpret_cast<Result *>(user_data);
            if (status == WGPURequestDeviceStatus_Success) {
                res->device = device;
                res->success = true;
            } else {
                std::cerr << "Failed to get WebGPU device: " << msg << std::endl;
            }
        },
        &result);

    if (!result.success) {
        throw std::runtime_error("Failed to get WebGPU device");
    }

    return wgpu::Device::Acquire(result.device);
}
#endif


void tdg::engine::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "Failed to init SDL: " << SDL_GetError() << "\n";
        return;
    }

    gpu.window = SDL_CreateWindow("SDL2 + WebGPU",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          starting_window_width,
                                          starting_window_height,
                                          0);

    tdg::engine::gpu.instance = wgpu::CreateInstance();
    tdg::engine::gpu.present_surface =
        wgpu::Surface::Acquire(sdl2GetWGPUSurface(tdg::engine::gpu.instance.Get(), gpu.window));

#ifdef EMSCRIPTEN
    // The adapter/device request has already been done for us in the TypeScript code
    // when running in Emscripten
    tdg::engine::gpu.device = wgpu::Device::Acquire(emscripten_webgpu_get_device());
#else
    wgpu::RequestAdapterOptions adapter_options = {};
    adapter_options.compatibleSurface = tdg::engine::gpu.present_surface;
    adapter_options.powerPreference = wgpu::PowerPreference::HighPerformance;
    tdg::engine::gpu.adapter = request_adapter(tdg::engine::gpu.instance, adapter_options);

    wgpu::DeviceDescriptor device_options = {};
    tdg::engine::gpu.device = request_device(tdg::engine::gpu.adapter, device_options);
#endif

    tdg::engine::gpu.device.SetUncapturedErrorCallback(
        [](WGPUErrorType type, const char *msg, void *data) {
            std::cout << "WebGPU Error: " << msg << "\n" << std::flush;
#ifdef EMSCRIPTEN
            emscripten_cancel_main_loop();
            emscripten_force_exit(1);
#endif
            std::exit(1);
        },
        nullptr);

    tdg::engine::gpu.queue = tdg::engine::gpu.device.GetQueue();

    wgpu::SurfaceConfiguration surface_config;
    surface_config.device = tdg::engine::gpu.device;
    surface_config.format = wgpu::TextureFormat::BGRA8Unorm;
    surface_config.width = starting_window_width;
    surface_config.height = starting_window_height;

    tdg::engine::gpu.present_surface.Configure(&surface_config);
}
