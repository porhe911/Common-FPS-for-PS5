# Common FPS Universal Stage 8.9 — DCE Standby

## Основание

Stage 8.8 подтвердил реальный FPS на FW 4.51 в PS4- и PS5-играх. В аппаратном
логе DCE adaptive fallback стабильно выдавал 30/59/60 FPS, когда динамический
VideoOut discovery не находил валидный per-game counter.

После этого оставалась одна лишняя нагрузка: даже при уже работающем DCE
предпочтительный VideoOut discovery продолжал периодически запускать тяжёлый
read-only scan по тысячам pointer matches и десяткам кандидатов.

Stage 8.9 оптимизирует только эту часть. Рабочий DCE/renderer путь Stage 8.8
не меняется.

## Политика sampler

Приоритет источников остаётся прежним:

1. `videoout-process`;
2. `hen-shared`;
3. `dce-fixed`;
4. `dce-adaptive`.

Новая логика:

- до первого валидного DCE FPS VideoOut discovery работает как раньше;
- после валидного DCE sample и неуспешного VideoOut attach текущий game PID
  переводится в `dce-standby`;
- следующий тяжёлый VideoOut discovery разрешается только через 60 секунд;
- если DCE три раза подряд перестал выдавать FPS, standby снимается немедленно
  и VideoOut discovery снова разрешается;
- при смене game PID весь standby state, baseline и старый sampler attachment
  сбрасываются;
- если VideoOut успешно оживает, он снова автоматически становится
  приоритетным источником.

Таким образом DCE остаётся дешёвым рабочим fallback, но preferred VideoOut path
не отключается навсегда.

## Новые строки журнала

Controller:

`/data/CommonFPS_universal_stage8_9.log`

Renderer:

`/data/CommonFPS_universal_stage8_9_shellui.log`

После успешного DCE fallback ожидается:

```
Sampler online pid=... backend=dce-adaptive ...
Sampler policy pid=... state=dce-standby backend=dce-adaptive videoout_retry_after_ms=60000
```

Если DCE пропадает:

```
Sampler policy pid=... state=dce-miss-wake ... videoout_retry_after_ms=0
```

Если preferred sampler восстановился:

```
Sampler policy pid=... state=videoout-resumed backend=videoout-process ...
```

## Подтверждённая аппаратная база

FW 4.51:

- PS4 game — visible real FPS;
- PS5 game — visible real FPS;
- DCE adaptive fallback confirmed;
- ShellUI stopped MDBG guard confirmed;
- 30 FPS and 60 FPS shown on screen;
- game-process changes handled.

Hardware evidence:

`docs/evidence/STAGE8_8_FW451_HARDWARE_20260928.md`

FW 9.60 остаётся отдельной ранее подтверждённой hardware baseline для
VideoOut/Application.Update chain path.

FW 7.60 и high-FW 10.xx пока требуют отдельного аппаратного regression test.

## Артефакты

- `Common_FPS_PS5_UNIVERSAL_STAGE8_9_DCE_STANDBY.elf`
- `Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_9_DCE_STANDBY.plugin`
- `Common_FPS_ShellUI_UNIVERSAL_STAGE8_9_DCE_STANDBY.elf` — внутренний renderer
- `SHA256SUMS.txt`

ELF и plugin одновременно не запускать.
