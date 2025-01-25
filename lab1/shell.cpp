#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> args;
    std::stringstream ss(line);
    std::string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }
    return args;
}

long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void execute_command_with_time(std::vector<std::string> args) {
    if (args.empty()) {
        return;
    }

    std::string command;
    std::vector<std::string> command_args;

    if (args.size() > 1 && args[0] == "-c") {
        command = "/bin/sh";
        std::string combined_command;
        for (size_t i = 1; i < args.size(); ++i) {
            combined_command += args[i] + (i < args.size() - 1 ? " " : "");
        }
        command_args = {"/bin/sh", "-c", combined_command};
    } else {
        command = args[0];
        command_args.push_back(args[0]);
        for (size_t i = 1; i < args.size(); ++i) {
            command_args.push_back(args[i]);
        }
    }

    std::vector<std::string> time_args = {"/usr/bin/time"};
    time_args.insert(time_args.end(), command_args.begin(), command_args.end());

    std::vector<char*> c_args;
    for (const auto& arg : time_args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        if (execv(c_args[0], c_args.data()) == -1) {
            perror("execv failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        close(pipefd[1]);
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            std::cout << "Exit status: " << exit_status << std::endl;
        }
        if (WIFSIGNALED(status)) {
            int signal_number = WTERMSIG(status);
            std::cout << "Terminated by signal: " << signal_number << std::endl;
        }

        std::string time_output;
        char buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            time_output += buffer;
        }
        close(pipefd[0]);

        std::regex user_regex(R"([\s]+([\d.]+)[\s]+user)", std::regex::icase);
        std::regex sys_regex(R"([\s]+([\d.]+)[\s]+sys)", std::regex::icase);
        std::regex real_regex(R"([\s]+([\d.:]+)[\s]+real)", std::regex::icase);

        std::smatch match;

        double real_time = 0.0;
        double user_time = 0.0;
        double sys_time = 0.0;

        if (std::regex_search(time_output, match, real_regex) && match.size() > 1) {
            std::string real_time_str = match[1];
            std::replace(real_time_str.begin(), real_time_str.end(), ':', '.');
            try {
                real_time = std::stod(real_time_str);
            } catch (const std::invalid_argument& e) {
            } catch (const std::out_of_range& e) {
            }
        }

        if (std::regex_search(time_output, match, user_regex) && match.size() > 1) {
            try {
                user_time = std::stod(match[1]);
            } catch (const std::invalid_argument& e) {
            } catch (const std::out_of_range& e) {
            }
        }
        if (std::regex_search(time_output, match, sys_regex) && match.size() > 1) {
            try {
                sys_time = std::stod(match[1]);
            } catch (const std::invalid_argument& e) {
            } catch (const std::out_of_range& e) {
            }
        }

        if (real_time > 0.0) {
            std::cout << "Real time: " << std::fixed << std::setprecision(2)
                      << real_time << " seconds" << std::endl;
            std::cout << "User time: " << std::fixed << std::setprecision(2)
                      << user_time << " seconds (" << std::fixed
                      << std::setprecision(2) << (user_time / real_time * 100) << "%)"
                      << std::endl;
            std::cout << "System time: " << std::fixed << std::setprecision(2)
                      << sys_time << " seconds (" << std::fixed
                      << std::setprecision(2) << (sys_time / real_time * 100) << "%)"
                      << std::endl;
        }
    } else {
        perror("fork failed");
    }
}

int main(int argc, char* argv[]) {
    std::string command_line;

    if (argc > 1) {
        std::vector<std::string> args(argv + 1, argv + argc);
        execute_command_with_time(args);
    } else {
        while (true) {
            std::cout << "$ ";
            std::getline(std::cin, command_line);
            if (command_line == "exit") {
                break;
            }
            std::vector<std::string> args = split_args(command_line);
            execute_command_with_time(args);
        }
    }

    return 0;
}