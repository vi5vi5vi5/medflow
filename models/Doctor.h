#pragma once
#include <string>
#include <iostream>
#include "../requirements/json.hpp"
using namespace std;


struct Doctor {
	int		id = -1;
	string	first_name = "Name_Unknown";
	string	last_name = "LName_Unknown";
	string	middle_name = "MName_Unknown";
	string	phone = "Phone_Unknown";
	int		specialization_id = -1;

	string name() const {
		return last_name + " " + first_name + " " + middle_name;
	}

	void print() const {
		cout << "ID: " << id << endl;
		cout << "Доктор: " << name() << endl;
		cout << "Телефон: " << phone << endl;
		cout << "Специализация: " << specialization_id << endl;
	}
};

inline void to_json(nlohmann::json& j, const Doctor& d) {
    j = nlohmann::json{
        {"id", d.id},
        {"first_name", d.first_name},
        {"last_name", d.last_name},
        {"middle_name", d.middle_name},
        {"phone", d.phone},
        {"specialization_id", d.specialization_id}
    };
}
inline void from_json(const nlohmann::json& j, Doctor& d) {
    d.id                = j.value("id", -1);
    d.first_name        = j.value("first_name", std::string{"Name_Unknown"});
    d.last_name         = j.value("last_name", std::string{"LName_Unknown"});
    d.middle_name       = j.value("middle_name", std::string{""});
    d.phone             = j.value("phone", std::string{"Phone_Unknown"});
    d.specialization_id = j.value("specialization_id", -1);
}