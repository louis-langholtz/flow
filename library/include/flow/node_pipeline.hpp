#ifndef system_pipeline_hpp
#define system_pipeline_hpp

#include <type_traits> // for std::is_default_constructible_v
#include <vector>

#include "flow/instance.hpp"
#include "flow/node.hpp"
#include "flow/wait_result.hpp"

namespace flow {

struct node_pipeline
{
    node_pipeline() = default;
    node_pipeline(node_pipeline&& other) = default;
    ~node_pipeline();
    auto operator=(node_pipeline&& other) -> node_pipeline& = default;

    auto append(const node& sys) -> node_pipeline&;
    auto append(const endpoint& end) -> node_pipeline&;
    auto instantiate() -> node_pipeline&;
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

static_assert(std::is_default_constructible_v<node_pipeline>);
static_assert(!std::is_copy_constructible_v<node_pipeline>);
static_assert(std::is_move_constructible_v<node_pipeline>);
static_assert(!std::is_copy_assignable_v<node_pipeline>);
static_assert(std::is_move_assignable_v<node_pipeline>);

auto operator|(const endpoint& lhs, const node& rhs)
    -> node_pipeline;
auto operator|(const node& lhs, const node& rhs)
    -> node_pipeline;
auto operator|(const node& lhs, const endpoint& rhs)
    -> node_pipeline;
auto operator|(node_pipeline& lhs, const node& rhs)
    -> node_pipeline&;
auto operator|(node_pipeline& lhs, const endpoint& rhs)
    -> node_pipeline&;

}

#endif /* system_pipeline_hpp */
