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
#include <memory>

namespace fs = std::filesystem;

void create_random_binary_file(const std::string& filename, size_t size) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        throw std::runtime_error("Error opening file");
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for (size_t i = 0; i < size; ++i) {
        char random_char = static_cast<char>(distrib(gen));
        file.write(&random_char, 1);
    }

    file.close();
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
                         const std::string& output_filename, size_t merge_chunk_size = 1024 * 1024 ) {
    std::vector<std::ifstream> chunk_files(chunk_filenames.size());
    std::vector<std::unique_ptr<char[]>> current_elements(chunk_filenames.size());
    std::vector<size_t> current_element_sizes(chunk_filenames.size(), 0);
    std::vector<size_t> current_element_offsets(chunk_filenames.size(), 0);

    std::vector<bool> eof_flags(chunk_filenames.size(), false);

    for (size_t i = 0; i < chunk_filenames.size(); ++i) {
        chunk_files[i].open(chunk_filenames[i], std::ios::binary);
        if (!chunk_files[i].is_open()) {
            std::cerr << "Error opening chunk file for reading: "
                      << chunk_filenames[i] << std::endl;
            throw std::runtime_error("Error opening file");
        }
        current_elements[i] = std::make_unique<char[]>(merge_chunk_size);
        current_element_sizes[i] = chunk_files[i].read(current_elements[i].get(), merge_chunk_size).gcount();
        if(current_element_sizes[i] == 0) {
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
                if (min_index == -1 || current_elements[i][current_element_offsets[i]] < min_element) {
                    min_index = i;
                    min_element = current_elements[i][current_element_offsets[i]];
                }
            }
        }


        if (min_index == -1) {
            break;
        }
        output_file.write(&min_element, 1);
        current_element_offsets[min_index]++;
        if(current_element_offsets[min_index] >= current_element_sizes[min_index]) {
            current_element_offsets[min_index] = 0;
            current_element_sizes[min_index] = chunk_files[min_index].read(current_elements[min_index].get(), merge_chunk_size).gcount();
            if(current_element_sizes[min_index] == 0) {
                eof_flags[min_index] = true;
            }
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
    long long file_size = static_cast<long long>(file_size_mb) * 1024 * 1024;
    long long chunk_size = static_cast<long long>(chunk_size_mb) * 1024 * 1024;


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
        std::vector<std::string> chunk_filenames;
        std::ifstream file(input_filename, std::ios::binary);

        if (!file.is_open()) {
            std::cerr << "Error opening input file: " << input_filename << std::endl;
            throw std::runtime_error("Error opening file");
        }
        size_t bytes_read = 0;
        int chunk_index = 0;
        while (bytes_read < file_size) {
            std::string chunk_filename = "chunk_" + std::to_string(chunk_index++) + ".bin";
            chunk_filenames.push_back(chunk_filename);
            std::vector<char> buffer(chunk_size);

            size_t read_size = file.read(buffer.data(), chunk_size).gcount();
            if(read_size > 0) {
                std::sort(buffer.begin(), buffer.begin() + read_size);
                write_binary_file(chunk_filename, buffer);

            } else {
                break;
            }
            bytes_read += read_size;
        }
        file.close();
        merge_sorted_chunks(chunk_filenames, output_filename);

        for (const auto& chunk_filename : chunk_filenames) {
            fs::remove(chunk_filename);
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
