#include "shader.h"
#include "engine.h"
#include "utils.h"
#include "tree_sitter/langs.h"
#include "tree_sitter/api.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>

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

/// Reflection

struct TokenMapping
{
    std::string token_type;
    std::string token_src;
};

static std::vector<std::string> g_relevant_token_types =
{
        "identifier",
        "attribute",
        "parameter",
        "int_literal",
        "type_declaration",
        "struct_declaration",
        "struct_member",
        "struct",
        "ERROR"
};

static constexpr bool g_enable_token_filtering = true;

static const std::string g_non_generic_alias_regex_str = "alias ([a-zA-Z0-9]*) = ([a-zA-Z0-9]*);";
static const std::string g_generic_alias_regex_str = "alias ([a-zA-Z0-9]*) = ([a-zA-Z0-9]*)<([a-zA-Z0-9]*)>";

static std::regex        g_non_generic_alias_regex(g_non_generic_alias_regex_str);
static std::regex        g_generic_alias_regex(g_generic_alias_regex_str);

struct Parser {
    struct type {
        std::string type_name;
        uint32_t type_size;
    };

    struct alias {
        std::string alias_name;
        uint32_t type_index;
    };

    struct generic_type
    {
        std::string generic_name;
        uint32_t    size_multi;
        uint32_t    size_offset;
    };

    std::unordered_map<std::string, type>           concrete_types;
    std::unordered_map<std::string, generic_type>   generic_types;
    std::unordered_map<std::string, alias>          aliases;

    Parser()
    {
        fill_default_types();
    }

    bool        is_generic_type(std::string& candidate)
    {
        bool is_generic = generic_types.contains(candidate);

        // also check generic regex
        return is_generic;
    }

    std::vector<std::string>    get_alias_matches(std::string& candidate)
    {
        auto non_generic_matches    = tdg::utils::get_matches(candidate, g_non_generic_alias_regex);
        auto generic_matches        = tdg::utils::get_matches(candidate, g_generic_alias_regex);

        if(!non_generic_matches.empty())
        {
            return non_generic_matches;
        }

        if(!generic_matches.empty())
        {
            return generic_matches;
        }

        return {};
    }

    std::string get_inner_generic_type(std::string& candidate)
    {

    }

    void fill_default_types()
    {
        concrete_types.emplace("i32", type{"i32", 4});
        concrete_types.emplace("u32", type{"u32", 4});
        concrete_types.emplace("f32", type{"f32", 4});
        concrete_types.emplace("f16", type{"f16", 2});
        concrete_types.emplace("bool", type{"bool", 1});

        generic_types.emplace("vec2", generic_type { "vec2", 2, 0});
        generic_types.emplace("vec3", generic_type { "vec3", 3, 0});
        generic_types.emplace("vec4", generic_type { "vec4", 4, 0});

        generic_types.emplace("mat2", generic_type { "mat2", 4, 0});
        generic_types.emplace("mat3", generic_type { "mat3", 9, 0});
        generic_types.emplace("mat4", generic_type { "mat4", 16, 0});

    }

    void handle_alias(std::vector<std::string> matches)
    {
        std::cout << "making an alias boiiii\n";

    }


    void parse(std::vector<TokenMapping> tokens)
    {
        for (int head = 0; head < tokens.size(); head++)
        {
            auto&   token = tokens[head];
            // Aliases
            {
                auto alias_matches = get_alias_matches(token.token_src);
                // found a type alias
                if (!alias_matches.empty()) {
                    handle_alias(alias_matches);
                }
            }

            // Non generic user-types
            {

            }
            // generic user types
            {

            }

        }

    }

};

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

void walk_tree(TSNode node, std::string& original_source, std::vector<TokenMapping>& tokens)
{
    std::string node_type(ts_node_type(node));
    std::string node_src = get_source_from_node(node, original_source);
    node_src = strip_formatting(node_src);

    if(g_enable_token_filtering)
    {
        if(std::find(
                g_relevant_token_types.begin(),
                g_relevant_token_types.end(),
                node_type) != g_relevant_token_types.end())
        {
            std::string node_src    = get_source_from_node(node,original_source);
            tokens.push_back({node_type, node_src});
        }
    }
    else
    {
        std::string node_src    = get_source_from_node(node,original_source);
        tokens.push_back({node_type, node_src});
    }


    for (auto i = 0; i < ts_node_child_count(node); i++)
    {
        walk_tree(ts_node_child(node, i), original_source, tokens);
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

    std::vector<TokenMapping> tokens {};

    walk_tree(root_node, original_source, tokens);

    for(auto& token : tokens)
    {
        std::cout << "Type : " << token.token_type << ", Src: " << token.token_src << "\n";
    }

    Parser token_parser {};
    token_parser.parse(tokens);

    return {};
}