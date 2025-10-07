// toolchains.ixx
// 最终设计版：Toolchain 在构造时接收配置，成为有状态对象，以简化后续调用。

export module toolchains;

// 使用 C++23 标准库模块
import std;

// 导入依赖的 executor 模块
import executor;

namespace importa
{

namespace toolchains
{
using namespace std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

// --- 核心构建配置 (无改动) ---
export enum class BuildMode
{
    Debug,
    Release
};

export enum class OptimizationLevel
{
    O0, // 无优化
    O1, // 大小优先
    O2, // 速度优先
    O3  // 更强的速度优化
};

export enum class DebugInfo
{
    None,
    Minimal,
    Full
};

export enum class MsvcRuntime
{
    MultiThreadedDebugDLL, // /MDd
    MultiThreadedDebug,    // /MTd
    MultiThreadedDLL,      // /MD
    MultiThreaded          // /MT
};

export enum class CppStandard
{
    Cpp20,
    Cpp23,
    CppLatest
};

export struct BuildConfiguration
{
    BuildMode mode;
    OptimizationLevel optimization;
    DebugInfo debug_info;
    MsvcRuntime msvc_runtime;
    CppStandard cpp_standard = CppStandard::CppLatest;

    std::vector<path> include_dirs;
    std::vector<path> library_dirs;
    std::vector<std::string> defines;
};

export struct BuildConfigurationFactory
{
    static BuildConfiguration create_debug_default();
    static BuildConfiguration create_release_default();
    static BuildConfiguration create_release_with_debug_info();
};

// --- 数据传输结构体 (无改动) ---

export struct ModuleReference
{
    std::string name;
    path ifc_path;
};

export struct EmitIFCArgs
{
    path interface_unit_path;
    path output_ifc_path;
    std::vector<ModuleReference> module_dependencies;
};

export struct CompileObjectArgs
{
    path source_file;
    path output_obj_path;
    std::vector<ModuleReference> module_dependencies;
};

export struct LinkArgs
{
    std::vector<path> object_files;
    path output_target_path; // .exe 或 .lib
    std::vector<std::string> link_libraries;
};

// --- 抽象接口 (修改点) ---

export class IToolchain
{
  public:
    virtual ~IToolchain() = default;

    // 修改点：移除了 config 参数
    virtual std::optional<executor::Command> generate_emit_ifc_command(
        const EmitIFCArgs& args) const = 0;

    // 修改点：移除了 config 参数
    virtual std::optional<executor::Command> generate_compile_obj_command(
        const CompileObjectArgs& args) const = 0;

    // 修改点：移除了 config 参数
    virtual std::optional<executor::Command> generate_link_command(
        const LinkArgs& args) const = 0;
};

// --- 具体工具链声明 (修改点) ---

export class MsvcToolchain final : public IToolchain
{
  public:
    // 修改点：构造函数接收 BuildConfiguration
    MsvcToolchain(path cl_path, path link_path, BuildConfiguration config);
    ~MsvcToolchain() override = default;

    std::optional<executor::Command> generate_emit_ifc_command(
        const EmitIFCArgs& args) const override;

    std::optional<executor::Command> generate_compile_obj_command(
        const CompileObjectArgs& args) const override;

    std::optional<executor::Command> generate_link_command(
        const LinkArgs& args) const override;

  private:
    path m_cl_path;
    path m_link_path;
    BuildConfiguration m_config; // 修改点：新增成员变量
};

export class ClangToolchain final : public IToolchain
{
  public:
    // 修改点：构造函数接收 BuildConfiguration
    ClangToolchain(path clang_cl_path, BuildConfiguration config);
    ~ClangToolchain() override = default;

    std::optional<executor::Command> generate_emit_ifc_command(
        const EmitIFCArgs& args) const override;

    std::optional<executor::Command> generate_compile_obj_command(
        const CompileObjectArgs& args) const override;

    std::optional<executor::Command> generate_link_command(
        const LinkArgs& args) const override;

    // Clangd 支持的专属功能
    std::optional<executor::Command> generate_pcm_command(
        const EmitIFCArgs& args) const;

  private:
    path m_clang_cl_path;
    BuildConfiguration m_config; // 修改点：新增成员变量
};
} // namespace toolchains
} // namespace importa