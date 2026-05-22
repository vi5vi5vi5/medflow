# MedFlow

> Учебный проект цифровизации работы поликлиник: запись пациентов,
> расписания врачей, история приёмов — в одном небольшом сервисе на C++.

`C++20` · `SQLite` · `cpp-httplib` · `nlohmann/json` · `React + JSX (без билда)` · `Docker`

---

## О проекте

**MedFlow** — это бэкенд + веб-морда для управления приёмами в сети
поликлиник. Учебный, но укомплектованный: домен реалистичный (клиники,
кабинеты, специальности, врачи, пациенты, расписания, приёмы), три
дашборда под разные роли (администратор клиники / врач / пациент),
полный CRUD через REST и работающий цикл резервного копирования.

Идея архитектуры — слоистая и максимально простая:

```
       ┌─────────────────────────────┐
       │       web/  (статика)       │   ← браузер
       └──────────────┬──────────────┘
                      │ fetch /api/...
       ┌──────────────┴──────────────┐
       │   api/  HTTP-слой (httplib) │   ← REST, CORS, ошибки → 4xx/5xx
       └──────────────┬──────────────┘
                      │ вызовы методов
       ┌──────────────┴──────────────┐
       │  services/  бизнес-логика   │   ← валидация, инварианты
       └──────────────┬──────────────┘
                      │ shared_ptr<IXxx>
       ┌──────────────┴──────────────┐
       │  interface/  репозитории    │   ← Abstract → InMemory / sqlite
       └──────────────┬──────────────┘
                      │
                ┌─────┴─────┐
                │ SQLite +  │
                │  WAL      │
                └───────────┘
```

Конкретные реализации репозиториев подменяются на уровне `main` — то
есть тот же сервисный слой работает и поверх SQLite, и поверх
in-memory `unordered_map` (см. [`interface/`](interface/README.md)).

---

## Стек

