#include "v1_loader_contract.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {
class MockBackend final : public common_fps::loader::Backend {
public:
    std::vector<std::string> calls;
    bool scene_ok = true;
    bool begin_ok = true;
    bool image_ok = true;
    bool args_ok = true;
    bool exec_ok = true;
    bool end_ok = true;

    bool wait_for_shellui_scene() override {
        calls.emplace_back("scene");
        return scene_ok;
    }
    bool begin_loader_session() override {
        calls.emplace_back("begin");
        return begin_ok;
    }
    std::optional<common_fps::loader::LoadedImage>
    load_renderer_image(const std::uint8_t*, std::size_t) override {
        calls.emplace_back("image");
        if (!image_ok)
            return std::nullopt;
        return common_fps::loader::LoadedImage{0x1234, 0x1000};
    }
    std::uintptr_t prepare_payload_args() override {
        calls.emplace_back("args");
        return args_ok ? 0x5678 : 0;
    }
    bool prepare_exec(std::uintptr_t entry,
                      std::uintptr_t args) override {
        calls.emplace_back("exec");
        assert(entry == 0x1234);
        assert(args == 0x5678);
        return exec_ok;
    }
    bool end_loader_session() override {
        calls.emplace_back("end");
        return end_ok;
    }
};
}

int main() {
    using common_fps::loader::install_renderer_v1_compatible;
    const std::uint8_t dummy[] = {0x7f, 'E', 'L', 'F'};

    MockBackend good;
    assert(install_renderer_v1_compatible(good, dummy, sizeof(dummy)));
    const std::vector<std::string> expected = {
        "scene", "begin", "image", "args", "exec", "end"
    };
    assert(good.calls == expected);

    // Payload args are created only after the image-load phase.
    assert(good.calls[2] == "image");
    assert(good.calls[3] == "args");

    // Failures after begin still close the same loader session.
    MockBackend image_fail;
    image_fail.image_ok = false;
    assert(!install_renderer_v1_compatible(
        image_fail, dummy, sizeof(dummy)));
    assert((image_fail.calls ==
            std::vector<std::string>{"scene", "begin", "image", "end"}));

    MockBackend args_fail;
    args_fail.args_ok = false;
    assert(!install_renderer_v1_compatible(
        args_fail, dummy, sizeof(dummy)));
    assert((args_fail.calls == std::vector<std::string>{
        "scene", "begin", "image", "args", "end"
    }));

    return 0;
}
