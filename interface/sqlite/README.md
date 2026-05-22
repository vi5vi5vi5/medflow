# interface/sqlite/

Боевая реализация репозиториев поверх SQLite. То, что использует
`main.cpp`. SQL — голый, без ORM; зависимость — амальгамация
[`requirements/sqlite3.c`](../../requirements/sqlite3.c).

## Файл-«ядро»

**`SqliteDb.h`** — единая обёртка над `sqlite3*`:

- открывает файл (по умолчанию `backups/medflow.db`), включает
  `foreign_keys = ON` и `journal_mode = WAL`,
- в `createSchema()` накатывает все `CREATE TABLE IF NOT EXISTS` —
  миграций как таковых нет, схема живёт прямо в коде,
- держит общий `std::mutex` — все репозитории берут его на каждой
  операции; sqlite в режиме SERIALIZED сам потокобезопасный, но
  prepared statement-ы не реентерабельны, и проще накрыть всё локом,
- в деструкторе делает `PRAGMA wal_checkpoint(TRUNCATE)` — после
  остановки приложения `medflow.db` самодостаточен, файлы
  `-wal`/`-shm` исчезают, можно копировать БД одним файлом.

Плюс `SqliteStmt` — RAII-обёртка над `sqlite3_stmt*` с `bind*`/`step()`/
`column*`. Использовать локально внутри метода репозитория.

## Репозитории

По файлу на сущность, все принимают `shared_ptr<SqliteDb>` и
сериализуют доступ через `lock_guard(db->mutex())`:

- `SqliteClinic` / `SqliteRoom` / `SqliteSpecialization`
- `SqliteDoctor` / `SqlitePatient`
- `SqliteSchedule` / `SqliteAppointment`

## Где живут файлы БД

Всё в папке `backups/` в корне проекта (создаётся приложением при
старте, монтируется в Docker как единственный bind-mount):

- `medflow.db` — основной файл,
- `medflow.db-wal`, `medflow.db-shm` — пока процесс жив,
- `bck_YYYY-MM-DD_HH-MM-SS.bck` — JSON-снапшоты, см.
  [`../../services/BackupService.h`](../../services/BackupService.h).
