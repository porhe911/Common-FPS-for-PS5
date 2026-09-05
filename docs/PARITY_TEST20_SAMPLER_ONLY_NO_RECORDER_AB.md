# PARITY TEST20: sampler-only, no-recorder A/B

## Verified artifacts

```text
b791baf6f85063d0c7ec57eebcbf60116dff58dfc4ec74aa6d9dedcc1ecc32ef  Common_FPS_PS5_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.elf
7bdcc16cc582bd89c7f1680471436ef58fdc53bab159c7e36278a5e8f1c8fa49  Common_FPS_PS5_etaHEN_v1.1.0_PARITY_TEST20_SAMPLER_ONLY_NO_RECORDER_AB.plugin
```

Plugin metadata is `CFPS00039 / 1.28`. Its 29-byte etaHEN header is followed
by the controller ELF byte-for-byte.

## Runtime contract

```text
main
  -> fork()
  -> parent returns immediately
  -> child observes ShellUI and samples the active game once per second
```

The sampling chain is:

```text
KERN_PROC/sysctl_tdname -> eboot.bin PID
  -> temporary self debugger Auth ID
  -> enumerate libSceVideoOut.sprx
  -> restore the original Auth ID
  -> read-only FW 9.60 DMAP page walk
  -> module base + 0x34980
  -> 7 records x 0x18 bytes
  -> first enabled record pointer -> uint64 root
  -> root + 0x768 -> uint32 frame/vsync counter
  -> elapsed-time delta -> rounded integer FPS
```

Renderer, ShellUI injection, overlay IPC, custom signal handlers and shutdown
recording are absent. Therefore no on-screen FPS counter is expected.

## Hardware result

Tested on PS5 FW 9.60 with etaHEN 2.6. The submitted log was:

```text
Common FPS v1.1.0 PARITY TEST20 sampler-only no-recorder A/B
Mode=fork-only parent_return=immediate resident_child=active shellui_observation=sysctl_tdname_1s renderer=disabled sampler=videoout_fw960_1s dmap=read_only shutdown_trace=disabled shutdown_writes=disabled signal_handlers=default stop_path=disabled
Child ready pid=112 ppid=111 first_shellui_pid=58 size_rc=0 data_rc=0 errno=0 bytes=88776 records=24 malformed=0 log_fd=closing periodic_log=disabled platform_log=compile_time_disabled
Sampler online pid=115 counter=0x800521248 first_fps=60 event_log=once_per_game_pid log_fd=closing
Sampler online pid=148 counter=0x80053d248 first_fps=59 event_log=once_per_game_pid log_fd=closing
```

Both games were found and the sampler followed the PID change. The user then
performed a normal system-menu restart. It completed automatically without a
physical-button recovery or improper-shutdown warning.

This is one successful hardware run, not proof that every future session will
restart correctly.

## Reproducibility proof

The TEST20 source was reconstructed from the retained source boundary and the
unstripped archived ELF. A clean Release build with PS5 Payload SDK v0.41 and
the pinned etaHEN 2.4B source produced the exact ELF hash above. Wrapping that
ELF with `CFPS00039 / 1.28` produced the exact plugin hash above. Both rebuilt
files compared byte-for-byte equal to the submitted hardware-tested files.

`tools/verify_test20_repro.py` enforces the two hashes, plugin metadata, plugin
body identity and TEST20/renderer-disabled markers during every source build.

## Release status

TEST20 is suitable as an open, reproducible diagnostic baseline. It must not
be presented as the finished visual FPS counter or promoted to stable v1.1.0.
The visible renderer added in TEST21 restored live on-screen FPS but also
restored the restart regression. Later TEST29 failed restart without an
on-screen renderer, so the overall root cause remains unresolved.
