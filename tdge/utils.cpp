#include "utils.h"
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
