# Common FPS for PS5 v1.2.4 — FW 9.x hardware test

**Status: test candidate, NOT stable.** The v1.2.3 release remains
unchanged until a successful PS5 runtime check.

## Root cause

The supplied CommonFPS.log reports `sdk=0x09050005`, and klog reports
`fw_version: 0x9050005`. The sampler reached `first_fps=60` using
`dce-fixed`, but the ShellUI probe selected `ptrace-io` and failed
with `read_rc=14` (EFAULT), `read_fallback_rc=-1`. The renderer
subsequently timed out and entered quarantine.

## Changes

- Route only families `0x09000000` and the observed `0x09050000` to the
  FW 9.00 MDBG probe and stopped one-byte guard backend.
- Preserve byte preimage verification, temporary protection changes,
  verification after writing and fail-closed behavior on failure.
- Keep existing 9.60/10.xx/11.60 routing unchanged.
- Move the controller and renderer session logs to
  `/data/CommonFPS/CommonFPS.log` and
  `/data/CommonFPS/CommonFPS_shellui.log`. Create the directory first.
- Preserve settings at `/data/CommonFPS/config.ini`, log bound (64 KiB),
  saved colors, Media shortcut, FPS filtering and sampling code.
- Add unit tests for FW9.05 MDBG choice, primary read and log paths.

## Console test

1. Reboot PS5 to unload the resident previous renderer.
2. Start just **one** v1.2.4 candidate (ELF or plugin).
3. Launch a previously affected game, check FPS overlay and Media settings.
4. Collect both files from the **new** log paths.
5. The first ShellUI probe should report
   `sdk=0x09050005 backend=mdbg read_rc=0`.
6. Check the subsequent one-byte guard and renderer runtime-ready records.
   A probe PASS alone does not count as success.
7. Test game switching, sleep/resume and stability separately.

Host/structural BUILD PASS is not PS5 runtime PASS. Do not publish stable
until hardware validation.
