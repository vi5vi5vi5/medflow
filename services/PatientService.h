#pragma once
#include "../interface/Abstract/IPatient.h"
#include "../interface/Abstract/IAppointment.h"
#include "../interface/Abstract/IClinic.h"
#include "../requirements/json.hpp"
#include "InsuranceValidator.h"


class PatientService
{
public:
	PatientService(std::shared_ptr<IPatient> patientRepo,
	               std::shared_ptr<IAppointment> apptRepo,
	               std::shared_ptr<IClinic> clinicRepo)
		: patientRepo_(std::move(patientRepo)),
		  apptRepo_(std::move(apptRepo)),
		  clinicRepo_(std::move(clinicRepo)) {}

	Patient newPatientJSON(const nlohmann::json& json) {//создание пациента по json: insuranceNum, first_name, last_name, middle_name, gender, birthDate, phone, clinic_id
		Gender gender = static_cast<Gender>(json.value("gender", static_cast<int>(Gender::Unknown))); //получить gender из id в json, если 0 - женшина, 1 - мужчина, иначе неизвестно
		
		return newPatient(
			json.value("insuranceNum", ""),
			json.value("first_name", ""),
			json.value("last_name", ""),
			json.value("middle_name", ""),
			gender,
			json.value("birthDate", "BDate_Unknown"),
			json.value("phone", "Phone_Unknown"),
			json.value("clinic_id", -1)
		);
	}

	Patient newPatient( // создание пациента по параметрам
		const std::string& insuranceNum, // Номер полиса
		const std::string& first_name, // Имя
		const std::string& last_name, // Фамилия
		const std::string& middle_name = "", // Отчество
		Gender gender = Gender::Unknown, // пол
		const std::string& birthDate = "BDate_Unknown", // Дата рождения
		const std::string& phone = "Phone_Unknown", // Телефон
		int clinic_id = -1) // ID клиники
	{


		if (first_name.empty() or last_name.empty()) {
			throw std::invalid_argument("Имя и фамилия не могут быть пустыми");
		}
		if (insuranceNum.empty()) {
			throw std::invalid_argument("Номер полиса не может быть пустым");
		}
		else if (!validateInsuranceNum(insuranceNum)) {
			throw std::invalid_argument("Номер полиса недействителен (16 цифр)");
		}

		// todo: фио номер и тп должны подтягиваться с сервера валидации страхового номера

		Patient p;
		p.first_name = first_name;
		p.last_name = last_name;
		p.middle_name = middle_name;
		p.gender = gender;
		p.birthDate = birthDate;
		p.phone = phone;
		p.insuranceNum = insuranceNum;
		p.clinic_id = clinic_id;

		patientRepo_->save(p);

		return p;

	};

	std::optional<Patient> findByID(int id) {return patientRepo_->findById(id);} // поиск пациента по id


	std::vector<Patient> findByJson(const nlohmann::json& criteria) { // поиск пациентов по json: insuranceNum name gender
		std::vector< std::function<bool(const Patient&)> > filters;

		if (criteria.contains("insuranceNum")) {
			std::string insuranceNum = criteria["insuranceNum"];
			filters.push_back([insuranceNum](const Patient& p) {
				return p.insuranceNum.find(insuranceNum) != std::string::npos;
			});
		}

		if (criteria.contains("name")) {
			std::string name = criteria["name"];
			filters.push_back([name](const Patient& p) {
				return p.name().find(name) != std::string::npos;
			});
		}

		if (criteria.contains("gender")) {
			Gender gender = static_cast<Gender>(criteria["gender"].get<int>());
			filters.push_back([gender](const Patient& p) {
				return p.gender == gender;
			});
		}

		if (criteria.contains("phone")) {
			std::string phone = criteria["phone"];
			filters.push_back([phone](const Patient& p) {
				return p.phone.find(phone) != std::string::npos;
			});
		}

		if (criteria.contains("clinic_id")) {
			int clinic_id = criteria["clinic_id"];
			filters.push_back([clinic_id](const Patient& p) {
				return p.clinic_id == clinic_id;
			});
		}

		if (criteria.contains("birthDate")) {
			std::string birthDate = criteria["birthDate"];
			filters.push_back([birthDate](const Patient& p) {
				return p.birthDate.find(birthDate) != std::string::npos;
			});
		}
		
		auto Filter = [filters](const Patient& p) {
			for (const auto& filter : filters) {
				if (!filter(p)) return false;
			}
			return true;
			};

		return patientRepo_->findBy(Filter);

	};

	Patient update(int id, const nlohmann::json& json) { // обновление данных пациента по id и json с полями для обновления
		auto patientOpt = patientRepo_->findById(id);
		if (!patientOpt) {
			throw std::invalid_argument("Пациент с таким ID не найден");
		}
		Patient p = patientOpt.value();
		if (json.contains("insuranceNum")) {
			std::string insuranceNum = json["insuranceNum"];
			if (!validateInsuranceNum(insuranceNum)) {
				throw std::invalid_argument("Номер полиса недействителен (16 цифр)");
			}
			p.insuranceNum = insuranceNum;
		}
		if (json.contains("first_name")) {
			p.first_name = json["first_name"];
		}
		if (json.contains("last_name")) {
			p.last_name = json["last_name"];
		}
		if (json.contains("middle_name")) {
			p.middle_name = json["middle_name"];
		}
		if (json.contains("gender")) {
			p.gender = static_cast<Gender>(json["gender"].get<int>());
		}
		if (json.contains("birthDate")) {
			p.birthDate = json["birthDate"];
		}
		if (json.contains("phone")) {
			p.phone = json["phone"];
		}
		if (json.contains("clinic_id")) {
			int clinic_id = json["clinic_id"];
			if (clinic_id != -1 && !clinicRepo_->findById(clinic_id).has_value()) {
				throw std::invalid_argument("Клиника с таким ID не найдена");
			}
			p.clinic_id = clinic_id;
		}

		patientRepo_->save(p);
		return p;
	}

	std::vector<Patient> findAll() { return patientRepo_->findAll(); } // Получение всех пациентов

	// Каскадное удаление пациента: удаляется вся его история приёмов
	// (любые статусы), затем сам пациент.
	bool removeByID(int id) {
		if (!patientRepo_->findById(id).has_value()) return false;
		for (const auto& a : apptRepo_->findBy([id](const Appointment& a) { return a.patientId == id; })) {
			apptRepo_->remove(a.id);
		}
		return patientRepo_->removeBy(id);
	}


private:
	std::shared_ptr<IPatient>     patientRepo_;
	std::shared_ptr<IAppointment> apptRepo_;
	std::shared_ptr<IClinic>      clinicRepo_;

	bool validateInsuranceNum(const std::string& insuranceNum) {
		// Реализация в InsuranceValidator.cpp: ходит на сервер заказчика
		// (httplib живёт в отдельной единице трансляции, чтобы не тянуть
		// windows-заголовки сюда — иначе ловим std::byte vs ::byte).
		return validateInsuranceNumRemote(insuranceNum);
	}
};