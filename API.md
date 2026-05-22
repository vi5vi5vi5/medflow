# MedFlow REST API

Справочник по всем эндпоинтам HTTP-сервера MedFlow. Для десктоп-/веб-клиентов.

- **База:** `http://localhost:8420/api`
- **Формат тела и ответа:** `application/json; charset=utf-8`
- **CORS:** `Access-Control-Allow-Origin: *`, методы `GET, POST, PATCH, DELETE, OPTIONS`
- **Авторизация:** отсутствует (учебный проект)
- **Сегодняшняя дата (захардкожена в `ScheduleService`):** `2026-04-20`, «текущее время» `05:00`

## Коды ответов

| Код | Когда |
|---|---|
| `200 OK` | Успех, в теле — JSON-результат |
| `201 Created` | Успех создания (`POST` на список/ресурс) |
| `204 No Content` | Ответ на CORS preflight (`OPTIONS`) |
| `400 Bad Request` | Битый JSON, невалидные параметры, `std::invalid_argument` от сервиса. Тело: `{"error":"..."}` |
| `404 Not Found` | Ресурс с id не найден, либо неизвестный роут |
| `500 Internal Server Error` | Непредвиденная ошибка. Тело: `{"error":"..."}` |

## Домены (shape JSON-объектов)

### Patient
```json
{
  "id": 0,
  "insuranceNum": "100001",
  "first_name": "Иван",
  "last_name": "Иванов",
  "middle_name": "Иванович",
  "gender": 1,               // 0 = Female, 1 = Male, 2 = Unknown
  "birthDate": "1990-01-01",
  "phone": "+375 (29) 000-00-00",
  "clinic_id": 0
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
  "specialization_id": 8
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
  "dayOfWeek": 1,            // 1=Mon, 2=Tue, ..., 7=Sun (ISO)
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
  "doctorId": 0,
  "scheduledAt": "2026-04-22 10:30",   // "YYYY-MM-DD HH:MM"
  "status": 0,                          // 0=Scheduled, 1=Completed, 2=Cancelled
  "notes": "",
  "createdAt": "2026-04-20 05:00"
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
«Текущие» дата и время, как их видит сервис (захардкожены).
```bash
curl http://localhost:8420/api/schedules/today
# → {"today":"2026-04-20","now":"05:00"}
```

---

## Patients — `/api/patients`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/patients` | Все пациенты |
| `GET` | `/api/patients/:id` | Один пациент (404 если нет) |
| `POST` | `/api/patients` | Создать |
| `POST` | `/api/patients/search` | Поиск по критериям |
| `PATCH` | `/api/patients/:id` | Частичное обновление |
| `DELETE` | `/api/patients/:id` | Удалить (каскадно: связанные appointments) |

### Создание
```bash
curl -X POST http://localhost:8420/api/patients \
  -H "Content-Type: application/json" \
  -d '{
    "insuranceNum": "123456",
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
Обязательным является `insuranceNum` (6 цифр, иначе 400). Остальные поля получают дефолты (`"*_Unknown"`, `gender=2`, `clinic_id=-1`), если не заданы.

### Поиск
Любое подмножество полей. Пустой body → все.
```bash
curl -X POST http://localhost:8420/api/patients/search \
  -H "Content-Type: application/json" \
  -d '{ "gender": 1, "clinic_id": 0 }'
# → [ { ...Patient }, ... ]
```
Принимаемые ключи: `insuranceNum`, `first_name`, `last_name`, `middle_name`, `gender`, `phone`, `clinic_id`, `birthDate`.

### Обновление
```bash
curl -X PATCH http://localhost:8420/api/patients/5 \
  -H "Content-Type: application/json" \
  -d '{ "phone": "+375 (29) 999-00-11" }'
# → {...обновлённый Patient}
```

### Удаление
```bash
curl -X DELETE http://localhost:8420/api/patients/5
# → {"removed":true}
```

---

## Doctors — `/api/doctors`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/doctors` | Все |
| `GET` | `/api/doctors/:id` | Один (404 если нет) |
| `POST` | `/api/doctors` | Создать |
| `POST` | `/api/doctors/search` | Поиск (`first_name`, `last_name`, `middle_name`, `phone`, `specialization_id`) |
| `PATCH` | `/api/doctors/:id` | Частичное обновление |
| `DELETE` | `/api/doctors/:id` | Удалить (каскадно: расписания и scheduled-заявки; completed/cancelled — только зануляется `doctorId`) |

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

