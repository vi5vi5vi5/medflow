# requirements/

Vendored-зависимости: всё лежит исходниками прямо в репе, чтобы сборка
не требовала ни менеджера пакетов, ни сети. Просто `#include` —
и поехали.

## Что внутри

| Файл              | Что это                                | Лицензия    |
|-------------------|----------------------------------------|-------------|
| `json.hpp`        | [nlohmann/json](https://github.com/nlohmann/json) — JSON для C++ | MIT |
| `httplib.h`       | [cpp-httplib](https://github.com/yhirose/cpp-httplib) — HTTP-сервер и клиент в одном header-only | MIT |
| `sqlite3.h`       | Публичный заголовок SQLite             | Public domain |
| `sqlite3.c`       | Амальгамация SQLite (один большой .c)  | Public domain |
| `sqlite3ext.h`    | Заголовок для расширений SQLite        | Public domain |
| `shell.c`         | Код `sqlite3` CLI (в сборке **не используется**, лежит «на всякий») | Public domain |

## Особенности сборки

- `sqlite3.c` компилируется **как C** отдельным шагом — в `.vcxproj`
  для него выставлено `CompileAs="CompileAsC"`; в `Dockerfile` он
  собирается отдельным вызовом `gcc` и линкуется в финальный
  бинарник. Дефайн `-DSQLITE_THREADSAFE=1` обязателен — приложение
  ходит в БД из нескольких потоков httplib.
- `httplib.h` на Windows тянет `<winsock2.h>` и часть COM-заголовков,
  где параметры объявлены через `byte`. Если в той же translation
  unit окажется `using namespace std;`, ловим неоднозначность
  `std::byte` vs `::byte`. Поэтому везде, где `httplib.h` нужен в
  заголовке, он включается **первым**, а где можно — выносится в
  отдельный `.cpp` (см. `services/InsuranceValidator.cpp`).

## Обновление

Зависимости подняты вручную: скачать новый файл, заменить, собрать,
прогнать smoke-тесты. Никаких submodules — это сознательное упрощение
для проекта, где зависимостей по сути три.
