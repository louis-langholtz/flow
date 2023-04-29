#include <array>
#include <iostream>
#include <memory>
#include <type_traits>

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/file_port.hpp"
#include "flow/instance.hpp"
#include "flow/io_type.hpp"
#include "flow/process_id.hpp"
#include "flow/prototype_name.hpp"
#include "flow/prototype_port.hpp"
#include "flow/prototype.hpp"
#include "flow/utility.hpp"

namespace {

using namespace flow;

/// @brief Finds the channel requested.
/// @return Pointer to channel of the type requested or <code>nullptr</code>.
template <class T>
auto find_channel(const system_prototype& system, instance& instance,
                  const connection& look_for) -> T*
{
    const auto found = find_channel_index(system.connections, look_for);
    return found? std::get_if<T>(&(instance.channels[*found])): nullptr;
}

auto do_lsof_system() -> void
{
    const auto lsof_process_name = prototype_name{"lsof"};
    system_prototype system;
    executable_prototype lsof_executable;
    lsof_executable.executable_file = "/usr/sbin/lsof";
    lsof_executable.working_directory = "/usr/local";
    lsof_executable.arguments = {"lsof", "-p", "$$"};
    system.prototypes.emplace(lsof_process_name, lsof_executable);
    const auto lsof_stdout = pipe_connection{
        prototype_port{lsof_process_name, descriptor_id{1}},
        prototype_port{},
    };
    const auto lsof_stderr = pipe_connection{
        prototype_port{lsof_process_name, descriptor_id{2}},
        prototype_port{},
    };
    system.connections.push_back(lsof_stdout);
    system.connections.push_back(lsof_stderr);
    {
        auto diags = temporary_fstream();
        auto object = instantiate(system, diags);
        wait(prototype_name{}, object, std::cerr, wait_mode::diagnostic);
        if (const auto pipe = find_channel<pipe_channel>(system, object, lsof_stdout)) {
            for (;;) {
                std::array<char, 4096> buffer{};
                const auto nread = pipe->read(buffer, std::cerr);
                if (nread == static_cast<std::size_t>(-1)) {
                    std::cerr << "lsof can't read stdout\n";
                }
                else if (nread != 0u) {
                    std::cout << buffer.data();
                }
                else {
                    break;
                }
            }
        }
        std::cerr << "Diagnostics for parent of lsof...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        write_diags(prototype_name{}, object, std::cerr);
    }
}

auto do_ls_system() -> void
{
    const auto input_file_port = file_port{"flow.in"};

    const auto output_file_port = file_port{"flow.out"};
    touch(output_file_port);
    std::cerr << "running in " << std::filesystem::current_path() << '\n';

    system_prototype system;

    const auto cat_process_name = prototype_name{"cat"};
    executable_prototype cat_executable;
    cat_executable.executable_file = "/bin/cat";
    system.prototypes.emplace(cat_process_name, cat_executable);

    const auto xargs_process_name = prototype_name{"xargs"};
    executable_prototype xargs_executable;
    xargs_executable.executable_file = "/usr/bin/xargs";
    xargs_executable.working_directory = "/fee/fii/foo/fum";
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    system.prototypes.emplace(xargs_process_name, xargs_executable);

    const auto cat_stdin = pipe_connection{
        prototype_port{},
        prototype_port{cat_process_name, descriptor_id{0}}
    };
    system.connections.push_back(cat_stdin);
    system.connections.push_back(pipe_connection{
        prototype_port{cat_process_name, descriptor_id{1}},
        prototype_port{xargs_process_name, descriptor_id{0}},
    });
    const auto xargs_stdout = pipe_connection{
        prototype_port{xargs_process_name, descriptor_id{1}},
        prototype_port{},
    };
    system.connections.push_back(xargs_stdout);
    const auto xargs_stderr = pipe_connection{
        prototype_port{xargs_process_name, descriptor_id{2}},
        prototype_port{},
    };
    system.connections.push_back(xargs_stderr);
#if 0
    system.connections.push_back(file_connection{
        output_file_port,
        io_type::out,
        prototype_port{cat_process_name, descriptor_id{1}}
    });
#endif

    {
        auto diags = temporary_fstream();
        auto object = instantiate(system, diags);
        if (const auto pipe = find_channel<pipe_channel>(system, object, cat_stdin)) {
            pipe->write("/bin\n/sbin", std::cerr);
            pipe->close(io_type::out, std::cerr);
        }
        wait(prototype_name{}, object, std::cerr, wait_mode::diagnostic);
        if (const auto pipe = find_channel<pipe_channel>(system, object, xargs_stderr)) {
            std::array<char, 1024> buffer{};
            const auto nread = pipe->read(buffer, std::cerr);
            if (nread == static_cast<std::size_t>(-1)) {
                std::cerr << "xargs can't read stderr\n";
            }
            else if (nread != 0u) {
                std::cerr << "xargs stderr: " << buffer.data() << "\n";
            }
        }
        if (const auto pipe = find_channel<pipe_channel>(system, object, xargs_stdout)) {
            for (;;) {
                std::array<char, 4096> buffer{};
                const auto nread = pipe->read(buffer, std::cerr);
                if (nread == static_cast<std::size_t>(-1)) {
                    std::cerr << "xargs can't read stdout\n";
                }
                else if (nread != 0u) {
                    std::cout << buffer.data();
                }
                else {
                    break;
                }
            }
        }
        std::cerr << "Diagnostics for root instance...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));

        write_diags(prototype_name{}, object, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[" << std::size(object.children) << "]";
        std::cerr << "\n";
    }
}

auto do_nested_system() -> void
{
    std::cerr << "Doing nested instance...\n";

    const auto cat_system_name = prototype_name{"cat-system"};
    const auto cat_process_name = prototype_name{"cat-process"};
    const auto xargs_system_name = prototype_name{"xargs-system"};
    const auto xargs_process_name = prototype_name{"xargs-process"};

    system_prototype cat_system;
    executable_prototype cat_executable;
    cat_executable.executable_file = "/bin/cat";
    cat_system.prototypes.emplace(cat_process_name, cat_executable);
    const auto cat_system_stdout = pipe_connection{
        prototype_port{cat_process_name, descriptor_id{1}},
        prototype_port{prototype_name{}, descriptor_id{1}},
    };
    cat_system.connections.push_back(cat_system_stdout);

    system_prototype xargs_system;
    executable_prototype xargs_executable;
    xargs_executable.executable_file = "/usr/bin/xargs";
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    xargs_system.prototypes.emplace(xargs_process_name, xargs_executable);

    system_prototype system;
    system.prototypes.emplace(cat_system_name, cat_system);
    system.prototypes.emplace(xargs_system_name, xargs_system);
    system.connections.push_back(pipe_connection{
        prototype_port{cat_system_name, descriptor_id{1}},
        prototype_port{xargs_system_name, descriptor_id{0}},
    });

    {
        auto diags = temporary_fstream();
        auto object = instantiate(system, diags);
        std::cerr << "Diagnostics for nested instance...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        wait(prototype_name{}, object, std::cerr, wait_mode::diagnostic);
    }
}

}

int main(int argc, const char * argv[])
{
    using namespace flow;
    set_signal_handler(signal::interrupt);
    set_signal_handler(signal::terminate);
    do_lsof_system();
    do_ls_system();
    do_nested_system();
    return 0;
}
