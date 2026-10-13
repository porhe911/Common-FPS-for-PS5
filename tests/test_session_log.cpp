/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "common_fps/session_log.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>

using namespace common_fps;

int main() {
    char directory[] = "/tmp/commonfps-log-XXXXXX";
    assert(mkdtemp(directory));
    const std::string path = std::string(directory) + "/session.log";
    reset_session_log(path.c_str());
    log_message(path.c_str(), "activation=%d", 1);

    // Concurrent processes share the same bound and cannot race past it.
    const pid_t child = fork();
    assert(child >= 0);
    const std::string payload(2048, child == 0 ? 'C' : 'P');
    for (unsigned i = 0; i < 500; ++i)
        write_session_log(path.c_str(), payload.data(), payload.size());
    if (child == 0)
        _exit(0);
    int child_status = -1;
    assert(waitpid(child, &child_status, 0) == child);
    assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
    struct stat status{};
    assert(stat(path.c_str(), &status) == 0);
    assert(status.st_size == static_cast<off_t>(kSessionLogLimit));
    std::ifstream first(path);
    std::string line;
    assert(std::getline(first, line) && line == "activation=1");
    first.close();

    // The next activation destroys old contents and begins a fresh small log.
    write_session_log(path.c_str(), "", 0, true, true);
    log_durable_message(path.c_str(), "activation=%d", 2);
    assert(stat(path.c_str(), &status) == 0 && status.st_size == 13);
    std::ifstream second(path);
    assert(std::getline(second, line) && line == "activation=2");
    assert(!std::getline(second, line));
    second.close();
    assert(unlink(path.c_str()) == 0);
    assert(rmdir(directory) == 0);
    std::cout << "Session reset, concurrent writes and 64 KiB hard bound: PASS\n";
}
