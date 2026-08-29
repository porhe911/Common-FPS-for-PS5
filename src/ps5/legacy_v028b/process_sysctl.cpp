#include "process_sysctl.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/sysctl.h>
#include <unistd.h>

namespace common_fps::legacy_v028b {
namespace {

constexpr std::size_t kPidOffset = 72U;
constexpr std::size_t kNameOffset = 447U;
constexpr char kGameName[] = "eboot.bin";

bool record_name_equals(
    const std::uint8_t* record,
    std::size_t record_size,
    const char* wanted) noexcept {

    if (!record || !wanted || record_size <= kNameOffset)
        return false;

    const std::size_t wanted_len = std::strlen(wanted);
    const std::size_t available = record_size - kNameOffset;
    if (available < wanted_len + 1U)
        return false;

    const auto* name = record + kNameOffset;
    return std::memcmp(name, wanted, wanted_len) == 0 &&
           name[wanted_len] == '\0';
}

} // namespace

pid_t find_game_pid_sysctl() noexcept {
    int mib[4] = {1, 14, 8, 0};

    std::size_t required = 0;
    if (sysctl(mib, 4, nullptr, &required, nullptr, 0) != 0 || required == 0)
        return -1;

    // Exact v0.28b behavior: allocate the size returned by the first sysctl,
    // then reuse that same size variable for the fill call.
    auto* buffer = static_cast<std::uint8_t*>(std::malloc(required));
    if (!buffer)
        return -1;

    std::size_t filled = required;
    if (sysctl(mib, 4, buffer, &filled, nullptr, 0) != 0 || filled > required) {
        std::free(buffer);
        return -1;
    }

    const pid_t self = getpid();
    pid_t result = -1;

    const std::uint8_t* ptr = buffer;
    const std::uint8_t* end = buffer + filled;

    while (ptr < end) {
        if (static_cast<std::size_t>(end - ptr) < sizeof(int))
            break;

        int record_size_signed = 0;
        std::memcpy(&record_size_signed, ptr, sizeof(record_size_signed));
        if (record_size_signed <= 0)
            break;

        const std::size_t record_size =
            static_cast<std::size_t>(record_size_signed);
        if (record_size > static_cast<std::size_t>(end - ptr))
            break;

        if (record_size >= kPidOffset + sizeof(pid_t) &&
            record_name_equals(ptr, record_size, kGameName)) {
            pid_t pid = -1;
            std::memcpy(&pid, ptr + kPidOffset, sizeof(pid));
            if (pid > 0 && pid != self)
                result = pid;
        }

        ptr += record_size;
    }

    std::free(buffer);
    return result;
}

} // namespace common_fps::legacy_v028b
