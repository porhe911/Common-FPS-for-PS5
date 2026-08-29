#include "common_fps/v1_stable_render_loop.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
class MockActions final : public common_fps::v1_stable::RenderActions {
public:
    bool create_result = true;
    int create_calls = 0;
    std::vector<std::string> raw_values;
    std::vector<std::string> split_values;

    bool create_fps_label() override {
        ++create_calls;
        return create_result;
    }
    void set_fps_text_raw(const char* value) override {
        raw_values.emplace_back(value ? value : "");
    }
    void set_fps_text(const char* a, const char* b) override {
        split_values.emplace_back(
            std::string(a ? a : "") + "|" + (b ? b : ""));
    }
};
}

int main() {
    using namespace common_fps::v1_stable;

    HookState state;
    RenderLoop loop;
    MockActions actions;
    state.publish_raw("FPS: loading\n");
    state.publish("FPS:", "60");

    for (int i = 0; i < 30; ++i)
        loop.update(state, actions);
    assert(actions.create_calls == 0);
    assert(!loop.label_ready());

    loop.update(state, actions);
    assert(actions.create_calls == 1);
    assert(loop.label_ready());
    assert(actions.raw_values.size() == 1);
    assert(actions.raw_values.back() == "FPS: loading\n");
    assert(actions.split_values.size() == 1);
    assert(actions.split_values.back() == "FPS:|60");

    loop.update(state, actions);
    assert(actions.raw_values.size() == 1);
    assert(actions.split_values.size() == 1);

    state.publish("FPS:", "59");
    loop.update(state, actions);
    assert(actions.split_values.size() == 2);
    assert(actions.split_values.back() == "FPS:|59");

    RenderLoop retry_loop;
    MockActions retry;
    retry.create_result = false;
    for (int i = 0; i < 30; ++i)
        retry_loop.update(state, retry);
    retry_loop.update(state, retry);
    assert(retry.create_calls == 1);
    assert(!retry_loop.label_ready());

    retry.create_result = true;
    retry_loop.update(state, retry);
    assert(retry.create_calls == 2);
    assert(retry_loop.label_ready());
    return 0;
}
