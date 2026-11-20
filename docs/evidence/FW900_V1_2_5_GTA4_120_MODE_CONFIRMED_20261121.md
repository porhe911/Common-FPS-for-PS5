# Common FPS v1.2.5 — user hardware confirmation (2026-11-21)

- Product: Common FPS for PS5 v1.2.5, DCE-only test build built by GitHub Actions from commit `849d4dff6e5736374e69ef5a4311355af2b1e0e0`.
- Console: user explicitly stated **PS5 firmware 9.0**.
- Previously supplied plugin log reported firmware SDK `0x09050005`.
  Do not silently equate this SDK number with the user's firmware label.
- TV: **60 Hz** (user statement); no TV supporting 120 Hz or additional
  console firmware available.
- Game: GTA IV PS5 port, option selected in game **120 FPS**.
- Prior observation: with stable Common FPS v1.2.4, game process
  `PPSA94004` crashed with SIGSEGV; diagnostic trace identified game
  worker `Kernel Host Tasks`.
- A/B baseline: after full reboot, GTA IV at 120 FPS did not crash;
  previous testing instructions called for no Common FPS enabled.
- New observation: with v1.2.5, user said **«Отлично, теперь всё
  работает шикарно»**, confirming the previously affected workflow
  operates and the crash did not recur.
- Explicit release authorization: **«у меня нет тв на 120 Гц и других
  прошивок, релизим пока так, проверял я на 9.0 прошивке»**.

## QA interpretation

- **PASS:** reported 9.0 PS5, GTA IV 120 FPS *game setting*, Common FPS
  v1.2.5 present, no reproduced crash.
- **PASS (offline):** source build + verifier + tests on DCE-only branch.
- **NOT TESTED:** output above 60 Hz, actual measured FPS of 120,
  240 Hz/uncapped game renderer FPS, other firmwares, rest mode, other
  games with this exact build.
- **NOT PROVEN:** exact mechanism of v1.2.4 crash; full firmware-wide
  runtime compatibility.
- **Source change scope:** compiled no-game-process-memory-sampling mode
  is firmware-wide. This is a code property, not equivalent to a
  multi-firmware hardware validation.
