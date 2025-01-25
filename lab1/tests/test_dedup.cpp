#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "test_utils.h"
namespace fs = std::filesystem;

void testDedupBasic() {
    std::cout << "Running basic dedup test..." << std::endl;
    std::string inputFile = "dedup_input.txt";
    std::string outputFile = "dedup_output.txt";
    createInputTextFile(inputFile, 1000, 10);
    createInputTextFile(outputFile, 1000, 10);

    std::string output = executeCommand("TEST=true ./dedup 1");

    try {
        readFile(outputFile);
    } catch (const std::runtime_error& e) {
        std::cerr << "Output file not found" << std::endl;
        assert(false);
    }

    fs::remove(inputFile);
    fs::remove(outputFile);
    std::cout << "Basic dedup test passed." << std::endl;
}

void testDedupEmpty() {
    std::cout << "Running empty dedup test..." << std::endl;
    std::string inputFile = "dedup_empty.txt";
    std::string outputFile = "dedup_output.txt";
    createInputTextFile(inputFile, 0, 0);
    createInputTextFile(outputFile, 0, 0);

    std::ofstream outfile(outputFile);
    outfile.close();

    std::string output = executeCommand("TEST=true ./dedup 1");

    if (fs::exists(outputFile)) {
        assert(compareFiles(inputFile, outputFile));
    } else {
        std::cerr << "output file not found" << std::endl;
        assert(false);
    }
    fs::remove(inputFile);
    fs::remove(outputFile);
    std::cout << "Empty dedup test passed." << std::endl;
}

void testDedupManyDuplicates() {
    std::cout << "Running dedup with many duplicates test..." << std::endl;
    std::string inputFile = "dedup_many_duplicates.txt";
    std::string outputFile = "dedup_many_duplicates_output.txt";
    createInputTextFile(inputFile, 1000, 10);
    createInputTextFile(outputFile, 1000, 10);
    std::ofstream file(inputFile, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << inputFile << std::endl;
        throw std::runtime_error("Error opening file");
    }
    for (int i = 0; i < 1000; ++i) {
        file << "aaaaaaaaaa" << std::endl;
    }
    file.close();

    std::string output = executeCommand("TEST=true ./dedup 1");

    try {
        readFile(outputFile);
    } catch (const std::runtime_error& e) {
        std::cerr << "Output file not found" << std::endl;
        assert(false);
    }

    fs::remove(inputFile);
    fs::remove(outputFile);
    std::cout << "Dedup with many duplicates test passed." << std::endl;
}

int main() {
    testDedupBasic();
    testDedupEmpty();
    testDedupManyDuplicates();
    return 0;
}