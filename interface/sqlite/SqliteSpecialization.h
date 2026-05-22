#pragma once
#include "../Abstract/ISpecialization.h"
#include "SqliteDb.h"
#include <memory>

class SqliteSpecialization : public ISpecialization {
public:
    explicit SqliteSpecialization(std::shared_ptr<SqliteDb> db) : db_(std::move(db)) {}

    void save(Specialization& sp) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        if (sp.id == -1) {
            SqliteStmt s(db_->handle(),
                "INSERT INTO specializations (name) VALUES (?);");
            s.bindText(1, sp.name);
            s.step();
            sp.id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));
        } else {
            SqliteStmt s(db_->handle(),
                "INSERT OR REPLACE INTO specializations (id, name) VALUES (?, ?);");
            s.bindInt (1, sp.id);
            s.bindText(2, sp.name);
            s.step();
        }
    }

    std::optional<Specialization> findBy(int id) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, name FROM specializations WHERE id = ?;");
        s.bindInt(1, id);
        if (s.step() == SQLITE_ROW) {
            Specialization sp;
            sp.id   = s.columnInt (0);
            sp.name = s.columnText(1);
            return sp;
        }
        return std::nullopt;
    }

    // Соответствует поведению InMemorySpecialization: поиск подстрокой
    // в name (LIKE '%name%'). Возвращает первую найденную запись.
    std::optional<Specialization> findBy(std::string name) const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, name FROM specializations WHERE name LIKE ? LIMIT 1;");
        s.bindText(1, "%" + name + "%");
        if (s.step() == SQLITE_ROW) {
            Specialization sp;
            sp.id   = s.columnInt (0);
            sp.name = s.columnText(1);
            return sp;
        }
        return std::nullopt;
    }

    std::vector<Specialization> findAll() const override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(),
            "SELECT id, name FROM specializations;");
        std::vector<Specialization> out;
        while (s.step() == SQLITE_ROW) {
            Specialization sp;
            sp.id   = s.columnInt (0);
            sp.name = s.columnText(1);
            out.push_back(std::move(sp));
        }
        return out;
    }

    bool removeBy(int id) override {
        std::lock_guard<std::mutex> lk(db_->mutex());
        SqliteStmt s(db_->handle(), "DELETE FROM specializations WHERE id = ?;");
        s.bindInt(1, id);
        s.step();
        return sqlite3_changes(db_->handle()) > 0;
    }

private:
    std::shared_ptr<SqliteDb> db_;
};
