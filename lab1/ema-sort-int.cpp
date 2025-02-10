#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;

void create_random_binary_file(const std::string& filename, size_t size) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        throw std::runtime_error("Error opening file");
    }

    std::vector<char> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<char>(distrib(gen));
    }

    file.write(data.data(), data.size());
    file.close();
}

std::vector<char> read_binary_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        throw std::runtime_error("Error opening file");
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (size > 0) {
        if (!file.read(buffer.data(), size)) {
            std::cerr << "Error reading from file: " << filename << std::endl;
            throw std::runtime_error("Error reading from file");
        }
    }

    return buffer;
}

void write_binary_file(const std::string& filename,
                       const std::vector<char>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        throw std::runtime_error("Error opening file");
    }

    file.write(data.data(), data.size());
    file.close();
}

void merge_sorted_chunks(const std::vector<std::string>& chunk_filenames,
                         const std::string& output_filename) {
    std::vector<std::ifstream> chunk_files(chunk_filenames.size());
    std::vector<char> current_elements(chunk_filenames.size());
    std::vector<bool> eof_flags(chunk_filenames.size(), false);

    for (size_t i = 0; i < chunk_filenames.size(); ++i) {
        chunk_files[i].open(chunk_filenames[i], std::ios::binary);
        if (!chunk_files[i].is_open()) {
            std::cerr << "Error opening chunk file for reading: "
                      << chunk_filenames[i] << std::endl;
            throw std::runtime_error("Error opening file");
        }

        if (chunk_files[i].read(&current_elements[i], 1)) {
        } else {
            eof_flags[i] = true;
        }
    }

    std::ofstream output_file(output_filename, std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "Error opening output file for writing: " << output_filename
                  << std::endl;
        throw std::runtime_error("Error opening file");
    }

    while (true) {
        int min_index = -1;
        char min_element = 0;

        for (size_t i = 0; i < chunk_filenames.size(); ++i) {
            if (!eof_flags[i]) {
                if (min_index == -1 || current_elements[i] < min_element) {
                    min_index = i;
                    min_element = current_elements[i];
                }
            }
        }

        if (min_index == -1) {
            break;
        }
        output_file.write(&min_element, 1);

        if (chunk_files[min_index].read(&current_elements[min_index], 1)) {
        } else {
            eof_flags[min_index] = true;
        }
    }

    for (auto& file : chunk_files) {
        file.close();
    }

    output_file.close();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr
                << "Usage: ema-sort-int <iterations> <file_size_mb> <chunk_size_mb>"
                << std::endl;
        return 1;
    }

    int iterations = std::stoi(argv[1]);
    int file_size_mb = std::stoi(argv[2]);
    int chunk_size_mb = std::stoi(argv[3]);

    size_t file_size = static_cast<size_t>(file_size_mb) * 1024 * 1024;
    size_t chunk_size = static_cast<size_t>(chunk_size_mb) * 1024 * 1024;

    std::string input_filename = "input.bin";
    std::string output_filename = "test_output.bin";

    if (std::getenv("TEST") != nullptr) {
        output_filename = "test_output.bin";
    }

    std::cout << "Starting ema-sort-int with " << iterations
              << " iterations, file size: " << file_size_mb
              << " MB, chunk size: " << chunk_size_mb << " MB" << std::endl;

    for (int i = 0; i < iterations; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();

        create_random_binary_file(input_filename, file_size);
        std::vector<char> data = read_binary_file(input_filename);

        if (data.size() <= chunk_size) {
            std::sort(data.begin(), data.end());
            write_binary_file(output_filename, data);

        } else {
            std::vector<std::string> chunk_filenames;
            for (size_t j = 0; j < data.size(); j += chunk_size) {
                size_t current_chunk_size = std::min(chunk_size, data.size() - j);
                std::vector<char> chunk(data.begin() + j,
                                        data.begin() + j + current_chunk_size);
                std::sort(chunk.begin(), chunk.end());

                std::string chunk_filename = "temp_chunk_" + std::to_string(j) + ".bin";
                write_binary_file(chunk_filename, chunk);
                chunk_filenames.push_back(chunk_filename);
            }
            merge_sorted_chunks(chunk_filenames, "merged_output.bin");
            std::vector<char> merged_data = read_binary_file("merged_output.bin");
            write_binary_file(output_filename, merged_data);

            for (const auto& filename : chunk_filenames) {
                remove(filename.c_str());
            }
            remove("merged_output.bin");
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time);
        double seconds = duration.count() / 1000000.0;
        std::cout << "Iteration " << i + 1 << ": Sorted " << file_size_mb
                  << " MB in " << std::fixed << std::setprecision(3) << seconds
                  << " seconds" << std::endl;

        remove(input_filename.c_str());

        if (!fs::exists(output_filename)) {
            std::ofstream outfile(output_filename, std::ios::binary);
            outfile.close();
        }
    }
    std::cout << "Exit status: 0" << std::endl;

    return 0;
}
