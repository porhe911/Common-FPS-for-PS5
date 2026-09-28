# Common FPS for PS5 — полная документация

**Публичная версия:** v1.2.0 Universal  
**Техническая ветка:** Universal Stage 8.9 DCE Standby  
**Автор проекта:** porhe911  
**Репозиторий:** porhe911/Common-FPS-for-PS5  
**Лицензия:** GPL-3.0-or-later  
**Дата аппаратной приёмки Stage 8.9:** 28.09.2026

---

## 1. Назначение

Common FPS for PS5 — открытый homebrew-счётчик реальной частоты кадров для
модифицированных PlayStation 5. Он отображает целочисленный FPS поверх
запущенной PS4- или PS5-игры.

Текущий публичный универсальный путь построен вокруг двух независимых способов
получения FPS:

1. per-game VideoOut counter — предпочтительный источник;
2. DCE adaptive fallback — аппаратный fallback через `/dev/dce`, который не
   требует hardcoded offsets конкретной игры.

ShellUI renderer отделён от sampler/controller и запускается только после
обнаружения стабильного игрового процесса.

---

## 2. Что подтверждено на реальном железе

### FW 4.51 — подтверждено

На FW 4.51 Stage 8.9 прошёл аппаратную приёмку:

- PS5-игры показывают реальный FPS;
- PS4-игры показывают реальный FPS;
- подтверждены значения 30 FPS и 60 FPS;
- переход в Rest Mode и последующее пробуждение не ломают счётчик;
- после пробуждения FPS восстанавливается;
- штатная перезагрузка консоли проходит нормально;
- после смены ShellUI PID renderer повторно поднимается;
- переключение игровых процессов обрабатывается;
- adaptive DCE fallback работает;
- старый опасный ранний ShellUI/Application.Update путь на 4.51 не используется.

### FW 9.60 — подтверждённая baseline-платформа

FW 9.60 остаётся аппаратно подтверждённой baseline-платформой предыдущих
стабильных Common FPS сборок:

- реальный FPS;
- запуск PS4/PS5 игр;
- переключение игр;
- etaHEN plugin/autoload;
- восстановление после Rest Mode;
- нормальный системный lifecycle.

Для 9.60 универсальная архитектура сохраняет отдельный hardware-proven
Application.Update chain path вместо принудительного перевода на
экспериментальный high-FW guard.

### FW 7.60

Требуется новый regression test именно текущего Stage 8.9. Старые тестовые
ветки на 7.60 имели ошибки, поэтому 7.60 нельзя считать подтверждённой
только по наличию backend в коде.

### FW 10.xx

В Stage 8.9 присутствует экспериментальный high-firmware backend на базе
stopped ptrace-I/O one-byte guard и adaptive DCE. До аппаратного теста
конкретной версии 10.xx это следует считать экспериментальной поддержкой,
а не подтверждённой совместимостью.

### Другие версии ПО

Наличие firmware backend в исходниках не означает аппаратную сертификацию
каждой промежуточной версии. В документации всегда разделяются:

- **hardware confirmed** — проверено на реальной консоли;
- **implemented / experimental** — путь реализован в коде, но требует
  отдельного теста.

---

## 3. Отображение

По умолчанию overlay имеет следующие параметры:

- позиция: левый нижний угол;
- размер шрифта: 26;
- отступы: 10 × 10 logical pixels;
- текст `FPS:`: фиолетовый;
- числовое значение: белое;
- только целые числа;
- во время ожидания источника FPS отображается `FPS: Loading`.

Текущий Stage 8.9 использует compile-time/default `OverlayConfig`.
Парсер конфигурации присутствует в исходниках, но runtime-загрузка пользовательского
INI в публичной Stage 8.9 ветке не подключена. Поэтому изменение позиции,
шрифта и отступов пользователем без пересборки пока не заявляется как
поддерживаемая функция.

---

## 4. Публичные файлы

Основные артефакты Stage 8.9:

