# Common FPS for PS5 v1.2.0 — Universal Stage 8.9

Это первый публичный universal-релиз новой архитектуры Common FPS.

## Главное

- Реальный FPS в PS4 и PS5 играх.
- FW 4.51 аппаратно подтверждён.
- На FW 4.51 подтверждены 30 FPS и 60 FPS.
- Rest Mode → resume: FPS восстанавливается.
- Штатная перезагрузка после тестов проходит нормально.
- Adaptive DCE fallback через `/dev/dce`.
- VideoOut остаётся preferred sampler.
- После успешного DCE тяжёлый VideoOut discovery уходит в 60-секундный standby.
- При трёх подряд DCE misses VideoOut автоматически просыпается.
- Lifecycle recovery при смене game PID и ShellUI PID.
- Отдельные безопасные ShellUI backend для разных firmware families.
- Полностью source-built controller/plugin/renderer.
- Host tests, PS5 source build и artifact verification проходят в CI.

## Статус прошивок

| FW | Статус |
|---|---|
| 4.51 | Hardware confirmed: PS4 + PS5, Rest Mode, reboot |
| 9.60 | Hardware-confirmed baseline |
| 7.60 | Требуется свежий Stage 8.9 regression test |
| 10.xx | Experimental high-FW backend; аппаратная проверка требуется |

Наличие backend в коде не считается аппаратным подтверждением конкретной
версии firmware.

## Что скачивать

Для ручного запуска:

`Common_FPS_PS5_UNIVERSAL_STAGE8_9_DCE_STANDBY.elf`

Для etaHEN Plugin system:

`Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_9_DCE_STANDBY.plugin`

Внутренний `Common_FPS_ShellUI_...` отдельно запускать не нужно.

**Не запускайте ELF и plugin одновременно.**

## Документация

Полное русское руководство:

`docs/COMMON_FPS_FULL_DOCUMENTATION_RU.md`

Техническое описание Stage 8.9:

`docs/UNIVERSAL_STAGE8_9_DCE_STANDBY_RU.md`

Аппаратные evidence:

`docs/evidence/STAGE8_8_FW451_HARDWARE_20260928.md`

## Лицензия

GPL-3.0-or-later.
