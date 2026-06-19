# MedFlow

> 🏁 **Проект завершён и больше не развивается.**
> Учебная работа, сдана в июне 2026. Репозиторий оставлен как есть, в режиме
> архива: новых фич, исправлений не будет. Этот README — финальная
> карта верхнего уровня; подробности по каждому слою лежат в `README.md`
> соответствующих папок.

Цифровизация работы поликлиник: запись пациентов, расписания врачей и история
приёмов — небольшой бэкенд на C++ со статичной веб-мордой.

`C++20` · `SQLite` · `cpp-httplib` · `nlohmann/json` · `React + JSX (без билда)` · `Docker`

---

## О проекте

**MedFlow** — бэкенд + веб-интерфейс для управления приёмами в сети поликлиник.
Учебный, но укомплектованный: реалистичный домен (клиники, кабинеты, специальности,
врачи, пациенты, расписания, приёмы), три дашборда под разные роли (администратор
клиники / врач / пациент), полный CRUD через REST и работающий цикл резервного
копирования.

Архитектура слоистая и намеренно простая:

```
   web/  (статика)          ← браузер
     │ fetch /api/...
   api/  HTTP-слой           ← REST на cpp-httplib, CORS, ошибки → 4xx/5xx
     │
   services/  бизнес-логика  ← валидация, доменные инварианты, каскады
     │ shared_ptr<IXxx>
   interface/  репозитории   ← Abstract → InMemory / sqlite
     │
   SQLite (WAL)
```

Сервисы зависят только от абстрактных репозиториев, а конкретная реализация
(`Sqlite*` или in-memory) подставляется в точке сборки — см.
[`interface/`](interface/README.md).

---

## Стек

| Слой | Что используется |
|---|---|
| Язык | **C++20** (`std::chrono`, `<filesystem>`, …) |
| HTTP | [cpp-httplib](https://github.com/yhirose/cpp-httplib) — header-only сервер/клиент |
| JSON | [nlohmann/json](https://github.com/nlohmann/json) — `to_json/from_json` на моделях |
| БД | SQLite (амальгамация в исходниках, WAL, чекпойнт на закрытие) |
| Фронт | HTML + React + Babel-standalone с CDN, без npm и build-шага |
| Контейнер | Multi-stage Docker (`gcc:13` → `debian:bookworm-slim`, статическая линковка) |
| Дев-сборка | MSBuild через `.vcxproj` на Windows; `g++ -std=c++20` в Docker |

Зависимости лежат в [`requirements/`](requirements/README.md) исходниками — ни npm,
ни Conan, ни vcpkg. Цель — `git clone && build` без доступа к интернету.

---

## Структура проекта

Каждая папка документирована отдельно — здесь только карта и ссылки.

| Папка / файл | Что внутри | Подробности |
|---|---|---|
| `api/` | HTTP-сервер, эндпоинты `/api/*`, раздача статики + **полный справочник REST API** | [`api/README.md`](api/README.md) |
| `services/` | Бизнес-логика: валидация, инварианты, каскадные удаления, бэкапы | [`services/README.md`](services/README.md) |
| `interface/` | Репозитории: абстракции + реализации | [`interface/README.md`](interface/README.md) |
| ├─ `Abstract/` | Чистые интерфейсы (контракты для сервисов) | [`interface/Abstract/README.md`](interface/Abstract/README.md) |
| ├─ `InMemory/` | Реализация на `unordered_map` (тесты/пробы) | [`interface/InMemory/README.md`](interface/InMemory/README.md) |
| └─ `sqlite/` | Боевая реализация поверх SQLite + схема | [`interface/sqlite/README.md`](interface/sqlite/README.md) |
| `models/` | Доменные структуры с JSON-сериализацией | [`models/README.md`](models/README.md) |
| `web/` | Статичный фронтенд: 3 дашборда | [`web/README.md`](web/README.md) |
| `requirements/` | Vendored-зависимости (httplib, json, sqlite3) | [`requirements/README.md`](requirements/README.md) |
| `tools/` | **Деплой/обновление** (Docker, `update.sh`/`.bat`) + генератор тестовых данных | [`tools/README.md`](tools/README.md) |
| `backups/` | Рантайм: `medflow.db` + JSON-снапшоты. Не в гите. | — |
| `MedFlow_BY_LLl-23.cpp` | Точка входа: собирает граф зависимостей, ловит сигналы | — |
| `Dockerfile` | Multi-stage сборка образа | — |

---

## Домен

Семь сущностей, связанных через `int`-овые ссылки (целостность проверяется в
[`services/`](services/README.md), а не в БД):

```
Clinic ─┬─ Room ─────┐
        │            │
        └─ Patient ──┼─ Appointment ─ Doctor ─┬─ Specialization
                     │                        │
                     └─ Schedule ─────────────┘
```

- **Clinic** — клиника (название, адрес, телефон).
- **Room** — кабинет, привязан к клинике.
- **Specialization** — специальность врача (терапевт, кардиолог, …).
- **Doctor** — врач со специальностью.
- **Patient** — пациент, прикреплён к клинике, с номером полиса.
- **Schedule** — рабочий слот врача в кабинете по дню недели.
- **Appointment** — конкретный приём: пациент ↔ врач, дата+время, статус, заметки.

Доменные правила (слоты расписания, запрет пересечений приёмов, каскадные удаления)
живут в [`services/`](services/README.md); описание полей и JSON-форма — в
[`models/README.md`](models/README.md).

---

## Запуск

Самый короткий путь — Docker:

```bash
docker build -t medflow .
docker run -d -p 8420:8420 -v "$PWD/backups:/app/backups" medflow
```

UI и API поднимутся на `http://localhost:8420`. Сборка под Windows/Visual Studio,
деплой на сервер и обновление — в [`tools/README.md`](tools/README.md).

---

## Лицензии

Код самого проекта — учебный, без явной лицензии.

Vendored-зависимости (см. [`requirements/`](requirements/README.md)):

- **cpp-httplib** — MIT
- **nlohmann/json** — MIT
- **SQLite** — Public Domain