```text
Common_FPS_PS5_UNIVERSAL_STAGE8_9_DCE_STANDBY.elf
Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_9_DCE_STANDBY.plugin
Common_FPS_ShellUI_UNIVERSAL_STAGE8_9_DCE_STANDBY.elf
SHA256SUMS.txt
```

Пользователю нужны только один из двух вариантов:

- `.elf` — ручной/standalone запуск через совместимый payload launcher;
- `.plugin` — постоянный запуск через etaHEN Plugin system.

`Common_FPS_ShellUI_....elf` — внутренний renderer, уже встроенный в
controller. Его **не нужно и нельзя запускать как отдельный пользовательский
payload**.

### Важно

**Не запускайте ELF и plugin одновременно.**

Это два способа запуска одного и того же controller. Одновременный запуск
создаёт ненужную конкуренцию за один renderer/lifecycle.

---

## 5. Установка и запуск

### Вариант A — standalone ELF

1. Запустите jailbreak/эксплойт и etaHEN обычным для вашей системы способом.
2. Передайте
   `Common_FPS_PS5_UNIVERSAL_STAGE8_9_DCE_STANDBY.elf`
   через Payload Manager, Netcat или другой поддерживаемый вашей средой
   ELF launcher.
3. Оставьте controller запущенным.
4. Запустите PS4- или PS5-игру.
5. После game/ShellUI stability gate overlay появится автоматически.
6. Если источник FPS ещё определяется, временно будет `FPS: Loading`.

### Вариант B — etaHEN plugin

1. Используйте
   `Common_FPS_PS5_etaHEN_UNIVERSAL_STAGE8_9_DCE_STANDBY.plugin`.
2. Установите его через plugin-механизм etaHEN.
3. Если ваша сборка etaHEN использует стандартный каталог плагинов, обычно
   применяется `/data/etaHEN/plugins/`; фактический путь зависит от вашей
   версии/сборки etaHEN.
4. Включите plugin/autoload в интерфейсе etaHEN.
5. После следующего запуска etaHEN controller должен стартовать автоматически.
6. Overlay инъектируется не на домашнем экране, а только когда обнаружена
   стабильная игра.

---

## 6. Архитектура

### 6.1 Controller

Controller работает в tracked process, созданном loader/etaHEN, и не создаёт
внутренний второй `fork()`.

Основной цикл:

```text
observe ShellUI
      ↓
observe game process
      ↓
3 стабильных наблюдения game PID
      ↓
FPS sampling
      ↓
10 стабильных наблюдений ShellUI PID
      ↓
ShellUI renderer injection
      ↓
UDP loopback state packets
      ↓
PUI overlay
```

### 6.2 Game gate

Renderer не внедряется на домашнем экране. Перед первым renderer injection
controller требует один и тот же игровой PID в течение трёх последовательных
опросов.

Это уменьшает риск раннего вмешательства в ShellUI до того, как контейнер
`Game` действительно существует.

### 6.3 ShellUI gate

Перед renderer injection требуется десять стабильных наблюдений одного
`SceShellUI` PID.

Если ShellUI PID меняется, например после системного lifecycle/Rest Mode,
старый renderer больше не считается online, gate начинается заново и renderer
поднимается в новом ShellUI.

---

## 7. Получение FPS

Stage 8.9 использует несколько источников с приоритетом.

### 7.1 VideoOut process sampler

Предпочтительный источник.

На hardware-proven FW 9.60 доступен fast path через известную VideoOut chain.
На других версиях может применяться read-only indirect discovery внутри
`libSceVideoOut.sprx`.

Кандидат принимается только если counter изменяется с правдоподобной
display-like частотой в нескольких независимых временных окнах.

Память игры этим discovery-путём **не изменяется**.

### 7.2 HEN shared sample

Если HEN публикует совместимый `/system_tmp/fps_sample`, Stage 8.9 может
использовать его как fallback.

### 7.3 DCE adaptive fallback

Если VideoOut counter не подтверждён, controller использует `/dev/dce`.

Порядок:

