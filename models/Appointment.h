#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"

enum class AppointmentStatus {
    Scheduled,  // запланирован
    Completed,  // завершён
    Cancelled   // отменён
};

inline std::string statusToString(AppointmentStatus s) {
    switch (s) {
    case AppointmentStatus::Scheduled: return "Запланирован";
    case AppointmentStatus::Completed: return "Завершён";
    case AppointmentStatus::Cancelled: return "Отменён";
    }
    return "Неизвестно";
}

struct Appointment {
    int               id = -1;
    int               patientId = -1;
    int               doctorId = -1;
    std::string       scheduledAt;
    AppointmentStatus status = AppointmentStatus::Scheduled;
    std::string       notes;
    std::string       createdAt;

    void print() const {
        std::cout << "Приём #" << id << std::endl;
        std::cout << "Пациент ID: " << patientId << std::endl;
        std::cout << "Доктор ID: " << doctorId << std::endl;
        std::cout << "Дата и время: " << scheduledAt << std::endl;
        std::cout << "Статус: " << statusToString(status) << std::endl;
        if (!notes.empty()) {
            std::cout << "Заметки: " << notes << std::endl;
        }
        std::cout << "Создан: " << createdAt << std::endl;
	}
};

inline void to_json(nlohmann::json& j, const Appointment& a) {
    j = nlohmann::json{
        {"id", a.id},
        {"patientId", a.patientId},
        {"doctorId", a.doctorId},
        {"scheduledAt", a.scheduledAt},
        {"status", static_cast<int>(a.status)},
        {"notes", a.notes},
        {"createdAt", a.createdAt}
    };
}
inline void from_json(const nlohmann::json& j, Appointment& a) {
    a.id          = j.value("id", -1);
    a.patientId   = j.value("patientId", -1);
    a.doctorId    = j.value("doctorId", -1);
    a.scheduledAt = j.value("scheduledAt", std::string{});
    a.status      = static_cast<AppointmentStatus>(j.value("status", 0));
    a.notes       = j.value("notes", std::string{});
    a.createdAt   = j.value("createdAt", std::string{});
}
