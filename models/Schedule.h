#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"

enum class DayOfWeek {
    Monday = 1,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday,
    Sunday
};

inline std::string dayToString(DayOfWeek d) {
    switch (d) {
    case DayOfWeek::Monday:    return "Понедельник";
    case DayOfWeek::Tuesday:   return "Вторник";
    case DayOfWeek::Wednesday: return "Среда";
    case DayOfWeek::Thursday:  return "Четверг";
    case DayOfWeek::Friday:    return "Пятница";
    case DayOfWeek::Saturday:  return "Суббота";
    case DayOfWeek::Sunday:    return "Воскресенье";
    }
    return "";
}

struct Schedule {
    int        id = -1;
    int        doctorId = -1;
    int        roomId = -1;
    DayOfWeek  dayOfWeek = DayOfWeek::Monday;
    std::string timeFrom = "00:00";       // "HH:MM"
    std::string timeTo = "00:00";         // "HH:MM"
    int        slotDurationMin = 0; // длина приёма
    int        breakAfterMin = 0;   // перерыв после приёма

    void print() const {
        std::cout << "ID врача: " << doctorId << std::endl;
        std::cout << "ID кабинета: " << roomId << std::endl;
        std::cout << "День недели: " << dayToString(dayOfWeek) << std::endl;
        std::cout << "Время с: " << timeFrom << std::endl;
        std::cout << "Время по: " << timeTo << std::endl;
        std::cout << "Длительность приёма (мин): " << slotDurationMin << std::endl;
        std::cout << "Перерыв после приёма (мин): " << breakAfterMin << std::endl;
	}
};

inline void to_json(nlohmann::json& j, const Schedule& s) {
    j = nlohmann::json{
        {"id", s.id},
        {"doctorId", s.doctorId},
        {"roomId", s.roomId},
        {"dayOfWeek", static_cast<int>(s.dayOfWeek)},
        {"timeFrom", s.timeFrom},
        {"timeTo", s.timeTo},
        {"slotDurationMin", s.slotDurationMin},
        {"breakAfterMin", s.breakAfterMin}
    };
}
inline void from_json(const nlohmann::json& j, Schedule& s) {
    s.id              = j.value("id", -1);
    s.doctorId        = j.value("doctorId", -1);
    s.roomId          = j.value("roomId", -1);
    s.dayOfWeek       = static_cast<DayOfWeek>(j.value("dayOfWeek", 1));
    s.timeFrom        = j.value("timeFrom", std::string{"00:00"});
    s.timeTo          = j.value("timeTo", std::string{"00:00"});
    s.slotDurationMin = j.value("slotDurationMin", 0);
    s.breakAfterMin   = j.value("breakAfterMin", 0);
}
