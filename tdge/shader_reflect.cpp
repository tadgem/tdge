#include "shader.h"
#include "utils.h"
#include "tree_sitter/langs.h"
#include "tree_sitter/api.h"
#include <algorithm>
#include <unordered_map>
#include <iostream>
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
        "uniform",
        "parameter",
        "int_literal",
        "type_declaration",
        "struct_declaration",
        "struct_member",
        "struct",
        "sampler",
        "texture_2d",
        "texture_3d",
        "ERROR",
        "{",
        "}",
};

static constexpr bool g_enable_token_filtering = true;

static const std::string g_non_generic_alias_regex_str = "alias ([a-zA-Z0-9]*) = ([a-zA-Z0-9]*)";
static const std::string g_generic_alias_regex_str = "alias ([a-zA-Z0-9]*) = ([a-zA-Z0-9]*)<([a-zA-Z0-9]*)>";

static std::regex        g_non_generic_alias_regex(g_non_generic_alias_regex_str);
static std::regex        g_generic_alias_regex(g_generic_alias_regex_str);

struct Parser {
    struct type {
        std::string type_name;
        uint32_t type_size;
    };

    struct generic_type
    {
        std::string generic_name;
        uint32_t    size_multi;
        uint32_t    size_offset;
    };

    struct struct_member {
        std::string name;
        std::string type;
        std::string attribute_name;
        std::string attribute_value;
        uint32_t    member_size;
        uint32_t    member_offset;
    };

    struct user_type {
        std::string                 type_name;
        std::vector<struct_member>  members;
        uint32_t                    type_size;
    };

    struct uniform
    {
        uint16_t    set;
        uint16_t    binding;
        std::string name;
        std::string type;
    };

    struct sampler
    {
        uint16_t        set;
        uint16_t        binding;
        std::string     name;

    };

    struct texture
    {
        uint16_t set;
        uint16_t binding;
        uint16_t dim;
        std::string name;
        std::string type;
    };

    std::unordered_map<std::string, std::string>    aliases;
    std::unordered_map<std::string, type>           concrete_types;
    std::unordered_map<std::string, generic_type>   generic_types;
    std::unordered_map<std::string, user_type>      user_types;
    std::unordered_map<std::string, uniform>        uniforms;
    std::unordered_map<std::string, sampler>        samplers;
    std::unordered_map<std::string, texture>        textures;
    std::string                                     vertex_input_structure;


    Parser()
    {
        fill_default_types();
    }

    std::vector<std::string>    get_alias_matches(std::string& candidate)
    {
        auto non_generic_matches    = tdg::utils::get_matches(candidate, g_non_generic_alias_regex);
        auto generic_matches        = tdg::utils::get_matches(candidate, g_generic_alias_regex);

        if(!generic_matches.empty())
        {
            return generic_matches;
        }

        if(!non_generic_matches.empty())
        {
            return non_generic_matches;
        }


        return {};
    }

    void handle_generic_alias(std::string alias_name, std::string generic_name, std::string concrete_name)
    {
        if(generic_types.find(generic_name) == generic_types.end())
        {
            std::cerr << "Unknown generic type : " << generic_name << std::endl;
            return;
        }

        auto& generic_type = generic_types[generic_name];

        if(concrete_types.find(concrete_name) == concrete_types.end())
        {
            std::cerr << "Unknown concrete type : " << concrete_name << std::endl;
            return;
        }

        std::string fully_qualified_generic_name = generic_name + "<" + concrete_name + ">";

        if (concrete_types.find(fully_qualified_generic_name) != concrete_types.end())
        {
            aliases.emplace(alias_name, fully_qualified_generic_name);
            return;
        }

        auto& concrete_type = concrete_types[concrete_name];

        type new_alias_type = {};
        new_alias_type.type_name = alias_name;
        new_alias_type.type_size = (concrete_type.type_size * generic_type.size_multi) + generic_type.size_offset;
        concrete_types.emplace(alias_name, new_alias_type);
    }

    void fill_default_types()
    {
        concrete_types.emplace("i32", type{"i32", 4});
        concrete_types.emplace("u32", type{"u32", 4});
        concrete_types.emplace("f32", type{"f32", 4});
        concrete_types.emplace("f16", type{"f16", 2});
        concrete_types.emplace("bool", type{"bool", 1});


        generic_types.emplace("vec2", generic_type{"vec2", 2, 0});
        generic_types.emplace("vec3", generic_type{"vec3", 3, 0});
        generic_types.emplace("vec4", generic_type{"vec4", 4, 0});

        generic_types.emplace("mat2x2", generic_type{"mat2x2", 4, 0});
        generic_types.emplace("mat3x3", generic_type{"mat3x3", 9, 0});
        generic_types.emplace("mat4x4", generic_type{"mat4x4", 16, 0});

        generic_types.emplace("texture_2d", generic_type{"texture_2d", 1, 0});
        generic_types.emplace("texture_3d", generic_type{"texture_2d", 1, 0});

        auto base_concrete_types = concrete_types;

        for (auto &[generic_name, generic_type] : generic_types) {
            for (auto &[concrete_name, concrete_type] : base_concrete_types) {
                std::string alias_name = generic_name  + "<" +
                                         concrete_name + ">";

                concrete_types.emplace(
                    alias_name, type {
                        alias_name, (concrete_type.type_size * generic_type.size_multi) +
                                        generic_type.size_offset});
            }
        }
        
        aliases.emplace("vec2f", "vec2<f32>");
        aliases.emplace("vec3f", "vec3<f32>");
        aliases.emplace("vec4f", "vec4<f32>");
    }

