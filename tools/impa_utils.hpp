#pragma once
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace impa::utils
{

// 读取文件全部内容
static std::string read_all(const fs::path& p)
{
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("cannot open: " + p.string());
    return std::string{ std::istreambuf_iterator<char>(ifs),
                        std::istreambuf_iterator<char>() };
}

// 简易命令行分词（处理双引号）
static std::vector<std::string> split_cmd(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    bool in_quote = false;
    for (char c : s)
    {
        if (c == '"')
        {
            in_quote = !in_quote;
            continue;
        }
        if (!in_quote && std::isspace(static_cast<unsigned char>(c)))
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

// 字符串不区分大小写后缀匹配
static bool ieq_ends_with(std::string_view s, std::string_view suf)
{
    if (s.size() < suf.size())
        return false;
    return std::equal(suf.rbegin(), suf.rend(), s.rbegin(),
                      [](char a, char b)
                      {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                      });
}

// 字符串前缀匹配
static bool starts_with(std::string_view s, std::string_view pfx)
{
    return s.rfind(pfx, 0) == 0;
}

// 执行命令行指令 (最终修正版，使用 start /wait)
static int run_command(const std::vector<std::string>& argv, bool echo = true) {
    if (argv.empty()) {
        return 1;
    }

    // 在Windows上，使用 `start /wait` 命令可以极其可靠地处理带空格的路径。
    // `start` 有一个特殊的参数解析器。
    // `/B` 表示不在新窗口中启动。
    // `/wait` 表示等待程序执行完成。
    // `""` 是一个必需的参数，作为新窗口的空标题，以防止 `start` 将我们的程序路径误认为是标题。
    std::string cmd = "start /B /wait \"\"";

    // 将所有参数（包括可执行文件本身）都用引号包裹起来，传递给 start 命令
    for (const auto& arg : argv) {
        cmd += " \"";
        
        std::string escaped_arg;
        // 如果参数本身包含引号，需要转义，但对于本项目场景，概率极低，暂不处理。
        // 若未来有需要，此处理论上应为：将 arg 中的每个 " 替换为 \"
        escaped_arg = arg;

        cmd += escaped_arg;
        cmd += '"';
    }

    if (echo) {
        std::cout << "[run] " << cmd << "\n";
    }

    return std::system(cmd.c_str());
}

} // namespace impa::utils