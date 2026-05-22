#pragma once
#include "../interface/Abstract/IClinic.h"
#include "../interface/Abstract/IRoom.h"
#include "../interface/Abstract/ISchedule.h"
#include "../interface/Abstract/IPatient.h"
#include "../requirements/json.hpp"

class ClinicService
{
public:
	ClinicService(std::shared_ptr<IClinic> clinicRepo,
	              std::shared_ptr<IRoom> roomRepo,
	              std::shared_ptr<ISchedule> schedRepo,
	              std::shared_ptr<IPatient> patientRepo)
		: clinicRepo_(std::move(clinicRepo)),
		  roomRepo_(std::move(roomRepo)),
		  schedRepo_(std::move(schedRepo)),
		  patientRepo_(std::move(patientRepo)) {}
	Clinic newClinicJSON(const nlohmann::json& json) { // создание клиники по json: name, address, phone
		return newClinic(
			json.value("name", ""),
			json.value("address", ""),
			json.value("phone", "")
		);
	}
	Clinic newClinic( // создание клиники по параметрам
		const std::string& name, // Название клиники
		const std::string& address, // Адрес
		const std::string& phone = "Phone_Unknown" // Телефон
	)
	{
		if (name.empty()) {
			throw std::invalid_argument("Название клиники не может быть пустым");
		}
		if (address.empty()) {
			throw std::invalid_argument("Адрес клиники не может быть пустым");
		}
		Clinic c;
		c.name = name;
		c.address = address;
		c.phone = phone;
		clinicRepo_->save(c);
		return c;
	};

	Clinic update(int id, const nlohmann::json& json) { // обновление данных клиники по id и json с полями для обновления
		auto clinicOpt = clinicRepo_->findById(id);
		if (!clinicOpt) {
			throw std::invalid_argument("Клиника с таким ID не найдена");
		}
		Clinic c = clinicOpt.value();
		if (json.contains("name")) {
			c.name = json["name"];
		}
		if (json.contains("address")) {
			c.address = json["address"];
		}
		if (json.contains("phone")) {
			c.phone = json["phone"];
		}
		clinicRepo_->save(c);
		return c;
	}

	std::optional<Clinic> findByID(int id) { return clinicRepo_->findById(id); } // поиск клиники по id
	std::vector<Clinic> findAll() { return clinicRepo_->findAll(); } // Получение всех клиник

	// Каскадное удаление клиники: все её кабинеты со всеми расписаниями,
	// у пациентов с этой клиникой сбрасывается clinic_id в -1 (пациент остаётся).
	bool removeByID(int id) {
		if (!clinicRepo_->findById(id).has_value()) return false;

		// 1. Найти все кабинеты клиники.
		auto rooms = roomRepo_->findBy([id](const Room& r) { return r.clinicId == id; });

		// 2. Для каждого кабинета удалить его расписания и сам кабинет.
		for (const auto& r : rooms) {
			for (const auto& s : schedRepo_->findBy([rid = r.id](const Schedule& s) { return s.roomId == rid; })) {
				schedRepo_->removeBy(s.id);
			}
			roomRepo_->removeBy(r.id);
		}

		// 3. Сбросить clinic_id у пациентов, привязанных к этой клинике.
		for (auto p : patientRepo_->findBy([id](const Patient& p) { return p.clinic_id == id; })) {
			p.clinic_id = -1;
			patientRepo_->save(p);
		}

		// 4. Удалить саму клинику.
		return clinicRepo_->removeBy(id);
	}

private:
	std::shared_ptr<IClinic>   clinicRepo_;
	std::shared_ptr<IRoom>     roomRepo_;
	std::shared_ptr<ISchedule> schedRepo_;
	std::shared_ptr<IPatient>  patientRepo_;
};