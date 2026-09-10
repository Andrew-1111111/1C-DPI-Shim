# 1C DPI Shim

DPI shim на **C++** для платформы **1С:Предприятие 8.3**. Увеличивает интерфейс **только 1С** (Конфигуратор / `1cv8.exe`). Масштаб Windows и остальные программы не меняются.

[English](README.en.md)

## Зачем это нужно

На мониторе **4K** Конфигуратор 1С выглядит слишком мелко. Типичный обходной путь — поднять **масштаб Windows** (например с 150–175% до 225%). Тогда 1С становится читаемой, но вместе с ней увеличиваются Firefox, Проводник, Visual Studio и всё остальное: кнопки и шрифты становятся огромными.

Windows не умеет задать **одному** процессу DPI выше системного. Compatibility, `/DisableHighDpiAware`, правка манифеста 1С и `SetProcessDpiAwarenessContext()` эту задачу не решают: они меняют режим осведомлённости, а не коэффициент масштаба.

Эта программа на C++ подменяет DPI-запросы **внутри процесса 1С**. Windows остаётся, например, на 175%, а Конфигуратор рисуется как при 200 / 225 / 250% и выше — в том числе **больше 255%** (до 400%).

Это не системный DPI, не 1C:EDT, не платформа 8.2 и не правка файлов установки 1С.

## Возможности

