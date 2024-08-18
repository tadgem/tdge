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
        "parameter",
        "int_literal",
        "type_declaration",
        "struct_declaration",
        "struct_member",
        "struct",
        "ERROR",
        "{",
        "}"
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

    std::unordered_map<std::string, type>           concrete_types;
    std::unordered_map<std::string, generic_type>   generic_types;

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

        auto& concrete_type = concrete_types[concrete_name];

        type new_alias_type = {};
        new_alias_type.type_name = alias_name;
        new_alias_type.type_size = (concrete_type.type_size * generic_type.size_multi) + generic_type.size_offset;
        concrete_types.emplace(alias_name, new_alias_type);
    }

    void handle_concrete_alias(std::string alias_name, std::string concrete_name)
    {
        if (concrete_types.find(concrete_name) == concrete_types.end())
        {
            std::cerr << "Unknown concrete type : " << concrete_name << std::endl;
            return;
        }
        type new_alias_type = {};
        new_alias_type.type_name = alias_name;
        new_alias_type.type_size = concrete_types[concrete_name].type_size;
        concrete_types.emplace(alias_name, new_alias_type);
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

    void handle_new_usertype(std::vector<TokenMapping>& tokens, int& head)
    {
        head++;
        if(tokens[head].token_type != "identifier")
        {
            std::cerr << "struct declared with no name" << std::endl;
            return;
        }
        type new_usertype = {tokens[head].token_src, 0};

        auto next_token = tokens[head];
        // while we have members to reflect in this struct
        while(next_token.token_type != "}")
        {
            // DO stuff
            if(next_token.token_type == "struct_member")
            {

            }

            head++;
            next_token = tokens[head];
        }
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
                if(token.token_type == "struct")
                {
                    handle_new_usertype(tokens, head);
                }
            }

            // TODO: generic user types
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