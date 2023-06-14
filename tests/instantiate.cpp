#include <chrono>
#include <sstream> // for std::ostringstream
#include <thread> // for std::this_thread

#include <gtest/gtest.h>

#include "flow/reference_descriptor.hpp"
#include "flow/instantiate.hpp"
#include "flow/utility.hpp"

namespace {
constexpr auto no_such_path = "/fee/fii/foo/fum";
}

using namespace flow;
using namespace flow::descriptors;

TEST(instantiate, default_custom)
{
    std::ostringstream os;
    const auto obj = instantiate(custom{}, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(std::holds_alternative<instance::custom>(obj.info));
    if (std::holds_alternative<instance::custom>(obj.info)) {
        const auto& info = std::get<instance::custom>(obj.info);
        EXPECT_TRUE(empty(info.children));
        EXPECT_TRUE(empty(info.channels));
        EXPECT_EQ(info.pgrp, no_process_id);
    }
}

TEST(instantiate, default_executable)
{
    std::ostringstream os;
    EXPECT_THROW(instantiate(executable{}, os),
                 invalid_executable);
    EXPECT_TRUE(empty(os.str()));
}

TEST(instantiate, empty_executable)
{
    const auto sys = flow::node{executable{}, {}};
    std::ostringstream os;
    auto obj = instance{};
    EXPECT_THROW(obj = instantiate(sys, os), invalid_executable);
    EXPECT_TRUE(empty(os.str()));
}

TEST(instantiate, ls_system)
{
    const auto input_file_endpoint = file_endpoint{"flow.in"};
    const auto output_file_endpoint = file_endpoint{"flow.out"};
    touch(output_file_endpoint);
    custom system;

    const auto cat_process_name = node_name{"cat"};
    executable cat_executable;
    cat_executable.file = "/bin/cat";
    system.nodes.emplace(cat_process_name, cat_executable);

    const auto xargs_process_name = node_name{"xargs"};
    executable xargs_executable;
    xargs_executable.file = "/usr/bin/xargs";
    xargs_executable.working_directory = no_such_path;
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    system.nodes.emplace(xargs_process_name, xargs_executable);

    const auto cat_stdin = unidirectional_link{
        user_endpoint{},
        node_endpoint{cat_process_name, reference_descriptor{0}}
    };
    const auto xargs_stdout = unidirectional_link{
        node_endpoint{xargs_process_name, reference_descriptor{1}},
        user_endpoint{},
    };
    system.links.push_back(cat_stdin);
    system.links.push_back(unidirectional_link{
        node_endpoint{cat_process_name, reference_descriptor{1}},
        node_endpoint{xargs_process_name, reference_descriptor{0}},
    });
    system.links.push_back(unidirectional_link{
        node_endpoint{cat_process_name, reference_descriptor{2}},
        file_endpoint::dev_null,
    });
    system.links.push_back(unidirectional_link{
        node_endpoint{xargs_process_name, reference_descriptor{2}},
        file_endpoint::dev_null,
    });
    system.links.push_back(xargs_stdout);

    {
        std::ostringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instantiate(system, diags);
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(os));
        EXPECT_FALSE(empty(os.str()));
        std::cerr << os.str() << '\n';
        EXPECT_TRUE(std::holds_alternative<instance::custom>(object.info));
        auto cat_stdin_pipe = static_cast<pipe_channel*>(nullptr);
        auto xargs_stdout_pipe = static_cast<pipe_channel*>(nullptr);
        auto cat_info = static_cast<instance::forked*>(nullptr);
        auto xargs_info = static_cast<instance::forked*>(nullptr);
        auto cat_pid = no_process_id;
        auto xargs_pid = no_process_id;
        if (const auto p = std::get_if<instance::custom>(&object.info)) {
            EXPECT_EQ(size(p->channels), 5u);
            if (size(p->channels) == 5u) {
                cat_stdin_pipe = std::get_if<pipe_channel>(&(p->channels[0]));
                xargs_stdout_pipe = std::get_if<pipe_channel>(&(p->channels[4u]));
            }
            EXPECT_EQ(size(p->children), 2u);
            if (size(p->children) == 2u) {
                if (const auto f = p->children.find(cat_process_name); f != p->children.end()) {
                    cat_info = std::get_if<instance::forked>(&(f->second.info));
                }
                if (const auto f = p->children.find(xargs_process_name); f != p->children.end()) {
                    xargs_info = std::get_if<instance::forked>(&(f->second.info));
                }
                EXPECT_EQ(p->children.count(cat_process_name), 1u);
                EXPECT_EQ(p->children.count(xargs_process_name), 1u);
            }
        }
        if (cat_info) {
            if (const auto p = std::get_if<owning_process_id>(&(cat_info->state))) {
                cat_pid = reference_process_id(*p);
            }
        }
        if (xargs_info) {
            if (const auto p = std::get_if<owning_process_id>(&(xargs_info->state))) {
                xargs_pid = reference_process_id(*p);
            }
        }
        EXPECT_NE(cat_pid, no_process_id);
        EXPECT_NE(cat_stdin_pipe, nullptr);
        EXPECT_NE(xargs_stdout_pipe, nullptr);
        if (cat_stdin_pipe) {
            EXPECT_NO_THROW(write(*cat_stdin_pipe, "/bin\n/sbin"));
        }
        auto waited = 0;
        for (auto&& result: flow::wait(object)) {
            EXPECT_TRUE(std::holds_alternative<info_wait_result>(result));
            if (std::holds_alternative<info_wait_result>(result)) {
                const auto& info = std::get<info_wait_result>(result);
                EXPECT_GT(info.id, flow::no_process_id);
                EXPECT_TRUE(info.id == cat_pid || info.id == xargs_pid);
                if (info.id == cat_pid) {
                    // Maybe there's a race between cat & xargs where if xargs
                    // exits before cat outputs, then cat gets SIGPIPE, else
                    // exits with status 0?
                    if (const auto p = std::get_if<wait_signaled_status>(&info.status)) {
                        EXPECT_EQ(p->signal, SIGPIPE);
                        EXPECT_FALSE(p->core_dumped);
                    }
                    else if (const auto p = std::get_if<wait_exit_status>(&info.status)) {
                        EXPECT_EQ(p->value, 0);
                    }
                }
                else if (info.id == xargs_pid) {
                    EXPECT_TRUE(std::holds_alternative<wait_exit_status>(info.status));
                    if (const auto p = std::get_if<wait_exit_status>(&info.status)) {
                        EXPECT_EQ(p->value, 1); // because no_such_path used
                    }
                }
                else {
                    FAIL(); // unknown pid
                }
            }
            ++waited;
        }
        EXPECT_EQ(waited, 2);
        if (xargs_stdout_pipe) {
            os.str(std::string());
            EXPECT_NO_THROW(read(*xargs_stdout_pipe,
                                 std::ostream_iterator<char>(os)));
            const auto result = os.str();
            EXPECT_TRUE(empty(result));
        }
        write_diags(object, os);
        const auto diags_output = os.str();
        EXPECT_NE(diags_output, std::string());
    }
}