- Только процессы `1cestart.exe`, `1cv8.exe`, `1cv8c.exe`, `1cv8s.exe`, `1cv8a.exe`
- Написан на **C++** (одно exe), без .NET / Python / PowerShell / NuGet
- MinHook встроен в `source\minhook`, внешних библиотек нет
- Каталог 1С не изменяется
- Сборка **Win32 / x86** (`bin\x86\`) — как у 1С 8.3
- Масштаб **50–400%** (можно выше 255%; Windows в параметрах экрана так не умеет)
- Включение и масштаб через `1c-dpi.ini`
- Диагностический лог рядом с exe

Только **x86**: разрядность должна совпадать с `1cv8.exe`, а платформа 1С 8.3 — 32-bit (`C:\Program Files (x86)\1cv8\...`).

## Требования

- Windows 10/11
- Visual Studio 2022 (v143) или новее (v145)
- Платформа **1С:Предприятие 8.3 x86** (проверено на 8.3.27.2342)

## Сборка

Откройте [`1C-DPI-Shim.sln`](1C-DPI-Shim.sln), платформа **Win32**.

Стартовый проект: `1C_DPI_Shim` (собирает `1C-DPI-Shim.exe` с встроенным shim). Сборка: Ctrl+Shift+B → `bin\x86\`.

Из командной строки:

```bat
msbuild 1C-DPI-Shim.sln /p:Configuration=Release /p:Platform=Win32
```

Результат:

```text
bin\x86\1C-DPI-Shim.exe
bin\x86\1c-dpi.ini
bin\x86\1C_DPI_Tests.exe
```

Тесты:

```bat
bin\x86\1C_DPI_Tests.exe
```

Проверяются перевод процентов в DPI, имена процессов 1С, разбор аргументов launcher и наличие `1c-dpi.ini`.

## Сборка на GitHub

Workflow [`.github/workflows/build.yml`](.github/workflows/build.yml) на `windows-2022`:

1. Собирает **Release Win32** → `bin/x86`
2. Запускает `1C_DPI_Tests.exe`
3. Упаковывает zip без тестового exe
4. Кладёт артефакт на вкладку **Actions → Artifacts**

Срабатывает на push в `main`/`master`, pull request, ручной **Run workflow** и теги `v*` (например `v1.0.0`) — тег публикует **GitHub Release** с `1C-DPI-Shim-x86-Release.zip`.

## Установка

Ничего не копируется в `C:\Program Files (x86)\1cv8`.

1. Соберите Release Win32 или скачайте zip из Actions / Release.
2. Скопируйте `1C-DPI-Shim.exe` и `1c-dpi.ini` в свою папку, например `C:\Tools\1C-DPI-Shim\`.
3. Создайте ярлык на `1C-DPI-Shim.exe` с аргументом `225`.

## Запуск

Путь к 1С задаётся в `1c-dpi.ini` рядом с launcher:

```ini
[launcher]
exe=C:\Program Files (x86)\1cv8\common\1cestart.exe
platform_exe=C:\Program Files (x86)\1cv8\8.3.27.2342\bin\1cv8.exe
```

```bat
1C-DPI-Shim.exe 225
1C-DPI-Shim.exe 225 --designer
1C-DPI-Shim.exe 225 --exe="C:\Program Files (x86)\1cv8\common\1cestart.exe"
1C-DPI-Shim.exe 200 --designer
1C-DPI-Shim.exe 250 --designer
```

Обычный ярлык `1cestart.exe` **минуя launcher** shim не загрузит.

## Настройка

```ini
[shim]
dpi=225
enabled=1
log=1
log_verbose=1
log_path=1c-dpi-shim.log
scale_system_metrics=0
scale_nonclient_metrics=0
scale_stock_fonts=1
block_per_monitor=1
```

| `dpi=` | Что видит 1С |
| --- | --- |
| 175 | 168 DPI (как Windows 175%, контроль) |
| 200 | 192 DPI |
| 225 | 216 DPI |
| 250 | 240 DPI |
| 300 | 288 DPI |
| 400 | 384 DPI |

Диапазон `dpi=` — **50–400**. Значения **выше 255%** допустимы: это не системный масштаб Windows и не ограничение типа `BYTE`.

Переменные окружения (перекрывают ini): `ONEC_DPI`, `ONEC_DPI_ENABLED`, `ONEC_DPI_INI`, `ONEC_DPI_LOG`, `ONEC_DPI_LOG_ENABLED`.

## Лог

По умолчанию: `<папка launcher>\1c-dpi-shim.log`.

```ini
log=0
```

или `1C-DPI-Shim.exe 225 --no-log`.

Успешный запуск при Windows 175% и режиме 225%:

```text
system_dpi=168 (175%) virtual_dpi=216 (225%)
spoof_active=yes
GetDeviceCaps(LOGPIXELSX) original=168 spoofed=216 spoof=yes
```

## Удаление

Закрыть 1С, не запускать launcher, удалить папку с exe/ini/логом и при желании `%LOCALAPPDATA%\1C-DPI-Shim`. Ярлыки 1С не менялись. Быстрый выключатель: `enabled=0`.

## Как это работает

```text
1C-DPI-Shim.exe --dpi=225
        │  CREATE_SUSPENDED + LoadLibrary(встроенный shim)
        ▼
 1cestart.exe          DPI не подменяется (процесс unaware)
        │  CreateProcess → инъекция в ребёнка
        ▼
 1cv8s.exe / 1cv8.exe  system-aware
        │  GetDeviceCaps / GetDpiForSystem / getContextDPI
        │  168 → 216
        ▼
 Конфигуратор рисует UI как 225%, остальные приложения — как были
```

1С с 8.3.6 сама масштабирует UI (`wbase::getContextDPI()` кэширует DPI на старте). Shim внедряется **до первого окна** и в каждый дочерний `1cv8*.exe`. Отдельный `1C_DPI_Shim.dll` в папке с программой не нужен: модуль встроен в exe и при запуске выкладывается в `%LOCALAPPDATA%\1C-DPI-Shim\`.

## Ограничения

- Только запуск через launcher
- Смена `dpi` требует перезапуска 1С
- Часть UI (WebKit, внешние компоненты, системные диалоги) может остаться на системном масштабе
- `CreateRemoteThread` могут блокировать политики / антивирус

## Лицензия

Код приложения можно использовать в своей среде.

MinHook (BSD): исходники встроены в проект (`source/minhook`), отдельной библиотеки или NuGet нет.
