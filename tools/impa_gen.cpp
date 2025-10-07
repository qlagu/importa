#include <iostream>
#include <map>
#include <string_view>
#include <vector>

#include "impa_converter.hpp"
#include "impa_parser.hpp"
#include "impa_utils.hpp"

// ===== 可按需调整的常量 =====
static const fs::path kInPath = "build/compile_commands.json";
static const fs::path kOutPath = "compile_commands.json";
static const fs::path kPcmDir = "build/pcm-cache";

// VS 自带 clang (已修正为您的原始路径)
static const fs::path kClangXX = R"(C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\Llvm\bin\clang++.exe)";
static const fs::path kClangCL = R"(C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\Llvm\bin\clang-cl.exe)";

// 预编译时使用的标准
static constexpr std::string_view kStd = "c++20";

int main()
{
    try
    {
        std::cout << "[impa] Input DB      : " << kInPath << "\n";
        std::cout << "[impa] Output DB     : " << kOutPath << "\n";
        std::cout << "[impa] PCM Cache Dir : " << kPcmDir << "\n";

        fs::create_directories(kPcmDir);
        auto entries = impa::parser::load_compile_commands(kInPath);

        // 1. 收集所有模块信息
        std::map<std::string, impa::parser::ModuleInfo> modules_info;
        for (const auto& e : entries)
        {
            if (!impa::utils::ieq_ends_with(e.file, ".ixx"))
                continue;

            auto name_opt = impa::parser::extract_module_name(e.file);
            if (!name_opt)
            {
                std::cerr << "[warn] Cannot find `export module ...;` in "
                          << e.file << "\n";
                continue;
            }

            impa::parser::ModuleInfo mi;
            mi.name = *name_opt;
            mi.ixx_path = fs::path(e.file);
            mi.pcm_path = kPcmDir / (mi.name + ".pcm");
            mi.dependencies = impa::parser::extract_imported_modules(e.file);
            mi.include_and_defines = e.args;
            modules_info[mi.name] = std::move(mi);
        }

        // 2. 拓扑排序确定编译顺序
        auto sorted_module_names_opt =
            impa::converter::topological_sort(modules_info);
        if (!sorted_module_names_opt)
            return 1;
        const auto& sorted_module_names = *sorted_module_names_opt;

        std::cout << "\n[impa] Determined module compilation order:\n";
        for (const auto& name : sorted_module_names)
            std::cout << "  -> " << name << "\n";

        // 3. 按顺序预编译模块
        std::cout << "\n[impa] Precompiling modules...\n";
        std::map<std::string, fs::path> module_name_to_pcm;
        for (const auto& name : sorted_module_names)
        {
            const auto& m = modules_info.at(name);
            std::vector<std::string> cmd = {
                kClangXX.string(), std::string("-std=") + std::string(kStd),
                "--precompile", "-x", "c++-module"
            };

            for (const auto& dep_name : m.dependencies)
            {
                if (module_name_to_pcm.count(dep_name))
                {
                    cmd.push_back(
                        "-fmodule-file=" + dep_name + "=" +
                        fs::absolute(module_name_to_pcm.at(dep_name)).string());
                }
            }

            auto clang_incdefs =
                impa::converter::to_clangxx_incdefs(m.include_and_defines);
            cmd.insert(cmd.end(), clang_incdefs.begin(), clang_incdefs.end());

            cmd.push_back(fs::absolute(m.ixx_path).string());
            cmd.push_back("-o");
            cmd.push_back(fs::absolute(m.pcm_path).string());

            if (impa::utils::run_command(cmd) != 0)
            {
                std::cerr << "[error] Precompilation failed for: " << m.ixx_path
                          << "\n";
                return 1;
            }
            module_name_to_pcm[m.name] = m.pcm_path;
        }

        // 4. 生成最终的 compile_commands.json
        json out_json = json::array();

        // 4.1 为模块接口 .ixx 文件生成条目
        for (const auto& [name, m] : modules_info)
        {
            json rec;
            rec["directory"] = fs::absolute(".").string();
            rec["file"] = fs::absolute(m.ixx_path).string();
            rec["arguments"] = { kClangCL.string(),
                                 "/TP",
                                 "/clang:-fsyntax-only",
                                 "/clang:-xc++-module",
                                 "/clang:-fmodule-name=" + m.name,
                                 std::string("/std:") + std::string(kStd) };
            // 也可在此处添加 /I /D 等参数
            rec["arguments"].push_back(fs::absolute(m.ixx_path).string());
            out_json.push_back(rec);
        }

        // 4.2 为普通 .cpp 文件生成条目
        for (const auto& e : entries)
        {
            if (impa::utils::ieq_ends_with(e.file, ".ixx"))
                continue;
            json rec;
            rec["directory"] = e.directory;
            rec["file"] = fs::absolute(e.file).string();
            rec["arguments"] = impa::converter::rewrite_for_clangcl(
                e.args, module_name_to_pcm, fs::absolute(e.file).string(),
                kClangCL, kStd);
            out_json.push_back(rec);
        }

        std::ofstream ofs(kOutPath, std::ios::binary);
        ofs << out_json.dump(2) << std::endl;

        std::cout << "\n[ok] Wrote clangd DB: " << kOutPath << "\n";
        std::cout << "[ok] Found " << modules_info.size() << " modules.\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[fatal] " << ex.what() << "\n";
        return 1;
    }
    return 0;
}