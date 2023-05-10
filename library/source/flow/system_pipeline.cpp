#include "flow/system_pipeline.hpp"
#include "flow/utility.hpp"
#include "flow/wait_result.hpp"

namespace flow {

namespace {

auto get_matching_set(const descriptor_map& descriptors, io_type io)
    -> std::set<descriptor_id>
{
    auto result = std::set<descriptor_id>{};
    for (auto&& entry: descriptors) {
        if (entry.second.direction == io) {
            result.insert(entry.first);
        }
    }
    return result;
}

}

system_pipeline::~system_pipeline()
{
    try {
        switch (current_state) {
        case state::setup:
            instantiate();
            [[fallthrough]];
        case state::instantiated:
            wait();
            [[fallthrough]];
        case state::waiting:
        case state::done:
            break;
        }
    }
    catch (...) {
        // Ignore any exceptions since in destructor!
    }
}

auto system_pipeline::append(const system& sys) -> system_pipeline&
{
    if (current_state != state::setup) {
        throw std::logic_error{"append only supported during setup"};
    }
    if (!std::holds_alternative<unset_endpoint>(dst_end)) {
        throw std::invalid_argument{"append system after dst end not allowed"};
    }
    auto sys_inputs = get_matching_set(sys.descriptors, io_type::in);
    const auto count = size(info.subsystems);
    const auto sys_name = system_name{std::to_string(count)};
    if (count == 0u) {
        if (std::holds_alternative<unset_endpoint>(src_end)) {
            if (!empty(sys_inputs)) {
                throw std::invalid_argument{"first system can't have inputs"};
            }
        }
        else {
            if (empty(sys_inputs)) {
                throw std::invalid_argument{"current system must have inputs"};
            }
            info.connections.emplace_back(unidirectional_connection{
                src_end, system_endpoint{sys_name, sys_inputs},
            });
        }
    }
    else {
        const auto last_name = system_name{std::to_string(count - 1u)};
        auto &last = info.subsystems.at(last_name);
        auto last_outputs = get_matching_set(last.descriptors, io_type::out);
        if (empty(last_outputs)) {
            throw std::invalid_argument{"last system must have outputs"};
        }
        if (empty(sys_inputs)) {
            throw std::invalid_argument{"current system must have inputs"};
        }
        info.connections.emplace_back(unidirectional_connection{
            system_endpoint{last_name, std::move(last_outputs)},
            system_endpoint{sys_name, sys_inputs},
        });
    }
    info.subsystems.emplace(sys_name, sys);
    return *this;
}

auto system_pipeline::append(const endpoint& end) -> system_pipeline&
{
    if (current_state != state::setup) {
        throw std::logic_error{"append only supported during setup"};
    }
    if (std::holds_alternative<system_endpoint>(end)) {
        throw std::invalid_argument{"appending system endpoint not allowed"};
    }
    if (std::holds_alternative<unset_endpoint>(end)) {
        throw std::invalid_argument{"appending unset endpoint not allowed"};
    }
    const auto count = size(info.subsystems);
    if (count == 0) {
        if (!std::holds_alternative<unset_endpoint>(src_end)) {
            throw std::invalid_argument{"already have source endpoint"};
        }
        src_end = end;
    }
    else {
        if (!std::holds_alternative<unset_endpoint>(dst_end)) {
            throw std::invalid_argument{"already have destination endpoint"};
        }
        dst_end = end;
        const auto last_name = system_name{std::to_string(count - 1u)};
        auto &last = info.subsystems.at(last_name);
        auto last_outputs = get_matching_set(last.descriptors, io_type::out);
        if (empty(last_outputs)) {
            throw std::invalid_argument{"last system must have outputs"};
        }
        info.connections.emplace_back(unidirectional_connection{
            system_endpoint{last_name, std::move(last_outputs)}, dst_end,
        });
    }
    return *this;
}

static_assert(std::is_move_assignable_v<instance::custom>);

auto system_pipeline::instantiate() -> system_pipeline&
{
    if (current_state != state::setup) {
        throw std::logic_error{"run only supported during setup"};
    }
    diags = temporary_fstream();
    object = flow::instantiate({}, info, diags);
    current_state = state::instantiated;
    return *this;
}

auto system_pipeline::wait() -> std::vector<wait_result>
{
    switch (current_state) {
    case state::waiting:
        throw std::logic_error{"already waiting"};
    case state::done:
        throw std::logic_error{"already done"};
    case state::setup:
        instantiate();
        break;
    case state::instantiated:
        break;
    }
    current_state = state::waiting;
    auto wait_results = flow::wait(system_name{}, object);
    current_state = state::done;
    return wait_results;
}

auto operator|(const endpoint& lhs, const system& rhs)
    -> system_pipeline
{
    auto pipeline = system_pipeline();
    pipeline.append(lhs).append(rhs);
    return pipeline;
}

auto operator|(const system& lhs, const system& rhs)
    -> system_pipeline
{
    auto pipeline = system_pipeline();
    pipeline.append(lhs).append(rhs);
    return pipeline;
}

auto operator|(const system& lhs, const endpoint& rhs)
    -> system_pipeline
{
    auto pipeline = system_pipeline();
    pipeline.append(lhs).append(rhs);
    return pipeline;
}

auto operator|(system_pipeline& lhs, const system& rhs)
    -> system_pipeline&
{
    return lhs.append(rhs);
}

auto operator|(system_pipeline& lhs, const endpoint& rhs)
    -> system_pipeline&
{
    return lhs.append(rhs);
}

}
