#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/resource.h>
#include <chrono>
#include <dlfcn.h>
#include <limits.h>

using namespace std;


vector<string> parseCommand(const string &input) {
    istringstream iss(input);
    vector<string> tokens;
    string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}


void printExecutionTime(chrono::high_resolution_clock::time_point start,
                        chrono::high_resolution_clock::time_point end,
                        const struct rusage &usage) {
    auto realTime = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    auto userTime = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000;
    auto sysTime = usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;

    cout << "Время: real = " << realTime << "ms, user = " << userTime << "ms, sys = " << sysTime << "ms" << endl;
}


int executeCommand(vector<string> args, bool measureTime) {
    if (args.empty()) return -1;

    auto start = chrono::high_resolution_clock::now();
    pid_t pid = fork();

    if (pid < 0) {
        perror("Ошибка: Не удалось создать процесс");
        return -1;
    } else if (pid == 0) {
        vector<char *> c_args;


        for (size_t i = 0; i < args.size(); ++i) {
            c_args.push_back(const_cast<char *>(args[i].c_str()));
        }
        c_args.push_back(nullptr);

        char absolute_path[PATH_MAX];
        if (realpath(c_args[0], absolute_path) == NULL) {
            perror("realpath failed");
            exit(1);
        }
        c_args[0] = absolute_path;

        execv(c_args[0], c_args.data());

        perror("Ошибка: Не удалось выполнить команду");
        exit(EXIT_FAILURE);
    } else {
        int status;
        struct rusage usage;

        if (wait4(pid, &status, 0, &usage) == -1) {
            perror("Ошибка: Не удалось дождаться завершения дочернего процесса");
            return -1;
        }

        auto end = chrono::high_resolution_clock::now();

        if (measureTime) {
            printExecutionTime(start, end, usage);
        }

        return WEXITSTATUS(status);
    }
}

void runShellLoop() {
    while (true) {
        std::cout << "\U0001F408 ";
        string command;
        if (!getline(cin, command)) {
            cout << endl;
            break;
        }

        vector<string> args = parseCommand(command);
        if (args.empty()) continue;

        if (args[0] == "exit") break;

        bool measureTime = false;
        if (args[0] == "time" && args.size() > 1) {
            measureTime = true;
            args.erase(args.begin());
        }

        if (args[0] == "cd") {
            if (args.size() < 2) {
                cerr << "Использование: cd <каталог>" << endl;
            } else if (chdir(args[1].c_str()) != 0) {
                perror("Ошибка: Не удалось сменить каталог");
            }
        } else {
            int status = executeCommand(args, measureTime);
            if (status != 0) {
                cerr << "Команда завершилась со статусом: " << status << endl;
            }
        }
    }
}

int main() {
    runShellLoop();
    return 0;
}