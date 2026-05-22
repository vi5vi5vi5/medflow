#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"

struct Room {
    int         id = -1;
    std::string number = "БезНомера";   // "214А"
    int         floor = 1;
    int         clinicId = -1;

    void print() const {
        std::cout << "ID: " << id << std::endl;
        std::cout << "Номер: " << number << std::endl;
        std::cout << "Этаж: " << floor << std::endl;
        std::cout << "ID клиники: " << clinicId << std::endl;
	}
};

inline void to_json(nlohmann::json& j, const Room& r) {
    j = nlohmann::json{
        {"id", r.id}, {"number", r.number},
        {"floor", r.floor}, {"clinicId", r.clinicId}
    };
}
inline void from_json(const nlohmann::json& j, Room& r) {
    r.id       = j.value("id", -1);
    r.number   = j.value("number", std::string{"БезНомера"});
    r.floor    = j.value("floor", 1);
    r.clinicId = j.value("clinicId", -1);
}
