#pragma once
#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "impa_utils.hpp"
#include "json.hpp" // 需要 nlohmann::json


namespace fs = std::filesystem;
using json = nlohmann::json;

namespace impa::parser
{

// 编译命令条目
struct CCEntry
{
    std::string directory;
    std::string file;
    std::vector<std::string> args;
};

// 模块信息
struct ModuleInfo
{
    std::string name;
    fs::path ixx_path;
    fs::path pcm_path;
    std::vector<std::string> include_and_defines;
    std::vector<std::string> dependencies; // 新增：依赖的模块名
};

// 从 .ixx 文件里提取 `export module <name>;`
static std::optional<std::string> extract_module_name(const fs::path& ixx)
{
    std::ifstream ifs(ixx);
    if (!ifs)
        return std::nullopt;
    std::string line;
    std::regex rgx(R"(\bexport\s+module\s+([A-Za-z0-9_:.\-]+)\s*;)");
    size_t scanned = 0;
    while (std::getline(ifs, line))
    {
        scanned += line.size();
        std::smatch m;
        if (std::regex_search(line, m, rgx))
            return m[1].str();
        if (scanned > 128 * 1024)
            break;
    }
    return std::nullopt;
}

// 从 .ixx 文件里提取 `import <name>;`
static std::vector<std::string> extract_imported_modules(const fs::path& ixx)
{
    std::vector<std::string> imports;
    std::ifstream ifs(ixx);
    if (!ifs)
        return imports;
    std::string line;
    std::regex rgx(R"(\bimport\s+([A-Za-z0-9_:.\-]+)\s*;)");
    size_t scanned = 0;
    while (std::getline(ifs, line))
    {
        scanned += line.size();
        for (auto it = std::sregex_iterator(line.begin(), line.end(), rgx);
             it != std::sregex_iterator(); ++it)
        {
            imports.push_back((*it)[1].str());
        }
        if (scanned > 128 * 1024)
            break;
    }
    return imports;
}

// 加载 compile_commands.json
static std::vector<CCEntry> load_compile_commands(const fs::path& in_path)
{
    auto txt = impa::utils::read_all(in_path);
    auto j = json::parse(txt, nullptr, false);
    if (j.is_discarded())
    {
        throw std::runtime_error("Failed to parse " + in_path.string());
    }

    std::vector<CCEntry> entries;
    for (const auto& e : j)
    {
        CCEntry ce;
        ce.directory = e.at("directory").get<std::string>();
        ce.file = e.at("file").get<std::string>();
        if (e.contains("arguments"))
        {
            ce.args = e.at("arguments").get<std::vector<std::string>>();
        }
        else if (e.contains("command"))
        {
            ce.args =
                impa::utils::split_cmd(e.at("command").get<std::string>());
        }
        entries.push_back(std::move(ce));
    }
    return entries;
}

} // namespace impa::parser