TEST(instantiate, ls_outerr_system)
{
    const auto ls_exe_name = node_name{"ls_exe"};
    const auto ls_exe_sys = executable{
        .file = "/bin/ls",
        .arguments = {"ls", no_such_path, "/"},
    };
    const auto ls_in = unidirectional_link{
        file_endpoint::dev_null,
        node_endpoint{ls_exe_name, stdin_id},
    };
    const auto ls_outerr = unidirectional_link{
        node_endpoint{ls_exe_name, stdout_id, stderr_id},
        user_endpoint{},
    };
    custom custom;
    custom.nodes = {{ls_exe_name, ls_exe_sys}};
    custom.links = {ls_in, ls_outerr};
    {
        std::ostringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instantiate(custom, diags);
        const auto info = std::get_if<instance::custom>(&object.info);
        auto pipe = static_cast<pipe_channel*>(nullptr);
        EXPECT_NE(info, nullptr);
        if (info) {
            EXPECT_EQ(size(info->channels), 2u);
            EXPECT_EQ(size(info->children), 1u);
            if (size(info->channels) == 2u) {
                pipe = std::get_if<pipe_channel>(&(info->channels[1]));
            }
        }
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(os));
        EXPECT_NE(os.str(), std::string());
        const auto pid = get_reference_process_id({ls_exe_name}, object);
        const auto wait_results = flow::wait(object);
        EXPECT_EQ(size(wait_results), 1u);
        for (auto&& result: wait_results) {
            EXPECT_TRUE(std::holds_alternative<info_wait_result>(result));
            if (std::holds_alternative<info_wait_result>(result)) {
                const auto& info_result = std::get<info_wait_result>(result);
                EXPECT_EQ(info_result.id, pid);
                EXPECT_EQ(info_result.status.index(), 1u);
                if (std::holds_alternative<wait_exit_status>(info_result.status)) {
                    const auto& exits = std::get<wait_exit_status>(info_result.status);
                    EXPECT_NE(exits.value, EXIT_SUCCESS);
                }
            }
        }
        EXPECT_NE(pipe, nullptr);
        if (pipe) {
            os.str(std::string());
            EXPECT_NO_THROW(read(*pipe, std::ostream_iterator<char>(os)));
            EXPECT_NE(os.str().find(no_such_path), std::string::npos);
        }
    }
}

