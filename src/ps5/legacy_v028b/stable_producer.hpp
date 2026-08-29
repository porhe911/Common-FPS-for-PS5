#pragma once

namespace common_fps::legacy_v028b {

// Reconstructed producer half of probe_main_entry(). Renderer injection is
// deliberately outside this function and remains a separate parity milestone.
[[noreturn]] void run_stable_producer_loop() noexcept;

} // namespace common_fps::legacy_v028b
