# FW 11.60: RC4 code probe succeeds, guard write fails

The user still observes no FPS overlay. Uploaded `CommonFPS(1).log`:
5,591 bytes, 42 lines, SHA-256
`9e200ffe86fd6276cec4f82cf15b90ad23b7cf861f46e8ef3db24d92a1ec2d0c`.
The header identifies v1.2.3 RC4 and SDK `0x11600005`.

The controller finds games at PID 100 and later PID 106, opens DCE and
injects into ShellUI PID 59. Bootstrap succeeds, all 67 imports resolve.
The probe at `0x82ba53550` now succeeds through checked MDBG:
`read_rc=0 read_primary_rc=14 read_fallback_rc=0 verified=1`.
The subsequent main-thread guard request reads and matches expected bytes,
but PT_IO writing fails: `status=-6 write_rc=14 verified=0 restored=1`.
Detach and authentication restoration succeed. The log ends at this request;
it contains no renderer-ready marker or final sampling-rate observation.

Thus RC4 resolves the recorded code-read blocker; it does not resolve code
writing. An RX/non-writable JIT mapping is a hypothesis, not a measured
protection value in this log. RC5 reads and records that value before any
protection change, enables writing only for the existing one-byte guard,
requires a complete successful MDBG reply, and verifies exact restoration.
Full expected-code and post-write/rollback checks are retained.

The pinned [SDK v0.41 `crt/kernel.c`](https://github.com/ps5-payload-dev/sdk/blob/v0.41/crt/kernel.c)
contains FW 11.60 offsets and VM protection query/set helpers. Its
`kernel_mprotect` updates containing VM entries directly; requesting a short
range does not isolate a hardware page. RC5 preserves the exact initial
protection byte, including extra bits, and retains it when recovering after
a restore failure. [SDK MDBG ABI](https://github.com/ps5-payload-dev/sdk/blob/v0.41/crt/mdbg.c)
is used for checked one-byte writes. The independent
[overlay reference](https://github.com/smoxa/ps5-new-overlay/blob/main/src/shellui/shellui_main.cpp)
also uses a writable protection window for this managed guard, but is not
evidence of Common FPS hardware success on FW 11.60.

Host tests cover production MDBG I/O with reply/credential errors and the
production protection transaction with setup, readback, write, restore and
rollback-query failures. They verify exact protection recovery and no writes
outside the guard byte. Hardware overlay operation remains unconfirmed.
