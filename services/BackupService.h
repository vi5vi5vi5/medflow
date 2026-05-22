#pragma once
#include "../interface/Abstract/IClinic.h"
#include "../interface/Abstract/IRoom.h"
#include "../interface/Abstract/ISpecialization.h"
#include "../interface/Abstract/IDoctor.h"
#include "../interface/Abstract/IPatient.h"
#include "../interface/Abstract/ISchedule.h"
#include "../interface/Abstract/IAppointment.h"
#include "../requirements/json.hpp"

#include <fstream>
#include <memory>
#include <string>


// Бэкап/восстановление всей БД через единый JSON-документ.
// Формат совпадает с форматом тестовых данных в main:
// { "clinics": [...], "specializations": [...], "rooms": [...],
//   "doctors": [...], "patients": [...], "schedules": [...],
//   "appointments": [...] }
class BackupService {
public:
    BackupService(std::shared_ptr<IClinic> clinicRepo,
                  std::shared_ptr<IRoom> roomRepo,
                  std::shared_ptr<ISpecialization> specRepo,
                  std::shared_ptr<IDoctor> doctorRepo,
                  std::shared_ptr<IPatient> patientRepo,
                  std::shared_ptr<ISchedule> schedRepo,
                  std::shared_ptr<IAppointment> apptRepo)
        : clinicRepo_(std::move(clinicRepo)),
          roomRepo_(std::move(roomRepo)),
          specRepo_(std::move(specRepo)),
          doctorRepo_(std::move(doctorRepo)),
          patientRepo_(std::move(patientRepo)),
          schedRepo_(std::move(schedRepo)),
          apptRepo_(std::move(apptRepo)) {}

    // Выгрузить всю БД в JSON-объект.
    // to_json для каждой модели делает сериализацию автоматически.
    nlohmann::json exportAll() const {
        return {
            {"clinics",         clinicRepo_->findAll()},
            {"specializations", specRepo_->findAll()},
            {"rooms",           roomRepo_->findAll()},
            {"doctors",         doctorRepo_->findAll()},
            {"patients",        patientRepo_->findAll()},
            {"schedules",       schedRepo_->findAll()},
            {"appointments",    apptRepo_->findAll()}
        };
    }

    // Записать бэкап в файл (pretty-print, 2 пробела).
    void saveToFile(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Не удалось открыть файл для записи: " + path);
        }
        file << exportAll().dump(2);
    }

    // Полная очистка БД. Порядок — обратный зависимостям (сначала
    // ссылающиеся сущности, потом те, на которые ссылаются).
    void clearAll() {
        for (const auto& a : apptRepo_->findAll())   apptRepo_->remove(a.id);
        for (const auto& s : schedRepo_->findAll())  schedRepo_->removeBy(s.id);
        for (const auto& p : patientRepo_->findAll()) patientRepo_->removeBy(p.id);
        for (const auto& d : doctorRepo_->findAll())  doctorRepo_->removeBy(d.id);
        for (const auto& r : roomRepo_->findAll())    roomRepo_->removeBy(r.id);
        for (const auto& s : specRepo_->findAll())    specRepo_->removeBy(s.id);
        for (const auto& c : clinicRepo_->findAll())  clinicRepo_->removeBy(c.id);
    }

    // Загрузить данные из JSON-объекта в репозитории. Бэкап — это полная
    // точка восстановления: перед заливкой полностью чистим БД, чтобы не
    // получить наложение старых и новых записей.
    // Порядок важен: сначала независимые сущности, потом ссылочно зависимые.
    // Каждая запись сохраняется с уже заданным id (из бэкапа), repo.save
    // не создаёт новый id, если поле id != -1.
    void importAll(const nlohmann::json& data) {
        clearAll();
        importArray<Clinic>(data, "clinics", [&](Clinic& c) { clinicRepo_->save(c); });
        importArray<Specialization>(data, "specializations", [&](Specialization& s) { specRepo_->save(s); });
        importArray<Room>(data, "rooms", [&](Room& r) { roomRepo_->save(r); });
        importArray<Doctor>(data, "doctors", [&](Doctor& d) { doctorRepo_->save(d); });
        importArray<Patient>(data, "patients", [&](Patient& p) { patientRepo_->save(p); });
        importArray<Schedule>(data, "schedules", [&](Schedule& s) { schedRepo_->save(s); });
        importArray<Appointment>(data, "appointments", [&](Appointment& a) { apptRepo_->save(a); });
    }

    // Прочитать бэкап из файла и залить в репозитории.
    void loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Не удалось открыть файл для чтения: " + path);
        }
        importAll(nlohmann::json::parse(file));
    }

private:
    // Вспомогательный шаблон: достаёт из документа массив по ключу и
    // скармливает каждый элемент репозиторию через лямбду save.
    template <typename T, typename SaveFn>
    static void importArray(const nlohmann::json& data, const std::string& key, SaveFn save) {
        if (!data.contains(key)) return;
        for (const auto& item : data.at(key)) {
            T obj = item.get<T>(); // вызывает from_json
            save(obj);
        }
    }

    std::shared_ptr<IClinic>         clinicRepo_;
    std::shared_ptr<IRoom>           roomRepo_;
    std::shared_ptr<ISpecialization> specRepo_;
    std::shared_ptr<IDoctor>         doctorRepo_;
    std::shared_ptr<IPatient>        patientRepo_;
    std::shared_ptr<ISchedule>       schedRepo_;
    std::shared_ptr<IAppointment>    apptRepo_;
};
