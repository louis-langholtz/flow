#include "flow/utility.hpp"

namespace flow {

std::vector<std::string> make_arg_bufs(const std::vector<std::string>& strings,
                                       const std::string& fallback)
{
    auto result = std::vector<std::string>{};
    if (strings.empty()) {
        if (!fallback.empty()) {
            result.push_back(fallback);
        }
    }
    else {
        for (auto&& string: strings) {
            result.push_back(string);
        }
    }
    return result;
}

std::vector<std::string>
make_arg_bufs(const std::map<std::string, std::string>& envars)
{
    auto result = std::vector<std::string>{};
    for (const auto& envar: envars) {
        result.push_back(envar.first + "=" + envar.second);
    }
    return result;
}

std::vector<char*> make_argv(const std::span<std::string>& args)
{
    auto result = std::vector<char*>{};
    for (auto&& arg: args) {
        result.push_back(arg.data());
    }
    result.push_back(nullptr); // last element must always be nullptr!
    return result;
}

}
