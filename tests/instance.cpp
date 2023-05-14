#include <sstream> // for std::ostringstream

#include <gtest/gtest.h>

#include "flow/instance.hpp"
#include "flow/system.hpp"
#include "flow/utility.hpp"

namespace {

constexpr auto no_such_path = "/fee/fii/foo/fum";

}

TEST(instance, default_construction)
{
    flow::instance obj;
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(obj.info));
}

TEST(instance, instantiate_default_custom)
{
    std::ostringstream os;
    const auto obj = instantiate(flow::system_name{},
                                 flow::system::custom{}, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(obj.info));
    if (std::holds_alternative<flow::instance::custom>(obj.info)) {
        const auto& info = std::get<flow::instance::custom>(obj.info);
        EXPECT_TRUE(empty(info.children));
        EXPECT_TRUE(empty(info.channels));
        EXPECT_EQ(info.pgrp, flow::no_process_id);
    }
}

TEST(instance, instantiate_default_executable)
{
    std::ostringstream os;
    EXPECT_THROW(instantiate(flow::system_name{}, flow::system::executable{},
                             os),
                 std::invalid_argument);
    EXPECT_TRUE(empty(os.str()));
}

TEST(instance, instantiate_empty_executable)
{
    const auto expected_msg =
        "execve of '' failed: system:2 (No such file or directory)\n";
    const auto sys = flow::system{flow::system::executable{}, {}, {}};
    std::ostringstream os;
    auto obj = instantiate(flow::system_name{}, sys, os);
    EXPECT_TRUE(empty(os.str()));
    EXPECT_TRUE(empty(obj.environment));
    EXPECT_TRUE(std::holds_alternative<flow::instance::forked>(obj.info));
    if (std::holds_alternative<flow::instance::forked>(obj.info)) {
        auto& info = std::get<flow::instance::forked>(obj.info);
        EXPECT_TRUE(info.diags.is_open());
        EXPECT_TRUE(info.diags.good());
        EXPECT_TRUE(std::holds_alternative<flow::owning_process_id>(info.state));
        if (std::holds_alternative<flow::owning_process_id>(info.state)) {
            auto& state = std::get<flow::owning_process_id>(info.state);
            const auto pid = int(state);
            EXPECT_GT(pid, 0);
            const auto result = state.wait();
            EXPECT_EQ(result.type(), flow::wait_result::has_info);
            EXPECT_EQ(flow::invalid_process_id,
                      flow::reference_process_id(state));
            if (result.holds_info()) {
                const auto info = result.info();
                EXPECT_EQ(flow::reference_process_id(pid), info.id);
                EXPECT_TRUE(std::holds_alternative<flow::wait_exit_status>(info.status));
                if (const auto p = std::get_if<flow::wait_exit_status>(&info.status)) {
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

TEST(instance, ls_system)
{
    const auto input_file_endpoint = flow::file_endpoint{"flow.in"};
    const auto output_file_endpoint = flow::file_endpoint{"flow.out"};
    touch(output_file_endpoint);
    flow::system::custom system;

    const auto cat_process_name = flow::system_name{"cat"};
    flow::system::executable cat_executable;
    cat_executable.executable_file = "/bin/cat";
    system.subsystems.emplace(cat_process_name, cat_executable);

    const auto xargs_process_name = flow::system_name{"xargs"};
    flow::system::executable xargs_executable;
    xargs_executable.executable_file = "/usr/bin/xargs";
    xargs_executable.working_directory = no_such_path;
    xargs_executable.arguments = {"xargs", "ls", "-alF"};
    system.subsystems.emplace(xargs_process_name, xargs_executable);

    const auto cat_stdin = flow::unidirectional_connection{
        flow::user_endpoint{},
        flow::system_endpoint{cat_process_name, flow::descriptor_id{0}}
    };
    const auto xargs_stdout = flow::unidirectional_connection{
        flow::system_endpoint{xargs_process_name, flow::descriptor_id{1}},
        flow::user_endpoint{},
    };
    system.connections.push_back(cat_stdin);
    system.connections.push_back(flow::unidirectional_connection{
        flow::system_endpoint{cat_process_name, flow::descriptor_id{1}},
        flow::system_endpoint{xargs_process_name, flow::descriptor_id{0}},
    });
    system.connections.push_back(flow::unidirectional_connection{
        flow::system_endpoint{cat_process_name, flow::descriptor_id{2}},
        flow::file_endpoint::dev_null,
    });
    system.connections.push_back(flow::unidirectional_connection{
        flow::system_endpoint{xargs_process_name, flow::descriptor_id{2}},
        flow::file_endpoint::dev_null,
    });
    system.connections.push_back(xargs_stdout);

    {
        std::ostringstream os;
        auto diags = ext::temporary_fstream();
        auto object = instantiate(flow::system_name{}, system, diags);
        diags.seekg(0);
        std::copy(std::istreambuf_iterator<char>(diags),
                  std::istreambuf_iterator<char>(),
                  std::ostream_iterator<char>(os));
        EXPECT_FALSE(empty(os.str()));
        EXPECT_TRUE(empty(object.environment));
        EXPECT_TRUE(std::holds_alternative<flow::instance::custom>(object.info));
        auto cat_stdin_pipe = static_cast<flow::pipe_channel*>(nullptr);
        auto xargs_stdout_pipe = static_cast<flow::pipe_channel*>(nullptr);
        auto cat_info = static_cast<flow::instance::forked*>(nullptr);
        auto xargs_info = static_cast<flow::instance::forked*>(nullptr);
        auto cat_pid = flow::no_process_id;
        auto xargs_pid = flow::no_process_id;
        if (const auto p = std::get_if<flow::instance::custom>(&object.info)) {
            EXPECT_EQ(size(p->channels), 5u);
            if (size(p->channels) == 5u) {
                cat_stdin_pipe = std::get_if<flow::pipe_channel>(&(p->channels[0]));
                xargs_stdout_pipe = std::get_if<flow::pipe_channel>(&(p->channels[4u]));
            }
            EXPECT_EQ(size(p->children), 2u);
            if (size(p->children) == 2u) {
                if (const auto f = p->children.find(cat_process_name); f != p->children.end()) {
                    cat_info = std::get_if<flow::instance::forked>(&(f->second.info));
                }
                if (const auto f = p->children.find(xargs_process_name); f != p->children.end()) {
                    xargs_info = std::get_if<flow::instance::forked>(&(f->second.info));
                }
                EXPECT_EQ(p->children.count(cat_process_name), 1u);
                EXPECT_EQ(p->children.count(xargs_process_name), 1u);
            }
        }
        if (cat_info) {
            if (const auto p = std::get_if<flow::owning_process_id>(&(cat_info->state))) {
                cat_pid = flow::reference_process_id(*p);
            }
        }
        if (xargs_info) {
            if (const auto p = std::get_if<flow::owning_process_id>(&(xargs_info->state))) {
                xargs_pid = flow::reference_process_id(*p);
            }
        }
        EXPECT_NE(cat_pid, flow::no_process_id);
        EXPECT_NE(cat_stdin_pipe, nullptr);
        EXPECT_NE(xargs_stdout_pipe, nullptr);
        if (cat_stdin_pipe) {
            write(*cat_stdin_pipe, "/bin\n/sbin");
        }
        auto waited = 0;
        for (auto&& wait_result: wait(flow::system_name{}, object)) {
            EXPECT_EQ(wait_result.type(), flow::wait_result::has_info);
            if (wait_result.holds_info()) {
                const auto& info = wait_result.info();
                EXPECT_TRUE(info.id == cat_pid || info.id == xargs_pid);
                if (info.id == cat_pid) {
                    EXPECT_TRUE(std::holds_alternative<flow::wait_signaled_status>(info.status));
                    if (const auto p = std::get_if<flow::wait_signaled_status>(&info.status)) {
                        EXPECT_EQ(p->signal, SIGPIPE);
                        EXPECT_FALSE(p->core_dumped);
                    }
                }
                else if (info.id == xargs_pid) {
                    EXPECT_TRUE(std::holds_alternative<flow::wait_exit_status>(info.status));
                    if (const auto p = std::get_if<flow::wait_exit_status>(&info.status)) {
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
            read(*xargs_stdout_pipe, std::ostream_iterator<char>(os));
            const auto result = os.str();
            EXPECT_FALSE(empty(result));
        }
    }
}
