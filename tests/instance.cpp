#include <sstream> // for std::ostringstream

#include <gtest/gtest.h>

#include "flow/descriptor_id.hpp"
#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace {

constexpr auto no_such_path = "/fee/fii/foo/fum";

}

using namespace flow;
using namespace flow::descriptors;

TEST(instance, default_construction)
{
    instance obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<instance::custom>(obj.info));
}

TEST(instantiate, default_custom)
{
    std::ostringstream os;
    const auto obj = instantiate(system::custom{}, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(empty(obj.environment));
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
    EXPECT_THROW(instantiate(system::executable{}, os),
                 std::invalid_argument);
    EXPECT_TRUE(empty(os.str()));
}

TEST(instantiate, empty_executable)
{
    const auto expected_msg =
        "execve of '' failed: system:2 (No such file or directory)\n";
    const auto sys = flow::system{system::executable{}, {}, {}};
    std::ostringstream os;
    auto obj = instantiate(sys, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<instance::forked>(obj.info));
    if (std::holds_alternative<instance::forked>(obj.info)) {
        auto& info = std::get<instance::forked>(obj.info);
        EXPECT_TRUE(info.diags.is_open());
        EXPECT_TRUE(info.diags.good());
        EXPECT_TRUE(std::holds_alternative<owning_process_id>(info.state));
        if (std::holds_alternative<owning_process_id>(info.state)) {
            auto& state = std::get<owning_process_id>(info.state);
            const auto pid = int(state);
            EXPECT_GT(pid, 0);
            const auto result = state.wait();
            EXPECT_TRUE(std::holds_alternative<info_wait_result>(result));
            EXPECT_EQ(invalid_process_id,
                      reference_process_id(state));
            if (std::holds_alternative<info_wait_result>(result)) {
                const auto info = std::get<info_wait_result>(result);
                EXPECT_EQ(reference_process_id(pid), info.id);
                EXPECT_TRUE(std::holds_alternative<wait_exit_status>(info.status));
                if (const auto p = std::get_if<wait_exit_status>(&info.status)) {
                    EXPECT_EQ(p->value, 1);
                }
            }
            info.diags.seekg(0, std::ios_base::end);
            EXPECT_TRUE(info.diags.good());
            EXPECT_EQ(info.diags.tellg(), 58);
            EXPECT_TRUE(info.diags.good());
            info.diags.seekg(0, std::ios_base::beg);
            os.clear();
            std::copy(std::istreambuf_iterator<char>(info.diags),
                      std::istreambuf_iterator<char>(),
                      std::ostream_iterator<char>(os));
            EXPECT_EQ(os.str(), std::string(expected_msg));
        }
    }
}

TEST(instantiate, ls_system)
{
    const auto input_file_endpoint = file_endpoint{"flow.in"};
    const auto output_file_endpoint = file_endpoint{"flow.out"};
    touch(output_file_endpoint);
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
    system.connections.push_back(unidirectional_connection{
        system_endpoint{xargs_process_name, descriptor_id{2}},
        file_endpoint::dev_null,
    });
    system.connections.push_back(xargs_stdout);

    {
        std::ostringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instantiate(system, diags);
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(os));
        EXPECT_FALSE(empty(os.str()));
        EXPECT_TRUE(empty(object.environment));
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
        for (auto&& result: wait(system_name{}, object)) {
            EXPECT_TRUE(std::holds_alternative<info_wait_result>(result));
            if (std::holds_alternative<info_wait_result>(result)) {
                const auto& info = std::get<info_wait_result>(result);
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
            os.clear();
            EXPECT_NO_THROW(read(*xargs_stdout_pipe,
                                 std::ostream_iterator<char>(os)));
            const auto result = os.str();
            EXPECT_FALSE(empty(result));
        }
    }
}

TEST(instantiate, ls_outerr_system)
{
    const auto ls_exe_name = system_name{"ls-exe"};
    const auto ls_exe_sys = system::executable{
        .executable_file = "/bin/ls",
        .arguments = {"ls", no_such_path, "/"},
    };
    const auto ls_in = unidirectional_connection{
        file_endpoint::dev_null,
        system_endpoint{ls_exe_name, stdin_id},
    };
    const auto ls_outerr = unidirectional_connection{
        system_endpoint{ls_exe_name, stdout_id, stderr_id},
        user_endpoint{},
    };
    system::custom custom;
    custom.subsystems = {{ls_exe_name, ls_exe_sys}};
    custom.connections = {ls_in, ls_outerr};
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
        EXPECT_TRUE(empty(object.environment));
        const auto pid = get_reference_process_id({ls_exe_name}, object);
        const auto expected_wait_result = wait_result{
            info_wait_result{pid, wait_exit_status{EXIT_FAILURE}}
        };
        const auto wait_results = wait(system_name{}, object);
        EXPECT_EQ(size(wait_results), 1u);
        for (auto&& result: wait_results) {
            EXPECT_EQ(result, expected_wait_result);
        }
        EXPECT_NE(pipe, nullptr);
        if (pipe) {
            os.clear();
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
    const auto env_exe_name = system_name{"env-exe"};
    const auto env_out = unidirectional_connection{
        system_endpoint{env_exe_name, stdout_id},
        user_endpoint{},
    };
    const auto env_exe_sys = flow::system{
        system::executable{"/usr/bin/env"},
        {stdout_descriptors_entry},
        {{base_env_name, derived_env_val}}
    };
    flow::system base;
    base.environment = {{base_env_name, base_env_val}};
    base.info = system::custom{
        .subsystems = {{env_exe_name, env_exe_sys}},
        .connections = {env_out},
    };
    {
        std::ostringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instance{};
        EXPECT_NO_THROW(object = instantiate(base, diags, get_environ()));
        EXPECT_FALSE(empty(object.environment));
        const auto info = std::get_if<instance::custom>(&object.info);
        EXPECT_NE(info, nullptr);
        auto pipe = static_cast<pipe_channel*>(nullptr);
        if (info) {
            EXPECT_EQ(size(info->channels), 1u);
            if (size(info->channels) == 1u) {
                pipe = std::get_if<pipe_channel>(&(info->channels[0]));
            }
            EXPECT_EQ(size(info->children), 1u);
        }
        const auto pid = get_reference_process_id({env_exe_name}, object);
        EXPECT_NE(pid, invalid_process_id);
        const auto expected_wait_result = wait_result{
            info_wait_result{pid, wait_exit_status{EXIT_SUCCESS}}
        };
        for (auto&& result: wait(system_name{}, object)) {
            EXPECT_EQ(result, expected_wait_result);
        }
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
    }
}
