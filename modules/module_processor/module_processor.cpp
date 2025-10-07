// ModuleProcessor.cpp
// 提供了 ModuleProcessor 类的具体实现。

// 声明此文件是 "ModuleProcessor" 模块的实现部分
module module_processor;

// 导入所需的模块
import std;
import executor;
import toolchains;

using namespace importa::module_processor;
using namespace importa::executor;
using namespace importa::toolchains;

// --- ModuleProcessor 实现 ---

ModuleProcessor::ModuleProcessor(
    const ModuleUnit& module_to_process, const IToolchain& toolchain,
    const path& build_dir, const std::map<std::string, path>& dependency_ifcs)
    : m_module(module_to_process), m_toolchain(toolchain),
      m_build_dir(build_dir), m_dependency_ifcs(dependency_ifcs)
{
    m_module_artifact_dir = m_build_dir / m_module.name;
    std::filesystem::create_directories(m_module_artifact_dir);
}

std::optional<ModuleBuildPlan> ModuleProcessor::generate_build_plan()
{
    ModuleBuildPlan plan;

    // 步骤 1: 解析外部依赖
    auto resolved_deps = resolve_dependencies();
    if (!resolved_deps)
    {
        std::cerr << "Error: Could not resolve some dependencies for module '"
                  << m_module.name << "'.\n";
        return std::nullopt;
    }

    // 步骤 2: [A-阶段] 规划分区编译
    for (const auto& partition_path : m_module.partitions)
    {
        CompileObjectArgs args;
        args.source_file = partition_path;
        args.output_obj_path = get_obj_path_for_source(partition_path);
        args.module_dependencies = *resolved_deps;

        if (auto cmd = m_toolchain.generate_compile_obj_command(args))
        {
            plan.actions.push_back({ *cmd, args.output_obj_path });
            // 修正点：在规划的同时，安全地记录产物
            plan.generated_obj_paths.push_back(args.output_obj_path);
        }
        else
        {
            std::cerr
                << "Error: Failed to generate compile command for partition '"
                << partition_path.string() << "'.\n";
            return std::nullopt;
        }
    }

    // 步骤 3: [B-阶段] 规划主接口编译
    if (!m_module.primary_interface.empty())
    {
        EmitIFCArgs args;
        args.interface_unit_path = m_module.primary_interface;
        args.output_ifc_path = m_module_artifact_dir / (m_module.name + ".ifc");
        args.module_dependencies = *resolved_deps;

        plan.final_ifc_path = args.output_ifc_path;

        if (auto cmd = m_toolchain.generate_emit_ifc_command(args))
        {
            plan.actions.push_back({ *cmd, args.output_ifc_path });

            // 修正点：安全地记录主接口附带的 .obj 产物
            path obj_path = get_obj_path_for_source(m_module.primary_interface);
            plan.generated_obj_paths.push_back(obj_path);
        }
        else
        {
            std::cerr << "Error: Failed to generate compile command for "
                         "primary interface '"
                      << m_module.primary_interface.string() << "'.\n";
            return std::nullopt;
        }
    }

    // 步骤 4: [C-阶段] 规划实现文件编译
    for (const auto& impl_path : m_module.implementations)
    {
        CompileObjectArgs args;
        args.source_file = impl_path;
        args.output_obj_path = get_obj_path_for_source(impl_path);
        args.module_dependencies = *resolved_deps;

        if (auto cmd = m_toolchain.generate_compile_obj_command(args))
        {
            plan.actions.push_back({ *cmd, args.output_obj_path });
            // 修正点：在规划的同时，安全地记录产物
            plan.generated_obj_paths.push_back(args.output_obj_path);
        }
        else
        {
            std::cerr << "Error: Failed to generate compile command for "
                         "implementation file '"
                      << impl_path.string() << "'.\n";
            return std::nullopt;
        }
    }

    return plan;
}

// --- 私有辅助函数实现 ---

std::optional<std::vector<ModuleReference>> ModuleProcessor::
    resolve_dependencies() const
{
    std::vector<ModuleReference> resolved;
    for (const auto& dep_name : m_module.dependencies)
    {
        auto it = m_dependency_ifcs.find(dep_name);
        if (it == m_dependency_ifcs.end())
        {
            // Error message in English
            std::cerr << "Error: While resolving dependencies for module '"
                      << m_module.name
                      << "', could not find the interface file path for "
                         "compiled module '"
                      << dep_name << "'.\n";
            return std::nullopt;
        }
        resolved.push_back({ dep_name, it->second });
    }
    return resolved;
}

path ModuleProcessor::get_obj_path_for_source(const path& source_path) const
{
    // 将源文件的文件名（带后缀）替换扩展名为 .obj，并放入此模块的专属产物目录
    return m_module_artifact_dir /
           source_path.filename().replace_extension(".obj");
}
