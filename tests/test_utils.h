#pragma once

#include <unistd.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

std::string getExecutablePath(const std::string& executable) {
#ifdef __APPLE__
    char path[PATH_MAX];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    std::string fullPath = std::string(path);
    std::size_t pos = fullPath.find_last_of('/');
    std::string dir = fullPath.substr(0, pos);
    return dir + "/" + executable;
  } else {
    throw std::runtime_error("_NSGetExecutablePath failed");
  }
#else
    char result[PATH_MAX];
    ssize_t count = readlink(("/proc/self/exe"), result, PATH_MAX);
    if (count != -1) {
        std::string path = std::string(result, count);
        std::size_t pos = path.find_last_of('/');
        std::string dir = path.substr(0, pos);
        return dir + "/" + executable;
    } else {
        throw std::runtime_error("readlink failed");
    }
#endif
}

std::string executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("pipe() failed");
    }

    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("fork() failed");
    } else if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execl("/bin/sh", "sh", "-c", command.c_str(), NULL);
        perror("execl() failed");
        exit(EXIT_FAILURE);
    } else {
        close(pipefd[1]);
        FILE* pipe_read = fdopen(pipefd[0], "r");
        if (pipe_read == nullptr) {
            throw std::runtime_error("fdopen failed");
        }
        while (fgets(buffer.data(), buffer.size(), pipe_read) != nullptr) {
            result += buffer.data();
        }
        fclose(pipe_read);
        int status;
        waitpid(pid, &status, 0);
    }

    return result;
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for reading: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool compareFiles(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1, std::ifstream::binary | std::ifstream::ate);
    std::ifstream f2(file2, std::ifstream::binary | std::ifstream::ate);

    if (!f1.is_open() || !f2.is_open()) {
        return false;
    }
    if (f1.tellg() != f2.tellg()) {
        return false;
    }

    f1.seekg(0, std::ios::beg);
    f2.seekg(0, std::ios::beg);
    return std::equal(std::istreambuf_iterator<char>(f1),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2));
}

void createRandomBinaryFile(const std::string& filename, size_t size) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for writing: " + filename);
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

void createInputTextFile(const std::string& filename, int num_lines,
                         int line_length) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for writing: " + filename);
    }
    static std::mt19937 generator(std::random_device{}());
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::uniform_int_distribution<> distribution(0, sizeof(charset) - 2);
    for (int i = 0; i < num_lines; ++i) {
        std::string str(line_length, 0);
        std::generate_n(str.begin(), line_length,
                        [&]() { return charset[distribution(generator)]; });
        file << str << std::endl;
    }
    file.close();
}
