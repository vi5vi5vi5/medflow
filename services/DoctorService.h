#pragma once
#include "../interface/Abstract/IDoctor.h"
#include "../interface/Abstract/ISpecialization.h"
#include "../interface/Abstract/ISchedule.h"
#include "../interface/Abstract/IAppointment.h"
#include "../requirements/json.hpp"


class DoctorService
{
public:
	DoctorService(std::shared_ptr<IDoctor> doctorRepo,
	              std::shared_ptr<ISpecialization> specializationRepo,
	              std::shared_ptr<ISchedule> schedRepo,
	              std::shared_ptr<IAppointment> apptRepo)
		: doctorRepo_(std::move(doctorRepo)),
		  specializationRepo_(std::move(specializationRepo)),
		  schedRepo_(std::move(schedRepo)),
		  apptRepo_(std::move(apptRepo)) {}

	Doctor newDoctorJSON(const nlohmann::json& json) {//создание врача по json: first_name, last_name, middle_name, phone, specialization_id

		return newDoctor(
			json.value("first_name", ""),
			json.value("last_name", ""),
			json.value("middle_name", ""),
			json.value("phone", "Phone_Unknown"),
			json.value("specialization_id", -1)
		);
	}

	Doctor newDoctor( // создание врача по параметрам
		const std::string& first_name, // Имя
		const std::string& last_name, // Фамилия
		const std::string& middle_name = "", // Отчество
		const std::string& phone = "Phone_Unknown", // Телефон
		int specialization_id = -1) // ID специализации
	{


		if (first_name.empty() or last_name.empty()) {
			throw std::invalid_argument("Имя и фамилия не могут быть пустыми");
		}
		if (specialization_id != -1) {
			auto specOpt = specializationRepo_->findBy(specialization_id);
			if (!specOpt) {
				throw std::invalid_argument("Специализация с таким ID не найдена");
			}
		}


		Doctor d;
		d.first_name = first_name;
		d.last_name = last_name;
		d.middle_name = middle_name;
		d.phone = phone;
		d.specialization_id = specialization_id;

		doctorRepo_->save(d);

		return d;

	};

	std::optional<Doctor> findByID(int id) {return doctorRepo_->findById(id);} // поиск врача по id

	std::vector<Doctor> findByJson(const nlohmann::json& criteria) { // поиск врачей по json: name, phone, specialization_id
		std::vector< std::function<bool(const Doctor&)> > filters;

		if (criteria.contains("name")) {
			std::string name = criteria["name"];
			filters.push_back([name](const Doctor& d) {
				return d.name().find(name) != std::string::npos;
			});
		}

		if (criteria.contains("phone")) {
			std::string phone = criteria["phone"];
			filters.push_back([phone](const Doctor& d) {
				return d.phone.find(phone) != std::string::npos;
			});
		}

		if (criteria.contains("specialization_id")) {
			int specialization_id = criteria["specialization_id"];
			filters.push_back([specialization_id](const Doctor& d) {
				return d.specialization_id == specialization_id;
			});
		}
		
		auto Filter = [filters](const Doctor& d) {
			for (const auto& filter : filters) {
				if (!filter(d)) return false;
			}
			return true;
			};

		return doctorRepo_->findBy(Filter);

	};

	Doctor update(int id, const nlohmann::json& json) { // обновление данных врача по id и json с полями для обновления
		auto doctorOpt = doctorRepo_->findById(id);
		if (!doctorOpt) {
			throw std::invalid_argument("Врач с таким ID не найден");
		}
		Doctor d = doctorOpt.value();
		if (json.contains("first_name")) {
			d.first_name = json["first_name"];
		}
		if (json.contains("last_name")) {
			d.last_name = json["last_name"];
		}
		if (json.contains("middle_name")) {
			d.middle_name = json["middle_name"];
		}
		if (json.contains("phone")) {
			d.phone = json["phone"];
		}
		if (json.contains("specialization_id")) {

			int specialization_id = json["specialization_id"];
			if (specialization_id != -1) {
				auto specOpt = specializationRepo_->findBy(specialization_id);
				if (!specOpt) {
					throw std::invalid_argument("Специализация с таким ID не найдена");
				}
			}
			d.specialization_id = specialization_id;
		}

		doctorRepo_->save(d);
		return d;
	}

	std::vector<Doctor> findAll() { return doctorRepo_->findAll(); } // Получение всех врачей

	// Каскадное удаление врача:
	// - удаляются все его расписания;
	// - удаляются только Scheduled приёмы (будущие незавершённые);
	// - Completed и Cancelled остаются как история (ссылаются на уже удалённого doctorId).
	bool removeByID(int id) {
		if (!doctorRepo_->findById(id).has_value()) return false;

		for (const auto& s : schedRepo_->findBy([id](const Schedule& s) { return s.doctorId == id; })) {
			schedRepo_->removeBy(s.id);
		}
		for (const auto& a : apptRepo_->findBy([id](const Appointment& a) {
			return a.doctorId == id && a.status == AppointmentStatus::Scheduled;
		})) {
			apptRepo_->remove(a.id);
		}
		for (auto& a : apptRepo_->findBy([id](const Appointment& a) {
			return a.doctorId == id;
			})) {
			a.doctorId = -1; // помечаем как удалённого врача
			apptRepo_->save(a);
		}
		return doctorRepo_->removeBy(id);
	}


private:
	std::shared_ptr<IDoctor>         doctorRepo_;
	std::shared_ptr<ISpecialization> specializationRepo_;
	std::shared_ptr<ISchedule>       schedRepo_;
	std::shared_ptr<IAppointment>    apptRepo_;
};