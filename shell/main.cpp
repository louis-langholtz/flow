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

int main(int argc, const char * argv[])
{
    const auto input_file_port = flow::file_port{"flow.in"};

    const auto output_file_port = flow::file_port{"flow.out"};
    touch(output_file_port);
    std::cerr << "running in " << std::filesystem::current_path() << '\n';

    flow::system_prototype system;

    const auto cat_process_name = flow::prototype_name{"cat"};
    flow::executable_prototype cat_executable;
    cat_executable.executable_file = "/bin/cat";
    system.prototypes.emplace(cat_process_name, cat_executable);

    const auto xargs_process_name = flow::prototype_name{"xargs"};
    flow::executable_prototype xargs_executable;
    xargs_executable.executable_file = "/usr/bin/xargs";
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    system.prototypes.emplace(xargs_process_name, xargs_executable);

    const auto cat_stdin = flow::pipe_connection{
        flow::prototype_port{},
        flow::prototype_port{cat_process_name, flow::descriptor_id{0}}
    };
    system.connections.push_back(cat_stdin);
    system.connections.push_back(flow::pipe_connection{
        flow::prototype_port{cat_process_name, flow::descriptor_id{1}},
        flow::prototype_port{xargs_process_name, flow::descriptor_id{0}},
    });
    const auto xargs_stdout = flow::pipe_connection{
        flow::prototype_port{xargs_process_name, flow::descriptor_id{1}},
        flow::prototype_port{},
    };
    system.connections.push_back(xargs_stdout);
    const auto xargs_stderr = flow::pipe_connection{
        flow::prototype_port{xargs_process_name, flow::descriptor_id{2}},
        flow::prototype_port{},
    };
    system.connections.push_back(xargs_stderr);
#if 0
    system.connections.push_back(flow::file_connection{
        output_file_port,
        flow::io_type::out,
        flow::prototype_port{cat_process_name, flow::descriptor_id{1}}
    });
#endif

    {
        auto errs = flow::temporary_fstream();
        auto instance = instantiate(system, errs);
        if (const auto pipe = find_channel<flow::pipe_channel>(system, instance, cat_stdin)) {
            pipe->write("/bin\n/sbin", std::cerr);
            pipe->close(flow::io_type::out, std::cerr);
        }
        wait(instance, std::cerr, flow::wait_diags::none);
        if (const auto pipe = find_channel<flow::pipe_channel>(system, instance, xargs_stderr)) {
            std::array<char, 1024> buffer{};
            const auto nread = pipe->read(buffer, std::cerr);
            if (nread == static_cast<std::size_t>(-1)) {
                std::cerr << "xargs can't read stderr\n";
            }
            else if (nread != 0u) {
                std::cerr << "xargs stderr: " << buffer.data() << "\n";
            }
        }
        if (const auto pipe = find_channel<flow::pipe_channel>(system, instance, xargs_stdout)) {
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
        errs.seekg(0);
        std::copy(std::istreambuf_iterator<char>(errs),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));

        show_diags(flow::prototype_name{}, instance, std::cerr);
        std::cerr << "system ran: ";
        std::cerr << ", children[" << std::size(instance.children) << "]";
        std::cerr << "\n";
    }
    return 0;
}
