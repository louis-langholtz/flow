#ifndef system_pipeline_hpp
#define system_pipeline_hpp

#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "flow/instance.hpp"
#include "flow/system.hpp"

namespace flow {

struct wait_result;

struct system_pipeline
{
    system_pipeline() = default;
    system_pipeline(system_pipeline&& other) = default;
    ~system_pipeline();
    auto operator=(system_pipeline&& other) -> system_pipeline& = default;

    auto append(const system& sys) -> system_pipeline&;
    auto append(const endpoint& end) -> system_pipeline&;
    auto instantiate() -> system_pipeline&;
    auto wait() -> std::vector<wait_result>;

private:
    system::custom info;
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

auto operator|(const endpoint& lhs, const system& rhs)
    -> system_pipeline;
auto operator|(const system& lhs, const system& rhs)
    -> system_pipeline;
auto operator|(const system& lhs, const endpoint& rhs)
    -> system_pipeline;
auto operator|(system_pipeline& lhs, const system& rhs)
    -> system_pipeline&;
auto operator|(system_pipeline& lhs, const endpoint& rhs)
    -> system_pipeline&;

}

#endif /* system_pipeline_hpp */