    void handle_concrete_alias(std::string alias_name, std::string concrete_name)
    {
        if (concrete_types.find(concrete_name) == concrete_types.end())
        {
            std::cerr << "Unknown concrete type : " << concrete_name << std::endl;
            return;
        }
        aliases.emplace(alias_name, concrete_name);
    }

    void handle_alias(std::vector<std::string> matches)
    {
        if (matches.size() == 4) {
            // generic
            auto alias_name = matches[1];
            auto generic_name = matches[2];
            auto concrete_name = matches[3];
            handle_generic_alias(alias_name, generic_name, concrete_name);
        } else if (matches.size() == 3) {
            // non generic
            auto alias_name = matches[1];
            auto alias_type = matches[2];
            handle_concrete_alias(alias_name, alias_type);
        } else {
            std::cerr << "Invalid format for alias detected" << std::endl;
        }
    }

    uint32_t get_type_size(std::string name)
    {
        if (user_types.find(name) != user_types.end())
        {
            return user_types[name].type_size;
        }

        if (aliases.find(name) != aliases.end())
        {
            return concrete_types[aliases[name]].type_size;
        }

        if (concrete_types.find(name) != concrete_types.end())
        {
            return concrete_types[name].type_size;
        }

        return UINT32_MAX;
    }

    void handle_new_usertype(std::vector<TokenMapping>& tokens, int& head)
    {
        head++;
        if(tokens[head].token_type != "identifier")
        {
            std::cerr << "struct declared with no name" << std::endl;
            return;
        }
        type new_usertype = {tokens[head].token_src, 0};
        std::vector<struct_member> members{};

        auto next_token = tokens[head];
        // while we have members to reflect in this struct
        // TODO: This isnt catching the struct end
        uint32_t offset_counter = 0;
        while(next_token.token_type != "}")
        {
            // DO stuff
            if(next_token.token_type == "struct_member")
            {
                auto next_token_candidate = tokens[head + 7];
                // members with attributes will have        7 entries before next item in tree,
                // next item may be a '}' indicating the end of the struct, or a new struct member
                if (next_token_candidate.token_type == "struct_member" ||
                    next_token_candidate.token_type == "}")
                {
                    std::string member_attribute_name = tokens[head + 2].token_src;
                    std::string member_attribute_value = tokens[head + 3].token_src;
                    std::string member_name = tokens[head + 4].token_src;
                    std::string member_type = tokens[head + 5].token_src;
                    uint32_t member_size = get_type_size(member_type);
                    uint32_t member_offset = offset_counter;
                    offset_counter += member_size;

                    members.push_back({member_name, member_type, member_attribute_name, member_attribute_value, member_size, member_offset});
                    // we have a struct member with attributes;
                    head++;
                    continue;
                }
                // members without attributes should have   3 entries before next item in tree,
                next_token_candidate = tokens[head + 4];
                if (next_token_candidate.token_type == "struct_member" ||
                    next_token_candidate.token_type == "}") {
                    std::string member_name = tokens[head + 1].token_src;
                    std::string raw_type = tokens[head + 2].token_src;
                    uint32_t member_size = get_type_size(raw_type);
                    uint32_t member_offset = offset_counter;
                    offset_counter += member_size;

                    members.push_back({member_name, raw_type, "", "", member_size, member_offset});
                    // we have a struct member with no attributes;
                }
            }

            head++;
            next_token = tokens[head];
        }

        uint32_t type_size = 0;
        for (auto &member : members) {
            type_size += member.member_size;
        }

        user_types.emplace(new_usertype.type_name, user_type{new_usertype.type_name, members, type_size});
    }

    void handle_new_uniform(std::vector<TokenMapping>& tokens, int& head)
    {
        std::string uniform_name = tokens[head + 1].token_src;
        std::string uniform_type = tokens[head + 2].token_src;
        uint16_t uniform_set = std::stoi(tokens[head - 4].token_src);
        uint16_t uniform_binding = std::stoi(tokens[head - 1].token_src);
        uniforms.emplace(uniform_name, uniform {uniform_set, uniform_binding, uniform_name, uniform_type});
    }

