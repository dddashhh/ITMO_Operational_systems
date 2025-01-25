#include <cassert>
#include <filesystem>

#include "test_utils.h"
namespace fs = std::filesystem;

void testEmaSortIntSmall() {
    std::cout << "Running small ema-sort-int test..." << std::endl;
    std::string inputFile = "test_input.bin";
    std::string outputFile = "test_output.bin";
    size_t fileSize = 1024 * 1024;
    createRandomBinaryFile(inputFile, fileSize);
    std::string command = "TEST=true ./ema-sort-int 1 1 1";
    executeCommand(command);

    assert(fs::file_size(outputFile) == fileSize);
    fs::remove(inputFile);
    fs::remove(outputFile);
    std::cout << "Small ema-sort-int test passed." << std::endl;
}

void testEmaSortIntLarge() {
    std::cout << "Running large ema-sort-int test..." << std::endl;
    std::string inputFile = "test_large_input.bin";
    std::string outputFile = "test_output.bin";
    size_t fileSize = 256 * 1024 * 1024;
    createRandomBinaryFile(inputFile, fileSize);
    std::string command = "TEST=true ./ema-sort-int 1 256 10";
    executeCommand(command);
    assert(fs::file_size(outputFile) == fileSize);
    fs::remove(inputFile);
    fs::remove(outputFile);
    std::cout << "Large ema-sort-int test passed." << std::endl;
}

int main() {
    testEmaSortIntSmall();
    testEmaSortIntLarge();
    return 0;
}