#include "shader.h"
#include "engine.h"
#include "tree_sitter/langs.h"
#include "tree_sitter/api.h"
#include <iostream>
#include <algorithm>

wgpu::ShaderModule tdg::shader::compile_wgsl(const char *src)
{
    wgpu::ShaderModule shader_module;
    {
        wgpu::ShaderModuleWGSLDescriptor shader_module_wgsl;
        shader_module_wgsl.code = src;

        wgpu::ShaderModuleDescriptor shader_module_desc;
        shader_module_desc.nextInChain = &shader_module_wgsl;
        shader_module = tdg::engine::gpu.device.CreateShaderModule(&shader_module_desc);

        shader_module.GetCompilationInfo(
            [](WGPUCompilationInfoRequestStatus status,
               WGPUCompilationInfo const *info,
               void *) {
                if (info->messageCount != 0) {
                    std::cerr << "Shader compilation info:\n";
                    for (uint32_t i = 0; i < info->messageCount; ++i) {
                        const auto &m = info->messages[i];
                        std::cout << m.lineNum << ":" << m.linePos << ": ";
                        switch (m.type) {
                        case WGPUCompilationMessageType_Error:
                            std::cout << "error";
                            break;
                        case WGPUCompilationMessageType_Warning:
                            std::cout << "warning";
                            break;
                        case WGPUCompilationMessageType_Info:
                            std::cout << "info";
                            break;
                        default:
                            break;
                        }

                        std::cout << ": " << m.message << "\n";
                    }
                }
            },
            nullptr);
    }

    return shader_module;
}


std::string get_source_from_node(TSNode node, std::string& original_source)
{
    auto start_byte = ts_node_start_byte(node);
    return original_source.substr(start_byte, ts_node_end_byte(node) - start_byte);
}

std::string strip_formatting(std::string& input)
{
    std::string t = input;
    std::replace(t.begin(), t.end(), '\n', ' ');
    return t;
}

void walk_tree(TSNode node, std::string& original_source)
{
    std::string node_type(ts_node_type(node));
    std::string node_src = get_source_from_node(node, original_source);
    node_src = strip_formatting(node_src);

    std::cout << "tdge : shader_reflect : " << node_src << "\n";

    for (auto i = 0; i < ts_node_child_count(node); i++)
    {
        walk_tree(ts_node_child(node, i), original_source);
    }

}


tdg::shader::reflection_data tdg::shader::reflect_wgsl(const char* src)
{
    // Create a parser.
    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_wgsl());

    TSTree* source_tree = ts_parser_parse_string(
        parser, NULL, src, static_cast<uint32_t>(strlen(src)));

    TSNode root_node = ts_tree_root_node(source_tree);
    std::string original_source = std::string(src);
    walk_tree(root_node, original_source);
    return {};
}