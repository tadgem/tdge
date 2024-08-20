//
// Created by liam_ on 8/17/2024.
//

#pragma once
#include <vector>
#include <string>
#include <regex>

namespace tdg {
class utils {
public:

    static std::vector<std::string> get_matches(std::string& input, std::regex& exp);

    static std::string              load_string_from_path(const std::string &path);


};
}