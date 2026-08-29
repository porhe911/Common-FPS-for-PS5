#include "common_fps/v1_stable_memory.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::uintptr_t idx(std::uintptr_t va, unsigned shift) {
    return (va >> shift) & 0x1ffull;
}

class FakePageTables final : public common_fps::v1_stable::PhysicalEntryReader {
public:
    std::unordered_map<std::uintptr_t, std::uint64_t> entries;

    bool read_entry(std::uintptr_t physical_address,
                    std::uint64_t& value) override {
        const auto it = entries.find(physical_address);
        if (it == entries.end())
            return false;
        value = it->second;
        return true;
    }

    void set(std::uintptr_t table, std::uintptr_t index, std::uint64_t value) {
        entries[table + index * 8] = value;
    }
};

class FakeDmap final : public common_fps::v1_stable::DmapReadBackend {
public:
    struct Call {
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

        // Keep a simple 1:1 virtual->physical mapping for the chunking model.
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

        calls.push_back(Call{physical_address, size});
        return true;
    }
};

} // namespace

int main() {
    using common_fps::v1_stable::proc_read_dmap;
    using common_fps::v1_stable::translate_x86_64;

    // Ordinary 4 KiB page walk.
    {
        constexpr std::uintptr_t va = 0x0000000123456789ull;
        constexpr std::uintptr_t cr3 = 0x1000;
        constexpr std::uintptr_t pdpt = 0x2000;
        constexpr std::uintptr_t pd = 0x3000;
        constexpr std::uintptr_t pt = 0x4000;
        constexpr std::uintptr_t frame = 0x0000000abcde0000ull;

        FakePageTables tables;
        tables.set(cr3, idx(va, 39), pdpt | 0x1);
        tables.set(pdpt, idx(va, 30), pd | 0x1);
        tables.set(pd, idx(va, 21), pt | 0x1);
        tables.set(pt, idx(va, 12), frame | 0x1);

        const auto translated = translate_x86_64(tables, cr3 | 0x123, va);
        assert(translated.has_value());
        assert(translated->page_size == 0x1000);
        assert(translated->physical == frame + (va & 0xfff));
    }

    // 2 MiB PDE large page.
    {
        constexpr std::uintptr_t va = 0x00000000456789abull;
        constexpr std::uintptr_t cr3 = 0x5000;
        constexpr std::uintptr_t pdpt = 0x6000;
        constexpr std::uintptr_t pd = 0x7000;
        constexpr std::uintptr_t frame = 0x0000000080000000ull;

        FakePageTables tables;
        tables.set(cr3, idx(va, 39), pdpt | 0x1);
        tables.set(pdpt, idx(va, 30), pd | 0x1);
        tables.set(pd, idx(va, 21), frame | 0x81); // present + PS

        const auto translated = translate_x86_64(tables, cr3, va);
        assert(translated.has_value());
        assert(translated->page_size == 0x200000);
        assert(translated->physical == frame + (va & 0x1fffff));
    }

    // 1 GiB PDPTE large page.
    {
        constexpr std::uintptr_t va = 0x0000000187654321ull;
        constexpr std::uintptr_t cr3 = 0x8000;
        constexpr std::uintptr_t pdpt = 0x9000;
        constexpr std::uintptr_t frame = 0x00000000c0000000ull;

        FakePageTables tables;
        tables.set(cr3, idx(va, 39), pdpt | 0x1);
        tables.set(pdpt, idx(va, 30), frame | 0x81); // present + PS

        const auto translated = translate_x86_64(tables, cr3, va);
        assert(translated.has_value());
        assert(translated->page_size == 0x40000000ull);
        assert(translated->physical == frame + (va & 0x3fffffffull));
    }

    // Missing/not-present entry is a clean translation failure.
    {
        constexpr std::uintptr_t va = 0x12345000;
        FakePageTables tables;
        tables.set(0x1000, idx(va, 39), 0x2000); // not present
        assert(!translate_x86_64(tables, 0x1000, va).has_value());
    }

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

    // The same chunker honors a large-page size returned by translate().
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
