#include <array>
#include <iostream>
#include <memory>
#include <type_traits>

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/instance.hpp"
#include "flow/process_id.hpp"
#include "flow/system_name.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace {

using namespace flow;

/// @brief Finds the channel requested.
/// @return Pointer to channel of the type requested or <code>nullptr</code>.
template <class T>
auto find_channel(const custom_system& system, instance& instance,
                  const connection& look_for) -> T*
{
    const auto found = find_index(system.connections, look_for);
    return found? std::get_if<T>(&(instance.channels[*found])): nullptr;
}

auto do_lsof_system() -> void
{
    std::cerr << "Doing lsof instance...\n";

    const auto lsof_process_name = system_name{"lsof"};
    custom_system system;
    executable_system lsof_executable;
    lsof_executable.executable_file = "/usr/sbin/lsof";
    lsof_executable.working_directory = "/usr/local";
    lsof_executable.arguments = {"lsof", "-p", "$$"};
    system.subsystems.emplace(lsof_process_name, lsof_executable);
    const auto lsof_stdout = unidirectional_connection{
        system_endpoint{lsof_process_name, descriptor_id{1}},
        user_endpoint{},
    };
    const auto lsof_stderr = unidirectional_connection{
        system_endpoint{lsof_process_name, descriptor_id{2}},
        file_endpoint{"/dev/null"},
    };
    system.connections.push_back(lsof_stdout);
    system.connections.push_back(lsof_stderr);
    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, system, diags, {}, {});
        std::cerr << "Diagnostics for parent of lsof...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        pretty_print(std::cerr, object);

        wait(system_name{}, object, std::cerr, wait_mode::diagnostic);
        if (const auto p = find_channel<pipe_channel>(system, object,
                                                      lsof_stdout)) {
            for (;;) {
                std::array<char, 4096> buffer{};
                const auto nread = p->read(buffer, std::cerr);
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
        else {
            std::cerr << "no pipe for lsof_stdout??!\n";
        }
        write_diags(system_name{}, object, std::cerr);
    }
}

