#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <string>
#include <unordered_set>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <dlfcn.h>


typedef int (*cache_init_func)(size_t);

typedef void (*cache_destroy_func)();

typedef int (*my_open_func)(const char *pathname, int flags, ...);

typedef ssize_t (*my_read_func)(int fd, void *buf, size_t count);

typedef ssize_t (*my_write_func)(int fd, const void *buf, size_t count);

typedef int (*my_close_func)(int fd);

typedef off_t (*my_lseek_func)(int fd, off_t offset, int whence);

std::string generateRandomString(size_t length) {
    std::mt19937 generator(std::random_device{}());
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::uniform_int_distribution<> distribution(0, sizeof(charset) - 2);

    std::string str(length, 0);
    std::generate_n(str.begin(), length,
                    [&]() { return charset[distribution(generator)]; });

    return str;
}

void createInputFile(const std::string &filename, int num_lines,
                     int line_length, my_open_func my_open, my_write_func my_write,
                     my_close_func my_close) {
    int fd = my_open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        std::cerr << "Ошибка открытия файла для записи: " << filename
                  << " - " << strerror(errno) << std::endl;
        return;
    }

    std::string line;
    for (int i = 0; i < num_lines; ++i) {
        line = generateRandomString(line_length) + "\n";
        ssize_t bytesWritten = my_write(fd, line.c_str(), line.length());
        if (bytesWritten == -1) {
            std::cerr << "Ошибка записи в файл: " << filename
                      << " - " << strerror(errno) << std::endl;
            my_close(fd);
            return;
        }
    }

    if (my_close(fd) == -1) {
        std::cerr << "Ошибка закрытия файла: " << filename
                  << " - " << strerror(errno) << std::endl;
    }
}

void runDedup(int num_iterations, const std::string &input_filename,
              my_open_func my_open, my_read_func my_read, my_write_func my_write,
              my_close_func my_close, my_lseek_func my_lseek, bool verbose) {

    for (int i = 0; i < num_iterations; ++i) {
        int num_lines = 10000;
        int line_length = 50;
        createInputFile(input_filename, num_lines, line_length, my_open, my_write, my_close);
        std::unordered_set<std::string> unique_lines;

        auto start = std::chrono::high_resolution_clock::now();

        int fd = my_open(input_filename.c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "Ошибка открытия файла для чтения: " << input_filename
                      << " - " << strerror(errno) << std::endl;
            return;
        }

        std::string line;
        std::vector<char> buffer(1024);
        ssize_t bytesRead;

        while ((bytesRead = my_read(fd, buffer.data(), buffer.size())) > 0) {
            std::string_view data(buffer.data(), bytesRead);
            size_t start_pos = 0, end_pos;
            while ((end_pos = data.find('\n', start_pos)) != std::string_view::npos) {
                std::string line_str(data.substr(start_pos, end_pos - start_pos));
                unique_lines.insert(line_str);
                start_pos = end_pos + 1;
            }
            if (start_pos < data.size()) {
                std::string line_str(data.substr(start_pos));
                unique_lines.insert(line_str);
            }
        }

        if (bytesRead == -1) {
            std::cerr << "Ошибка чтения файла: " << input_filename
                      << " - " << strerror(errno) << std::endl;
            my_close(fd);
            return;
        }

        if (my_close(fd) == -1) {
            std::cerr << "Ошибка закрытия файла: " << input_filename
                      << " - " << strerror(errno) << std::endl;
            return;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start)
                .count();
        if (verbose) {
            std::cout << "Итерация " << i + 1 << ": " << duration << " мс"
                      << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    int num_iterations;
    std::string input_filename = "input.txt";
    std::string cache_library_path;
    void *cache_library = nullptr;
    cache_init_func cache_init = nullptr;
    cache_destroy_func cache_destroy = nullptr;
    bool verbose = false;


    my_open_func my_open = ::open;
    my_read_func my_read = ::read;
    my_write_func my_write = ::write;
    my_close_func my_close = ::close;
    my_lseek_func my_lseek = ::lseek;


    if (argc < 2) {
        std::cerr << "Использование: dedup <количество_итераций> [путь_к_cache.so] [-v]" << std::endl;
        return 1;
    }


    num_iterations = std::stoi(argv[1]);


    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "-v") {
            verbose = true;
        } else {
            cache_library_path = argv[i];
        }
    }


    if (!cache_library_path.empty()) {
        cache_library = dlopen(cache_library_path.c_str(), RTLD_NOW);
        if (!cache_library) {
            std::cerr << "Ошибка загрузки библиотеки кэша: " << dlerror() << std::endl;
            return 1;
        }

        cache_init = (cache_init_func) dlsym(cache_library, "cache_init");
        cache_destroy = (cache_destroy_func) dlsym(cache_library, "cache_destroy");

        if (!cache_init || !cache_destroy) {
            std::cerr << "Ошибка загрузки символов из библиотеки кэша." << std::endl;
            dlclose(cache_library);
            return 1;
        }


        if (cache_init(10) != 0) {
            std::cerr << "Ошибка инициализации кэша." << std::endl;
            dlclose(cache_library);
            return 1;
        }
        std::cout << "Используем кэш из библиотеки: " << cache_library_path << std::endl;


        my_open = (my_open_func) dlsym(RTLD_DEFAULT, "open");
        my_read = (my_read_func) dlsym(RTLD_DEFAULT, "read");
        my_write = (my_write_func) dlsym(RTLD_DEFAULT, "write");
        my_close = (my_close_func) dlsym(RTLD_DEFAULT, "close");
        my_lseek = (my_lseek_func) dlsym(RTLD_DEFAULT, "lseek");

        if (!my_open || !my_read || !my_write || !my_close || !my_lseek) {
            std::cerr << "Ошибка загрузки перехваченных системных вызовов." << std::endl;
            dlclose(cache_library);
            return 1;
        }


        std::cout << "Запуск с нашим кэшем..." << std::endl;
        runDedup(num_iterations, input_filename, my_open, my_read, my_write, my_close, my_lseek, verbose);

    } else {
        std::cout << "Запуск без кэша..." << std::endl;
        runDedup(num_iterations, input_filename, ::open, ::read, ::write, ::close, ::lseek, verbose);
    }


    if (cache_library) {
        cache_destroy();
        dlclose(cache_library);
    }

    return 0;
}