    void handle_new_sampler(std::vector<TokenMapping>& tokens, int& head)
    {
        std::string sampler_name    = tokens[head - 2].token_src;
        uint16_t set                = std::stoi(tokens[head - 6].token_src);
        uint16_t binding            = std::stoi(tokens[head - 3].token_src);

        samplers.emplace(sampler_name, sampler{set, binding, sampler_name});
    }

    void handle_new_texture_2d(std::vector<TokenMapping>& tokens, int& head)
    {
        uint16_t set        = std::stoi(tokens[head - 6].token_src);
        uint16_t binding    = std::stoi(tokens[head - 3].token_src);
        std::string name    = tokens[head - 2].token_src;
        std::string type    = tokens[head - 1].token_src;

        textures.emplace(name, texture{set, binding, 2, name, type});
    }

    wgpu::VertexFormat get_vertex_format_from_reflected_type(std::string type)
    {
        std::string type_t = type;
        if (aliases.find(type) != aliases.end())
        {
            type_t = aliases[type];
        }

        if (type_t == "f32") { return wgpu::VertexFormat::Float32; }
        if (type_t == "vec2<f32>") { return wgpu::VertexFormat::Float32x2; }
        if (type_t == "vec3<f32>") { return wgpu::VertexFormat::Float32x3; }
        if (type_t == "vec4<f32>") { return wgpu::VertexFormat::Float32x4; }

        if (type_t == "vec2<f16>") { return wgpu::VertexFormat::Float16x2; }
        if (type_t == "vec4<f16>") { return wgpu::VertexFormat::Float16x4; }

        if (type_t == "i32")       { return wgpu::VertexFormat::Sint32;    }
        if (type_t == "vec2<i32>") { return wgpu::VertexFormat::Sint32x2;  }
        if (type_t == "vec3<i32>") { return wgpu::VertexFormat::Sint32x3;  }
        if (type_t == "vec4<i32>") { return wgpu::VertexFormat::Sint32x4;  }
        
        if (type_t == "u32") { return wgpu::VertexFormat::Uint32; }
        if (type_t == "vec2<u32>") { return wgpu::VertexFormat::Uint32x2;}
        if (type_t == "vec3<u32>") { return wgpu::VertexFormat::Uint32x3;}
        if (type_t == "vec4<u32>") { return wgpu::VertexFormat::Uint32x4;}


        return wgpu::VertexFormat::Undefined;
    }

    void handle_wgpu_vertex_attributes(tdg::shader::reflection_data* data)
    {
        if (vertex_input_structure == "")
        {
            std::cerr << "Unable to identify the vertex shader input structure" << std::endl;
            return;
        }

        auto &type = user_types[vertex_input_structure];

        std::vector<wgpu::VertexAttribute> attributes;
        uint32_t stride = 0;
        for (auto& member : type.members)
        {
            wgpu::VertexAttribute attr{};
            attr.offset += member.member_offset;
            attr.format = get_vertex_format_from_reflected_type(member.type);
            attr.shaderLocation = std::stoi(member.attribute_value);
            stride += member.member_size;
            attributes.push_back(attr);
        
        }

        auto& vert_data = data->vertex_attributes.emplace_back(tdg::shader::reflection_data::vertex_attibute_data{});
        vert_data.attributes = attributes;
        wgpu::VertexBufferLayout attributes_layout{};
        attributes_layout.arrayStride = stride;
        attributes_layout.attributeCount = vert_data.attributes.size();
        attributes_layout.attributes = vert_data.attributes.data();
        vert_data.layout = attributes_layout;
    }

    std::unique_ptr<tdg::shader::reflection_data> parse(std::vector<TokenMapping> tokens)
    {
        for (int head = 0; head < tokens.size(); head++)
        {
            auto&   token = tokens[head];

            auto alias_matches = get_alias_matches(token.token_src);
            // found a type alias
            if (!alias_matches.empty()) {
                handle_alias(alias_matches);
            }

            // Non generic user-types
            if(token.token_type == "struct")
            {
                handle_new_usertype(tokens, head);
            }

            // uniforms
            if (token.token_type == "uniform")
            {
                handle_new_uniform(tokens, head);
            }

            // samplers
            if (token.token_type == "sampler")
            {
                handle_new_sampler(tokens, head);
            }

            if (token.token_type == "texture_2d")
            {
                handle_new_texture_2d(tokens, head);
            }

            if (token.token_type == "attribute" && token.token_src == "@vertex")
            {
                vertex_input_structure = tokens[head + 5].token_src;
            }
        }
        auto reflection_data = std::make_unique<tdg::shader::reflection_data>();

        handle_wgpu_vertex_attributes(reflection_data.get());


        return reflection_data;
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


std::unique_ptr<tdg::shader::reflection_data> tdg::shader::reflect_wgsl(const char* src)
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
    return token_parser.parse(tokens);
}