```text
open /dev/dce
      ↓
ioctl display query
      ↓
извлечение 0x60-byte response
      ↓
проверка fixed field
      ↓
adaptive scan 64-bit monotonic fields
      ↓
выбор стабильного counter
      ↓
целочисленный FPS
```

На FW 4.51 именно adaptive DCE стал рабочим универсальным источником и
подтвердил 30/60 FPS в PS4 и PS5 играх.

DCE fallback не читает и не пишет память игрового процесса.

---

## 8. DCE Standby — оптимизация Stage 8.9

Stage 8.8 уже давал правильный FPS, но после успешного DCE продолжал
периодически выполнять тяжёлый VideoOut indirect scan.

Stage 8.9 добавляет backoff:

- после первого валидного DCE FPS для текущего game PID VideoOut discovery
  переходит в standby;
- тяжёлый discovery разрешается не чаще одного раза в 60 секунд;
- DCE продолжает выдавать FPS каждую рабочую итерацию;
- после трёх подряд DCE misses standby немедленно снимается;
- при смене game PID все sampler baseline/standby states сбрасываются;
- если VideoOut позже успешно находится, он снова становится приоритетным.

В логе это отображается строками:

```text
Sampler policy ... state=dce-standby ... videoout_retry_after_ms=60000
Sampler policy ... state=dce-miss-wake ...
Sampler policy ... state=videoout-resumed ...
```

---

## 9. ShellUI renderer

Renderer создаёт два PUI label:

- статический `FPS:`;
- динамическое значение `Loading` или целый FPS.

Controller отправляет renderer только валидированное состояние через loopback
IPC. Renderer не занимается поиском игрового FPS самостоятельно.

### FW 4.51 / low-mid firmware path

Используется stopped MDBG transaction:

1. controller определяет `Diagnostics.CheckRunningOnMainThread`;
2. ShellUI останавливается;
3. читаются expected bytes;
4. изменяется только один проверенный байт на `RET`;
5. выполняется readback/verification;
6. восстанавливается auth;
7. процесс возобновляется;
8. renderer работает через background PUI path.

### FW 9.60

Сохраняется hardware-proven Application.Update chain backend.

### High-FW experimental path

Для выбранных более новых firmware семей Stage 8.9 содержит stopped ptrace-I/O
one-byte guard. При несовпадении expected bytes, ошибке attach/write/readback
или неподдержанной firmware renderer должен завершить попытку fail-closed,
а не выполнять слепую запись.

---

## 10. Rest Mode и lifecycle

Controller не предполагает, что ShellUI PID вечен.

При изменении/перезапуске ShellUI:

1. старый PID перестаёт считаться активным renderer target;
2. controller заново ждёт стабильный ShellUI;
3. выполняет новый bootstrap;
4. повторно поднимает renderer;
5. FPS sampling продолжает работать.

Аппаратная приёмка FW 4.51 от 28.09.2026 подтвердила:

- Rest Mode → пробуждение → FPS работает;
- последующая системная перезагрузка проходит штатно;
- после нового ShellUI PID renderer снова становится online;
- adaptive DCE снова выдаёт реальный FPS.

---

## 11. Логи

### Controller

```text
/data/CommonFPS_universal_stage8_9.log
```

### ShellUI renderer

```text
/data/CommonFPS_universal_stage8_9_shellui.log
```

### Важные успешные строки

```text
ShellUI bootstrap ... rc=0 ...
ShellUI hook request ... status=0 ... verified=1 ...
ShellUI renderer online ...
DCE adaptive counter selected ...
Fallback sampler online backend=dce-adaptive ...
Sampler policy ... state=dce-standby ...
Sampler online ... first_fps=...
```

### Если FPS остаётся Loading

Проверить в controller log:

1. найден ли `eboot.bin`;
2. найден ли `libSceVideoOut.sprx`;
3. прошёл ли VideoOut validation;
4. открылся ли `/dev/dce`;
5. прошёл ли DCE ioctl;
6. выбран ли adaptive counter;
7. появился ли `Fallback sampler online`;
8. поднялся ли `ShellUI renderer online`.

