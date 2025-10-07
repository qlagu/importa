// tests_module_processor.cpp
// Contains unit tests for the ModuleProcessor, using a mock toolchain.

import std;
import executor;
import toolchains;
import module_processor;

#include <cassert>

using path = std::filesystem::path;

using namespace importa;
using namespace importa::toolchains;
using namespace importa::executor;
using namespace importa::module_processor;

// --- Mock Object Definition ---

struct MockToolchain : public IToolchain
{
    struct CallRecord
    {
        std::string function_name;
        path source_file;
    };

    mutable std::vector<CallRecord> call_history;

    MockToolchain(BuildConfiguration config) : m_config(std::move(config)) {}

    std::optional<Command> generate_emit_ifc_command(
        const EmitIFCArgs& args) const override
    {
        call_history.push_back({ "emit_ifc", args.interface_unit_path });
        return Command{};
    }

    std::optional<Command> generate_compile_obj_command(
        const CompileObjectArgs& args) const override
    {
        call_history.push_back({ "compile_obj", args.source_file });
        return Command{};
    }

    std::optional<Command> generate_link_command(
        const LinkArgs& args) const override
    {
        call_history.push_back({ "link" });
        return Command{};
    }

  private:
    BuildConfiguration m_config;
};

// --- Test Suite for ModuleProcessor ---

void test_module_processor()
{
    std::cout << "--- Running Test Suite: ModuleProcessor ---\n";

    // Test 2A: generate_build_plan call order
    {
        ModuleUnit test_module;
        test_module.name = "TestGfx";
        test_module.partitions = { "gfx/renderer.ixx", "gfx/shader.cpp" };
        test_module.primary_interface = "gfx/graphics.ixx";
        test_module.implementations = { "gfx/utils.cpp" };
        test_module.dependencies = { "Core" };

        auto debug_config = BuildConfigurationFactory::create_debug_default();
        MockToolchain mock_toolchain(debug_config);

        std::map<std::string, path> dependency_ifcs = {
            { "Core", "build/Core/Core.ifc" }
        };

        ModuleProcessor processor(test_module, mock_toolchain, "build",
                                  dependency_ifcs);

        auto plan_opt = processor.generate_build_plan();
        assert(plan_opt.has_value());

        const auto& history = mock_toolchain.call_history;

        assert(history.size() == 4);

        // Verify call order and content
        assert(history[0].function_name == "compile_obj");
        assert(history[0].source_file == "gfx/renderer.ixx");

        assert(history[1].function_name == "compile_obj");
        assert(history[1].source_file == "gfx/shader.cpp");

        assert(history[2].function_name == "emit_ifc");
        assert(history[2].source_file == "gfx/graphics.ixx");

        assert(history[3].function_name == "compile_obj");
        assert(history[3].source_file == "gfx/utils.cpp");

        std::cout << "  Test 2A: generate_build_plan call order... Passed\n";
    }
    std::cout << "--- ModuleProcessor tests all passed ---\n\n";
}

int main()
{
    try
    {
        test_module_processor();
    }
    catch (const std::exception& e)
    {
        std::cerr << "!!! A test failed with exception: " << e.what()
                  << std::endl;
        return 1;
    }

    std::cout << "All ModuleProcessor tests passed successfully!\n";
    return 0;
}