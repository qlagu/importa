// impa_gen.cpp
// 生成 clangd 友好的 compile_commands.json：
// 1) 扫描 build/compile_commands.json，找出所有 .ixx，提取 module 名并用
// clang++ 预编译生成 .pcm 2) 写根目录 ./compile_commands.json：
//    - 为每个 .ixx 写一条 syntax-only
//    的索引记录（clang-cl，便于跳转到接口源码）
//    - 为每个非 .ixx 的 TU 追加 -fmodule-file=<name>=<pcm>，驱动改为 clang-cl
//
// 需要：nlohmann::json 单头（#include "json.hpp"）
// 编译：cl /std:c++20 impa_gen.cpp  或  clang-cl /std:c++20 impa_gen.cpp

#include "json.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <vector>


namespace fs = std::filesystem;
using json = nlohmann::json;

// ===== 可按需调整的常量 =====
static const fs::path kIn = "build/compile_commands.json";
static const fs::path kOut =
    "compile_commands.json"; // 输出到根目录（clangd 使用）
static const fs::path kPcmDir = "build/pcm-cache";

// VS 自带 clang（按你环境）
static const fs::path kClangXX =
    R"(C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\Llvm\bin\clang++.exe)";
static const fs::path kClangCL =
    R"(C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\Llvm\bin\clang-cl.exe)";

// 预编译时使用的标准（建议 c++20，更稳）
static constexpr std::string_view kStd = "c++20";

// ===== 小工具函数 =====
static std::string read_all(const fs::path &p) {
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs)
    throw std::runtime_error("cannot open: " + p.string());
  return std::string{std::istreambuf_iterator<char>(ifs),
                     std::istreambuf_iterator<char>()};
}

// 简易命令行分词（双引号包裹）
static std::vector<std::string> split_cmd(const std::string &s) {
  std::vector<std::string> out;
  std::string cur;
  bool q = false;
  for (char c : s) {
    if (c == '"') {
      q = !q;
      continue;
    }
    if (!q && std::isspace((unsigned char)c)) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else
      cur.push_back(c);
  }
  if (!cur.empty())
    out.push_back(cur);
  return out;
}

static bool ieq_ends_with(std::string_view s, std::string_view suf) {
  if (s.size() < suf.size())
    return false;
  for (size_t i = 0; i < suf.size(); ++i) {
    char a = (char)std::tolower((unsigned char)s[s.size() - suf.size() + i]);
    char b = (char)std::tolower((unsigned char)suf[i]);
    if (a != b)
      return false;
  }
  return true;
}

static bool starts_with(std::string_view s, std::string_view pfx) {
  return s.rfind(pfx, 0) == 0;
}

// 从 .ixx 文件里提取 `export module <name>;`
static std::optional<std::string> extract_module_name(const fs::path &ixx) {
  std::ifstream ifs(ixx);
  if (!ifs)
    return std::nullopt;
  std::string line;
  std::regex rgx(R"(\bexport\s+module\s+([A-Za-z0-9_:.\-]+)\s*;)");
  size_t scanned = 0;
  while (std::getline(ifs, line)) {
    scanned += line.size();
    std::smatch m;
    if (std::regex_search(line, m, rgx))
      return m[1].str();
    if (scanned > 128 * 1024)
      break; // 防止扫太多
  }
  return std::nullopt;
}

static int run(const std::vector<std::string> &argv, bool echo = true) {
  std::string cmd;
  for (auto &a : argv) {
    if (a.find_first_of(" \t\"") != std::string::npos) {
      cmd += '"';
      cmd += a;
      cmd += "\" ";
    } else {
      cmd += a;
      cmd += ' ';
    }
  }
  if (echo)
    std::cout << "[run] " << cmd << "\n";
  return std::system(cmd.c_str());
}

struct CCEntry {
  std::string directory;
  std::string file;
  std::vector<std::string>
      args; // 如果 compile_commands 给的是 "command"，我们会拆成 args
};

static std::vector<CCEntry> load_cc(const fs::path &in) {
  auto txt = read_all(in);
  auto j = json::parse(txt);

  std::vector<CCEntry> out;
  for (auto &e : j) {
    CCEntry ce;
    ce.directory = e.at("directory").get<std::string>();
    ce.file = e.at("file").get<std::string>();
    if (e.contains("arguments")) {
      for (auto &a : e["arguments"])
        ce.args.push_back(a.get<std::string>());
    } else if (e.contains("command")) {
      ce.args = split_cmd(e["command"].get<std::string>());
    }
    out.push_back(std::move(ce));
  }
  return out;
}

