# api/ — HTTP-слой и REST API MedFlow

HTTP-сервер MedFlow: REST-эндпоинты + раздача статического веб-интерфейса.
Источник правды по поведению — [`ApiServer.cpp`](ApiServer.cpp) и сервисы из
[`../services`](../services). Этот README описывает и **устройство слоя**, и
**полный справочник эндпоинтов**.

- **База API:** `http://localhost:8420/api`
- **Слушает:** `0.0.0.0:8420` (порт задаётся константой `kPort` в [`../MedFlow_BY_LLl-23.cpp`](../MedFlow_BY_LLl-23.cpp) и пробрасывается в конструктор `ApiServer`)
- **Формат тела и ответа:** `application/json` (UTF-8)
- **CORS:** `Access-Control-Allow-Origin: *`, методы `GET, POST, PATCH, DELETE, OPTIONS`, заголовки `Content-Type`. Preflight (`OPTIONS`) на любой путь → `204`.
- **Авторизация:** отсутствует (учебный проект).
- **Хранилище:** SQLite-файл `backups/medflow.db`. Схема накатывается при старте, данные между запусками сохраняются. JSON-бэкап на старте **не** загружается — восстановление только явно, через [`POST /api/backup/load`](#backup--apibackup). При штатном завершении (`SIGINT`/`SIGTERM`) сервер пишет снапшот `backups/bck_<timestamp>.bck`.
- **«Сегодня» и «сейчас»:** берутся из реальных системных часов сервера (не захардкожены). `today` — текущая дата по UTC, `now` — текущее локальное время сервера. Эти значения управляют фильтрацией прошедших слотов и доступных дней — проверить их можно через [`GET /api/schedules/today`](#get-apischedulestoday).

---

## Архитектура слоя

Один файл — один сервер на [cpp-httplib](../requirements/httplib.h). Внешний
интерфейс намеренно крошечный, вся механика спрятана за PIMPL.

### Файлы

- **[`ApiServer.h`](ApiServer.h)** — публичный интерфейс: конструктор (порт + ссылки на восемь сервисов), `run()`, `stop()`. Зависит от сервисов из [`../services`](../services), но не от конкретных репозиториев — слой знает только бизнес-логику, не хранилище.
- **[`ApiServer.cpp`](ApiServer.cpp)** — реализация. Вся логика в `struct ApiServer::Impl` (PIMPL через `std::unique_ptr`), маршруты разложены по группам `setupPatients()` / `setupDoctors()` / … / `setupBackup()` / `setupMisc()`, которые собирает `setupAll()` в конструкторе.

### Поток запроса

```
HTTP → httplib::Server → лямбда-роут → сервис (services/) → JSON-ответ
                              │
                              └─ исключение → set_exception_handler → {"error": ...}
```

### Хелперы (внутри `Impl`)

| Хелпер | Назначение |
|---|---|
| `ok(res, json, status=200)` | Успешный ответ: ставит статус и `application/json`-тело |
| `err(res, status, msg)` | Ошибка единым форматом `{"error": "msg"}` |
| `pathInt(req, idx)` | Достаёт `:id` из regex-группы пути (`std::stoi`) |
| `parseBody(req)` | Парсит тело; пустое тело → `json::object()` (а не ошибка) |

### Соглашения

- Все ответы и ошибки идут только через `ok()` / `err()` — единый формат `{"error": "..."}` для всех 4xx/5xx.
- Исключения сервисного слоя ловятся централизованно в `set_exception_handler`. **Бросить `throw` из сервиса — это штатный способ вернуть пользователю 400** (см. [Обработка ошибок](#обработка-ошибок)).
- Неизвестный роут отрабатывает `set_error_handler`: если тело пустое — отдаёт `{"error":"not found"}` с текущим статусом.
- CORS открыт настежь (`*`), потому что фронт ходит с того же origin. Появятся отдельные домены — ужать в `setupCors()`.
- **`httplib.h` обязан включаться первым** (см. комментарий в начале `ApiServer.cpp`): он тянет `<windows.h>`, и без этого ниже ловится неоднозначность `std::byte` vs `::byte` из Windows SDK.

### Статика и pretty URL

`setupMisc()` монтирует каталог `web/`, пробуя по очереди `./web`, `../web`,
`../../web` (чтобы работало и из дев-сборки, и из Docker-образа). Прописаны
MIME-типы для `html/js/jsx/css` (UTF-8). Поверх статики висят «pretty URL»
(`/clinic/:id`, `/doctor/:id`, `/patient/:id`), которые отдают соответствующий
`*.html`. Если каталог `web/` не найден — на `/` отдаётся минимальная заглушка,
а API продолжает работать. Подробнее — [Веб-интерфейс](#веб-интерфейс-и-pretty-url).

---

## Коды ответов

| Код | Когда |
|---|---|
| `200 OK` | Успех, в теле — JSON-результат |
| `201 Created` | Успех создания (`POST` на коллекцию) |
| `204 No Content` | Ответ на CORS preflight (`OPTIONS`) |
| `302 Found` | Редирект «pretty URL» без id (`/clinic`, `/doctor`, `/patient`) на `/` |
| `400 Bad Request` | Битый JSON, отсутствует обязательный query-параметр, `std::invalid_argument`/`std::out_of_range` от сервиса. Тело: `{"error":"..."}` |
| `404 Not Found` | Ресурс с указанным id не найден либо неизвестный роут. Тело: `{"error":"..."}` |
| `500 Internal Server Error` | Непредвиденная ошибка. Тело: `{"error":"..."}` |

### Обработка ошибок

Все исключения сервисного слоя ловятся централизованно (`set_exception_handler`):

- `std::invalid_argument`, `std::out_of_range` → `400` с текстом исключения (как правило, на русском — это и есть бизнес-сообщение от сервиса);
- `nlohmann::json::exception` (битый JSON, отсутствующее обязательное поле) → `400` с префиксом `JSON error: ...`;
- любое другое исключение → `500`.

```bash
# Невалидный номер полиса
curl -X POST http://localhost:8420/api/patients \
  -H "Content-Type: application/json" \
  -d '{ "first_name":"Иван", "last_name":"Петров", "insuranceNum":"abc" }'
# → 400 {"error":"Номер полиса недействителен (16 цифр)"}

# Битый JSON
curl -X POST http://localhost:8420/api/patients -d 'not-json'
# → 400 {"error":"JSON error: [json.exception.parse_error.101] ..."}

# Несуществующий id
curl -i http://localhost:8420/api/patients/99999
# → 404 {"error":"patient not found"}
```

---

## Домены (shape JSON-объектов)

> Имена полей в JSON именно такие, как ниже (часть в `snake_case`, часть в `camelCase` — так исторически сложилось в моделях). Поля enum'ов сериализуются как целые числа. Определения структур — в [`../models`](../models).

### Patient
```json
{
  "id": 0,
  "insuranceNum": "1234567890123456",
  "first_name": "Иван",
  "last_name": "Иванов",
  "middle_name": "Иванович",
  "gender": 1,               // 0 = Female, 1 = Male, 2 = Unknown
  "birthDate": "1990-01-01",
  "phone": "+375 (29) 000-00-00",
  "clinic_id": 0             // -1 = не привязан к клинике
}
```

### Doctor
```json
{
  "id": 0,
  "first_name": "Ольга",
  "last_name": "Петрова",
  "middle_name": "Ивановна",
  "phone": "+375 (29) 100-00-00",
  "specialization_id": 8     // -1 = без специализации
}
```

### Clinic
```json
{
  "id": 0,
  "name": "Центральная поликлиника №1",
  "address": "г. Минск, ул. Ленина, 15",
  "phone": "+375 (17) 200-10-10"
}
```

### Room
```json
{
  "id": 0,
  "number": "214А",
  "floor": 2,
  "clinicId": 0
}
```

### Specialization
```json
{ "id": 8, "name": "Терапевт" }
```

### Schedule
```json
{
  "id": 0,
  "doctorId": 0,
  "roomId": 0,
  "dayOfWeek": 1,            // 1=Пн, 2=Вт, ..., 7=Вс (ISO)
  "timeFrom": "09:00",
  "timeTo": "17:00",
  "slotDurationMin": 30,
  "breakAfterMin": 5
}
```

### Appointment
```json
{
  "id": 0,
  "patientId": 0,
  "doctorId": 0,                       // -1, если врач был удалён, а приём остался в истории
  "scheduledAt": "2026-04-22 10:30",   // "YYYY-MM-DD HH:MM"
  "status": 0,                          // 0=Scheduled, 1=Completed, 2=Cancelled
  "notes": "",
  "createdAt": "2026-04-20 05:00"       // проставляется сервером
}
```

### TimeSlot (результат поиска свободных окон)
```json
{ "date": "2026-04-22", "time": "10:30", "durationMin": 30, "roomId": 0 }
```

---

## Служебное

### `GET /api/health`
Проверка живости.
```bash
curl http://localhost:8420/api/health
# → {"status":"ok","port":8420}
```

### `GET /api/schedules/today`
Текущие дата и время сервера (реальные системные часы).
```bash
curl http://localhost:8420/api/schedules/today
# → {"today":"2026-06-16","now":"14:05"}
```

---

## Patients — `/api/patients`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/patients` | Все пациенты |
| `GET` | `/api/patients/:id` | Один пациент (`404` если нет) |
| `POST` | `/api/patients` | Создать |
| `POST` | `/api/patients/search` | Поиск по критериям |
| `PATCH` | `/api/patients/:id` | Частичное обновление |
| `DELETE` | `/api/patients/:id` | Удалить (каскадно: вся история приёмов пациента) |

### Создание
```bash
curl -X POST http://localhost:8420/api/patients \
  -H "Content-Type: application/json" \
  -d '{
    "insuranceNum": "1234567890123456",
    "first_name": "Иван",
    "last_name": "Петров",
    "middle_name": "Сергеевич",
    "gender": 1,
    "birthDate": "1985-03-15",
    "phone": "+375 (29) 111-22-33",
    "clinic_id": 0
  }'
# → 201 {"id":5, ... поля как передали}
```
**Обязательны:** `first_name`, `last_name` (непустые) и `insuranceNum`. Остальные поля получают дефолты, если не заданы: `middle_name=""`, `gender=2` (Unknown), `birthDate="BDate_Unknown"`, `phone="Phone_Unknown"`, `clinic_id=-1`.

> ⚠️ **Валидация полиса — удалённая.** `insuranceNum` проверяется HTTP-запросом к внешнему сервису `http://82.40.57.187:8123/check?policy=<num>` (таймаут 3 c). Полис считается валидным, только если сервис ответил `200`. Если сервис недоступен — создание/обновление с этим полем вернёт `400 {"error":"Номер полиса недействителен (16 цифр)"}`. То же при обновлении поля `insuranceNum`.

### Поиск — `POST /api/patients/search`
Тело — объект с любым подмножеством критериев. Пустое тело → вернуть всех.
```bash
curl -X POST http://localhost:8420/api/patients/search \
  -H "Content-Type: application/json" \
  -d '{ "name": "Петров", "gender": 1, "clinic_id": 0 }'
# → [ { ...Patient }, ... ]
```
Принимаемые ключи:

| Ключ | Тип | Сопоставление |
|---|---|---|
| `insuranceNum` | string | подстрока |
| `name` | string | подстрока в `"Фамилия Имя Отчество"` |
| `phone` | string | подстрока |
| `birthDate` | string | подстрока |
| `gender` | int | точное совпадение |
| `clinic_id` | int | точное совпадение |

> Отдельных ключей `first_name`/`last_name`/`middle_name` в поиске нет — ФИО ищется одним полем `name`.

### Обновление
Любое подмножество полей модели Patient. Поле `clinic_id` (если не `-1`) проверяется на существование клиники.
```bash
curl -X PATCH http://localhost:8420/api/patients/5 \
  -H "Content-Type: application/json" \
  -d '{ "phone": "+375 (29) 999-00-11" }'
# → {...обновлённый Patient}
```

### Удаление
Удаляет пациента и **всю** его историю приёмов (любых статусов).
```bash
curl -X DELETE http://localhost:8420/api/patients/5
# → {"removed":true}   // false, если пациента с таким id не было
```

---

## Doctors — `/api/doctors`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/doctors` | Все |
| `GET` | `/api/doctors/:id` | Один (`404` если нет) |
| `POST` | `/api/doctors` | Создать |
| `POST` | `/api/doctors/search` | Поиск |
| `PATCH` | `/api/doctors/:id` | Частичное обновление |
| `DELETE` | `/api/doctors/:id` | Удалить (каскад, см. ниже) |

### Создание
```bash
curl -X POST http://localhost:8420/api/doctors \
  -H "Content-Type: application/json" \
  -d '{
    "first_name": "Ольга",
    "last_name": "Петрова",
    "middle_name": "Ивановна",
    "phone": "+375 (29) 100-00-00",
    "specialization_id": 8
  }'
# → 201 {"id":..., ...}
```
**Обязательны:** `first_name`, `last_name` (непустые). `specialization_id` опционален; если задан и не `-1`, специализация должна существовать (иначе `400`). Дефолты: `middle_name=""`, `phone="Phone_Unknown"`, `specialization_id=-1`.

### Поиск — `POST /api/doctors/search`
| Ключ | Тип | Сопоставление |
|---|---|---|
| `name` | string | подстрока в `"Фамилия Имя Отчество"` |
| `phone` | string | подстрока |
| `specialization_id` | int | точное совпадение |

### Удаление (каскад)
- удаляются все расписания врача;
- удаляются только приёмы со статусом `Scheduled` (будущие незавершённые);
- у приёмов со статусом `Completed`/`Cancelled` обнуляется `doctorId` (`-1`) — они остаются как история.
```bash
curl -X DELETE http://localhost:8420/api/doctors/3
# → {"removed":true}
```

---

## Clinics — `/api/clinics`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/clinics` | Все |
| `GET` | `/api/clinics/:id` | Одна (`404` если нет) |
| `POST` | `/api/clinics` | Создать |
| `PATCH` | `/api/clinics/:id` | Обновить |
| `DELETE` | `/api/clinics/:id` | Удалить (каскад, см. ниже) |

**Обязательны при создании:** `name` и `address` (непустые). `phone` опционален.
```bash
curl -X POST http://localhost:8420/api/clinics \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Частная клиника «Альфа»",
    "address": "г. Минск, ул. Тестовая, 1",
    "phone": "+375 (17) 000-00-00"
  }'
# → 201 {...Clinic}
```

### Удаление (каскад)
Удаляются все кабинеты клиники вместе с их расписаниями; у пациентов этой клиники `clinic_id` сбрасывается в `-1` (сами пациенты остаются).

---

## Rooms — `/api/rooms`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/rooms` | Все |
| `GET` | `/api/rooms/:id` | Одна (`404` если нет) |
| `GET` | `/api/rooms/by-clinic/:clinicId` | Все кабинеты клиники |
| `POST` | `/api/rooms` | Создать |
| `POST` | `/api/rooms/search` | Поиск |
| `PATCH` | `/api/rooms/:id` | Обновить |
| `DELETE` | `/api/rooms/:id` | Удалить (каскадно: расписания этого кабинета) |

**Создание:** `clinicId` должен указывать на существующую клинику (иначе `400`). Дефолты: `number="БезНомера"`, `floor=1`, `clinicId=-1`.
```bash
curl -X POST http://localhost:8420/api/rooms \
  -H "Content-Type: application/json" \
  -d '{ "number": "305Б", "floor": 3, "clinicId": 0 }'
```

### Поиск — `POST /api/rooms/search`
| Ключ | Тип | Сопоставление |
|---|---|---|
| `number` | string | точное совпадение |
| `floor` | int | точное совпадение |
| `clinicId` | int | точное совпадение |

---

## Specializations — `/api/specializations`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/specializations` | Все |
| `GET` | `/api/specializations/:id` | Одна (`404` если нет) |
| `GET` | `/api/specializations/by-name?name=...` | По имени (`400` без `name`, `404` если не найдена) |
| `POST` | `/api/specializations` | Создать (поле `name`, непустое) |
| `PATCH` | `/api/specializations/:id` | Переименовать |
| `DELETE` | `/api/specializations/:id` | Удалить (у докторов `specialization_id` → `-1`) |

```bash
curl "http://localhost:8420/api/specializations/by-name?name=Терапевт"
curl -X POST http://localhost:8420/api/specializations \
  -H "Content-Type: application/json" -d '{ "name": "Офтальмолог" }'
```

---

## Appointments — `/api/appointments`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/appointments` | Все |
| `GET` | `/api/appointments/by-patient/:patientId` | Приёмы пациента |
| `POST` | `/api/appointments` | Создать |
| `POST` | `/api/appointments/search` | Поиск |
| `PATCH` | `/api/appointments/:id/status` | Сменить статус |
| `PATCH` | `/api/appointments/:id/notes` | Обновить заметки |
| `DELETE` | `/api/appointments/:id` | Удалить |

### Создание
В теле нужны только `patientId`, `doctorId`, `scheduledAt`. Остальное проставляет сервер: `status=0` (Scheduled), `notes=""`, `createdAt` = текущие дата+время сервера.
```bash
curl -X POST http://localhost:8420/api/appointments \
  -H "Content-Type: application/json" \
  -d '{
    "patientId": 0,
    "doctorId": 0,
    "scheduledAt": "2026-04-22 10:30"
  }'
# → 201 {...Appointment}
```
Проверки сервиса:
- пациент и доктор существуют (иначе `400`);
- `scheduledAt` строго в формате `YYYY-MM-DD HH:MM` (16 символов) и является корректной календарной датой/временем;
- нет пересечения по времени с другим **не отменённым** приёмом этого **доктора** → иначе `400 {"error":"Доктор уже занят в это время"}`;
- нет пересечения с другим **не отменённым** приёмом этого **пациента** → иначе `400 {"error":"Пациент уже имеет приём в это время"}`.

> Длительность приёма для проверки пересечений берётся из расписания врача, в чей рабочий диапазон попадает время; если подходящего расписания нет — считается 1 минута. Отдельной проверки, что время попадает в рабочее расписание врача, **нет** — формально можно создать приём вне расписания. Чтобы предлагать пользователю только валидные окна, используйте [`/api/schedules/time-slots`](#свободные-временны́е-окна-у-доктора).

### Поиск — `POST /api/appointments/search`
| Ключ | Тип | Сопоставление |
|---|---|---|
| `patientId` | int | точное (`-1`/отсутствует = не фильтровать) |
| `doctorId` | int | точное (`-1`/отсутствует = не фильтровать) |
| `scheduledAt` | string | точное совпадение всей строки |
| `status` | int | точное (`-1`/отсутствует = не фильтровать) |

### Смена статуса
Тело: `{"status": <int>}`, где `0=Scheduled`, `1=Completed`, `2=Cancelled`.
```bash
curl -X PATCH http://localhost:8420/api/appointments/3/status \
  -H "Content-Type: application/json" -d '{ "status": 1 }'
# → {...Appointment с обновлённым status}
```

### Заметки
```bash
curl -X PATCH http://localhost:8420/api/appointments/3/notes \
  -H "Content-Type: application/json" \
  -d '{ "notes": "Пациент явился, жалобы на головную боль" }'
```

### Удаление
```bash
curl -X DELETE http://localhost:8420/api/appointments/3
# → {"removed":true}
```

---

## Schedules — `/api/schedules`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/schedules` | Все расписания |
| `GET` | `/api/schedules/by-doctor/:docId` | Расписания доктора |
| `POST` | `/api/schedules` | Создать |
| `DELETE` | `/api/schedules/:id` | Удалить |
| `GET` | `/api/schedules/available-days?specId=&clinicId=&daysAhead=` | Массив доступных дат (`YYYY-MM-DD`) |
| `GET` | `/api/schedules/doctors-by-day?specId=&date=&clinicId=` | Массив `doctorId`, принимающих в этот день |
| `GET` | `/api/schedules/time-slots?doctorId=&date=&clinicId=` | Массив `TimeSlot` |
| `GET` | `/api/schedules/today` | `{"today":"...", "now":"..."}` |

### Создание
```bash
curl -X POST http://localhost:8420/api/schedules \
  -H "Content-Type: application/json" \
  -d '{
    "doctorId": 0,
    "roomId": 0,
    "dayOfWeek": 1,
    "timeFrom": "09:00",
    "timeTo": "17:00",
    "slotDurationMin": 30,
    "breakAfterMin": 5
  }'
# → 201 {...Schedule}
```
Проверки сервиса: доктор и кабинет существуют; `timeFrom`/`timeTo` парсятся как `HH:MM`; `timeTo > timeFrom`; нет пересечения с существующими расписаниями того же врача в тот же `dayOfWeek`; `slotDurationMin > 0`; `breakAfterMin >= 0`.

### Доступные даты по специализации
Перебирает `daysAhead` дней начиная с сегодняшнего и оставляет те, где у хотя бы одного врача нужной специализации есть свободный слот. `daysAhead` по умолчанию `14`, `clinicId` по умолчанию `-1` (без фильтра по клинике). `specId` обязателен.
```bash
curl "http://localhost:8420/api/schedules/available-days?specId=8&clinicId=0&daysAhead=7"
# → ["2026-06-16","2026-06-17","2026-06-19", ...]
```

### Доктора, принимающие в конкретный день
`specId` и `date` обязательны, `clinicId` по умолчанию `-1`.
```bash
curl "http://localhost:8420/api/schedules/doctors-by-day?specId=8&date=2026-06-19&clinicId=0"
# → [0, 3, 7]
```

### Свободные временны́е окна у доктора
`doctorId` и `date` обязательны, `clinicId` по умолчанию `-1`.
```bash
curl "http://localhost:8420/api/schedules/time-slots?doctorId=0&date=2026-06-19&clinicId=0"
# → [
#   {"date":"2026-06-19","time":"09:00","durationMin":30,"roomId":0},
#   {"date":"2026-06-19","time":"09:35","durationMin":30,"roomId":0},
#   ...
# ]
```
Из выдачи исключаются: слоты, занятые приёмами со статусом ≠ `Cancelled`, и (если `date` = сегодня) уже прошедшие по серверному времени. Если задан `clinicId`, учитываются только расписания, чей кабинет принадлежит этой клинике.

---

## Backup — `/api/backup`

> 🚨 **Автозагрузки бэкапа на старте НЕТ — и это сделано намеренно.**
> При запуске сервер только открывает/создаёт `medflow.db` (SQLite) и накатывает схему; **ни один `.bck` автоматически не подтягивается, даже если БД пустая.** Источник правды — сама БД, а `.bck` — это снимки для ручного/экстренного восстановления. Восстановление из снапшота — всегда явное действие через [`POST /api/backup/load`](#load-загрузка-с-сервера) и целиком на ответственности пользователя.

Полный дамп/восстановление всей БД одним JSON-документом. Формат:
```json
{
  "clinics":         [...],
  "specializations": [...],
  "rooms":           [...],
  "doctors":         [...],
  "patients":        [...],
  "schedules":       [...],
  "appointments":    [...]
}
```

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/backup/export` | Отдаёт весь дамп (JSON) |
| `POST` | `/api/backup/import` | Принимает дамп в теле, **полностью заменяет** состояние БД |
| `POST` | `/api/backup/save` | Сохраняет дамп в файл на сервере |
| `POST` | `/api/backup/load` | Загружает дамп из файла на сервере |
| `GET` | `/api/backup/list` | HTML-страница со списком файлов из `backups/` (ссылки на скачивание) |
| `GET` | `/api/backup/download?file=...` | Скачать конкретный `.bck`-файл |

> **Импорт/загрузка = полное восстановление.** Перед заливкой БД очищается целиком, затем записи вставляются с их собственными `id` из дампа. Это не слияние, а замена снимка.

### Export / Import
```bash
# Скачать весь стейт в локальный файл
curl http://localhost:8420/api/backup/export > dump.json

# Восстановить состояние из локального файла
curl -X POST http://localhost:8420/api/backup/import \
  -H "Content-Type: application/json" --data-binary @dump.json
# → {"imported":true}
```

### Save (сохранение на сервере)
Тело: `{"path":"..."}`. Если `path` не задан — `backups/bck_manual.bck`. Каталог `backups/` создаётся автоматически.
```bash
curl -X POST http://localhost:8420/api/backup/save \
  -H "Content-Type: application/json" -d '{"path":"backups/snapshot1.bck"}'
# → {"saved":true,"path":"backups/snapshot1.bck"}
```

### Load (загрузка с сервера)
Тело: `{"path":"..."}`. Если `path` **не задан** — грузится самый свежий `.bck` из `backups/` (по времени изменения файла, а не по имени — ручные сейвы вроде `bck_manual.bck` не ломают выбор). Если бэкапов нет — `404`.
```bash
# Загрузить последний снапшот
curl -X POST http://localhost:8420/api/backup/load \
  -H "Content-Type: application/json" -d '{}'
# → {"loaded":true,"path":"backups/bck_2026-06-16_14-05-00.bck"}

# Загрузить конкретный файл
curl -X POST http://localhost:8420/api/backup/load \
  -H "Content-Type: application/json" -d '{"path":"backups/snapshot1.bck"}'
```

### List / Download
`GET /api/backup/list` возвращает простую HTML-страницу (свежие — сверху, по времени изменения файла) со ссылками вида `/api/backup/download?file=<имя>`. Скачивание защищено от path-traversal: имя файла — только базовое, без `/`, `\` и `..`, иначе `400`.
```bash
curl "http://localhost:8420/api/backup/download?file=bck_2026-06-16_14-05-00.bck" -o restore.bck
```

---

## Веб-интерфейс и «pretty URL»

Помимо API сервер раздаёт статический фронтенд из каталога [`../web`](../web) (ищется как `./web`, затем `../web`, затем `../../web` относительно бинаря). Подробнее — [`../web/README.md`](../web/README.md).

| Путь | Что отдаёт |
|---|---|
| `GET /` | `web/index.html` — выбор роли (клиника / врач / пациент) |
| `GET /clinic/:id` | `web/clinic.html` — дашборд администратора клиники |
| `GET /doctor/:id` | `web/doctor.html` — дашборд врача |
| `GET /patient/:id` | `web/patient.html` — дашборд пациента |
| `GET /clinic`, `/doctor`, `/patient` | `302`-редирект на `/` (id не указан) |

Если каталог `web/` не найден, на `/` отдаётся минимальная HTML-заглушка, а API продолжает работать.

---

## Типовые сценарии

### 1. Записать пациента на приём (UI-flow)
```
1. GET  /api/specializations                                    → выбрать специализацию
2. GET  /api/schedules/available-days?specId=8&clinicId=0       → выбрать дату
3. GET  /api/schedules/doctors-by-day?specId=8&date=...&clinicId=0  → выбрать доктора
4. GET  /api/schedules/time-slots?doctorId=0&date=...&clinicId=0    → выбрать слот
5. POST /api/appointments { patientId, doctorId, scheduledAt }  → создана запись
```

### 2. Отменить запись
```
PATCH /api/appointments/<id>/status   body: {"status": 2}
```

### 3. Список приёмов пациента
```
GET /api/appointments/by-patient/<patientId>
```

### 4. Все свободные дни терапевтов клиники №0 на 2 недели
```
GET /api/schedules/available-days?specId=8&clinicId=0&daysAhead=14
```

### 5. Снять и восстановить полный снапшот БД
```
GET  /api/backup/export                       → сохранить dump.json локально
POST /api/backup/import  (тело = dump.json)   → восстановить состояние
```

---

## Запуск

Порт — `8420`. Рабочие данные (SQLite-БД и `.bck`-снапшоты) лежат в каталоге [`../backups`](../backups) рядом с бинарём; в Docker этот каталог имеет смысл монтировать наружу для персистентности.

### Локально (Windows, MSVC, x64)
Собрать решение в Visual Studio и запустить бинарь (точка входа — [`../MedFlow_BY_LLl-23.cpp`](../MedFlow_BY_LLl-23.cpp)). При старте создаётся `backups/medflow.db`. Health-чек: `http://localhost:8420/api/health`.

### Docker (Linux)
```bash
docker build -t medflow .
docker run --rm -p 8420:8420 -v "$(pwd)/backups:/app/backups" medflow
```
Контейнер слушает `8420`, корректно реагирует на `docker stop` (`SIGTERM` → graceful-остановка + снапшот на выходе). Статика `web/` копируется в образ рядом с бинарём. Полный гайд по деплою — [`../DEPLOY.md`](../DEPLOY.md).

> Валидация страховых полисов ходит на внешний сервис `82.40.57.187:8123` — для создания/обновления пациентов с `insuranceNum` контейнеру нужен исходящий доступ в сеть.
