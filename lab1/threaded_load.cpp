#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

void emaSort(int num_iterations) {
    size_t array_size = 1000000;

    for (int i = 0; i < num_iterations; ++i) {
        std::vector<int> data(array_size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 1000000);
        for (size_t j = 0; j < array_size; j++) {
            data[j] = distrib(gen);
        }

        auto start = std::chrono::high_resolution_clock::now();
        std::sort(data.begin(), data.end());
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> duration = end - start;
        std::cout << "ema-sort-int thread: Iteration " << i + 1 << ": Sorted "
                  << array_size << " elements in " << duration.count() << " seconds"
                  << std::endl;
    }
}

std::string generateRandomString(size_t length) {
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
    std::string str;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, sizeof(alphanum) - 2);

    for (size_t i = 0; i < length; ++i) {
        str += alphanum[distrib(gen)];
    }
    return str;
}

void createInputFile(const std::string& filename, size_t num_lines,
                     size_t string_length) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    for (size_t i = 0; i < num_lines; ++i) {
        file << generateRandomString(string_length) << std::endl;
    }

    file.close();
}

void dedup(int num_iterations) {
    std::string input_filename = "input.txt";
    std::string output_filename = "output.txt";
    size_t num_lines = 10000;
    size_t string_length = 100;

    createInputFile(input_filename, num_lines, string_length);

    for (int i = 0; i < num_iterations; ++i) {
        std::unordered_set<std::string> unique_lines;
        std::vector<std::string> all_lines;

        std::ifstream input_file(input_filename);
        if (!input_file.is_open()) {
            std::cerr << "Error opening file for reading: " << input_filename
                      << std::endl;
            return;
        }

        std::string line;
        while (std::getline(input_file, line)) {
            all_lines.push_back(line);
        }
        input_file.close();

        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& l : all_lines) {
            unique_lines.insert(l);
        }

        std::ofstream output_file(output_filename);
        if (!output_file.is_open()) {
            std::cerr << "Error opening file for writing: " << output_filename
                      << std::endl;
            return;
        }

        for (const auto& l : unique_lines) {
            output_file << l << std::endl;
        }
        output_file.close();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "dedup thread: Iteration " << i + 1 << ": Deduped "
                  << all_lines.size() << " lines in " << duration.count()
                  << " seconds" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: threaded_load <num_iterations>" << std::endl;
        return 1;
    }

    int num_iterations = std::stoi(argv[1]);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread thread1(emaSort, num_iterations);
    std::thread thread2(dedup, num_iterations);

    thread1.join();
    thread2.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Total time with threads: " << duration.count() << " seconds"
              << std::endl;

    return 0;
}