---

## Clinics — `/api/clinics`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/clinics` | Все клиники |
| `GET` | `/api/clinics/:id` | Одна (404) |
| `POST` | `/api/clinics` | Создать |
| `PATCH` | `/api/clinics/:id` | Обновить |
| `DELETE` | `/api/clinics/:id` | Удалить (каскад: все её комнаты и расписания, у пациентов `clinic_id = -1`) |

```bash
curl -X POST http://localhost:8420/api/clinics \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Частная клиника «Альфа»",
    "address": "г. Минск, ул. Тестовая, 1",
    "phone": "+375 (17) 000-00-00"
  }'
```

---

## Rooms — `/api/rooms`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/rooms` | Все |
| `GET` | `/api/rooms/:id` | Одна (404) |
| `GET` | `/api/rooms/by-clinic/:clinicId` | Все комнаты клиники |
| `POST` | `/api/rooms` | Создать |
| `POST` | `/api/rooms/search` | Поиск (`number`, `floor`, `clinicId`) |
| `PATCH` | `/api/rooms/:id` | Обновить |
| `DELETE` | `/api/rooms/:id` | Удалить (каскадно: связанные расписания) |

```bash
curl -X POST http://localhost:8420/api/rooms \
  -H "Content-Type: application/json" \
  -d '{ "number": "305Б", "floor": 3, "clinicId": 0 }'
```

---

## Specializations — `/api/specializations`

| Метод | Путь | Описание |
|---|---|---|
| `GET` | `/api/specializations` | Все |
| `GET` | `/api/specializations/:id` | Одна (404) |
| `GET` | `/api/specializations/by-name?name=...` | По имени (404 если нет) |
| `POST` | `/api/specializations` | Создать |
| `PATCH` | `/api/specializations/:id` | Переименовать |
| `DELETE` | `/api/specializations/:id` | Удалить (у докторов `specialization_id = -1`) |

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
| `GET` | `/api/appointments/by-patient/:patientId` | По пациенту |
| `POST` | `/api/appointments` | Создать |
| `POST` | `/api/appointments/search` | Поиск (`patientId`, `doctorId`, `scheduledAt`, `status`) |
| `PATCH` | `/api/appointments/:id/status` | Сменить статус |
| `PATCH` | `/api/appointments/:id/notes` | Обновить заметки |
| `DELETE` | `/api/appointments/:id` | Удалить |

### Создание
`scheduledAt` строго `YYYY-MM-DD HH:MM` (16 символов), пациент и доктор должны существовать, слот должен быть свободен.
```bash
curl -X POST http://localhost:8420/api/appointments \
  -H "Content-Type: application/json" \
  -d '{
    "patientId": 0,
    "doctorId": 0,
    "scheduledAt": "2026-04-22 10:30"
  }'
# → 201 Appointment (createdAt заполняется сервером)
```

### Смена статуса
Тело: `{"status": <int>}`, где `0=Scheduled`, `1=Completed`, `2=Cancelled`.
```bash
curl -X PATCH http://localhost:8420/api/appointments/3/status \
  -H "Content-Type: application/json" -d '{ "status": 1 }'
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
```
Сервис валидирует: существуют доктор и комната, `timeTo > timeFrom`, нет пересечений с уже существующими расписаниями доктора в тот же `dayOfWeek`, `slotDurationMin > 0`, `breakAfterMin >= 0`.

### Доступные даты по специализации
`daysAhead` по умолчанию 14, `clinicId` по умолчанию -1 (без фильтра).
```bash
curl "http://localhost:8420/api/schedules/available-days?specId=8&clinicId=0&daysAhead=7"
# → ["2026-04-20","2026-04-21","2026-04-22", ...]
```

### Доктора, принимающие в конкретный день
```bash
curl "http://localhost:8420/api/schedules/doctors-by-day?specId=8&date=2026-04-22&clinicId=0"
# → [0, 3, 7]
```

