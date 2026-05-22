#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"
using namespace std;

struct Clinic {
    int         id = -1;
    string name = "БезНазвания";
    string address = "БезАдреса";
    string phone = "БезТелефона";

    void print() const {
        cout << "ID: " << id << endl;
        cout << "Название: " << name << endl;
        cout << "Адрес: " << address << endl;
        cout << "Телефон: " << phone << endl;
	}
};

// JSON-сериализация для nlohmann::json (ADL).
inline void to_json(nlohmann::json& j, const Clinic& c) {
    j = nlohmann::json{
        {"id", c.id}, {"name", c.name},
        {"address", c.address}, {"phone", c.phone}
    };
}
inline void from_json(const nlohmann::json& j, Clinic& c) {
    c.id      = j.value("id", -1);
    c.name    = j.value("name", std::string{"БезНазвания"});
    c.address = j.value("address", std::string{"БезАдреса"});
    c.phone   = j.value("phone", std::string{"БезТелефона"});
}
