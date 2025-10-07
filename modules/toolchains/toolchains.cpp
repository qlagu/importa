// toolchains.cpp
// 提供了 toolchains 模块中声明的各个函数和类的具体实现。
// 最终版：采用有状态对象模型，Toolchain 在构造时接收配置。

// 声明此文件是 "toolchains" 模块的实现部分。
module toolchains;

// 导入所需的模块
import std;
import executor;

using namespace importa::executor;

namespace importa::toolchains
{

// --- 内部辅助函数 ---
namespace
{ // 放在匿名命名空间中，作为此文件的内部实现细节

// 将通用的 MSVC 编译选项从 BuildConfiguration 翻译并添加到命令参数列表中
void add_common_msvc_compile_options(std::vector<std::string>& args,
                                     const BuildConfiguration& config)
{
    // C++ 标准
    switch (config.cpp_standard)
    {
        case CppStandard::Cpp20:
            args.push_back("/std:c++20");
            break;
        case CppStandard::Cpp23:
            args.push_back("/std:c++23");
            break;
        case CppStandard::CppLatest:
            args.push_back("/std:c++latest");
            break;
    }

    // 构建模式与优化
    if (config.mode == BuildMode::Debug)
    {
        args.push_back("/Od"); // Debug模式：关闭优化
    }
    else
    { // Release 模式
        switch (config.optimization)
        {
            case OptimizationLevel::O0:
                args.push_back("/Od");
                break;
            case OptimizationLevel::O1:
                args.push_back("/O1");
                break; // 最小化大小
            case OptimizationLevel::O2:
                args.push_back("/O2");
                break; // 最大化速度
            case OptimizationLevel::O3:
                args.push_back("/Ox");
                break; // MSVC中 /Ox 是全速优化
        }
    }

    // 调试信息
    if (config.debug_info == DebugInfo::Full)
    {
        args.push_back("/Zi"); // 生成 PDB
    }
    else if (config.debug_info == DebugInfo::Minimal)
    {
        args.push_back("/Z7"); // 信息嵌入 .obj
    }

    // 运行时库
    switch (config.msvc_runtime)
    {
        case MsvcRuntime::MultiThreadedDebugDLL:
            args.push_back("/MDd");
            break;
        case MsvcRuntime::MultiThreadedDebug:
            args.push_back("/MTd");
            break;
        case MsvcRuntime::MultiThreadedDLL:
            args.push_back("/MD");
            break;
        case MsvcRuntime::MultiThreaded:
            args.push_back("/MT");
            break;
    }

    // 添加其他通用编译标志
    args.push_back("/EHsc");        // 启用 C++ 异常处理
    args.push_back("/nologo");      // 禁止显示启动版权标志
    args.push_back("/c");           // 只编译，不链接
    args.push_back("/TP");          // 将所有文件视为 C++ 文件
    args.push_back("/permissive-"); // 更严格的标准一致性

    // 添加宏定义
    for (const auto& def : config.defines)
    {
        args.push_back("/D" + def);
    }

    // 添加头文件包含目录
    for (const auto& dir : config.include_dirs)
    {
        args.push_back("/I" + dir.string());
    }
}

// Clang 的通用编译选项辅助函数
void add_common_clang_compile_options(std::vector<std::string>& args,
                                      const BuildConfiguration& config)
{
    // C++ 标准
    switch (config.cpp_standard)
    {
        case CppStandard::Cpp20:
            args.push_back("-std=c++20");
            break;
        case CppStandard::Cpp23:
            args.push_back("-std=c++2b");
            break;
        case CppStandard::CppLatest:
            args.push_back("-std=c++2b");
            break;
    }

    // 优化级别 (Clang 风格)
    if (config.mode == BuildMode::Debug)
    {
        args.push_back("-O0");
    }
    else
    {
        switch (config.optimization)
        {
            case OptimizationLevel::O0:
                args.push_back("-O0");
                break;
            case OptimizationLevel::O1:
                args.push_back("-O1");
                break;
            case OptimizationLevel::O2:
                args.push_back("-O2");
                break;
            case OptimizationLevel::O3:
                args.push_back("-O3");
                break;
        }
    }

    // 调试信息 (Clang 风格)
    if (config.debug_info == DebugInfo::Full ||
        config.debug_info == DebugInfo::Minimal)
    {
        args.push_back("-g");
    }

    // 在Windows上使用clang-cl时，它能理解MSVC的运行时标志
    switch (config.msvc_runtime)
    {
        case MsvcRuntime::MultiThreadedDebugDLL:
            args.push_back("/MDd");
            break;
        case MsvcRuntime::MultiThreadedDebug:
            args.push_back("/MTd");
            break;
        case MsvcRuntime::MultiThreadedDLL:
            args.push_back("/MD");
            break;
        case MsvcRuntime::MultiThreaded:
            args.push_back("/MT");
            break;
    }

    // 其他标志
    args.push_back("-fms-compatibility"); // 开启与 MSVC 的兼容模式
    args.push_back("-Wno-msvc-include");  // 禁用一些关于 MSVC include 的警告

    // 宏定义和包含目录
    for (const auto& def : config.defines)
    {
        args.push_back("-D" + def);
    }
    for (const auto& dir : config.include_dirs)
    {
        args.push_back("-I" + dir.string());
    }
}
} // namespace

// --- BuildConfigurationFactory 实现 ---

BuildConfiguration BuildConfigurationFactory::create_debug_default()
{
    BuildConfiguration config;
    config.mode = BuildMode::Debug;
    config.optimization = OptimizationLevel::O0;
    config.debug_info = DebugInfo::Full;
    config.msvc_runtime = MsvcRuntime::MultiThreadedDebugDLL;
    config.defines.push_back("_DEBUG");
    return config;
}

BuildConfiguration BuildConfigurationFactory::create_release_default()
{
    BuildConfiguration config;
    config.mode = BuildMode::Release;
    config.optimization = OptimizationLevel::O2;
    config.debug_info = DebugInfo::None;
    config.msvc_runtime = MsvcRuntime::MultiThreadedDLL;
    config.defines.push_back("NDEBUG");
    return config;
}

BuildConfiguration BuildConfigurationFactory::create_release_with_debug_info()
{
    BuildConfiguration config = create_release_default();
    config.debug_info = DebugInfo::Full;
    return config;
}

// --- MsvcToolchain 实现 ---

MsvcToolchain::MsvcToolchain(path cl_path, path link_path,
                             BuildConfiguration config)
    : m_cl_path(std::move(cl_path)), m_link_path(std::move(link_path)),
      m_config(std::move(config))
{
}

std::optional<Command> MsvcToolchain::generate_emit_ifc_command(
    const EmitIFCArgs& args) const
{
    Command cmd;
    cmd.executable = m_cl_path;
    add_common_msvc_compile_options(cmd.arguments, m_config);
    cmd.arguments.push_back("/interface");
    cmd.arguments.push_back(args.interface_unit_path.string());
    cmd.arguments.push_back("/ifcOutput");
    cmd.arguments.push_back(args.output_ifc_path.string());
    auto obj_path = args.output_ifc_path.parent_path() /
                    (args.output_ifc_path.stem().string() + ".obj");
    cmd.arguments.push_back("/Fo:" + obj_path.string());
    for (const auto& dep : args.module_dependencies)
    {
        cmd.arguments.push_back("/reference");
        cmd.arguments.push_back(dep.name + "=" + dep.ifc_path.string());
    }
    return cmd;
}

std::optional<Command> MsvcToolchain::generate_compile_obj_command(
    const CompileObjectArgs& args) const
{
    Command cmd;
    cmd.executable = m_cl_path;
    add_common_msvc_compile_options(cmd.arguments, m_config);
    cmd.arguments.push_back(args.source_file.string());
    cmd.arguments.push_back("/Fo:" + args.output_obj_path.string());
    for (const auto& dep : args.module_dependencies)
    {
        cmd.arguments.push_back("/reference");
        cmd.arguments.push_back(dep.name + "=" + dep.ifc_path.string());
    }
    return cmd;
}

std::optional<Command> MsvcToolchain::generate_link_command(
    const LinkArgs& args) const
{
    Command cmd;
    cmd.executable = m_link_path;
    cmd.arguments.push_back("/nologo");
    cmd.arguments.push_back("/OUT:" + args.output_target_path.string());
    if (m_config.debug_info == DebugInfo::Full)
    {
        cmd.arguments.push_back("/DEBUG:FULL");
    }
    if (m_config.mode == BuildMode::Release &&
        m_config.debug_info == DebugInfo::Full)
    {
        cmd.arguments.push_back("/OPT:REF");
        cmd.arguments.push_back("/OPT:ICF");
    }
    for (const auto& dir : m_config.library_dirs)
    {
        cmd.arguments.push_back("/LIBPATH:\"" + dir.string() + "\"");
    }
    for (const auto& obj : args.object_files)
    {
        cmd.arguments.push_back(obj.string());
    }
    for (const auto& lib : args.link_libraries)
    {
        cmd.arguments.push_back(lib);
    }
    return cmd;
}

// --- ClangToolchain 完整实现 ---

ClangToolchain::ClangToolchain(path clang_cl_path, BuildConfiguration config)
    : m_clang_cl_path(std::move(clang_cl_path)), m_config(std::move(config))
{
}

std::optional<Command> ClangToolchain::generate_emit_ifc_command(
    const EmitIFCArgs& args) const
{
    Command cmd;
    cmd.executable = m_clang_cl_path;
    add_common_clang_compile_options(cmd.arguments, m_config);
    cmd.arguments.push_back("--precompile");
    cmd.arguments.push_back("-x");
    cmd.arguments.push_back("c++-module");
    cmd.arguments.push_back(args.interface_unit_path.string());
    path pcm_path = args.output_ifc_path;
    pcm_path.replace_extension(".pcm");
    cmd.arguments.push_back("-o");
    cmd.arguments.push_back(pcm_path.string());
    for (const auto& dep : args.module_dependencies)
    {
        path dep_pcm_path = dep.ifc_path;
        dep_pcm_path.replace_extension(".pcm");
        cmd.arguments.push_back("-fmodule-file=" + dep.name + "=" +
                                dep_pcm_path.string());
    }
    return cmd;
}

std::optional<Command> ClangToolchain::generate_compile_obj_command(
    const CompileObjectArgs& args) const
{
    Command cmd;
    cmd.executable = m_clang_cl_path;
    add_common_clang_compile_options(cmd.arguments, m_config);
    cmd.arguments.push_back("-c");
    cmd.arguments.push_back(args.source_file.string());
    cmd.arguments.push_back("-o");
    cmd.arguments.push_back(args.output_obj_path.string());
    for (const auto& dep : args.module_dependencies)
    {
        path dep_pcm_path = dep.ifc_path;
        dep_pcm_path.replace_extension(".pcm");
        cmd.arguments.push_back("-fmodule-file=" + dep.name + "=" +
                                dep_pcm_path.string());
    }
    return cmd;
}

std::optional<Command> ClangToolchain::generate_link_command(
    const LinkArgs& args) const
{
    Command cmd;
    cmd.executable = m_clang_cl_path;
    cmd.arguments.push_back("-o");
    cmd.arguments.push_back(args.output_target_path.string());
    if (m_config.debug_info == DebugInfo::Full)
    {
        cmd.arguments.push_back("-g");
    }
    for (const auto& dir : m_config.library_dirs)
    {
        cmd.arguments.push_back("-L" + dir.string());
    }
    for (const auto& obj : args.object_files)
    {
        cmd.arguments.push_back(obj.string());
    }
    // clang-cl 驱动程序通常可以直接理解 .lib 文件名，无需 -l 转换
    for (const auto& lib : args.link_libraries)
    {
        cmd.arguments.push_back(lib);
    }
    return cmd;
}

std::optional<Command> ClangToolchain::generate_pcm_command(
    const EmitIFCArgs& args) const
{
    // 此函数的职责已与 generate_emit_ifc_command 合并，直接调用它即可
    return this->generate_emit_ifc_command(args);
}
}