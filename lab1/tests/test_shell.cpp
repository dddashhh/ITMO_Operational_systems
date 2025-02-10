#include <cassert>
#include <iostream>
#include <regex>

#include "test_utils.h"

void testShellSmoke() {
    std::cout << "Running smoke test for shell..." << std::endl;
    std::string shell_path = getExecutablePath("shell");
    std::string output =
            executeCommand(shell_path + " -c \"echo Hello from shell\"");
    assert(output.find("Hello from shell") != std::string::npos);
    std::cout << "Shell smoke test passed." << std::endl;
}

void testShellEmaSortInt() {
    std::cout << "Running ema-sort-int test via shell..." << std::endl;
    std::string shell_path = getExecutablePath("shell");
    std::string ema_sort_int_path = getExecutablePath("ema-sort-int");
    std::string output =
            executeCommand(shell_path + " -c \"" + ema_sort_int_path + " 1 10 1\"");

    std::regex result_regex(R"(Iteration 1: Sorted 10 MB in)");
    std::regex user_time_regex(R"(User time: \d+\.\d+ seconds \(\d+\.\d+%\))");
    std::regex system_time_regex(
            R"(System time: \d+\.\d+ seconds \(\d+\.\d+%\))");
    std::regex real_time_regex(R"(Real time: \d+\.\d+ seconds)");

    std::smatch match;

    bool found = std::regex_search(output, match, result_regex);
    assert(found);

    found = std::regex_search(output, match, user_time_regex);
    assert(found);

    found = std::regex_search(output, match, system_time_regex);
    assert(found);

    found = std::regex_search(output, match, real_time_regex);
    assert(found);

    std::cout << "ema-sort-int test via shell passed." << std::endl;
}

void testShellDedup() {
    std::cout << "Running dedup test via shell..." << std::endl;
    std::string shell_path = getExecutablePath("shell");
    std::string dedup_path = getExecutablePath("dedup");
    std::string output =
            executeCommand(shell_path + " -c \"" + dedup_path + " 1\"");

    std::regex result_regex(R"(Iteration 1: Deduped \d+ lines)");
    std::regex user_time_regex(R"(User time: \d+\.\d+ seconds \(\d+\.\d+%\))");
    std::regex system_time_regex(
            R"(System time: \d+\.\d+ seconds \(\d+\.\d+%\))");
    std::regex real_time_regex(R"(Real time: \d+\.\d+ seconds)");

    std::smatch match;
    bool found = std::regex_search(output, match, result_regex);
    assert(found);

    found = std::regex_search(output, match, user_time_regex);
    assert(found);

    found = std::regex_search(output, match, system_time_regex);
    assert(found);

    found = std::regex_search(output, match, real_time_regex);
    assert(found);
    std::cout << "Dedup test via shell passed." << std::endl;
}

int main() {
    testShellSmoke();
    testShellEmaSortInt();
    testShellDedup();
    return 0;
}