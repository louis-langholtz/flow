#include <cstddef> // for std::ptrdiff_t
#include <cstdio> // for ::tmpfile, std::fclose
#include <memory> // for std::unique_ptr
#include <streambuf>
#include <string> // for std::char_traits

#include <stdlib.h> // for ::mkstemp
#include <unistd.h> // for ::close

#include "flow/descriptor_id.hpp"
#include "flow/instance.hpp"
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

void show_diags(const prototype_name& name, instance& object, std::ostream& os)
{
    if (object.diags.is_open()) {
        object.diags.seekg(0, std::ios_base::end);
        if (object.diags.tellg() != 0) {
            object.diags.seekg(0, std::ios_base::beg);
            os << "Diagnostics for " << name << "...\n";
            // istream_iterator skips ws, so use istreambuf_iterator...
            std::copy(std::istreambuf_iterator<char>(object.diags.rdbuf()),
                      std::istreambuf_iterator<char>(),
                      std::ostream_iterator<char>(os));
        }
    }
    for (auto&& map_entry: object.children) {
        const auto full_name = name.value + "." + map_entry.first.value;
        show_diags(prototype_name{full_name}, map_entry.second, os);
    }
}

}
