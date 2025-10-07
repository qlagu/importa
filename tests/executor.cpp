#include <cassert>
import executor;
import std;

// Use using directive to simplify test code
using namespace importa::executor;
namespace fs = std::filesystem;

// --- Unit Tests ---

void test_command_to_string() {
    std::cout << "--- Running unit test: Command::to_string ---\n";

    // Test 1.1: Simple command, no spaces
    Command cmd1;
    cmd1.executable = "git";
    cmd1.arguments = {"--version"};
    assert(cmd1.to_string() == "\"git\" --version");
    std::cout << "  Test 1.1: Simple command... Passed\n";  

    // Test 1.2: Executable path contains spaces
    Command cmd2;
    cmd2.executable = "C:\\Program Files\\My App\\app.exe";
    cmd2.arguments = {"-a", "-b"};
    assert(cmd2.to_string() == "\"C:\\Program Files\\My App\\app.exe\" -a -b");
    std::cout << "  Test 1.2: Path with spaces... Passed\n";

    // Test 1.3: Argument contains spaces
    Command cmd3;
    cmd3.executable = "my_app.exe";
    cmd3.arguments = {"arg1", "hello world", "arg3"};
    assert(cmd3.to_string() == "\"my_app.exe\" arg1 \"hello world\" arg3");
    std::cout << "  Test 1.3: Argument with spaces... Passed\n";

    // Test 1.4: No arguments
    Command cmd4;
    cmd4.executable = "tool.exe";
    assert(cmd4.to_string() == "\"tool.exe\"");
    std::cout << "  Test 1.4: No arguments... Passed\n";

    std::cout << "--- All Command::to_string tests passed ---\n\n";
}

void test_dry_run_executor() {
    std::cout << "--- Running unit test: DryRunExecutor ---\n";
    
    // Test 2.1: Basic functionality
    std::stringstream ss;
    DryRunExecutor executor(ss);
    Command cmd;
    cmd.executable = "test.exe";
    cmd.arguments = {"--config", "path/to file"};

    auto result = executor.execute(cmd);
    
    assert(result.success == true);
    assert(result.exit_code == 0);
    const std::string expected_output = "[DRY RUN] \"test.exe\" --config \"path/to file\"\n";
    assert(ss.str() == expected_output);
    std::cout << "  Test 2.1: Basic functionality... Passed\n";

    std::cout << "--- All DryRunExecutor tests passed ---\n\n";
}


// --- Integration Tests ---

void test_local_executor() {
    std::cout << "--- Running integration test: LocalExecutor ---\n";
    LocalExecutor executor;

    // Test 3.1: Successful execution and capture stdout
    Command cmd_stdout;
    cmd_stdout.executable = "cmd.exe";
    cmd_stdout.arguments = {"/c", "echo hello executor"};
    auto result_stdout = executor.execute(cmd_stdout);
    assert(result_stdout.success);
    assert(result_stdout.exit_code == 0);
    assert(result_stdout.std_out == "hello executor\r\n");
    assert(result_stdout.std_err.empty());
    std::cout << "  Test 3.1: Capture stdout... Passed\n";

    // Test 3.2: Process returns non-zero exit code
    Command cmd_exit_code;
    cmd_exit_code.executable = "cmd.exe";
    cmd_exit_code.arguments = {"/c", "exit 99"};
    auto result_exit_code = executor.execute(cmd_exit_code);
    assert(!result_exit_code.success);
    assert(result_exit_code.exit_code == 99);
    std::cout << "  Test 3.2: Non-zero exit code... Passed\n";

    // Test 3.3: Capture stderr
    Command cmd_stderr;
    cmd_stderr.executable = "cmd.exe";
    cmd_stderr.arguments = {"/c", "echo hello error >&2"};
    auto result_stderr = executor.execute(cmd_stderr);
    assert(result_stderr.success);
    assert(result_stderr.exit_code == 0);
    assert(result_stderr.std_err == "hello error \r\n");
    assert(result_stderr.std_out.empty());
    std::cout << "  Test 3.3: Capture stderr... Passed\n";

    // Test 3.4: Start a non-existent command
    Command cmd_non_existent;
    cmd_non_existent.executable = "this_command_does_not_exist_12345.exe";
    bool exception_thrown = false;
    try {
        executor.execute(cmd_non_existent);
    } catch (const std::runtime_error&) {
        exception_thrown = true;
    }
    assert(exception_thrown);
    std::cout << "  Test 3.4: Non-existent command throws exception... Passed\n";

    // Test 3.5: Test working directory
    auto temp_dir = fs::temp_directory_path() / "importa_test_wd";
    fs::create_directory(temp_dir);
    
    Command cmd_wd;
    cmd_wd.executable = "cmd.exe";
    cmd_wd.arguments = {"/c", "cd"};
    cmd_wd.working_directory = temp_dir;
    
    auto result_wd = executor.execute(cmd_wd);
    assert(result_wd.success);
    // 'cd' command on Windows prints the current directory and ends with \r\n
    std::string expected_path_str = temp_dir.string() + "\r\n";
    assert(result_wd.std_out == expected_path_str);
    fs::remove(temp_dir); // Clean up temp directory
    std::cout << "  Test 3.5: Specify working directory... Passed\n";

    std::cout << "--- All LocalExecutor tests passed ---\n\n";
}


int main() {
    std::cout << "=================================\n";
    std::cout << "     Start running executor tests    \n";
    std::cout << "=================================\n\n";
    
    try {
        // Run all unit tests
        test_command_to_string();
        test_dry_run_executor();

        // Run all integration tests
        test_local_executor();

    } catch (const std::exception& e) {
        std::cerr << "!!! Test failed, uncaught exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "=================================\n";
    std::cout << "   All tests passed successfully!   \n";
    std::cout << "=================================\n";

    return 0;
}