---

## 12. Типовые состояния и диагностика

### FPS: Loading, но игра не падает

Renderer уже жив. Проблема находится на стороне sampler/source FPS.
Нужен controller log.

### Overlay отсутствует полностью

Проверить:

- действительно ли controller/plugin запущен;
- определён ли game PID;
- стабилен ли ShellUI PID;
- дошёл ли лог до `ShellUI renderer online`.

### После Rest Mode overlay исчез

Подождать завершения нового ShellUI stability gate. Если overlay не
восстанавливается, снять оба Stage 8.9 лога после пробуждения.

### DCE ioctl failed

Одиночная ошибка при закрытии/смене игры сама по себе не означает аварии.
Stage 8.9 имеет fallback wake/recovery и сбрасывает состояние при game PID
transition.

---

## 13. Проверка после установки

Минимальный acceptance test:

1. запустить Common FPS;
2. запустить PS5 игру с известным 30/60 FPS режимом;
3. убедиться, что число реагирует на режим;
4. закрыть игру;
5. запустить PS4 игру;
6. проверить FPS;
7. оставить игру более 60 секунд;
8. убедиться, что overlay не пропадает после VideoOut periodic reprobe;
9. перевести PS5 в Rest Mode;
10. разбудить консоль и запустить/вернуться в игру;
11. убедиться, что FPS восстановился;
12. выполнить обычную перезагрузку;
13. убедиться в отсутствии improper shutdown/system software error.

---

## 14. Сборка из исходников

Рекомендуемый путь — GitHub Actions `PS5 Source Build`.

Проект фиксирует исходные зависимости и собирает:

- controller ELF;
- etaHEN plugin wrapper;
- внутренний ShellUI renderer;
- SHA256SUMS.

Для локальной сборки см. `BUILDING.md`.

Ключевой принцип проекта: публичные бинарники должны быть воспроизводимыми
из открытых исходников, а hardware validation рассматривается отдельно от
успешной компиляции.

---

## 15. Безопасностные ограничения проекта

В текущем universal path соблюдаются следующие правила:

- game memory discovery — read-only;
- DCE fallback не изменяет game memory;
- системный method write выполняется controller-side, а не renderer-side;
- перед write проверяются expected bytes;
- после write выполняется verify readback;
- auth-id восстанавливается после короткого privileged window;
- неизвестные/неподтверждённые случаи должны fail-closed;
- renderer не внедряется до появления стабильной игры;
- controller не использует внутренний fork;
- shutdown-specific writes/handlers отключены.

---

## 16. Известные ограничения

- Не каждая версия системного ПО 1.xx–10.xx аппаратно проверена.
- 7.60 требует свежего Stage 8.9 regression test.
- 10.xx требует отдельного аппаратного подтверждения текущей high-FW ветки.
- Runtime INI/customization для позиции/шрифта в Stage 8.9 не считается
  публично подключённой функцией.
- FPS отображается целым числом, без десятых.
- Один экземпляр controller должен использоваться либо как ELF, либо как
  etaHEN plugin.

---

## 17. Что считать стабильной публикацией

Для v1.2.0 Universal опубликованная база — это Stage 8.9 со следующими
свойствами:

- исходники находятся в `main`;
- Host Source Tests проходят;
- PS5 Source Build проходит;
- artifact verifier проходит;
- FW 4.51 подтверждён PS4/PS5 играми;
- Rest Mode recovery подтверждён;
- штатная перезагрузка подтверждена;
- FW 9.60 остаётся hardware-proven baseline;
- release notes явно отделяют подтверждённые firmware от experimental.

---

## 18. Лицензия и ответственность

Common FPS for PS5 распространяется по GPL-3.0-or-later.

Это homebrew для модифицированных PlayStation 5. Использование выполняется
пользователем на собственный риск. Совместимость зависит от версии системного
ПО, jailbreak/etaHEN окружения и изменений Sony/сторонних loader components.
