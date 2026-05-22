#pragma once
#include "../../requirements/sqlite3.h"
#include <string>
#include <stdexcept>
#include <mutex>

// Единая обёртка над соединением sqlite3*. Все Sqlite*-репозитории
// разделяют один экземпляр SqliteDb через shared_ptr и сериализуют доступ
// общим мьютексом — sqlite3 в режиме SERIALIZED тоже потокобезопасен,
// но prepared statement-ы не реентерабельны, поэтому проще держать lock_guard
// на время каждой операции.
class SqliteDb {
public:
    explicit SqliteDb(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::string err = db_ ? sqlite3_errmsg(db_) : "open failed";
            if (db_) sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Не удалось открыть БД: " + err);
        }
        execRaw("PRAGMA foreign_keys = ON;");
        execRaw("PRAGMA journal_mode = WAL;");
        createSchema();
    }

    ~SqliteDb() {
        if (db_) {
            // Сливаем WAL в основной файл: после закрытия можно перетянуть
            // один medflow.db в новую папку проекта без -wal/-shm и не
            // потерять данные.
            sqlite3_exec(db_, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
            sqlite3_close(db_);
        }
    }

    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;

    sqlite3*    handle() { return db_; }
    std::mutex& mutex()  { return mtx_; }

    void execRaw(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown SQL error";
            sqlite3_free(err);
            throw std::runtime_error("SQL: " + msg + " | " + sql);
        }
    }

private:
    void createSchema() {
        execRaw(
            "CREATE TABLE IF NOT EXISTS clinics ("
            "  id      INTEGER PRIMARY KEY,"
            "  name    TEXT NOT NULL,"
            "  address TEXT NOT NULL,"
            "  phone   TEXT NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS specializations ("
            "  id   INTEGER PRIMARY KEY,"
            "  name TEXT NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS rooms ("
            "  id        INTEGER PRIMARY KEY,"
            "  number    TEXT NOT NULL,"
            "  floor     INTEGER NOT NULL,"
            "  clinic_id INTEGER NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS doctors ("
            "  id                INTEGER PRIMARY KEY,"
            "  first_name        TEXT NOT NULL,"
            "  last_name         TEXT NOT NULL,"
            "  middle_name       TEXT NOT NULL,"
            "  phone             TEXT NOT NULL,"
            "  specialization_id INTEGER NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS patients ("
            "  id            INTEGER PRIMARY KEY,"
            "  first_name    TEXT NOT NULL,"
            "  last_name     TEXT NOT NULL,"
            "  middle_name   TEXT NOT NULL,"
            "  gender        INTEGER NOT NULL,"
            "  birth_date    TEXT NOT NULL,"
            "  phone         TEXT NOT NULL,"
            "  insurance_num TEXT NOT NULL,"
            "  clinic_id     INTEGER NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS schedules ("
            "  id                INTEGER PRIMARY KEY,"
            "  doctor_id         INTEGER NOT NULL,"
            "  room_id           INTEGER NOT NULL,"
            "  day_of_week       INTEGER NOT NULL,"
            "  time_from         TEXT NOT NULL,"
            "  time_to           TEXT NOT NULL,"
            "  slot_duration_min INTEGER NOT NULL,"
            "  break_after_min   INTEGER NOT NULL"
            ");"

            "CREATE TABLE IF NOT EXISTS appointments ("
            "  id           INTEGER PRIMARY KEY,"
            "  patient_id   INTEGER NOT NULL,"
            "  doctor_id    INTEGER NOT NULL,"
            "  scheduled_at TEXT NOT NULL,"
            "  status       INTEGER NOT NULL,"
            "  notes        TEXT NOT NULL,"
            "  created_at   TEXT NOT NULL"
            ");"
        );
    }

    sqlite3*   db_ = nullptr;
    std::mutex mtx_;
};

// RAII-обёртка над sqlite3_stmt. Использовать локально, в пределах одной
// операции репозитория. Все bind*-параметры 1-based, column*-индексы 0-based.
class SqliteStmt {
public:
    SqliteStmt(sqlite3* db, const std::string& sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("SQL prepare: ") + sqlite3_errmsg(db) + " | " + sql);
        }
    }
    ~SqliteStmt() { if (stmt_) sqlite3_finalize(stmt_); }

    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;

    void bindInt (int idx, int v)                 { sqlite3_bind_int (stmt_, idx, v); }
    void bindText(int idx, const std::string& v)  { sqlite3_bind_text(stmt_, idx, v.c_str(), -1, SQLITE_TRANSIENT); }

    int step() {
        int rc = sqlite3_step(stmt_);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            throw std::runtime_error(std::string("SQL step: ") + sqlite3_errmsg(db_));
        }
        return rc;
    }

    int         columnInt (int col) { return sqlite3_column_int(stmt_, col); }
    std::string columnText(int col) {
        const unsigned char* p = sqlite3_column_text(stmt_, col);
        return p ? std::string(reinterpret_cast<const char*>(p)) : std::string();
    }

private:
    sqlite3*      db_   = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};