TEST(instantiate, env_system)
{
    const auto base_env_name = env_name{"base"};
    const auto base_env_val = env_value{"base value"};
    const auto derived_env_val = env_value{"derived value"};
    const auto env_exe_name = node_name{"env_exe"};
    const auto env_out = unidirectional_link{
        node_endpoint{env_exe_name, stdout_id},
        user_endpoint{},
    };
    const auto env_exe_sys = flow::node{
        executable{"/usr/bin/env"},
        {stdout_ports_entry}
    };
    auto overriding_system = flow::node{custom{
        .environment = {{base_env_name, derived_env_val}},
        .nodes = {{env_exe_name, env_exe_sys}},
        .links = {env_out},
    }};
    auto base = flow::node{custom{
        .environment = {{base_env_name, base_env_val}},
        .nodes = {{"overrider", overriding_system}},
    }};
    {
        std::ostringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instance{};
        const auto opts = instantiate_options{
            .environment = get_environ()
        };
        EXPECT_NO_THROW(object = instantiate(base, diags, opts));
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(os));
        EXPECT_FALSE(empty(os.str()));
        os.str({});
        ASSERT_TRUE(std::holds_alternative<instance::custom>(object.info));
        auto info = std::get_if<instance::custom>(&object.info);
        ASSERT_EQ(size(info->children), 1u);
        EXPECT_EQ(size(info->channels), 0u);
        auto& child = info->children.begin()->second;
        info = std::get_if<instance::custom>(&child.info);
        EXPECT_NE(info, nullptr);
        auto pipe = static_cast<pipe_channel*>(nullptr);
        if (info) {
            EXPECT_EQ(size(info->channels), 1u);
            if (size(info->channels) == 1u) {
                pipe = std::get_if<pipe_channel>(&(info->channels[0]));
            }
            EXPECT_EQ(size(info->children), 1u);
        }
        const auto pid = get_reference_process_id({env_exe_name}, child);
        EXPECT_NE(pid, invalid_process_id);
        const auto expected_wait_result = wait_result{
            info_wait_result{pid, wait_exit_status{EXIT_SUCCESS}}
        };
        auto nresults = 0;
        for (auto&& result: flow::wait(object)) {
            EXPECT_EQ(result, expected_wait_result);
            ++nresults;
        }
        EXPECT_EQ(nresults, 1);
        EXPECT_NE(pipe, nullptr);
        if (pipe) {
            EXPECT_NO_THROW(read(*pipe, std::ostream_iterator<char>(os)));
        }
        const auto output = os.str();
        EXPECT_FALSE(empty(output));
        const auto base_name = std::string(base_env_name);
        auto found = output.find(base_name);
        EXPECT_NE(found, std::string::npos);
        if (found != std::string::npos) {
            found += base_name.length();
            EXPECT_EQ(output.at(found), '=');
            if (output.at(found) == '=') {
                ++found;
            }
            EXPECT_EQ(output.find(std::string(base_env_val), found),
                      std::string::npos);
            EXPECT_NE(output.find(std::string(derived_env_val), found),
                      std::string::npos);
        }
        os.str({});
        write_diags(object, os);
        const auto diags_output = os.str();
        EXPECT_NE(diags_output, std::string());
    }
}

