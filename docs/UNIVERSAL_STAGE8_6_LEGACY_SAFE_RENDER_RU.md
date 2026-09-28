# Common FPS Universal Stage 8.6 — Legacy Safe Renderer

## Цель

Stage 8.6 устраняет опасный путь, который на старых прошивках мог приводить к
перезапуску `SceShellUI`, закрытию игры и CE-108262-9 во время подготовки
`Application.Update`.

## Архитектура

- Контроллер по-прежнему ждёт стабильный `SceShellUI` и стабильный game PID.
- До появления игры рендерер в ShellUI не внедряется.
- Для семейств ПО 3.00–8.20 используется отдельный legacy backend:
  - `Application.Update` не компилируется и не изменяется;
  - определяется `Diagnostics.CheckRunningOnMainThread`;
  - контроллер останавливает ShellUI через ptrace;
  - читает и сверяет 16 байт;
  - через MDBG меняет только первый байт на `RET`;
  - проверяет все 16 байт обратным чтением;
  - только после подтверждения background Mono thread создаёт/обновляет PUI.
- Для 9.60 legacy guard намеренно отклоняется и остаётся существующий
  `Application.Update` chain backend.
- Для неподтверждённых новых прошивок запись не расширяется автоматически:
  renderer fail-closed вместо записи по непроверенному backend.

## Почему это отличается от Stage 8.5

Stage 8.5 сделал запись `Application.Update` остановленной и проверяемой, но
всё ещё требовал компиляции/подготовки этого метода на старых прошивках.
Stage 8.6 полностью исключает этот путь для 3.00–8.20.

## Защиты

- один renderer на PID;
- game gate: три одинаковых наблюдения game PID;
- ShellUI gate: десять одинаковых наблюдений PID;
- никакого live `mprotect`/self-write системного метода из renderer;
- legacy guard: ровно один проверенный байт;
- expected-byte check перед записью;
- полный readback после записи;
- попытка rollback при verify failure;
- неизвестный firmware — fail-closed;
- injected thread не возвращается в loader.

## Журналы

- `/data/CommonFPS_universal_stage8_6.log`
- `/data/CommonFPS_universal_stage8_6_shellui.log`
- `/system_tmp/commonfps_shellui.stage`

Для legacy backend успешная последовательность содержит:

```
legacy_guard_compile
legacy_guard_probe
legacy_guard_request
legacy_guard_ack_wait
legacy_guard_ready
renderer backend selected mode=legacy-background-pui
```

Контроллер должен записать транзакцию `mode=thread-guard`,
`phase=guard`, `status=0`, `verified=1`.

## Артефакты

- `Common_FPS_PS5_UNIVERSAL_STAGE8_6_LEGACY_SAFE.elf`
- `Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_6_LEGACY_SAFE.plugin`
- `Common_FPS_ShellUI_UNIVERSAL_STAGE8_6_LEGACY_SAFE.elf` — внутренний renderer,
  отдельно не запускать.
- `SHA256SUMS.txt`

ELF и plugin одновременно не запускать.
