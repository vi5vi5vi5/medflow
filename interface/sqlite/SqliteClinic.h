#pragma once
#include "../Abstract/IClinic.h"
#include "SqliteDb.h"
#include <memory>

class SqliteClinic : public IClinic {
public:
    explicit SqliteClinic(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Clinic& c) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (c.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO clinics (name, address, phone) VALUES (?, ?, ?);");
            s.bindText(1, c.name);
            s.bindText(2, c.address);
            s.bindText(3, c.phone);
            s.step();
            c.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO clinics (id, name, address, phone) VALUES (?, ?, ?, ?);");
            s.bindInt (1, c.id);
            s.bindText(2, c.name);
            s.bindText(3, c.address);
            s.bindText(4, c.phone);
            s.step();
        }
    }

    std::optional<Clinic> findById(int id) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, name, address, phone FROM clinics WHERE id = ?;");
        s.bindInt(1, id);
        if (s.step() == SQLITE_ROW) {
            Clinic c;
            c.id      = s.columnInt (0);
            c.name    = s.columnText(1);
            c.address = s.columnText(2);
            c.phone   = s.columnText(3);
            return c;
        }
        return std::nullopt;
    }

    std::vector<Clinic> findAll() const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, name, address, phone FROM clinics;");
        std::vector<Clinic> out;
        while (s.step() == SQLITE_ROW) {
            Clinic c;
            c.id      = s.columnInt (0);
            c.name    = s.columnText(1);
            c.address = s.columnText(2);
            c.phone   = s.columnText(3);
            out.push_back(std::move(c));
        }
        return out;
    }

    bool removeBy(int id) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM clinics WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

    // Поиск по произвольному C++-предикату. Лямбда непрозрачна для SQLite,
    // поэтому грузим все строки через findAll() и фильтруем в памяти.
    // Замок мьютекса findAll берёт сам — здесь его повторно не захватываем,
    // иначе будет deadlock.
    std::vector<Clinic> findBy(const std::function<bool(const Clinic&)>& pred) const override {
        std::vector<Clinic> result;
        for (const auto& c : findAll()) {
            if (pred(c)) result.push_back(c);
        }
        return result;
    }

private:
    std::shared_ptr<SqliteDb> db_;
};
