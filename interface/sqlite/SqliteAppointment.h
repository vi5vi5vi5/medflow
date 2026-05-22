#pragma once
#include "../Abstract/IAppointment.h"
#include "SqliteDb.h"
#include <memory>

class SqliteAppointment : public IAppointment {
public:
    explicit SqliteAppointment(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Appointment& a) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (a.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO appointments "
                "(patient_id, doctor_id, scheduled_at, status, notes, created_at) "
                "VALUES (?, ?, ?, ?, ?, ?);");
            s.bindInt (1, a.patientId);
            s.bindInt (2, a.doctorId);
            s.bindText(3, a.scheduledAt);
            s.bindInt (4, static_cast<int>(a.status));
            s.bindText(5, a.notes);
            s.bindText(6, a.createdAt);
            s.step();
            a.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO appointments "
                "(id, patient_id, doctor_id, scheduled_at, status, notes, created_at) "
                "VALUES (?, ?, ?, ?, ?, ?, ?);");
            s.bindInt (1, a.id);
            s.bindInt (2, a.patientId);
            s.bindInt (3, a.doctorId);
            s.bindText(4, a.scheduledAt);
            s.bindInt (5, static_cast<int>(a.status));
            s.bindText(6, a.notes);
            s.bindText(7, a.createdAt);
            s.step();
        }
    }

    bool remove(int appointmentId) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM appointments WHERE id = ?;");
        s.bindInt(1, appointmentId);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

    std::optional<Appointment> findById(int appointmentId) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, patient_id, doctor_id, scheduled_at, status, notes, created_at "
            "FROM appointments WHERE id = ?;");
        s.bindInt(1, appointmentId);
        if (s.step() == SQLITE_ROW) {
            return readRow(s);
        }
        return std::nullopt;
    }

    std::vector<Appointment> findAll() const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, patient_id, doctor_id, scheduled_at, status, notes, created_at "
            "FROM appointments;");
        std::vector<Appointment> out;
        while (s.step() == SQLITE_ROW) {
            out.push_back(readRow(s));
        }
        return out;
    }

    // Лямбда непрозрачна для SQLite — грузим всё через findAll() и фильтруем.
    std::vector<Appointment> findBy(const std::function<bool(const Appointment&)>& pred) const override {
        std::vector<Appointment> result;
        for (const auto& a : findAll()) {
            if (pred(a)) result.push_back(a);
        }
        return result;
    }

private:
    static Appointment readRow(SqliteStmt& s) {
        Appointment a;
        a.id          = s.columnInt (0);
        a.patientId   = s.columnInt (1);
        a.doctorId    = s.columnInt (2);
        a.scheduledAt = s.columnText(3);
        a.status      = static_cast<AppointmentStatus>(s.columnInt(4));
        a.notes       = s.columnText(5);
        a.createdAt   = s.columnText(6);
        return a;
    }

    std::shared_ptr<SqliteDb> db_;
};
