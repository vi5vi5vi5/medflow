#pragma once
#include "../Abstract/ISchedule.h"
#include "SqliteDb.h"
#include <memory>

class SqliteSchedule : public ISchedule {
public:
    explicit SqliteSchedule(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Schedule& sch) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (sch.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO schedules "
                "(doctor_id, room_id, day_of_week, time_from, time_to, slot_duration_min, break_after_min) "
                "VALUES (?, ?, ?, ?, ?, ?, ?);");
            s.bindInt (1, sch.doctorId);
            s.bindInt (2, sch.roomId);
            s.bindInt (3, static_cast<int>(sch.dayOfWeek));
            s.bindText(4, sch.timeFrom);
            s.bindText(5, sch.timeTo);
            s.bindInt (6, sch.slotDurationMin);
            s.bindInt (7, sch.breakAfterMin);
            s.step();
            sch.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO schedules "
                "(id, doctor_id, room_id, day_of_week, time_from, time_to, slot_duration_min, break_after_min) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
            s.bindInt (1, sch.id);
            s.bindInt (2, sch.doctorId);
            s.bindInt (3, sch.roomId);
            s.bindInt (4, static_cast<int>(sch.dayOfWeek));
            s.bindText(5, sch.timeFrom);
            s.bindText(6, sch.timeTo);
            s.bindInt (7, sch.slotDurationMin);
            s.bindInt (8, sch.breakAfterMin);
            s.step();
        }
    }

    std::optional<Schedule> findById(int id) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, doctor_id, room_id, day_of_week, time_from, time_to, slot_duration_min, break_after_min "
            "FROM schedules WHERE id = ?;");
        s.bindInt(1, id);
        if (s.step() == SQLITE_ROW) {
            return readRow(s);
        }
        return std::nullopt;
    }

    std::vector<Schedule> findAll() const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, doctor_id, room_id, day_of_week, time_from, time_to, slot_duration_min, break_after_min "
            "FROM schedules;");
        std::vector<Schedule> out;
        while (s.step() == SQLITE_ROW) {
            out.push_back(readRow(s));
        }
        return out;
    }

    bool removeBy(int id) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM schedules WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

    // Лямбда непрозрачна для SQLite — грузим всё через findAll() и фильтруем.
    std::vector<Schedule> findBy(const std::function<bool(const Schedule&)>& pred) const override {
        std::vector<Schedule> result;
        for (const auto& s : findAll()) {
            if (pred(s)) result.push_back(s);
        }
        return result;
    }

private:
    static Schedule readRow(SqliteStmt& s) {
        Schedule sch;
        sch.id              = s.columnInt (0);
        sch.doctorId        = s.columnInt (1);
        sch.roomId          = s.columnInt (2);
        sch.dayOfWeek       = static_cast<DayOfWeek>(s.columnInt(3));
        sch.timeFrom        = s.columnText(4);
        sch.timeTo          = s.columnText(5);
        sch.slotDurationMin = s.columnInt (6);
        sch.breakAfterMin   = s.columnInt (7);
        return sch;
    }

    std::shared_ptr<SqliteDb> db_;
};
