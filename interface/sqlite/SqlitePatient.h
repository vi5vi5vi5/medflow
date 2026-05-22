#pragma once
#include "../Abstract/IPatient.h"
#include "SqliteDb.h"
#include <memory>

class SqlitePatient : public IPatient {
public:
    explicit SqlitePatient(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Patient& p) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (p.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO patients "
                "(first_name, last_name, middle_name, gender, birth_date, phone, insurance_num, clinic_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
            s.bindText(1, p.first_name);
            s.bindText(2, p.last_name);
            s.bindText(3, p.middle_name);
            s.bindInt (4, static_cast<int>(p.gender));
            s.bindText(5, p.birthDate);
            s.bindText(6, p.phone);
            s.bindText(7, p.insuranceNum);
            s.bindInt (8, p.clinic_id);
            s.step();
            p.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO patients "
                "(id, first_name, last_name, middle_name, gender, birth_date, phone, insurance_num, clinic_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);");
            s.bindInt (1, p.id);
            s.bindText(2, p.first_name);
            s.bindText(3, p.last_name);
            s.bindText(4, p.middle_name);
            s.bindInt (5, static_cast<int>(p.gender));
            s.bindText(6, p.birthDate);
            s.bindText(7, p.phone);
            s.bindText(8, p.insuranceNum);
            s.bindInt (9, p.clinic_id);
            s.step();
        }
    }

    bool removeBy(int id) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM patients WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

    std::optional<Patient> findById(int id) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, first_name, last_name, middle_name, gender, birth_date, phone, insurance_num, clinic_id "
            "FROM patients WHERE id = ?;");
        s.bindInt(1, id);
        if (s.step() == SQLITE_ROW) {
            return readRow(s);
        }
        return std::nullopt;
    }

    std::vector<Patient> findAll() override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, first_name, last_name, middle_name, gender, birth_date, phone, insurance_num, clinic_id "
            "FROM patients;");
        std::vector<Patient> out;
        while (s.step() == SQLITE_ROW) {
            out.push_back(readRow(s));
        }
        return out;
    }

    // Лямбда непрозрачна для SQLite — грузим всё через findAll() и фильтруем.
    std::vector<Patient> findBy(const std::function<bool(const Patient&)>& pred) override {
        std::vector<Patient> result;
        for (const auto& p : findAll()) {
            if (pred(p)) result.push_back(p);
        }
        return result;
    }

private:
    static Patient readRow(SqliteStmt& s) {
        Patient p;
        p.id           = s.columnInt (0);
        p.first_name   = s.columnText(1);
        p.last_name    = s.columnText(2);
        p.middle_name  = s.columnText(3);
        p.gender       = static_cast<Gender>(s.columnInt(4));
        p.birthDate    = s.columnText(5);
        p.phone        = s.columnText(6);
        p.insuranceNum = s.columnText(7);
        p.clinic_id    = s.columnInt (8);
        return p;
    }

    std::shared_ptr<SqliteDb> db_;
};