auto do_ls_system() -> void
{
    std::cerr << "Doing ls instance...\n";

    const auto input_file_endpoint = file_endpoint{"flow.in"};

    const auto output_file_endpoint = file_endpoint{"flow.out"};
    touch(output_file_endpoint);
    std::cerr << "running in " << std::filesystem::current_path() << '\n';

    custom_system system;

    const auto cat_process_name = system_name{"cat"};
    executable_system cat_executable;
    cat_executable.executable_file = "/bin/cat";
    system.subsystems.emplace(cat_process_name, cat_executable);

    const auto xargs_process_name = system_name{"xargs"};
    executable_system xargs_executable;
    xargs_executable.executable_file = "/usr/bin/xargs";
    xargs_executable.working_directory = "/fee/fii/foo/fum";
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    system.subsystems.emplace(xargs_process_name, xargs_executable);

    const auto cat_stdin = unidirectional_connection{
        user_endpoint{},
        system_endpoint{cat_process_name, descriptor_id{0}}
    };
    const auto xargs_stdout = unidirectional_connection{
        system_endpoint{xargs_process_name, descriptor_id{1}},
        user_endpoint{},
    };
    system.connections.push_back(cat_stdin);
    system.connections.push_back(unidirectional_connection{
        system_endpoint{cat_process_name, descriptor_id{1}},
        system_endpoint{xargs_process_name, descriptor_id{0}},
    });
    system.connections.push_back(xargs_stdout);

    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, system, diags, {}, {});
        std::cerr << "Diagnostics for root instance...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        if (const auto pipe = find_channel<pipe_channel>(system, object,
                                                         cat_stdin)) {
            pipe->write("/bin\n/sbin", std::cerr);
            pipe->close(pipe_channel::io::write, std::cerr);
        }
        else {
            std::cerr << "no pipe for cat_stdin??!\n";
        }
        const auto outpipe = find_channel<pipe_channel>(system, object,
                                                        xargs_stdout);
        if (!outpipe) {
            std::cerr << "no pipe for xargs_stdout??!\n";
        }
        wait(system_name{}, object, std::cerr, wait_mode::diagnostic);
        if (outpipe) {
            for (;;) {
                std::array<char, 4096> buffer{};
                const auto nread = outpipe->read(buffer, std::cerr);
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

        write_diags(system_name{}, object, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[" << std::size(object.children) << "]";
        std::cerr << "\n";
    }
}

auto do_nested_system() -> void
{
    std::cerr << "Doing nested instance...\n";

    const auto cat_system_name = system_name{"cat-system"};
    const auto cat_process_name = system_name{"cat-process"};
    const auto xargs_system_name = system_name{"xargs-system"};
    const auto xargs_process_name = system_name{"xargs-process"};

    custom_system system;

    {
        custom_system cat_system;
        executable_system cat_executable;
        cat_executable.executable_file = "/bin/cat";
        cat_system.subsystems.emplace(cat_process_name, cat_executable);
        cat_system.connections.push_back(unidirectional_connection{
            system_endpoint{system_name{}, descriptor_id{0}},
            system_endpoint{cat_process_name, descriptor_id{0}}
        });
        cat_system.connections.push_back(unidirectional_connection{
            system_endpoint{cat_process_name, descriptor_id{1}},
            system_endpoint{system_name{}, descriptor_id{1}},
        });
        system.subsystems.emplace(cat_system_name, cat_system);
    }

    {
        custom_system xargs_system;
        executable_system xargs_executable;
        xargs_executable.executable_file = "/usr/bin/xargs";
        xargs_executable.arguments = {"xargs", "ls", "-alF"};
        xargs_system.subsystems.emplace(xargs_process_name, xargs_executable);
        xargs_system.connections.push_back(unidirectional_connection{
            system_endpoint{system_name{}, descriptor_id{0}},
            system_endpoint{xargs_process_name, descriptor_id{0}}
        });
        xargs_system.connections.push_back(unidirectional_connection{
            system_endpoint{xargs_process_name, descriptor_id{1}},
            system_endpoint{system_name{}, descriptor_id{1}},
        });
        system.subsystems.emplace(xargs_system_name, xargs_system);
    }

    const auto system_stdin = unidirectional_connection{
        user_endpoint{},
        system_endpoint{cat_system_name, descriptor_id{0}},
    };
    const auto catout_to_xargsin = unidirectional_connection{
        system_endpoint{cat_system_name, descriptor_id{1}},
        system_endpoint{xargs_system_name, descriptor_id{0}},
    };
    const auto system_stdout = unidirectional_connection{
        system_endpoint{xargs_system_name, descriptor_id{1}},
        user_endpoint{},
    };

    system.connections.push_back(system_stdin);
    system.connections.push_back(catout_to_xargsin);
    system.connections.push_back(system_stdout);

    {
        auto diags = temporary_fstream();
        auto parent_channels = std::vector<channel>{};
        auto object = instantiate(system_name{}, system, diags,
                                  std::vector<connection>{},
                                  parent_channels);
        std::cerr << "Diagnostics for nested instance...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        pretty_print(std::cerr, object);
        if (const auto pipe = find_channel<pipe_channel>(system, object,
                                                         system_stdin)) {
            pipe->write("/bin\n/sbin", std::cerr);
            pipe->close(pipe_channel::io::write, std::cerr);
        }
        else {
            std::cerr << "can't find " << system_stdin << "\n";
        }
        wait(system_name{}, object, std::cerr, wait_mode::diagnostic);
        if (const auto pipe = find_channel<pipe_channel>(system, object, system_stdout)) {
            for (;;) {
                std::array<char, 4096> buffer{};
                const auto nread = pipe->read(buffer, std::cerr);
                if (nread == static_cast<std::size_t>(-1)) {
                    std::cerr << "ls can't read stdout\n";
                }
                else if (nread != 0u) {
                    std::cout << buffer.data();
                }
                else {
                    break;
                }
            }
        }
        else {
            std::cerr << "can't find " << system_stdout << "\n";
        }
        write_diags(system_name{}, object, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[" << std::size(object.children) << "]";
        std::cerr << "\n";
    }
}

}

auto main(int argc, const char * argv[]) -> int
{
    using namespace flow;
    set_signal_handler(signal::interrupt);
    set_signal_handler(signal::terminate);
    do_lsof_system();
    do_ls_system();
    do_nested_system();
    return 0;
}
