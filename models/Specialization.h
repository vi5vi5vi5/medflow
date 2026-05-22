#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"
using namespace std;

struct Specialization {
    int     id = -1;
    string name = "БезСпецмализации";  // "Терапевт", "Хирург", ...

    void print() const {
        cout << "Специализация: " << name << endl;
	}
};

inline void to_json(nlohmann::json& j, const Specialization& s) {
    j = nlohmann::json{ {"id", s.id}, {"name", s.name} };
}
inline void from_json(const nlohmann::json& j, Specialization& s) {
    s.id   = j.value("id", -1);
    s.name = j.value("name", std::string{"БезСпецмализации"});
}
