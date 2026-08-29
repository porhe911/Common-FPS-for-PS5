#include "common_fps/v1_stable_memory.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace {

class FakeDmap final : public common_fps::v1_stable::DmapReadBackend {
public:
    struct Call {
        std::uintptr_t virtual_address;
        std::uintptr_t physical_address;
        std::size_t size;
    };

    std::vector<Call> calls;
    bool fail_translate = false;
    bool fail_copy = false;
    std::size_t page_size = 0x1000;

    std::optional<common_fps::v1_stable::PageTranslation>
    translate(common_fps::ProcessId,
              std::uintptr_t virtual_address) override {
        if (fail_translate)
            return std::nullopt;

        // Keep a simple 1:1 virtual->physical mapping for the host model.
        return common_fps::v1_stable::PageTranslation{
            virtual_address,
            page_size,
        };
    }

    bool copy_physical(std::uintptr_t physical_address,
                       void* output,
                       std::size_t size) override {
        if (fail_copy)
            return false;

        auto* bytes = static_cast<std::uint8_t*>(output);
        for (std::size_t i = 0; i < size; ++i)
            bytes[i] = static_cast<std::uint8_t>((physical_address + i) & 0xff);

        calls.push_back(Call{physical_address, physical_address, size});
        return true;
    }
};

} // namespace

int main() {
    using common_fps::v1_stable::proc_read_dmap;

    // Stable prw::proc_read treats zero-length reads as success.
    {
        FakeDmap backend;
        assert(proc_read_dmap(backend, 42, 0x1234, nullptr, 0));
        assert(backend.calls.empty());
    }

    // One read fully contained in one page -> one physical copy.
    {
        FakeDmap backend;
        std::uint8_t output[16]{};
        assert(proc_read_dmap(backend, 42, 0x1200, output, sizeof(output)));
        assert(backend.calls.size() == 1);
        assert(backend.calls[0].physical_address == 0x1200);
        assert(backend.calls[0].size == sizeof(output));
        assert(output[0] == 0x00);
        assert(output[1] == 0x01);
    }

    // Crossing a 4 KiB mapping must split exactly at the page boundary.
    {
        FakeDmap backend;
        std::uint8_t output[32]{};
        assert(proc_read_dmap(backend, 42, 0x0ff8, output, sizeof(output)));
        assert(backend.calls.size() == 2);
        assert(backend.calls[0].physical_address == 0x0ff8);
        assert(backend.calls[0].size == 8);
        assert(backend.calls[1].physical_address == 0x1000);
        assert(backend.calls[1].size == 24);
    }

    // The same algorithm must honor a large-page size returned by translate().
    {
        FakeDmap backend;
        backend.page_size = 0x200000;
        std::uint8_t output[32]{};
        assert(proc_read_dmap(
            backend, 42, 0x1ffff8, output, sizeof(output)));
        assert(backend.calls.size() == 2);
        assert(backend.calls[0].size == 8);
        assert(backend.calls[1].size == 24);
    }

    {
        FakeDmap backend;
        backend.fail_translate = true;
        std::uint8_t output[4]{};
        assert(!proc_read_dmap(backend, 42, 0x1000, output, sizeof(output)));
    }

    {
        FakeDmap backend;
        backend.fail_copy = true;
        std::uint8_t output[4]{};
        assert(!proc_read_dmap(backend, 42, 0x1000, output, sizeof(output)));
    }

    return 0;
}
