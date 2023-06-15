#include "flow/instantiate.hpp"
#include "flow/node_pipeline.hpp"
#include "flow/wait_result.hpp"

namespace flow {

node_pipeline::~node_pipeline()
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

auto node_pipeline::append(const node& sys) -> node_pipeline&
{
    if (current_state != state::setup) {
        throw std::logic_error{"append only supported during setup"};
    }
    if (!std::holds_alternative<unset_endpoint>(dst_end)) {
        throw std::invalid_argument{"append node after dst end not allowed"};
    }
    auto sys_inputs = get_matching_set(sys, io_type::in);
    const auto count = size(info.nodes);
    const auto sys_name = node_name{std::to_string(count)};
    if (count == 0u) {
        if (std::holds_alternative<unset_endpoint>(src_end)) {
            if (!empty(sys_inputs)) {
                throw std::invalid_argument{"first node can't have inputs"};
            }
        }
        else {
            if (empty(sys_inputs)) {
                throw std::invalid_argument{"current node must have inputs"};
            }
            info.links.emplace_back(unidirectional_link{
                src_end, node_endpoint{sys_name, sys_inputs},
            });
        }
    }
    else {
        const auto last_name = node_name{std::to_string(count - 1u)};
        auto &last = info.nodes.at(last_name);
        auto last_outputs = get_matching_set(last, io_type::out);
        if (empty(last_outputs)) {
            throw std::invalid_argument{"last node must have outputs"};
        }
        if (empty(sys_inputs)) {
            throw std::invalid_argument{"current node must have inputs"};
        }
        info.links.emplace_back(unidirectional_link{
            node_endpoint{last_name, std::move(last_outputs)},
            node_endpoint{sys_name, sys_inputs},
        });
    }
    info.nodes.emplace(sys_name, sys);
    return *this;
}

auto node_pipeline::append(const endpoint& end) -> node_pipeline&
{
    if (current_state != state::setup) {
        throw std::logic_error{"append only supported during setup"};
    }
    if (std::holds_alternative<node_endpoint>(end)) {
        throw std::invalid_argument{"appending node endpoint not allowed"};
    }
    if (std::holds_alternative<unset_endpoint>(end)) {
        throw std::invalid_argument{"appending unset endpoint not allowed"};
    }
    const auto count = size(info.nodes);
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
        const auto last_name = node_name{std::to_string(count - 1u)};
        auto &last = info.nodes.at(last_name);
        auto last_outputs = get_matching_set(last, io_type::out);
        if (empty(last_outputs)) {
            throw std::invalid_argument{"last node must have outputs"};
        }
        info.links.emplace_back(unidirectional_link{
            node_endpoint{last_name, std::move(last_outputs)}, dst_end,
        });
    }
    return *this;
}

static_assert(std::is_move_assignable_v<instance::system>);

auto node_pipeline::instantiate() -> node_pipeline&
{
    if (current_state != state::setup) {
        throw std::logic_error{"run only supported during setup"};
    }
    diags = ext::temporary_fstream();
    object = flow::instantiate(info, diags);
    current_state = state::instantiated;
    return *this;
}

auto node_pipeline::wait() -> std::vector<wait_result>
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
    auto wait_results = flow::wait(object);
    current_state = state::done;
    return wait_results;
}

auto operator|(const endpoint& lhs, const node& rhs)
    -> node_pipeline
{
    auto pipeline = node_pipeline();
    pipeline.append(lhs).append(rhs);
    return pipeline;
}

auto operator|(const node& lhs, const node& rhs)
    -> node_pipeline
{
    auto pipeline = node_pipeline();
    pipeline.append(lhs).append(rhs);
    return pipeline;
}

auto operator|(const node& lhs, const endpoint& rhs)
    -> node_pipeline
{
    auto pipeline = node_pipeline();
    pipeline.append(lhs).append(rhs);
    return pipeline;
}

auto operator|(node_pipeline& lhs, const node& rhs)
    -> node_pipeline&
{
    return lhs.append(rhs);
}

auto operator|(node_pipeline& lhs, const endpoint& rhs)
    -> node_pipeline&
{
    return lhs.append(rhs);
}

}
