# interface/sqlite/ — боевая реализация на SQLite

Репозитории поверх SQLite — то, что собирает [`../../MedFlow_BY_LLl-23.cpp`](../../MedFlow_BY_LLl-23.cpp).
SQL голый, без ORM; единственная зависимость — амальгамация
[`../../requirements/sqlite3.c`](../../requirements/sqlite3.c).

## `SqliteDb.h` — ядро

Две RAII-обёртки, обе header-only.

### `SqliteDb` — общее соединение

Один `sqlite3*` на всё приложение, расшаренный между репозиториями через
`shared_ptr<SqliteDb>`.

- **Конструктор:** открывает файл (в `main` — `backups/medflow.db`), включает
  `PRAGMA foreign_keys = ON` и `PRAGMA journal_mode = WAL`, затем `createSchema()`.
- **`createSchema()`:** набор `CREATE TABLE IF NOT EXISTS`. Полноценных миграций
  нет — схема живёт прямо в коде; меняешь структуру → правишь здесь.
- **Мьютекс (`mutex()`):** репозитории берут `lock_guard` на каждую операцию.
  SQLite в режиме SERIALIZED и сам потокобезопасен, но prepared statement-ы не
  реентерабельны — проще накрыть всё локом, чем ловить гонки.
- **Деструктор:** `PRAGMA wal_checkpoint(TRUNCATE)` + `close`. После остановки
  приложения `medflow.db` самодостаточен — файлы `-wal`/`-shm` схлопываются, и БД
  можно скопировать/перенести одним файлом.

### `SqliteStmt` — обёртка над `sqlite3_stmt`

RAII над prepared statement (`prepare_v2` в конструкторе, `finalize` в деструкторе).
Использовать локально, в пределах одного метода репозитория.

- `bindInt` / `bindText` — параметры **1-based**, текст копируется (`SQLITE_TRANSIENT`).
- `step()` — бросает `std::runtime_error` на всё, кроме `SQLITE_ROW`/`SQLITE_DONE`.
- `columnInt` / `columnText` — колонки **0-based**.
- Поддержаны только `int` и `text` — других типов в схеме нет (даты/время хранятся
  как TEXT, enum'ы — как INTEGER).

## Схема таблиц

Все id — `INTEGER PRIMARY KEY` (autoincrement-подобный rowid). Имена колонок —
`snake_case`, поля моделей — частью `camelCase` (`birth_date`↔`birthDate`,
`insurance_num`↔`insuranceNum`, `scheduled_at`↔`scheduledAt` и т.п.); маппинг
делается вручную в `readRow`/`save` каждого репозитория.

| Таблица | Колонки |
|---|---|
| `clinics` | id, name, address, phone |
| `specializations` | id, name |
| `rooms` | id, number, floor, clinic_id |
| `doctors` | id, first_name, last_name, middle_name, phone, specialization_id |
| `patients` | id, first_name, last_name, middle_name, gender, birth_date, phone, insurance_num, clinic_id |
| `schedules` | id, doctor_id, room_id, day_of_week, time_from, time_to, slot_duration_min, break_after_min |
| `appointments` | id, patient_id, doctor_id, scheduled_at, status, notes, created_at |

> ⚠️ **Внешних ключей в схеме нет.** `PRAGMA foreign_keys = ON` включён, но ни одна
> таблица не объявляет `FOREIGN KEY` — поля вроде `clinic_id`/`doctor_id` это просто
> `INTEGER NOT NULL`. То есть прагма по факту ни на что не влияет, а вся
> ссылочная целостность и каскадные удаления держатся на сервисном слое
> ([`../../services`](../../services)), а не на БД. Это осознанно: каскады в MedFlow
> сложнее «ON DELETE CASCADE» (например, у врача приёмы-история не удаляются, а
> обнуляют `doctor_id`).

## Репозитории

По файлу на сущность, все принимают `shared_ptr<SqliteDb>` и сериализуют доступ
через `lock_guard(db->mutex())`:

`SqliteClinic` · `SqliteRoom` · `SqliteSpecialization` · `SqliteDoctor` ·
`SqlitePatient` · `SqliteSchedule` · `SqliteAppointment`.

Общие приёмы реализации:

- **`save`** — upsert по контракту из [`../Abstract`](../Abstract):
  `id == -1` → `INSERT` + `sqlite3_last_insert_rowid()` обратно в `obj.id`;
  иначе → `INSERT OR REPLACE` с явным id.
- **`removeBy`/`remove`** — `DELETE WHERE id = ?`, результат = `sqlite3_changes() > 0`.
- **`findBy(predicate)`** — лямбда непрозрачна для SQL, поэтому метод грузит всё
  через `findAll()` и фильтрует в C++. Фильтрация в SQL **не** делается — на текущих
  объёмах это нормально, но помни об этом, если данные вырастут.
- **`ISpecialization::findBy(name)`** — единственное исключение, где фильтр уходит
  в SQL: `WHERE name LIKE '%name%' LIMIT 1`.

## Где живут файлы БД

Всё в каталоге `backups/` в корне проекта (создаётся приложением при старте,
монтируется в Docker как единственный bind-mount — см.
[`../../tools/README.md`](../../tools/README.md)):

- `medflow.db` — основной файл;
- `medflow.db-wal`, `medflow.db-shm` — пока процесс жив (схлопываются на выходе);
- `bck_YYYY-MM-DD_HH-MM-SS.bck` — JSON-снапшоты, см.
  [`../../services/BackupService.h`](../../services/BackupService.h).
