#pragma once
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "impa_parser.hpp"
#include "impa_utils.hpp"


namespace fs = std::filesystem;
using json = nlohmann::json;

namespace impa::converter
{

// 对模块进行拓扑排序以确定编译顺序
static std::optional<std::vector<std::string>> topological_sort(
    const std::map<std::string, parser::ModuleInfo>& modules)
{
    std::map<std::string, int> in_degree;
    std::map<std::string, std::vector<std::string>>
        reverse_adj; // B -> {A} (A 依赖 B)

    for (const auto& [name, info] : modules)
    {
        in_degree[name] = info.dependencies.size();
        for (const auto& dep : info.dependencies)
        {
            if (modules.count(dep))
            {
                reverse_adj[dep].push_back(name);
            }
        }
    }

    std::vector<std::string> queue;
    for (const auto& [name, degree] : in_degree)
    {
        if (degree == 0)
        {
            queue.push_back(name);
        }
    }

    std::vector<std::string> sorted_order;
    size_t head = 0;
    while (head < queue.size())
    {
        std::string u = queue[head++];
        sorted_order.push_back(u);

        if (reverse_adj.count(u))
        {
            for (const auto& v : reverse_adj.at(u))
            {
                in_degree[v]--;
                if (in_degree[v] == 0)
                {
                    queue.push_back(v);
                }
            }
        }
    }

    if (sorted_order.size() != modules.size())
    {
        std::cerr << "[error] Circular module dependency detected. Aborting."
                  << std::endl;
        return std::nullopt;
    }

    return sorted_order;
}

// 转换 /I /D 参数为 clang++ 风格
static std::vector<std::string> to_clangxx_incdefs(
    const std::vector<std::string>& args)
{
    std::vector<std::string> out;
    for (size_t i = 0; i < args.size(); ++i)
    {
        const auto& t = args[i];
        if (utils::starts_with(t, "/I"))
        {
            out.push_back("-I" + (t.size() > 2 ? t.substr(2) : args[++i]));
        }
        else if (utils::starts_with(t, "/D"))
        {
            out.push_back("-D" + (t.size() > 2 ? t.substr(2) : args[++i]));
        }
        else if (utils::starts_with(t, "-I") || utils::starts_with(t, "-D"))
        {
            out.push_back(t);
        }
    }
    return out;
}

// 重写非模块文件的编译命令，供 clangd 使用
static std::vector<std::string> rewrite_for_clangcl(
    const std::vector<std::string>& in_args,
    const std::map<std::string, fs::path>& module_map,
    const std::string& file_path, const fs::path& clang_cl_path,
    std::string_view cpp_std)
{
    std::vector<std::string> a;
    a.push_back(clang_cl_path.string());
    a.push_back("/TP");

    bool has_std = false;
    for (size_t i = 0; i < in_args.size(); ++i)
    {
        const auto& t = in_args[i];
        if (utils::starts_with(t, "/I") || utils::starts_with(t, "-I") ||
            utils::starts_with(t, "/D") || utils::starts_with(t, "-D") ||
            utils::starts_with(t, "/Zc:") || utils::starts_with(t, "/EHsc") ||
            utils::starts_with(t, "/MD") || utils::starts_with(t, "/MT"))
        {
            a.push_back(t);
        }
        else if (utils::starts_with(t, "/std:"))
        {
            a.push_back(t);
            has_std = true;
        }
    }

    if (!has_std)
    {
        a.push_back(std::string("/std:") + std::string(cpp_std));
    }

    a.push_back("/c");
    a.push_back(file_path);

    for (const auto& [name, pcm_path] : module_map)
    {
        a.push_back("-Xclang");
        a.push_back("-fmodule-file=" + name + "=" +
                    fs::absolute(pcm_path).string());
    }
    return a;
}

} // namespace impa::converter