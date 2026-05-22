
#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"
using namespace std;


enum class Gender{ Female, Male, Unknown };

inline string to_string(Gender g) {
	switch (g) {
		case Gender::Female	: return "Женщина";
		case Gender::Male: return "Мужчина";
		default: return "БезПола";
	}
};

struct Patient {
	int		id = -1;
	string	first_name = "Name_Unknown";
	string	last_name = "LName_Unknown";
	string	middle_name = "MName_Unknown";
	Gender	gender = Gender::Unknown;
	string	birthDate = "BDate_Unknown";
	string	phone = "Phone_Unknown";
	string	insuranceNum = "Insurance_Unknown";
	int		clinic_id = -1;

	string name() const {
		return last_name + " " + first_name + " " + middle_name;
	}

	void print() const {
		cout << "ID: " << id << endl;
		cout << "Пациент: " << name() << endl;
		cout << "Пол: " << to_string(gender) << endl;
		cout << "Дата рождения: " << birthDate << endl;
		cout << "Телефон: " << phone << endl;
		cout << "Номер полиса: " << insuranceNum << endl;
		cout << "Клиника: " << clinic_id << endl;
	}
};

inline void to_json(nlohmann::json& j, const Patient& p) {
    j = nlohmann::json{
        {"id", p.id},
        {"insuranceNum", p.insuranceNum},
        {"first_name", p.first_name},
        {"last_name", p.last_name},
        {"middle_name", p.middle_name},
        {"gender", static_cast<int>(p.gender)},
        {"birthDate", p.birthDate},
        {"phone", p.phone},
        {"clinic_id", p.clinic_id}
    };
}
inline void from_json(const nlohmann::json& j, Patient& p) {
    p.id           = j.value("id", -1);
    p.insuranceNum = j.value("insuranceNum", std::string{"Insurance_Unknown"});
    p.first_name   = j.value("first_name", std::string{"Name_Unknown"});
    p.last_name    = j.value("last_name", std::string{"LName_Unknown"});
    p.middle_name  = j.value("middle_name", std::string{""});
    p.gender       = static_cast<Gender>(j.value("gender", static_cast<int>(Gender::Unknown)));
    p.birthDate    = j.value("birthDate", std::string{"BDate_Unknown"});
    p.phone        = j.value("phone", std::string{"Phone_Unknown"});
    p.clinic_id    = j.value("clinic_id", -1);
}