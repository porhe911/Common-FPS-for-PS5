# Common FPS Universal Stage 8.8 — Authenticated Adaptive DCE

## Зачем нужен Stage 8.8

Stage 8.6 подтвердил, что на FW 4.51 ShellUI renderer стабильно запускается.
Stage 8.7 добавил DCE fallback, но на 4.51 HUD остался в состоянии
`FPS: Loading`. Поэтому Stage 8.8 усиливает именно источник FPS, не меняя
уже рабочую схему ShellUI renderer.

## Источники FPS

Приоритет такой:

1. `videoout-process` — существующий per-game VideoOut sampler.
2. `hen-shared` — если HEN публикует `/system_tmp/fps_sample`.
3. `dce-fixed` — известный DCE flip counter в поле +0x08.
4. `dce-adaptive` — автоматический поиск стабильного 64-bit monotonic
   counter внутри 0x60-байтового DCE response.

Если preferred VideoOut sampler начинает выдавать FPS, он всегда имеет
приоритет над fallback.

## Что исправлено относительно Stage 8.7

### DCE permissions

`/dev/dce` сначала открывается обычным `O_RDWR`.

Если firmware не разрешает это обычным credentials, controller создаёт
короткое auth-id окно:

- сохраняет исходный auth id;
- временно устанавливает тот же service/ptrace auth id, который уже
  используется стабильным injector path;
- открывает `/dev/dce`;
- немедленно восстанавливает исходный auth id.

Если ioctl после обычного open всё равно отклоняется, выполняется один
такой же краткий auth-window retry для ioctl и auth сразу восстанавливается.

### Adaptive DCE layout

Stage 8.7 предполагал, что flip counter всегда лежит по offset `+0x08`.
Stage 8.8 всё ещё предпочитает это поле, но если оно не даёт валидную
частоту, проверяет все выровненные 64-bit поля DCE response.

Неизвестное поле принимается только после двух последовательных правдоподобных
дельт. После выбора offset используется как текущий DCE backend. При нескольких
невалидных выборках discovery запускается снова.

### HEN shared fallback

Если существует `/system_tmp/fps_sample` и sample валиден, он используется
до DCE. Это позволяет работать совместно с HEN, который уже предоставляет
собственный FPS producer.

## Безопасность

- DCE sampler не читает и не пишет память игры.
- Auth id меняется только на время open/ioctl и восстанавливается сразу.
- ShellUI renderer остаётся game-gated.
- FW 4.51 использует уже подтверждённый stopped MDBG one-byte guard.
- FW 9.60 сохраняет ранее проверенный Application.Update chain backend.
- high-FW ptrace guard остаётся экспериментальным и fail-closed.
- внутренний ShellUI ELF отдельно не запускается.

## Логи

Controller:

`/data/CommonFPS_universal_stage8_8.log`

Renderer:

`/data/CommonFPS_universal_stage8_8_shellui.log`

Полезные строки:

```
DCE device online ... open_mode=normal-rw
DCE device online ... open_mode=privileged-window
DCE query online ... w0=... w1=...
DCE adaptive counter selected offset=...
Fallback sampler online backend=dce-fixed ...
Fallback sampler online backend=dce-adaptive ...
Fallback sampler online backend=hen-shared ...
Sampler online ... backend=...
```

Если DCE не доступен, лог теперь содержит errno обычного open, errno
privileged open, ioctl errno, был ли auth retry и восстановился ли auth.

## Артефакты

- `Common_FPS_PS5_UNIVERSAL_STAGE8_8_DCE_AUTH_ADAPTIVE.elf`
- `Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_8_DCE_AUTH_ADAPTIVE.plugin`
- `Common_FPS_ShellUI_UNIVERSAL_STAGE8_8_DCE_AUTH_ADAPTIVE.elf` — внутренний renderer
- `SHA256SUMS.txt`

ELF и plugin одновременно не запускать.
