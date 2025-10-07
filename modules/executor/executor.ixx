// executor.ixx (fixed typo and module name)
//
// Style: Export individual items for better API clarity.

// Global Module Fragment
module;

// Module declaration (updated name)
export module executor;

import std;

// The namespace itself is NOT exported.
namespace importa
{

namespace fs = std::filesystem;

// Export this struct specifically.
export struct Command
{
    fs::path executable;
    std::vector<std::string> arguments;
    fs::path working_directory;
    std::map<std::string, std::string> environment_variables;

    std::string to_string() const;
};

// Export this struct specifically.
export struct ExecutionResult
{
    bool success = false;
    int exit_code = -1;
    std::string std_out;
    std::string std_err;

    explicit operator bool() const;
};

// Export this interface specifically.
export class IExecutor
{
  public:
    virtual ~IExecutor() = default;
    virtual ExecutionResult execute(const Command& command) = 0;
};

// Export this concrete class specifically.
export class LocalExecutor final : public IExecutor
{
  public:
    LocalExecutor() = default;
    ~LocalExecutor() override = default;
    // CORRECTED LINE: No hyphen in ExecutionResult
    ExecutionResult execute(const Command& command) override;
};

// Export this concrete class specifically.
export class DryRunExecutor final : public IExecutor
{
  public:
    explicit DryRunExecutor(std::ostream& output_stream);
    ~DryRunExecutor() override = default;
    ExecutionResult execute(const Command& command) override;

  private:
    std::ostream& m_output_stream;
};

} // namespace importa