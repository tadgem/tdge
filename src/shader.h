//
// Created by liam_ on 8/15/2024.
//

#pragma once
#include <webgpu/webgpu_cpp.h>

namespace tdg {
class shader {
public:
    static wgpu::ShaderModule compile_wgsl(const char* src);
};
}