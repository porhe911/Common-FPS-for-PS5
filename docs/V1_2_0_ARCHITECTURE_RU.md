# Common FPS for PS5 v1.2.0 — архитектура и lifecycle

## Назначение

Common FPS v1.2.0 состоит из controller/sampler и встроенного ShellUI-компонента.
Controller отслеживает игру, получает FPS и передаёт состояние в ShellUI.
ShellUI-компонент отвечает только за отображение.

## Жизненный цикл

Основной порядок работы:

```text
поиск SceShellUI
      ↓
поиск игрового eboot.bin
      ↓
стабилизация game PID
      ↓
запуск FPS sampler
      ↓
стабилизация ShellUI PID
      ↓
загрузка ShellUI-компонента
      ↓
передача FPS через loopback IPC
      ↓
отрисовка FPS
```

Renderer не загружается на домашнем экране заранее. Это уменьшает число
опасных ранних вмешательств в ShellUI.

## Источники FPS

### VideoOut

VideoOut является предпочтительным источником. На FW 9.60 используется
аппаратно проверенный быстрый путь. На других версиях возможен read-only поиск
подходящего счётчика внутри `libSceVideoOut.sprx`.

Кандидат принимается только после временной проверки его поведения.

### DCE fallback

Если VideoOut не подтверждён, используется `/dev/dce`.

Алгоритм:

1. открыть DCE device;
2. выполнить display query;
3. получить блок счётчиков;
4. проверить известное поле;
5. при необходимости найти стабильный monotonic counter;
6. вычислить целый FPS.

DCE fallback не записывает данные в память игры.

## 60-секундный backoff

Когда DCE уже стабильно выдаёт FPS, повторный тяжёлый VideoOut discovery
откладывается на 60 секунд.

Если DCE несколько раз подряд перестаёт давать валидный sample, backoff
снимается и VideoOut recovery выполняется сразу.

При смене game PID состояние sampler сбрасывается.

## ShellUI

На FW 4.51 используется остановленный и проверяемый one-byte guard через
MDBG. Перед изменением проверяются ожидаемые байты, после изменения выполняется
readback, затем восстанавливается auth и ShellUI продолжает работу.

FW 9.60 сохраняет ранее аппаратно проверенный Application.Update chain path.

Для части более новых firmware реализован stopped ptrace-I/O path, который до
аппаратной проверки конкретной версии считается экспериментальным.

## Rest Mode

Если после сна ShellUI получает новый PID:

- старый renderer target отбрасывается;
- controller ждёт стабилизацию нового ShellUI;
- компонент загружается повторно;
- overlay восстанавливается.

FW 4.51 прошёл тест Rest Mode → resume с рабочим FPS после пробуждения.

## Диагностические файлы

```text
/data/CommonFPS_v1_2_0.log
/data/CommonFPS_v1_2_0_shellui.log
```

Успешная последовательность обычно содержит:

```text
ShellUI bootstrap ... rc=0
ShellUI hook request ... status=0 ... verified=1
ShellUI renderer online
Fallback sampler online ...
Sampler online ... first_fps=...
```

## Безопасностные принципы

- FPS sampler не пишет в память игры.
- DCE fallback не зависит от hardcoded offsets конкретной игры.
- Системные изменения выполняются только после проверки expected bytes.
- После изменения выполняется readback.
- Auth восстанавливается после короткого privileged window.
- Неизвестные случаи должны завершаться fail-closed.
- ELF и plugin не должны запускаться одновременно.
