#include <iostream>
#include <iterator>
#include <memory>
#include <string_view>
#include <type_traits>

#include "flow/channel.hpp"
#include "flow/connection.hpp"
#include "flow/descriptor_id.hpp"
#include "flow/indenting_ostreambuf.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/system_name.hpp"
#include "flow/system_pipeline.hpp"
#include "flow/utility.hpp"
#include "flow/wait_result.hpp"

namespace {

using namespace flow;

constexpr auto no_such_path = "/fee/fii/foo/fum";

/// @brief Finds the channel requested.
/// @return Pointer to channel of the type requested or <code>nullptr</code>.
template <class T>
auto find_channel(const flow::system::custom& custom,
                  instance& instance,
                  const connection& look_for) -> T*
{
    if (const auto found = find_index(custom.connections, look_for)) {
        if (const auto p = std::get_if<instance::custom>(&instance.info)) {
            return std::get_if<T>(&(p->channels[*found]));
        }
    }
    return nullptr;
}

using namespace descriptors;

auto do_lsof_system() -> void
{
    std::cerr << "Doing lsof instance...\n";

    const auto lsof_name = system_name{"lsof"};
    const auto lsof_stdout = unidirectional_connection{
        system_endpoint{lsof_name, stdout_id}, user_endpoint{},
    };

    flow::system::custom custom;
    custom.subsystems.emplace(lsof_name, flow::system::executable{
        .executable_file = "/usr/sbin/lsof",
        .arguments = {"lsof", "-p", "$$"},
        .working_directory = "/usr/local",
    });
    custom.connections.push_back(unidirectional_connection{
        file_endpoint::dev_null, system_endpoint{lsof_name, stdin_id},
    });
    custom.connections.push_back(lsof_stdout);
    custom.connections.push_back(unidirectional_connection{
        system_endpoint{lsof_name, stderr_id}, file_endpoint::dev_null,
    });
    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, custom, diags);
        {
            std::cerr << "Diagnostics for instantiation of lsof...\n";
            diags.seekg(0);
            flow::detail::indenting_ostreambuf indenter{std::cerr};
            std::copy(std::istreambuf_iterator<char>(diags),
                      std::istreambuf_iterator<char>(),
                      std::ostream_iterator<char>(std::cerr));
            pretty_print(std::cerr, object);
        }
        for (auto&& wait_result: wait(system_name{}, object)) {
            std::cerr << "wait-result: " << wait_result << "\n";
        }
        if (const auto p = std::get_if<instance::custom>(&object.info)) {
            const auto ws = get_wait_status(p->children[lsof_name]);
            if (ws != wait_status{wait_exit_status{0}}) {
                std::cerr << "unexpected wait status: " << ws << "\n";
            }
        }
        if (const auto p = find_channel<pipe_channel>(custom, object,
                                                      lsof_stdout)) {
            read(*p, std::ostream_iterator<char>(std::cerr));
        }
        else {
            std::cerr << "no pipe for lsof_stdout?!\n";
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

    system::custom system;

    const auto cat_process_name = system_name{"cat"};
    system::executable cat_executable;
    cat_executable.executable_file = "/bin/cat";
    system.subsystems.emplace(cat_process_name, cat_executable);

    const auto xargs_process_name = system_name{"xargs"};
    system::executable xargs_executable;
    xargs_executable.executable_file = "/usr/bin/xargs";
    xargs_executable.working_directory = no_such_path;
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
    system.connections.push_back(unidirectional_connection{
        system_endpoint{cat_process_name, descriptor_id{2}},
        file_endpoint::dev_null,
    });
    system.connections.push_back(xargs_stdout);
    system.connections.push_back(unidirectional_connection{
        system_endpoint{xargs_process_name, descriptor_id{2}},
        file_endpoint::dev_null,
    });

    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, system, diags);
        std::cerr << "Diagnostics for root instance...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        if (const auto pipe = find_channel<pipe_channel>(system, object,
                                                         cat_stdin)) {
            write(*pipe, "/bin\n/sbin");
        }
        else {
            std::cerr << "no pipe for cat_stdin?!\n";
        }
        const auto outpipe = find_channel<pipe_channel>(system, object,
                                                        xargs_stdout);
        if (!outpipe) {
            std::cerr << "no pipe for xargs_stdout?!\n";
        }
        for (auto&& wait_result: wait(system_name{}, object)) {
            std::cerr << "wait-result: " << wait_result << "\n";
        }
        if (outpipe) {
            read(*outpipe, std::ostream_iterator<char>(std::cerr));
        }

        write_diags(system_name{}, object, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[";
        std::cerr << size(std::get<instance::custom>(object.info).children);
        std::cerr << "]";
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

    system::custom system;

    {
        system::custom cat_system;
        system::executable cat_executable;
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
        cat_system.connections.push_back(unidirectional_connection{
            system_endpoint{cat_process_name, descriptor_id{2}},
            system_endpoint{system_name{}, descriptor_id{2}},
        });
        system.subsystems.emplace(cat_system_name,
                                  flow::system{cat_system, std_descriptors});
    }

    {
        system::custom xargs_system;
        system::executable xargs_executable;
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
        xargs_system.connections.push_back(unidirectional_connection{
            system_endpoint{xargs_process_name, descriptor_id{2}},
            system_endpoint{system_name{}, descriptor_id{2}},
        });
        system.subsystems.emplace(xargs_system_name,
                                  flow::system{xargs_system, std_descriptors});
    }

    const auto system_stdin = unidirectional_connection{
        user_endpoint{},
        system_endpoint{cat_system_name, descriptor_id{0}},
    };
    const auto system_stdout = unidirectional_connection{
        system_endpoint{xargs_system_name, descriptor_id{1}},
        user_endpoint{},
    };

    system.connections.push_back(system_stdin);
    system.connections.push_back(unidirectional_connection{
        system_endpoint{cat_system_name, descriptor_id{1}},
        system_endpoint{xargs_system_name, descriptor_id{0}},
    });
    system.connections.push_back(unidirectional_connection{
        system_endpoint{cat_system_name, descriptor_id{2}},
        file_endpoint::dev_null,
    });
    system.connections.push_back(unidirectional_connection{
        system_endpoint{xargs_system_name, descriptor_id{2}},
        file_endpoint::dev_null,
    });
    system.connections.push_back(system_stdout);

    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, system, diags);
        std::cerr << "Diagnostics for nested instance...\n";
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        pretty_print(std::cerr, object);
        if (const auto pipe = find_channel<pipe_channel>(system, object,
                                                         system_stdin)) {
            write(*pipe, "/bin\n/sbin");
        }
        else {
            std::cerr << "can't find " << system_stdin << "\n";
        }
        for (auto&& wait_result: wait(system_name{}, object)) {
            std::cerr << "wait-result: " << wait_result << "\n";
        }
        if (const auto pipe = find_channel<pipe_channel>(system, object, system_stdout)) {
            read(*pipe, std::ostream_iterator<char>(std::cerr));
        }
        else {
            std::cerr << "can't find " << system_stdout << "\n";
        }
        write_diags(system_name{}, object, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[";
        std::cerr << size(std::get<instance::custom>(object.info).children);
        std::cerr << "]";
        std::cerr << "\n";
    }
}

auto do_env_system() -> void
{
    std::cerr << "Doing env instance...\n";
    const auto env_system_name = system_name{"env-system"};
    const auto env_out = unidirectional_connection{
        system_endpoint{env_system_name, stdout_id},
        user_endpoint{},
    };
    const auto custom = system::custom{
        .subsystems = {{
            system_name{"env-system"},
            flow::system{
                system::executable{"/usr/bin/env"},
                {stdout_descriptors_entry},
                {{"base", "derived value"}}
            }
        }},
        .connections = {env_out},
    };
    flow::system base;
    base.environment = {{"base", "base value"}};
    base.info = custom;
    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, base, diags, get_environ());
        for (auto&& wait_result: wait(system_name{}, object)) {
            std::cerr << "wait-result: " << wait_result << "\n";
        }
        if (const auto pipe = find_channel<pipe_channel>(custom, object,
                                                         env_out)) {
            read(*pipe, std::ostream_iterator<char>(std::cerr));
        }
    }
}

