#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>

std::string generateRandomString(size_t length) {
    static std::mt19937 generator(std::random_device{}());
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::uniform_int_distribution<> distribution(0, sizeof(charset) - 2);

    std::string str(length, 0);
    std::generate_n(str.begin(), length,
                    [&]() { return charset[distribution(generator)]; });

    return str;
}

void createInputFile(const std::string& filename, int num_lines,
                     int line_length) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    for (int i = 0; i < num_lines; ++i) {
        file << generateRandomString(line_length) << std::endl;
    }
    file.close();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: dedup <num_iterations>" << std::endl;
        return 1;
    }

    int num_iterations = std::stoi(argv[1]);
    int num_lines = 10000;
    int line_length = 50;
    std::string input_filename = "input.txt";
    std::string output_filename = "output.txt";

    std::cout << "Starting dedup with " << num_iterations
              << " iterations and file: " << input_filename << std::endl;

    for (int i = 0; i < num_iterations; ++i) {
        createInputFile(input_filename, num_lines, line_length);

        std::unordered_set<std::string> unique_lines;

        auto start = std::chrono::high_resolution_clock::now();

        std::ifstream file(input_filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file for reading: " << input_filename
                      << std::endl;
            return 1;
        }

        std::string line;
        while (std::getline(file, line)) {
            unique_lines.insert(line);
        }
        file.close();

        std::ofstream outfile(output_filename);
        if (!outfile.is_open()) {
            std::cerr << "Error opening file for writing: " << output_filename
                      << std::endl;
            return 1;
        }
        for (const auto& l : unique_lines) {
            outfile << l << std::endl;
        }
        outfile.close();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "Iteration " << i + 1 << ": Deduped " << num_lines
                  << " lines in " << std::fixed << std::setprecision(7)
                  << duration.count() << " seconds" << std::endl;
    }
    return 0;
}