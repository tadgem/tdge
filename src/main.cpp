#include <array>
#include <iostream>
#include "engine.h"
#include "shader.h"
#include "arcball_camera.h"
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include "embedded_files.h"



struct AppState {
    wgpu::RenderPipeline    render_pipeline;
    wgpu::Buffer            vertex_buf;
    wgpu::Buffer            view_param_buf;
    wgpu::BindGroup         bind_group;
    ArcballCamera           camera;
    glm::mat4               proj;

    bool                    done = false;
    bool                    camera_changed = true;
    glm::vec2               prev_mouse = glm::vec2(-2.f);
};

uint32_t win_width = 1280;
uint32_t win_height = 720;

glm::vec2 transform_mouse(glm::vec2 in)
{
    return glm::vec2(in.x * 2.f / win_width - 1.f, 1.f - 2.f * in.y / win_height);
}

void app_loop(void *_app_state);

int main(int argc, const char **argv)
{
    AppState *app_state = new AppState;

    tdg::engine::init();
    const char* wgsl_src = reinterpret_cast<const char *>(triangle_wgsl);
    wgpu::ShaderModule              shader_module   = tdg::shader::compile_wgsl(wgsl_src);
    tdg::shader::reflection_data    reflect_data    = tdg::shader::reflect_wgsl(wgsl_src);

    // Upload vertex data
    const std::vector<float> vertex_data = {
        1,  -1, 0, 1,  // position
        1,  0,  0, 1,  // color
        -1, -1, 0, 1,  // position
        0,  1,  0, 1,  // color
        0,  1,  0, 1,  // position
        0,  0,  1, 1,  // color
    };
    app_state->vertex_buf = tdg::engine::create_buffer(vertex_data.data(), vertex_data.size(), wgpu::BufferUsage::Vertex);

    std::array<wgpu::VertexAttribute, 2> vertex_attributes;
    vertex_attributes[0].format = wgpu::VertexFormat::Float32x4;
    vertex_attributes[0].offset = 0;
    vertex_attributes[0].shaderLocation = 0;

    vertex_attributes[1].format = wgpu::VertexFormat::Float32x4;
    vertex_attributes[1].offset = 4 * 4;
    vertex_attributes[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vertex_buf_layout;
    vertex_buf_layout.arrayStride = 2 * 4 * 4;
    vertex_buf_layout.attributeCount = vertex_attributes.size();
    vertex_buf_layout.attributes = vertex_attributes.data();

    wgpu::VertexState vertex_state;
    vertex_state.module = shader_module;
    vertex_state.entryPoint = "vertex_main";
    vertex_state.bufferCount = 1;
    vertex_state.buffers = &vertex_buf_layout;

    wgpu::ColorTargetState render_target_state;
    render_target_state.format = wgpu::TextureFormat::BGRA8Unorm;

    wgpu::FragmentState fragment_state;
    fragment_state.module = shader_module;
    fragment_state.entryPoint = "fragment_main";
    fragment_state.targetCount = 1;
    fragment_state.targets = &render_target_state;

    wgpu::BindGroupLayoutEntry view_param_layout_entry = {};
    view_param_layout_entry.binding = 0;
    view_param_layout_entry.buffer.hasDynamicOffset = false;
    view_param_layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    view_param_layout_entry.visibility = wgpu::ShaderStage::Vertex;

    wgpu::BindGroupLayoutDescriptor view_params_bg_layout_desc = {};
    view_params_bg_layout_desc.entryCount = 1;
    view_params_bg_layout_desc.entries = &view_param_layout_entry;

    wgpu::BindGroupLayout view_params_bg_layout =
        tdg::engine::gpu.device.CreateBindGroupLayout(&view_params_bg_layout_desc);

    wgpu::PipelineLayoutDescriptor pipeline_layout_desc = {};
    pipeline_layout_desc.bindGroupLayoutCount = 1;
    pipeline_layout_desc.bindGroupLayouts = &view_params_bg_layout;

    wgpu::PipelineLayout pipeline_layout =
        tdg::engine::gpu.device.CreatePipelineLayout(&pipeline_layout_desc);

    wgpu::RenderPipelineDescriptor render_pipeline_desc;
    render_pipeline_desc.vertex = vertex_state;
    render_pipeline_desc.fragment = &fragment_state;
    render_pipeline_desc.layout = pipeline_layout;
    // Default primitive state is what we want, triangle list, no indices

    app_state->render_pipeline = tdg::engine::gpu.device.CreateRenderPipeline(&render_pipeline_desc);

    // Create the UBO for our bind group
    wgpu::BufferDescriptor ubo_buffer_desc;
    ubo_buffer_desc.mappedAtCreation = false;
    ubo_buffer_desc.size = 16 * sizeof(float);
    ubo_buffer_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    app_state->view_param_buf = tdg::engine::gpu.device.CreateBuffer(&ubo_buffer_desc);

    wgpu::BindGroupEntry view_param_bg_entry = {};
    view_param_bg_entry.binding = 0;
    view_param_bg_entry.buffer = app_state->view_param_buf;
    view_param_bg_entry.size = ubo_buffer_desc.size;

    wgpu::BindGroupDescriptor bind_group_desc = {};
    bind_group_desc.layout = view_params_bg_layout;
    bind_group_desc.entryCount = 1;
    bind_group_desc.entries = &view_param_bg_entry;

    app_state->bind_group = tdg::engine::gpu.device.CreateBindGroup(&bind_group_desc);

    app_state->proj = glm::perspective(
        glm::radians(50.f), static_cast<float>(win_width) / win_height, 0.1f, 100.f);
    app_state->camera = ArcballCamera(glm::vec3(0, 0, -2.5), glm::vec3(0), glm::vec3(0, 1, 0));

#ifdef EMSCRIPTEN
    emscripten_set_main_loop_arg(app_loop, app_state, -1, 0);
#else
    while (!app_state->done) {
        app_loop(app_state);
    }
    SDL_DestroyWindow(tdg::engine::gpu.window);
    SDL_Quit();
#endif

    return 0;
}

void app_loop(void *_app_state)
{
    AppState *app_state = reinterpret_cast<AppState *>(_app_state);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            app_state->done = true;
        } else if (event.type == SDL_MOUSEMOTION) {
            const glm::vec2 cur_mouse =
                transform_mouse(glm::vec2(event.motion.x, event.motion.y));
            if (event.motion.state & SDL_BUTTON_LMASK) {
                app_state->camera.rotate(app_state->prev_mouse, cur_mouse);
                app_state->camera_changed = true;
            } else if (event.motion.state & SDL_BUTTON_RMASK) {
                app_state->camera.pan(cur_mouse - app_state->prev_mouse);
                app_state->camera_changed = true;
            }
            app_state->prev_mouse = cur_mouse;
        } else if (event.type == SDL_MOUSEWHEEL) {
#ifdef EMSCRIPTEN
            // We get wheel values at ~10x smaller values in Emscripten environment,
            // so just apply a scale factor to make it match native scroll
            float wheel_scale = 10.f;
#else
            float wheel_scale = 1.f;
#endif
            app_state->camera.zoom(-event.wheel.preciseY * 0.005f * wheel_scale);
            app_state->camera_changed = true;
        }
    }

    wgpu::Buffer upload_buf;
    if (app_state->camera_changed) {
        wgpu::BufferDescriptor upload_buffer_desc;
        upload_buffer_desc.mappedAtCreation = true;
        upload_buffer_desc.size = 16 * sizeof(float);
        upload_buffer_desc.usage = wgpu::BufferUsage::CopySrc;
        upload_buf = tdg::engine::gpu.device.CreateBuffer(&upload_buffer_desc);

        const glm::mat4 proj_view = app_state->proj * app_state->camera.transform();

        std::memcpy(
            upload_buf.GetMappedRange(), glm::value_ptr(proj_view), 16 * sizeof(float));
        upload_buf.Unmap();
    }

    wgpu::SurfaceTexture surface_texture;
    tdg::engine::gpu.present_surface.GetCurrentTexture(&surface_texture);

    wgpu::TextureViewDescriptor texture_view_desc;
    texture_view_desc.format = surface_texture.texture.GetFormat();
    texture_view_desc.dimension = wgpu::TextureViewDimension::e2D;
    texture_view_desc.mipLevelCount = 1;
    texture_view_desc.arrayLayerCount = 1;

    wgpu::RenderPassColorAttachment color_attachment;

    color_attachment.view = surface_texture.texture.CreateView(&texture_view_desc);
    color_attachment.clearValue.r = 0.f;
    color_attachment.clearValue.g = 0.f;
    color_attachment.clearValue.b = 0.f;
    color_attachment.clearValue.a = 1.f;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor pass_desc;
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;

    wgpu::CommandEncoder encoder = tdg::engine::gpu.device.CreateCommandEncoder();

    if (app_state->camera_changed) {
        encoder.CopyBufferToBuffer(
            upload_buf, 0, app_state->view_param_buf, 0, 16 * sizeof(float));
    }

    wgpu::RenderPassEncoder render_pass_enc = encoder.BeginRenderPass(&pass_desc);
    render_pass_enc.SetPipeline(app_state->render_pipeline);
    render_pass_enc.SetVertexBuffer(0, app_state->vertex_buf);
    render_pass_enc.SetBindGroup(0, app_state->bind_group);
    render_pass_enc.Draw(3);
    render_pass_enc.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    tdg::engine::gpu.queue.Submit(1, &commands);

#ifndef __EMSCRIPTEN__
    tdg::engine::gpu.present_surface.Present();
#endif
    app_state->camera_changed = false;
}