| Слой         | Что используется |
|--------------|------------------|
| Язык         | **C++20** (`std::chrono`, `<format>`, `<filesystem>`) |
| HTTP         | [cpp-httplib](https://github.com/yhirose/cpp-httplib) — header-only сервер и клиент |
| JSON         | [nlohmann/json](https://github.com/nlohmann/json) — `to_json/from_json` на каждой модели |
| БД           | SQLite (амальгамация в исходниках, WAL, чекпойнт на закрытие) |
| Фронт        | Чистый HTML + React + Babel-standalone с CDN, без npm и build-шага |
| Контейнер    | Multi-stage Docker (gcc:13-bookworm → debian:bookworm-slim, бинарь линкуется statically) |
| Дев-сборка   | MSBuild через `.vcxproj` на Windows; `g++ -std=c++20` в Docker |

Зависимости лежат в [`requirements/`](requirements/README.md) исходниками
— ни npm, ни Conan, ни vcpkg. Цель — собрать `git clone && build` без
доступа к интернету.

---

## Быстрый старт

### Через Docker (рекомендуется)

```bash
# 1. Собрать образ
docker build -t medflow .

# 2. Запустить (создаёт ./backups при первом старте — там и БД, и снапшоты)
docker run -d --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v "$PWD/backups:/app/backups" \
  medflow

# 3. Проверить
curl http://localhost:8420/api/health
# → {"port":8420,"status":"ok"}

# 4. Открыть UI
xdg-open http://localhost:8420
```

На Windows PowerShell — `-v "${PWD}\backups:/app/backups"`.

Подробнее — в [`DEPLOY.md`](DEPLOY.md).

### Локально, без Docker (Windows + Visual Studio)

Открой `MedFlow_BY_LLl-23.slnx`, собери конфигурацию `Debug|x64`,
запусти. Сервер сам создаст `backups/medflow.db` и поднимется на
`http://localhost:8420`.

---

## Структура проекта

| Папка                                   | Что внутри |
|-----------------------------------------|------------|
| [`api/`](api/README.md)                 | HTTP-сервер на cpp-httplib, эндпоинты `/api/*`, раздача статики |
| [`services/`](services/README.md)       | Бизнес-логика: валидация, инварианты, каскадные удаления, бэкапы |
| [`interface/`](interface/README.md)     | Абстрактные интерфейсы репозиториев + их реализации |
| ├─ [`Abstract/`](interface/Abstract/README.md) | Чистые интерфейсы (то, на что опираются сервисы) |
| ├─ [`InMemory/`](interface/InMemory/README.md) | `unordered_map`-реализация (для тестов и проб) |
| └─ [`sqlite/`](interface/sqlite/README.md)     | Боевая реализация поверх SQLite |
| [`models/`](models/README.md)           | Доменные структуры (Clinic, Doctor, Patient, …) с JSON-сериализацией |
| [`web/`](web/README.md)                 | Статичный фронтенд: 3 дашборда (clinic / doctor / patient) |
| [`requirements/`](requirements/README.md) | Vendored-зависимости (httplib, json, sqlite3) |
| [`tools/`](tools/README.md)             | Вспомогательные скрипты (генератор тестового бэкапа) |
| `backups/`                              | Создаётся в рантайме: `medflow.db` + JSON-снапшоты. **Не в гите.** |
| `MedFlow_BY_LLl-23.cpp`                 | Точка входа: собирает граф зависимостей, ловит сигналы, делает exit-снапшот |
| [`API.md`](API.md)                      | Полный справочник REST-эндпоинтов |
| [`DEPLOY.md`](DEPLOY.md)                | Деплой на Linux-сервер (Docker, nginx, HTTPS) |
| `Dockerfile`                            | Multi-stage сборка: gcc:13 → debian:bookworm-slim |

В каждой папке лежит свой `README.md` — там детали по слою. Этот файл —
карта верхнего уровня.

---

## Домен

Семь сущностей, связанных через `int`-овые ссылки (доменная целостность
проверяется в [`services/`](services/README.md), не в БД):

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
- **Patient** — пациент, прикреплён к клинике, имеет номер полиса
  (валидируется внешним HTTP-сервисом — см. ниже).
- **Schedule** — рабочий слот врача в кабинете по дню недели
  (`time_from / time_to / slot_duration_min / break_after_min`).
- **Appointment** — конкретный приём: пациент ↔ врач, дата+время,
  статус, заметки.

Доменные правила (вынесены в сервисный слой):

- Приём попадает в один из слотов расписания врача в кабинете нужной
  клиники.
- У врача нет двух `Schedule` на один день недели.
- Нет пересечений приёмов по одному врачу и по одному пациенту.
- Удаление клиники/врача/пациента — каскадное, через сервис, а не
  через FK в БД.

Подробнее по полям — [`models/README.md`](models/README.md);
по эндпоинтам — [`API.md`](API.md).

---

## Резервное копирование

Бэкап — это **полный** снимок БД в одном JSON-файле. Любая загрузка
из бэкапа полностью чистит БД перед заливкой, поэтому снапшот всегда
является точкой восстановления (а не «дельтой»).

```
backups/
├── medflow.db                         ← живая БД (SQLite)
├── bck_2026-05-22_19-36-18.bck        ← снапшот при остановке
├── bck_2026-05-22_19-42-44.bck
└── bck_2026-05-22_19-52-04.bck
```

| Действие | Эндпоинт / момент |
|---|---|
| Снапшот при остановке | автоматически, при `SIGINT`/`SIGTERM` (т.е. `docker stop`) |
| Экспорт «прямо сейчас» | `GET  /api/backup/export` → JSON в ответе |
| Сохранение в файл      | `POST /api/backup/save` `{"path": "..."}` |
| Список бэкапов         | `GET  /api/backup/list` — HTML со скачиваемыми ссылками |
| Скачать бэкап          | `GET  /api/backup/download?file=<name>` |
| Восстановить из последнего | `POST /api/backup/load` `{}` |
| Восстановить из конкретного | `POST /api/backup/load` `{"path": "backups/bck_..."}` |
| Залить произвольный JSON  | `POST /api/backup/import` (тело — содержимое .bck) |

Залить файл бэкапа из командной строки:

```bash
curl -X POST -H "Content-Type: application/json" \
     --data-binary @bck_2026-05-22_19-36-18.bck \
     http://localhost:8420/api/backup/import
```

SQLite работает в WAL-режиме; в деструкторе `SqliteDb` вызывается
`PRAGMA wal_checkpoint(TRUNCATE)`, поэтому `medflow.db` после
остановки приложения — самодостаточный файл, без `-wal`/`-shm`.
Можно копировать одним движением.

---

## REST API в двух словах

База — `http://localhost:8420/api`. Каждая сущность даёт классический
CRUD + (где это нужно) специализированные поиски и доменные операции:

```
GET    /api/patients                    список
GET    /api/patients/{id}               по id
POST   /api/patients                    создать
POST   /api/patients/search             поиск по JSON-критериям
PATCH  /api/patients/{id}               частичное обновление
DELETE /api/patients/{id}               каскадное удаление

# те же шаблоны — для /doctors, /clinics, /rooms, /specializations,
# /appointments, /schedules плюс доменные:
GET /api/schedules/available-days?specId=...&clinicId=...
GET /api/schedules/doctors-by-day?specId=...&date=...
GET /api/schedules/time-slots?doctorId=...&date=...
GET /api/schedules/today                 что сервер считает «сегодня и сейчас»

GET /api/health                          живость
```

Ошибки приходят единым форматом: `{"error": "сообщение"}` + HTTP-код.
Сервис бросает `std::invalid_argument` → 400, `std::out_of_range` →
400, json::exception → 400, остальное → 500.

Полный список — в [`API.md`](API.md).

---

## Примечательные решения и истории

Этот раздел — чтобы будущий ты (или тот, кто будет читать код) не
наступил на те же грабли.

### `using namespace std;` и история про `byte`

В моделях встречается `using namespace std;` в глобальном скоупе.
Это наследие, и оно остаётся — но накладывает ограничение на порядок
инклудов: `cpp-httplib` на Windows тянет `<winsock2.h>` и COM-заголовки
(`objidl.h`, `oaidl.h`), где есть параметры типа `byte`. Если в той
же translation unit окажется `std::byte`, ловим неоднозначность.

Решения в коде:

- В `api/ApiServer.cpp` `httplib.h` включён **первым**, до всего
  остального (комментарий на месте).
- HTTP-валидация полиса вынесена в отдельный
  [`services/InsuranceValidator.cpp`](services/InsuranceValidator.cpp),
  чтобы не тянуть httplib в `PatientService.h` — иначе цепочка
  `PatientService.h → IPatient.h → Patient.h (using namespace std;)`
  ломает сборку.

### HTTP-валидация номера полиса

Полис проверяется не локально, а внешним HTTP-вызовом:

```cpp
GET http://<validator-host>/check?policy=<номер>
```

По сути сервер только проверяет, что номер из 16 цифр — но требование
заказчика именно такое (валидация по HTTP). Таймауты выставлены по
3 секунды; если валидатор недоступен — полис считается невалидным
(закрыто по умолчанию).

### WAL-чекпойнт на закрытие

SQLite по умолчанию пишет в `*.db-wal`, основной файл обновляется
лениво. Это нормально для работы, но неудобно для бэкапов и
переноса: пользователю нужен **один** файл `medflow.db`. Поэтому в
деструкторе `SqliteDb` вызывается `PRAGMA wal_checkpoint(TRUNCATE)`
— после graceful-остановки `-wal`/`-shm` физически удаляются.

### Папка `backups/` — единственная точка монтирования

И живая БД, и снапшоты лежат в одной папке `backups/`. В Docker
монтируется одна эта папка (`-v $PWD/backups:/app/backups`).
`.gitignore` уже исключает `*.db`, `*.bck`, `*.db-wal`, `*.db-shm` —
ничего «личного» в репу случайно не утечёт.

При обновлении версии: `git pull` → `docker build` → `docker run`;
данные на месте.

### Vendored-зависимости

Никаких пакетных менеджеров — `httplib.h`, `json.hpp`, `sqlite3.c/.h`
лежат прямо в репе ([`requirements/`](requirements/README.md)). Это
сознательное упрощение для проекта, где зависимостей по сути три, а
сборку хочется иметь воспроизводимой без сети.

### `findBy(predicate)` и философия «понятный код»

Репозитории используют `findBy(std::function<bool(const T&)>)` —
сервис передаёт лямбду, репозиторий делает `findAll()` и фильтрует.
Это медленно на больших данных и идеологически «неправильно» (на
SQL-бэкенде естественнее было бы делать `WHERE`), но абсолютно
понятно глазу. Учебный проект, читаемость > производительность.

---

## Сборка

### Docker (одна команда)

```bash
docker build -t medflow .
```

Multi-stage: первый этап на `gcc:13-bookworm` собирает бинарь
(static-linked libstdc++/libgcc), второй — кладёт его в
`debian:bookworm-slim` рядом с папкой `web/`. Финальный образ — ~80 MB.

### Windows / Visual Studio

```
MedFlow_BY_LLl-23.slnx  →  Build → Debug | x64
```

Зависимости (`sqlite3.c`, `httplib.h`, `json.hpp`) уже включены в
`.vcxproj`. Бинарь падает в `x64/Debug/MedFlow_BY_LLl-23.exe`.

### Linux / g++ напрямую

```bash
gcc -O2 -DSQLITE_THREADSAFE=1 -c requirements/sqlite3.c -o /tmp/sqlite3.o
g++ -std=c++20 -O2 -pthread -static-libstdc++ -static-libgcc \
    MedFlow_BY_LLl-23.cpp api/ApiServer.cpp services/InsuranceValidator.cpp \
    /tmp/sqlite3.o -ldl -o medflow
```

(Это ровно то, что делает `Dockerfile` в build-стадии.)

---

## Тестовые данные

В [`tools/gen_seed.py`](tools/README.md) лежит генератор реалистичного
бэкапа, соблюдающий все доменные инварианты (иначе импорт упадёт):

```bash
cd tools
python gen_seed.py
# → ../seed.bck

curl -X POST -H "Content-Type: application/json" \
     --data-binary @../seed.bck \
     http://localhost:8420/api/backup/import
```

Сид детерминирован (`SEED = 42`).

---

## Лицензии

Код самого проекта — учебный, без явной лицензии.

Vendored-зависимости (см. [`requirements/`](requirements/README.md)):

- **cpp-httplib** — MIT
- **nlohmann/json** — MIT
- **SQLite** — Public Domain
