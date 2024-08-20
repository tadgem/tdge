#include "utils.h"
#include <fstream>
#include <sstream>

std::vector<std::string> tdg::utils::get_matches(std::string &input, std::regex &exp)
{
    std::smatch matches;
    if(std::regex_search(input, matches, exp)) {
        std::vector<std::string> match_strings{};
        for (auto i = 0; i < matches.size(); i++)
        {
            match_strings.push_back(matches.str(i));
        }
        return match_strings;
    }
    return {};
}

std::string tdg::utils::load_string_from_path(const std::string &path)
{
    std::ifstream input(path);
    std::stringstream str_stream;
    str_stream << input.rdbuf();
    return str_stream.str();
}
