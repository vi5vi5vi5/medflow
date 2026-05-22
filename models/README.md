# models/

Доменные сущности — обычные `struct`-ы с публичными полями. Никакой
логики кроме сериализации в JSON.

## Файлы

- `Clinic.h` — клиника (id, name, address, phone).
- `Room.h` — кабинет (id, number, floor, clinic_id).
- `Specialization.h` — специальность врача (id, name).
- `Doctor.h` — врач (id, ФИО, phone, specialization_id).
- `Patient.h` — пациент (id, ФИО, gender, birthDate, phone,
  insuranceNum, clinic_id) + `enum class Gender`.
- `Schedule.h` — рабочий слот врача в кабинете (day_of_week,
  time_from/to, slot_duration_min, break_after_min).
- `Appointment.h` — приём (patient_id, doctor_id, scheduled_at,
  status, notes, created_at) + `enum class AppointmentStatus`.

## Соглашения

- `id = -1` означает «ещё не сохранено». Репозитории
  (см. [`../interface/Abstract`](../interface/Abstract)) на этом
  значении в `save` присваивают новый id.
- Каждая модель определяет `to_json` / `from_json` для
  [nlohmann/json](../requirements/json.hpp) — благодаря этому
  весь API-слой и `BackupService` работают через `json{obj}`
  без ручной упаковки полей.
- Связи (`clinic_id`, `doctor_id`, …) — это просто int-ы. Целостность
  проверяется в сервисном слое (см. [`../services`](../services)),
  не в моделях и не в БД (FK-колонки объявлены, но `FOREIGN KEY` в
  схеме не используются — нужная проверка делается явно).
- `using namespace std;` в некоторых заголовках — наследие, трогать
  не стоит без необходимости: убирать его придётся комплексно вместе
  с порядком инклудов httplib (см. `services/PatientService.h` —
  там HTTP-валидация специально вынесена в `.cpp` из-за этого).

## Поля даты/времени

Даты — строки в ISO-формате (`YYYY-MM-DD` для дней, `HH:MM` для
времени, `YYYY-MM-DD HH:MM` для меток приёмов). Никаких `chrono`
в моделях — это упрощает JSON и SQL, но валидацию формата делает
сервис, а не модель.