TEST(instantiate, lsof_system)
{
    const auto lsof_name = node_name{"lsof"};
    const auto lsof_stdout = unidirectional_link{
        node_endpoint{lsof_name, stdout_id}, user_endpoint{},
    };

    flow::custom custom;
    custom.environment = get_environ();
    custom.nodes.emplace(lsof_name, flow::node{flow::executable{
        .file = "lsof",
        .arguments = {"lsof", "-p", "$$"},
        .working_directory = "/usr/local",
    }, std_ports});
    custom.links.push_back(unidirectional_link{
        file_endpoint::dev_null, node_endpoint{lsof_name, stdin_id},
    });
    custom.links.push_back(lsof_stdout);
    custom.links.push_back(unidirectional_link{
        node_endpoint{lsof_name, stderr_id}, file_endpoint::dev_null,
    });
    {
        std::stringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instantiate(custom, diags);
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(os));
        EXPECT_FALSE(empty(os.str()));
        os.str(std::string());

        const auto info = std::get_if<instance::custom>(&object.info);
        auto pipe = static_cast<pipe_channel*>(nullptr);
        EXPECT_NE(info, nullptr);
        if (info) {
            EXPECT_EQ(size(info->channels), 3u);
            EXPECT_EQ(size(info->children), 1u);
            if (size(info->channels) == 3u) {
                EXPECT_NE(std::get_if<file_channel>(&(info->channels[0])),
                          nullptr);
                EXPECT_NE(std::get_if<pipe_channel>(&(info->channels[1])),
                          nullptr);
                EXPECT_NE(std::get_if<file_channel>(&(info->channels[2])),
                          nullptr);
                pipe = std::get_if<pipe_channel>(&(info->channels[1]));
            }
        }
        const auto pid = get_reference_process_id({lsof_name}, object);
        EXPECT_NE(pid, invalid_process_id);
        const auto expected_wait_result = wait_result{
            info_wait_result{pid, wait_exit_status{EXIT_SUCCESS}}
        };
        auto waited = 0;
        for (auto&& result: flow::wait(object)) {
            EXPECT_EQ(result, expected_wait_result);
            ++waited;
        }
        EXPECT_EQ(waited, 1);
        if (const auto p = std::get_if<instance::custom>(&object.info)) {
            const auto ws = get_wait_status(p->children[lsof_name]);
            EXPECT_EQ(ws, wait_status(wait_exit_status{0}));
        }
        EXPECT_NE(pipe, nullptr);
        if (pipe) {
            EXPECT_NO_THROW(read(*pipe, std::ostream_iterator<char>(os)));
        }
        const auto pipe_output = os.str();
        EXPECT_NE(pipe_output, std::string());
        os.str(std::string());
        write_diags(object, os);
        const auto diags_output = os.str();
        EXPECT_NE(diags_output, std::string());
        //std::cerr << diags_output;
    }
}

