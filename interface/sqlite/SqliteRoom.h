#pragma once
#include "../Abstract/IRoom.h"
#include "SqliteDb.h"
#include <memory>

class SqliteRoom : public IRoom {
public:
    explicit SqliteRoom(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Room& r) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (r.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO rooms (number, floor, clinic_id) VALUES (?, ?, ?);");
            s.bindText(1, r.number);
            s.bindInt (2, r.floor);
            s.bindInt (3, r.clinicId);
            s.step();
            r.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO rooms (id, number, floor, clinic_id) VALUES (?, ?, ?, ?);");
            s.bindInt (1, r.id);
            s.bindText(2, r.number);
            s.bindInt (3, r.floor);
            s.bindInt (4, r.clinicId);
            s.step();
        }
    }

    std::optional<Room> findById(int id) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, number, floor, clinic_id FROM rooms WHERE id = ?;");
        s.bindInt(1, id);
        if (s.step() == SQLITE_ROW) {
            Room r;
            r.id       = s.columnInt (0);
            r.number   = s.columnText(1);
            r.floor    = s.columnInt (2);
            r.clinicId = s.columnInt (3);
            return r;
        }
        return std::nullopt;
    }

    std::vector<Room> findAll() const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, number, floor, clinic_id FROM rooms;");
        std::vector<Room> out;
        while (s.step() == SQLITE_ROW) {
            Room r;
            r.id       = s.columnInt (0);
            r.number   = s.columnText(1);
            r.floor    = s.columnInt (2);
            r.clinicId = s.columnInt (3);
            out.push_back(std::move(r));
        }
        return out;
    }

    bool removeBy(int id) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM rooms WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

    // Лямбда непрозрачна для SQLite — грузим всё через findAll() и фильтруем.
    std::vector<Room> findBy(const std::function<bool(const Room&)>& pred) const override {
        std::vector<Room> result;
        for (const auto& r : findAll()) {
            if (pred(r)) result.push_back(r);
        }
        return result;
    }

private:
    std::shared_ptr<SqliteDb> db_;
};
