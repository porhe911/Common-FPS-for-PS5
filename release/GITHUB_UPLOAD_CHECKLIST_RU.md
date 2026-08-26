# Что загружать на GitHub

Загружай **всё содержимое этой папки** в корень:

```text
Common-FPS-for-PS5
```

Обязательно должны попасть:

```text
.github/
include/
src/
integration/
tests/
tools/
docs/
release/

LICENSE
README.md
BUILDING.md
CONTRIBUTING.md
THIRD_PARTY_NOTICES.md
DEPENDENCIES.lock.json
CMakeLists.txt
Makefile
config.ini.example
```

## После загрузки

1. Открой вкладку `Actions`.
2. Убедись, что `Host Source Tests` зелёный.
3. Открой `PS5 Source Build`.
4. Нажми `Run workflow`.
5. Если workflow зелёный — скачай artifact.
6. Только эти файлы тестируем на PS5.
7. Пока тест не пройден, название версии — `v1.1.0-rc1`, не `v1.1.0 stable`.

## Что не загружать

Не загружай старые рабочие каталоги:

```text
alpha1
alpha2
alpha3
alpha4
alpha5
alpha6
PHU Games Tools
старые тестовые ELF/plugin
```

Исторический `v1.0.0` остаётся отдельным GitHub Release.
