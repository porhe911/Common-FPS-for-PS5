#include "commonfps_shellui.hpp"
#include "HookedFuncs.hpp"

#include <unistd.h>

MonoImage* pui_img = nullptr;
MonoObject* Game = nullptr;

namespace {

bool mono_context_ready()
{
    return Root_Domain != nullptr &&
           pui_img != nullptr &&
           Game != nullptr &&
           mono_class_from_name != nullptr &&
           mono_class_get_property_from_name != nullptr &&
           mono_property_get_get_method != nullptr &&
           mono_compile_method != nullptr &&
           mono_string_new != nullptr;
}

}

int main(int, const char**)
{
    using namespace common_fps::ps5::shellui;

    if (!initialize_receiver())
        return 1;

    for (;;) {
        if (mono_context_ready())
            apply_latest_state();

        usleep(16000);
    }

    return 0;
}