struct ModuleInfo {
  std::string name;
  fs::path ixx;
  fs::path pcm;
  std::vector<std::string> incdefs; // 从原始 TU 携带的 /I /D
};

// 把 /Ixxx /Dxxx（或者分离写法 /I xxx）转换为 clang++ 友好的 -I/-D
static std::vector<std::string>
to_clangxx_incdefs(const std::vector<std::string> &in) {
  std::vector<std::string> out;
  for (size_t i = 0; i < in.size(); ++i) {
    const auto &t = in[i];
    auto emit_pair = [&](std::string_view flag) {
      if (i + 1 < in.size()) {
        out.emplace_back(std::string(flag) + in[i + 1]);
        ++i;
      }
    };
    if (starts_with(t, "/I")) {
      if (t.size() > 2)
        out.emplace_back("-I" + t.substr(2));
      else
        emit_pair("-I");
    } else if (starts_with(t, "/D")) {
      if (t.size() > 2)
        out.emplace_back("-D" + t.substr(2));
      else
        emit_pair("-D");
    } else if (starts_with(t, "-I") || starts_with(t, "-D")) {
      out.push_back(t);
    }
  }
  return out;
}

// 过滤/翻译一条 cl/clang-cl 的参数，产出 clang-cl 友好的“只给 clangd 用”的 args
static std::vector<std::string>
rewrite_for_clangcl(const std::vector<std::string> &inArgs,
                    const std::map<std::string, fs::path> &moduleMap,
                    const std::string &file) {
  std::vector<std::string> a;
  a.push_back(kClangCL.string());
  a.push_back("/TP");

  // 保留常见的编译上下文
  auto keep = [&](const std::string &t) {
    return starts_with(t, "/I") || starts_with(t, "-I") ||
           starts_with(t, "/D") || starts_with(t, "-D") ||
           starts_with(t, "/std:") || starts_with(t, "/Fo") ||
           starts_with(t, "/Fd") || starts_with(t, "/Zc:") ||
           starts_with(t, "/EH") || starts_with(t, "/MD") ||
           starts_with(t, "/MT");
  };

  for (size_t i = 0; i < inArgs.size(); ++i) {
    const std::string &t = inArgs[i];
    if (t.find("cl.exe") != std::string::npos ||
        t.find("clang") != std::string::npos)
      continue;
    if (!t.empty() && t[0] == '@')
      continue; // @rsp (CMake module map)，clangd 用不到
    if (starts_with(t, "/ifc") || starts_with(t, "-ifc") ||
        starts_with(t, "/reference") || starts_with(t, "-reference"))
      continue; // MSVC 模块私参
    if (keep(t)) {
      a.push_back(t);
      continue;
    }

    // 其它大部分开关忽略无伤（/nologo /Zi /Od ...）
  }

  // 追加 /c + 源文件
  a.push_back("/c");
  a.push_back(file);

  // 确保有标准开关
  bool hasStd = false;
  for (auto &t : a)
    if (starts_with(t, "/std:")) {
      hasStd = true;
      break;
    }
  if (!hasStd)
    a.push_back(std::string("/std:") + std::string(kStd));

  // 追加所有已知模块映射（按需加载，不会多余报错）
  for (auto &[name, pcm] : moduleMap) {
    a.push_back("-Xclang");
    a.push_back("-fmodule-file=" + name + "=" + fs::absolute(pcm).string());
  }
  return a;
}