### Свободные временны́е окна у доктора
```bash
curl "http://localhost:8420/api/schedules/time-slots?doctorId=0&date=2026-04-22&clinicId=0"
# → [
#   {"date":"2026-04-22","time":"09:00","durationMin":30,"roomId":0},
#   {"date":"2026-04-22","time":"09:35","durationMin":30,"roomId":0},
#   ...
# ]
```
Слоты, уже занятые appointments (`status != Cancelled`) и прошедшие (если дата = `today`, а время < `now`), отфильтровываются.

---

## Backup — `/api/backup`

Полный дамп/восстановление всей БД через единый JSON. Формат:
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
| `GET` | `/api/backup/export` | Отдаёт весь дамп |
| `POST` | `/api/backup/import` | Принимает дамп в теле, перезаписывает состояние |
| `POST` | `/api/backup/save` | Сохраняет дамп в файл на сервере. Body: `{"path":"bck1.bck"}` (по умолчанию `bck1.bck`) |
| `POST` | `/api/backup/load` | Загружает из файла. Body: `{"path":"bck1.bck"}` |

```bash
# Скачать весь стейт в файл
curl http://localhost:8420/api/backup/export > dump.json

# Восстановить из файла
curl -X POST http://localhost:8420/api/backup/import \
  -H "Content-Type: application/json" --data-binary @dump.json

# Сохранить на сервере
curl -X POST http://localhost:8420/api/backup/save \
  -H "Content-Type: application/json" -d '{"path":"bck1.bck"}'
# → {"saved":true,"path":"bck1.bck"}
```

---

## Примеры типовых сценариев

### 1. «Записать пациента на приём» (UI-flow)
```
1. GET /api/specializations                   → выбрать специализацию
2. GET /api/schedules/available-days?specId=8&clinicId=0
                                              → выбрать дату
3. GET /api/schedules/doctors-by-day?specId=8&date=2026-04-22&clinicId=0
                                              → выбрать доктора
4. GET /api/schedules/time-slots?doctorId=0&date=2026-04-22&clinicId=0
                                              → выбрать слот
5. POST /api/appointments { patientId, doctorId, scheduledAt }
                                              → создана запись
```

### 2. «Отменить запись»
```
PATCH /api/appointments/<id>/status   body: {"status": 2}
```

### 3. «Список приёмов пациента»
```
GET /api/appointments/by-patient/<patientId>
```

### 4. «Все свободные дни любого терапевта в клинике №0 на 2 недели»
```
GET /api/schedules/available-days?specId=8&clinicId=0&daysAhead=14
```

---

## Обработка ошибок

Любое исключение от сервиса → `400` с описанием на русском от самого сервиса:
```bash
curl -X POST http://localhost:8420/api/patients \
  -H "Content-Type: application/json" \
  -d '{ "insuranceNum": "abc" }'
# → 400 {"error":"Номер страхового полиса должен состоять из 6 цифр"}
```

Битый JSON:
```bash
curl -X POST http://localhost:8420/api/patients -d 'not-json'
# → 400 {"error":"JSON error: [json.exception.parse_error.101] ..."}
```

Неизвестный роут/несуществующий id:
```bash
curl -i http://localhost:8420/api/patients/99999
# HTTP/1.1 404
# {"error":"patient not found"}
```

---

## Зарезервированные UI-роуты

На том же порту 8420 будут жить будущие веб-интерфейсы. Сейчас возвращают `501 Not Implemented` с заглушкой:

- `GET /` — корневая
- `GET /clinic` — админка клиники
- `GET /doctor` — кабинет врача
- `GET /patient` — кабинет пациента

---

## Запуск

### Локально (Windows, MSVC Release x64)
Сборка в Visual Studio → `x64/Release/MedFlow_BY_LLl-23.exe`. Бинарь ищет `bck1.bck` в текущем каталоге при старте.

### Docker (Linux)
```bash
docker build -t medflow .
docker run --rm -p 8420:8420 -v "$(pwd)/bck1.bck:/app/bck1.bck" medflow
```