auto do_ls_outerr_system() -> void
{
    std::cerr << "Doing ls_outerr instance...\n";
    const auto ls_exe_name = system_name{"ls-exe"};
    const auto ls_outerr = unidirectional_connection{
        system_endpoint{ls_exe_name, stdout_id, stderr_id},
        user_endpoint{},
    };
    system::custom custom;
    custom.subsystems.emplace(ls_exe_name, system::executable{
        .executable_file = "/bin/ls",
        .arguments = {"ls", no_such_path, "/"},
    });
    custom.connections.push_back(unidirectional_connection{
        file_endpoint::dev_null,
        system_endpoint{ls_exe_name, stdin_id},
    });
    custom.connections.push_back(ls_outerr);
    {
        auto diags = temporary_fstream();
        auto object = instantiate(system_name{}, custom, diags);
        const auto pid = get_reference_process_id({system_name{"ls-exe"}},
                                                  object);
        const auto expected_wait_result = wait_result{
            wait_result::info_t{pid, wait_exit_status{1}}
        };
        const auto wait_results = wait(system_name{}, object);
        if (size(wait_results) != 1u) {
            std::cerr << "unexpected count of wait results:\n";
        }
        for (auto&& result: wait_results) {
            if (result != expected_wait_result) {
                std::cerr << "unexpected wait-result: " << result << "\n";
            }
        }
        if (const auto pipe = find_channel<pipe_channel>(custom, object,
                                                         ls_outerr)) {
            std::ostringstream os;
            read(*pipe, std::ostream_iterator<char>(os));
            if (std::string_view{os.str()}.find(no_such_path) ==
                std::string_view::npos) {
                std::cerr << "Search string '" << no_such_path;
                std::cerr << "' not found in outerr!\n";
            }
            else {
                std::cerr << "Search string '" << no_such_path;
                std::cerr << "' was found in outerr. Hooray!\n";
            }
        }
        else {
            std::cerr << "can't find " << ls_outerr << "\n";
        }
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
    do_env_system();
    do_ls_outerr_system();
    return 0;
}