int main() {
  try {
    std::cout << "[impa] input : " << kIn << "\n";
    std::cout << "[impa] output: " << kOut << "\n";
    std::cout << "[impa] pcmDir: " << kPcmDir << "\n";

    fs::create_directories(kPcmDir);

    auto entries = load_cc(kIn);

    // 收集所有 .ixx 与其 TU 的 /I /D
    std::vector<ModuleInfo> modules;
    for (auto &e : entries) {
      if (!ieq_ends_with(e.file, ".ixx"))
        continue;

      auto name = extract_module_name(e.file);
      if (!name) {
        std::cerr << "[warn] cannot find `export module ...;` in " << e.file
                  << "\n";
        continue;
      }
      ModuleInfo mi;
      mi.name = *name;
      mi.ixx = fs::path(e.file);
      mi.pcm = kPcmDir / (mi.name + ".pcm");

      // 携带 /I /D
      const auto &args =
          e.args.empty() ? split_cmd("")
                         : e.args; // 如果 e.args 为空但 e.file 是 .ixx，通常
                                   // compile_commands 仍会有 args；防御而已
      for (size_t i = 0; i < args.size(); ++i) {
        const std::string &t = args[i];
        auto push_pair = [&](std::string_view flag) {
          if (i + 1 < args.size()) {
            mi.incdefs.push_back(std::string(flag));
            mi.incdefs.push_back(args[++i]);
          }
        };
        if (starts_with(t, "/I")) {
          if (t.size() > 2)
            mi.incdefs.push_back(t);
          else
            push_pair("/I");
        } else if (starts_with(t, "/D")) {
          if (t.size() > 2)
            mi.incdefs.push_back(t);
          else
            push_pair("/D");
        } else if (starts_with(t, "-I") || starts_with(t, "-D")) {
          mi.incdefs.push_back(t);
        }
      }
      modules.push_back(std::move(mi));
    }

    // Step 1: 预编译 .ixx → .pcm（用 clang++ --precompile -x c++-module）
    for (auto &m : modules) {
      auto incdefs = to_clangxx_incdefs(m.incdefs);

      std::vector<std::string> cmd;
      cmd.push_back(kClangXX.string());
      cmd.push_back(std::string("-std=") + std::string(kStd));
      cmd.push_back("--precompile");
      cmd.push_back("-x");
      cmd.push_back("c++-module");
      for (auto &id : incdefs)
        cmd.push_back(id);
      cmd.push_back(fs::absolute(m.ixx).string());
      cmd.push_back("-o");
      cmd.push_back(fs::absolute(m.pcm).string());

      int rc = run(cmd);
      if (rc != 0) {
        std::cerr << "[error] precompile failed: " << m.ixx << " (rc=" << rc
                  << ")\n";
        return rc;
      }
    }

    // 建模块名 → pcm 路径映射
    std::map<std::string, fs::path> mod2pcm;
    for (auto &m : modules)
      mod2pcm[m.name] = m.pcm;

    // Step 2: 生成 clangd 友好的 compile_commands
    json out = json::array();

    // 2.1 为每个 .ixx 写一条 syntax-only 的索引记录（clang-cl，不产物）
    for (auto &m : modules) {
      json rec;
      rec["directory"] = fs::absolute(".").string();
      rec["file"] = fs::absolute(m.ixx).string();

      std::vector<std::string> argv;
      argv.push_back(kClangCL.string());
      argv.push_back("/TP");
      argv.push_back("/clang:-fsyntax-only");
      argv.push_back("/clang:-xc++-module");
      argv.push_back("/clang:-fmodule-name=" + m.name);
      // 携带原 TU 的 /I /D
      for (auto &id : m.incdefs)
        argv.push_back(id);
      argv.push_back(std::string("/std:") + std::string(kStd));
      argv.push_back(fs::absolute(m.ixx).string());

      rec["arguments"] = argv;
      out.push_back(std::move(rec));
    }

    // 2.2 处理非 .ixx 的记录
    for (auto &e : entries) {
      if (ieq_ends_with(e.file, ".ixx"))
        continue;

      const auto &inArgs =
          !e.args.empty()
              ? e.args
              : (e.file.size() ? split_cmd("") : std::vector<std::string>{});
      auto argv =
          rewrite_for_clangcl(inArgs, mod2pcm, fs::absolute(e.file).string());

      json rec;
      rec["directory"] = e.directory;
      rec["file"] = fs::absolute(e.file).string();
      rec["arguments"] = argv;
      out.push_back(std::move(rec));
    }

    // 写出
    fs::create_directories(kOut.parent_path().empty() ? "."
                                                      : kOut.parent_path());
    std::ofstream ofs(kOut, std::ios::binary);
    ofs << out.dump(2) << std::endl;

    std::cout << "\n[ok] wrote clangd DB: " << kOut << "\n";
    std::cout << "[ok] pcm dir        : " << kPcmDir << "\n";
    std::cout << "[ok] modules        :";
    for (auto &m : modules)
      std::cout << ' ' << m.name;
    std::cout << "\n";
    return 0;

  } catch (const std::exception &ex) {
    std::cerr << "[fatal] " << ex.what() << "\n";
    return 1;
  }
}