TEST(instantiate, nested_system)
{
    const auto cat_system_name = node_name{"cat_system"};
    const auto cat_process_name = node_name{"cat_process"};
    const auto xargs_system_name = node_name{"xargs_system"};
    const auto xargs_process_name = node_name{"xargs_process"};
    const custom system{
        .nodes = {
            {
                cat_system_name, flow::node{custom{
                    .nodes = {
                        {cat_process_name, executable{
                            .file = "/bin/cat"
                        }}
                    },
                    .links = {
                        unidirectional_link{
                            node_endpoint{node_name{}, reference_descriptor{0}},
                            node_endpoint{cat_process_name, reference_descriptor{0}}
                        },
                        unidirectional_link{
                            node_endpoint{cat_process_name, reference_descriptor{1}},
                            node_endpoint{node_name{}, reference_descriptor{1}},
                        },
                        unidirectional_link{
                            node_endpoint{cat_process_name, reference_descriptor{2}},
                            node_endpoint{node_name{}, reference_descriptor{2}},
                        }
                    }
                }, std_ports}
            },
            {
                xargs_system_name, flow::node{custom{
                    .nodes = {
                        {xargs_process_name, executable{
                            .file = "/usr/bin/xargs",
                            .arguments = {"xargs", "ls", "-alF"}
                        }}
                    },
                    .links = {
                        unidirectional_link{
                            node_endpoint{node_name{}, reference_descriptor{0}},
                            node_endpoint{xargs_process_name, reference_descriptor{0}}
                        },
                        unidirectional_link{
                            node_endpoint{xargs_process_name, reference_descriptor{1}},
                            node_endpoint{node_name{}, reference_descriptor{1}},
                        },
                        unidirectional_link{
                            node_endpoint{xargs_process_name, reference_descriptor{2}},
                            node_endpoint{node_name{}, reference_descriptor{2}},
                        }
                    }
                }, std_ports}
            }
        },
        .links = {
            unidirectional_link{
                user_endpoint{},
                node_endpoint{cat_system_name, reference_descriptor{0}},
            },
            unidirectional_link{
                node_endpoint{cat_system_name, reference_descriptor{1}},
                node_endpoint{xargs_system_name, reference_descriptor{0}},
            },
            unidirectional_link{
                node_endpoint{cat_system_name, reference_descriptor{2}},
                file_endpoint::dev_null,
            },
            unidirectional_link{
                node_endpoint{xargs_system_name, reference_descriptor{2}},
                file_endpoint::dev_null,
            },
            unidirectional_link{
                node_endpoint{xargs_system_name, reference_descriptor{1}},
                user_endpoint{},
            }
        }
    };
    {
        std::stringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instantiate(system, diags);
        const auto cat_names = {cat_process_name, cat_system_name};
        const auto cat_pid = get_reference_process_id(cat_names, object);
        EXPECT_NE(cat_pid, invalid_process_id);
        const auto xarg_names = {xargs_process_name, xargs_system_name};
        const auto xargs_pid = get_reference_process_id(xarg_names, object);
        EXPECT_NE(xargs_pid, invalid_process_id);
        const auto info = std::get_if<instance::custom>(&object.info);
        auto in_pipe = static_cast<pipe_channel*>(nullptr);
        auto out_pipe = static_cast<pipe_channel*>(nullptr);
        EXPECT_NE(info, nullptr);
        if (info) {
            EXPECT_EQ(size(info->channels), 5u);
            EXPECT_EQ(size(info->children), 2u);
            if (size(info->channels) == 5u) {
                EXPECT_NE(std::get_if<pipe_channel>(&(info->channels[0])),
                          nullptr);
                EXPECT_NE(std::get_if<pipe_channel>(&(info->channels[1])),
                          nullptr);
                EXPECT_NE(std::get_if<file_channel>(&(info->channels[2])),
                          nullptr);
                EXPECT_NE(std::get_if<file_channel>(&(info->channels[3])),
                          nullptr);
                EXPECT_NE(std::get_if<pipe_channel>(&(info->channels[4])),
                          nullptr);
                in_pipe = std::get_if<pipe_channel>(&(info->channels[0]));
                out_pipe = std::get_if<pipe_channel>(&(info->channels[4]));
            }
        }
        if (in_pipe) {
            EXPECT_NO_THROW(write(*in_pipe, "/bin\n/sbin"));
        }
        for (auto&& result: flow::wait(object)) {
            std::visit(detail::overloaded{
                [](auto){
                    FAIL();
                },
                [&](const info_wait_result& arg){
                    EXPECT_TRUE(arg.id == cat_pid || arg.id == xargs_pid);
                    EXPECT_EQ(arg.status,
                              wait_status(wait_exit_status{EXIT_SUCCESS}));
                }
            }, result);
        }
        if (out_pipe) {
            EXPECT_NO_THROW(read(*out_pipe, std::ostream_iterator<char>(os)));
        }
        const auto output = os.str();
        EXPECT_NE(output, std::string());
        std::copy(begin(output), end(output),
                  std::ostream_iterator<char>(std::cerr));
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(std::cerr));
        write_diags(object, std::cerr);
    }
}

