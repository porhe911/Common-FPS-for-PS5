# Common FPS Universal Stage 8.7 — DCE + High-Firmware Hybrid

## Назначение

Stage 8.7 расширяет универсальную ветку после успешного Stage 8.6 на FW 4.51.
Stage 8.6 подтвердил, что ShellUI renderer и legacy one-byte UI-thread guard
запускаются без прежнего падения игры, но FPS оставался в состоянии Loading.
Stage 8.7 добавляет второй независимый источник FPS и расширяет безопасный
renderer backend на семейства системного ПО до 10.xx.

## FPS: два независимых backend

1. `videoout-process` — существующий preferred backend:
   - находит игровой процесс;
   - находит `libSceVideoOut.sprx`;
   - выполняет read-only dynamic discovery;
   - считает FPS по игровому VideoOut counter.

2. `dce-fallback` — новый firmware-neutral fallback:
   - открывает `/dev/dce`;
   - читает display flip counter через ioctl `0x80308217`;
   - не читает и не пишет память игрового процесса;
   - используется только когда preferred process sampler ещё не дал FPS;
   - автоматически сбрасывает baseline при смене игры.

Process sampler всегда имеет приоритет, если оба backend доступны.

## ShellUI renderer

### FW 1.xx–8.20

Используется остановленный MDBG guard:
- `Diagnostics.CheckRunningOnMainThread` компилируется;
- controller читает 16 байт и сверяет expected bytes;
- меняется только первый байт на `RET`;
- выполняется полный readback;
- ShellUI возобновляется только после проверки;
- `Application.Update` не изменяется.

### FW 9.60

Сохраняется hardware-proven Stage 8.5/8.6 backend через
`Application.Update` chain. Он не заменён новым экспериментальным путём.

### FW 8.30–9.50 и 9.70–10.xx

Используется тот же one-byte main-thread guard, но controller применяет его
через ptrace I/O. Это экспериментальный high-firmware backend. При любой
ошибке expected-byte/readback/attach/write renderer завершается fail-closed и
не пытается вслепую менять другой системный метод.

### FW > 10.xx

Автоматическая запись системных методов намеренно не включена. Неизвестная
будущая версия должна сначала пройти отдельную аппаратную проверку.

## Защиты

- renderer injection только после стабильного game PID;
- десять стабильных наблюдений ShellUI PID;
- один renderer на ShellUI PID;
- no internal fork;
- no game memory writes;
- DCE fallback не зависит от offsets игры;
- one-byte guard имеет expected-byte check и полный verify readback;
- FW 9.60 сохраняет известный рабочий backend;
- неподдержанная прошивка fail-closed;
- injected thread не возвращается через loader.

## Логи

Controller:

`/data/CommonFPS_universal_stage8_7.log`

Renderer:

`/data/CommonFPS_universal_stage8_7_shellui.log`

Успешный DCE fallback:

```
DCE device online
DCE sampler online ioctl=0x80308217 first_fps=...
Sampler online ... backend=dce-fallback ...
```

Успешный high-FW renderer:

```
phase=probe ... backend=ptrace-io ... status=0
mode=thread-guard ... phase=guard ... status=0 verified=1
renderer backend selected mode=legacy-background-pui
```

## Артефакты

- `Common_FPS_PS5_UNIVERSAL_STAGE8_7_DCE_HIGHFW.elf`
- `Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_7_DCE_HIGHFW.plugin`
- `Common_FPS_ShellUI_UNIVERSAL_STAGE8_7_DCE_HIGHFW.elf` — внутренний renderer,
  отдельно не запускать.
- `SHA256SUMS.txt`

ELF и plugin одновременно не запускать.
