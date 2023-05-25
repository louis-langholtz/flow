#include "flow/indenting_ostreambuf.hpp"
#include "flow/instance.hpp"
#include "flow/os_error_code.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace flow {

auto operator<<(std::ostream& os, const instance& value) -> std::ostream&
{
    os << "instance{";
    if (const auto p = std::get_if<instance::custom>(&value.info)) {
        os << ",.pgrp=" << p->pgrp;
        os << ",.children={";
        for (auto&& entry: p->children) {
            if (&entry != &(*p->children.begin())) {
                os << ",";
            }
            os << "{";
            os << ".first=" << entry.first;
            os << ",.second=" << entry.second;
            os << "}";
        }
        os << "}";
        os << ",.channels={";
        for (auto&& elem: p->channels) {
            if (&elem != &(*p->channels.begin())) {
                os << ",";
            }
            os << elem;
        }
        os << "}";
    }
    else if (const auto p = std::get_if<instance::forked>(&value.info)) {
        os << ",.state=" << p->state;
    }
    os << "}";
    return os;
}

auto pretty_print(std::ostream& os, const instance& value) -> void
{
    os << "{\n";
    if (const auto p = std::get_if<instance::custom>(&value.info)) {
        os << "  .pgrp=" << p->pgrp << ",\n";
        if (p->children.empty()) {
            os << "  .children={},\n";
        }
        else {
            os << "  .children={\n";
            for (auto&& entry: p->children) {
                os << "    {\n";
                os << "      .first=" << entry.first << ",\n";
                os << "      .second=";
                {
                    const auto opts = detail::indenting_ostreambuf_options{
                        6, false
                    };
                    const detail::indenting_ostreambuf child_indent{os, opts};
                    pretty_print(os, entry.second);
                }
                os << "    },\n";
            }
            os << "  },\n";
        }
        if (p->channels.empty()) {
            os << "  .channels={}\n";
        }
        else {
            os << "  .channels={\n";
            for (auto&& channel: p->channels) {
                os << "    " << channel << " (" << &channel << "),\n";
            }
            os << "  }\n";
        }
    }
    else if (const auto p = std::get_if<instance::forked>(&value.info)) {
        os << "  .state=" << p->state;
    }
    os << "}\n";
}

auto get_reference_process_id(const instance::forked& object)
    -> reference_process_id
{
    if (const auto p = std::get_if<owning_process_id>(&object.state)) {
        return reference_process_id(*p);
    }
    return invalid_process_id;
}

auto get_reference_process_id(const std::vector<system_name>& names,
                              const instance& object)
    -> reference_process_id
{
    auto* info = &object.info;
    for (auto comps = names; !empty(comps); comps.pop_back()) {
        const auto& comp = comps.back();
        auto found = false;
        if (const auto p = std::get_if<instance::custom>(info)) {
            for (auto&& entry: p->children) {
                if (entry.first == comp) {
                    info = &entry.second.info;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            throw std::invalid_argument{"no such component"};
        }
    }
    if (const auto p = std::get_if<instance::forked>(info)) {
        return get_reference_process_id(*p);
    }
    throw std::invalid_argument{"wrong instance type found"};
}

auto total_descendants(const instance& object) -> std::size_t
{
    auto result = std::size_t{0};
    if (const auto p = std::get_if<instance::custom>(&object.info)) {
        for (auto&& child: p->children) {
            result += total_descendants(child.second) + 1u;
        }
    }
    return result;
}

auto total_channels(const instance& object) -> std::size_t
{
    auto result = std::size_t{};
    if (const auto p = std::get_if<instance::custom>(&object.info)) {
        result += std::size(p->channels);
        for (auto&& child: p->children) {
            result += total_channels(child.second);
        }
    }
    return result;
}

auto get_wait_status(const instance& object) -> wait_status
{
    if (const auto q = std::get_if<instance::forked>(&object.info)) {
        if (const auto r = std::get_if<wait_status>(&(q->state))) {
            return *r;
        }
    }
    return {wait_unknown_status{}};
}

}
