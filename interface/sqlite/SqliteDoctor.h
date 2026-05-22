#pragma once
#include "../Abstract/IDoctor.h"
#include "SqliteDb.h"
#include <memory>

class SqliteDoctor : public IDoctor {
public:
    explicit SqliteDoctor(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Doctor& d) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (d.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO doctors "
                "(first_name, last_name, middle_name, phone, specialization_id) "
                "VALUES (?, ?, ?, ?, ?);");
            s.bindText(1, d.first_name);
            s.bindText(2, d.last_name);
            s.bindText(3, d.middle_name);
            s.bindText(4, d.phone);
            s.bindInt (5, d.specialization_id);
            s.step();
            d.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO doctors "
                "(id, first_name, last_name, middle_name, phone, specialization_id) "
                "VALUES (?, ?, ?, ?, ?, ?);");
            s.bindInt (1, d.id);
            s.bindText(2, d.first_name);
            s.bindText(3, d.last_name);
            s.bindText(4, d.middle_name);
            s.bindText(5, d.phone);
            s.bindInt (6, d.specialization_id);
            s.step();
        }
    }

    bool removeBy(int id) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM doctors WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

    std::optional<Doctor> findById(int id) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, first_name, last_name, middle_name, phone, specialization_id "
            "FROM doctors WHERE id = ?;");
        s.bindInt(1, id);
        if (s.step() == SQLITE_ROW) {
            return readRow(s);
        }
        return std::nullopt;
    }

    std::vector<Doctor> findAll() const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, first_name, last_name, middle_name, phone, specialization_id "
            "FROM doctors;");
        std::vector<Doctor> out;
        while (s.step() == SQLITE_ROW) {
            out.push_back(readRow(s));
        }
        return out;
    }

    // Лямбда непрозрачна для SQLite — грузим всё через findAll() и фильтруем.
    std::vector<Doctor> findBy(const std::function<bool(const Doctor&)>& pred) const override {
        std::vector<Doctor> result;
        for (const auto& d : findAll()) {
            if (pred(d)) result.push_back(d);
        }
        return result;
    }

private:
    static Doctor readRow(SqliteStmt& s) {
        Doctor d;
        d.id                = s.columnInt (0);
        d.first_name        = s.columnText(1);
        d.last_name         = s.columnText(2);
        d.middle_name       = s.columnText(3);
        d.phone             = s.columnText(4);
        d.specialization_id = s.columnInt (5);
        return d;
    }

    std::shared_ptr<SqliteDb> db_;
};