TEST(instantiate, bin_dd)
{
    const auto exe_name = node_name{"dd"};
    auto sys = flow::node{custom{
        .nodes = {{exe_name, {
            executable{
                .file = "/bin/dd",
                .arguments = {
                    "dd",
                    "iflag=fullblock",
                    "if=/dev/zero",
                    "of=/dev/null",
                    "count=50000000",
                    "bs=1000"
                }
            },
            {
                {
                    reference_descriptor{2},
                    {"stderr", io_type::out}
                },
                {
                    signals::dd_progress(),
                    {"puts progress info on stderr", io_type::in}
                },
            }
        }}},
        .links = {unidirectional_link{
            node_endpoint{exe_name, reference_descriptor{2}},
            user_endpoint{},
        }},
    }};
    {
        using namespace std::chrono_literals;
        std::stringstream os;
        auto instance = instantiate(sys, std::cerr);
        ASSERT_TRUE(std::holds_alternative<instance::custom>(instance.info));
        auto& info = std::get<instance::custom>(instance.info);
        ASSERT_EQ(size(info.channels), 1u);
        // Wait a moment before sending SIGINFO to help ensure dd is up
        // and has it's SIGINFO handler registered...
        std::this_thread::sleep_for(200ms);
        send_signal(signals::dd_progress(), instance, std::cerr);
        ASSERT_TRUE(std::holds_alternative<pipe_channel>(info.channels.front()));
        auto& pipe = std::get<pipe_channel>(info.channels.front());
        auto buffer = std::array<char, 1u>{};
        auto linenum = 0;
        while (linenum < 3) {
            if (pipe.read(buffer, std::cerr) == -1) {
                break;
            }
            const auto c = *buffer.data();
            std::cerr << char(c);
            os << c;
            if (c == '\n') {
                ++linenum;
                switch (linenum) {
                case 1:
                    EXPECT_NE(os.str().find("records in"), std::string::npos);
                    break;
                case 2:
                    EXPECT_NE(os.str().find("records out"), std::string::npos);
                    break;
                case 3:
                    EXPECT_NE(os.str().find("bytes"), std::string::npos);
                    break;
                }
                os.str(std::string{});
            }
        }
        EXPECT_EQ(linenum, 3);
        send_signal(signals::kill(), instance, std::cerr);
        const auto waits = wait(instance);
        ASSERT_EQ(size(waits), 1u);
        ASSERT_TRUE(std::holds_alternative<info_wait_result>(waits.front()));
        const auto& rv = std::get<info_wait_result>(waits.front());
        ASSERT_TRUE(std::holds_alternative<wait_signaled_status>(rv.status));
        const auto& es = std::get<wait_signaled_status>(rv.status);
        EXPECT_EQ(es.signal, int(signals::kill()));
    }
}
