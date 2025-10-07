// ModuleProcessor.ixx
//
// 定义了 ModuleProcessor 类，其核心职责是为单个模块单元（ModuleUnit）
// 生成一个完整的、有序的构建计划（ModuleBuildPlan）。

export module module_processor;

// 导入依赖的模块
import std;
import executor;
import toolchains;

using path = std::filesystem::path;
using namespace importa::executor;
using namespace importa::toolchains;

namespace importa
{

namespace module_processor
{

// --- 项目模型定义 ---
export struct ModuleUnit
{
    std::string name;
    path primary_interface;
    std::vector<path> partitions;
    std::vector<path> implementations;
    std::vector<std::string> dependencies;
};

export struct Project
{
    std::string name;
    path root_directory;
    std::vector<ModuleUnit> modules;

    path output_executable;
    std::string main_module_name;
    std::vector<std::string> link_libraries;
};

// --- 构建计划定义 ---
export struct BuildAction
{
    executor::Command command;
    path primary_output;
};

/**
 * @struct ModuleBuildPlan (修正版)
 * @brief 描述了构建一个模块所需的一系列有序的构建动作。
 */
export struct ModuleBuildPlan
{
    std::vector<BuildAction> actions;
    path final_ifc_path;

    // --- 修改点：将此成员加回来 ---
    std::vector<path> generated_obj_paths; // 最终生成的所有 .obj 文件路径
};

// --- 模块处理器 ---
export class ModuleProcessor
{
  public:
    ModuleProcessor(const ModuleUnit& module_to_process,
                    const IToolchain& toolchain, const path& build_dir,
                    const std::map<std::string, path>& dependency_ifcs);

    std::optional<ModuleBuildPlan> generate_build_plan();

  private:
    const ModuleUnit& m_module;
    const IToolchain& m_toolchain;
    const path& m_build_dir;
    const std::map<std::string, path>& m_dependency_ifcs;
    path m_module_artifact_dir;

    std::optional<std::vector<ModuleReference>> resolve_dependencies() const;
    path get_obj_path_for_source(const path& source_path) const;
};

} // namespace ModuleProcessor
} // namespace importa