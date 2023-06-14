#ifndef system_pipeline_hpp
#define system_pipeline_hpp

#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "flow/instance.hpp"
#include "flow/node.hpp"
#include "flow/wait_result.hpp"

namespace flow {

struct system_pipeline
{
    system_pipeline() = default;
    system_pipeline(system_pipeline&& other) = default;
    ~system_pipeline();
    auto operator=(system_pipeline&& other) -> system_pipeline& = default;

    auto append(const node& sys) -> system_pipeline&;
    auto append(const endpoint& end) -> system_pipeline&;
    auto instantiate() -> system_pipeline&;
    auto wait() -> std::vector<wait_result>;

private:
    custom info;
    endpoint src_end;
    endpoint dst_end;
    instance object;
    ext::fstream diags;
    enum class state { setup, instantiated, waiting, done };
    state current_state{state::setup};
};

static_assert(std::is_default_constructible_v<system_pipeline>);
static_assert(!std::is_copy_constructible_v<system_pipeline>);
static_assert(std::is_move_constructible_v<system_pipeline>);
static_assert(!std::is_copy_assignable_v<system_pipeline>);
static_assert(std::is_move_assignable_v<system_pipeline>);

auto operator|(const endpoint& lhs, const node& rhs)
    -> system_pipeline;
auto operator|(const node& lhs, const node& rhs)
    -> system_pipeline;
auto operator|(const node& lhs, const endpoint& rhs)
    -> system_pipeline;
auto operator|(system_pipeline& lhs, const node& rhs)
    -> system_pipeline&;
auto operator|(system_pipeline& lhs, const endpoint& rhs)
    -> system_pipeline&;

}

#endif /* system_pipeline_hpp */
