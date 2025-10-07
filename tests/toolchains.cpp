// tests_toolchain.cpp
// Contains unit tests for the Toolchain module, specifically MsvcToolchain.

import std;
import executor;
import toolchains;

#include <cassert>

using namespace importa;
using namespace importa::toolchains;
using namespace importa::executor;
using path = std::filesystem::path;

// --- Helper Utilities ---
namespace
{
bool has_flag(const std::vector<std::string>& args, std::string_view flag)
{
    for (const auto& arg : args)
    {
        if (arg == flag)
        {
            return true;
        }
    }
    return false;
}

bool has_flag_with_prefix(const std::vector<std::string>& args,
                          std::string_view prefix)
{
    for (const auto& arg : args)
    {
        if (arg.starts_with(prefix))
        {
            return true;
        }
    }
    return false;
}
} // namespace

// --- Test Suite for MsvcToolchain ---

void test_msvc_toolchain()
{
    std::cout << "--- Running Test Suite: MsvcToolchain ---\n";

    auto debug_config = BuildConfigurationFactory::create_debug_default();
    debug_config.include_dirs.push_back("C:/includes");
    debug_config.library_dirs.push_back("C:/libs");

    MsvcToolchain msvc("cl.exe", "link.exe", debug_config);

    // Test 1A: generate_compile_obj_command (for modular code)
    {
        CompileObjectArgs args;
        args.source_file = "src/main.cpp";
        args.output_obj_path = "build/main.obj";
        args.module_dependencies.push_back(
            { .name = "Core", .ifc_path = "build/Core.ifc" });

        auto cmd_opt = msvc.generate_compile_obj_command(args);
        assert(cmd_opt.has_value());
        const auto& cmd = *cmd_opt;

        assert(cmd.executable == "cl.exe");
        assert(has_flag(cmd.arguments, "/Od"));
        assert(has_flag(cmd.arguments, "/Zi"));
        assert(has_flag(cmd.arguments, "/MDd"));
        assert(has_flag(cmd.arguments, "/D_DEBUG"));
        assert(has_flag_with_prefix(cmd.arguments, "/IC:/includes"));
        assert(has_flag(cmd.arguments, "src/main.cpp"));
        assert(has_flag_with_prefix(cmd.arguments, "/Fo:build/main.obj"));
        assert(has_flag(cmd.arguments, "Core=build/Core.ifc"));
        std::cout << "  Test 1A: generate_compile_obj_command... Passed\n";
    }

    // Test 1B: generate_emit_ifc_command
    {
        EmitIFCArgs args;
        args.interface_unit_path = "src/Core.ixx";
        args.output_ifc_path = "build/Core.ifc";

        auto cmd_opt = msvc.generate_emit_ifc_command(args);
        assert(cmd_opt.has_value());
        const auto& cmd = *cmd_opt;

        assert(has_flag(cmd.arguments, "/interface"));
        assert(has_flag(cmd.arguments, "src/Core.ixx"));
        assert(has_flag_with_prefix(cmd.arguments, "/ifcOutput"));
        assert(has_flag(cmd.arguments, "build/Core.ifc"));

        //assert(has_flag_with_prefix(cmd.arguments, "/Fo:build/Core.obj"));

        // 1. 使用与实现相同的逻辑来构造预期的 .obj 路径
        path expected_obj_path = args.output_ifc_path.parent_path() / (args.output_ifc_path.stem().string() + ".obj");
        
        // 2. 用这个构造好的路径来创建预期的标志字符串
        std::string expected_fo_flag = "/Fo:" + expected_obj_path.string();
        
        // 3. 用这个自适应的字符串进行断言
        assert(has_flag_with_prefix(cmd.arguments, expected_fo_flag));
        std::cout << "  Test 1B: generate_emit_ifc_command... Passed\n";
    }

    // Test 1C: generate_link_command
    {
        LinkArgs args;
        args.object_files = { "build/main.obj", "build/Core.obj" };
        args.output_target_path = "build/app.exe";
        args.link_libraries = { "kernel32.lib" };

        auto cmd_opt = msvc.generate_link_command(args);
        assert(cmd_opt.has_value());
        const auto& cmd = *cmd_opt;

        assert(cmd.executable == "link.exe");
        assert(has_flag_with_prefix(cmd.arguments, "/OUT:build/app.exe"));
        assert(has_flag(cmd.arguments, "/DEBUG:FULL"));
        assert(has_flag_with_prefix(cmd.arguments, "/LIBPATH:\"C:/libs\""));
        assert(has_flag(cmd.arguments, "build/main.obj"));
        assert(has_flag(cmd.arguments, "build/Core.obj"));
        assert(has_flag(cmd.arguments, "kernel32.lib"));
        std::cout << "  Test 1C: generate_link_command... Passed\n";
    }
    std::cout << "--- MsvcToolchain tests all passed ---\n\n";
}

int main()
{
    try
    {
        test_msvc_toolchain();
    }
    catch (const std::exception& e)
    {
        std::cerr << "!!! A test failed with exception: " << e.what()
                  << std::endl;
        return 1;
    }

    std::cout << "All Toolchain tests passed successfully!\n";
    return 0;
}