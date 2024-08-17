//
// Created by liam_ on 8/15/2024.
//

#pragma once
#include <webgpu/webgpu_cpp.h>
#include <vector>

namespace tdg {
class shader {
public:
    struct reflection_data {
        struct binding_data {
        };

        struct vertex_attibute_data {
        };

        struct sampler_data {
        };

        std::vector<binding_data>           descriptor_bindings;
        std::vector<vertex_attibute_data>   vertex_attributes;
        std::vector<sampler_data>           samplers;
    };

    static wgpu::ShaderModule   compile_wgsl(const char* src);
    static reflection_data      reflect_wgsl(const char* src);
};
}